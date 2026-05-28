#!/bin/bash
RUNDIR="$(pwd -P)"
export TS_TOPDIR="$(cd "$(dirname "$(which "$0")")"/.. ; pwd -P)"
export TE_TS_TSF_PRINTING=${TS_TOPDIR}/tsf-printing-ts

function show_usage() {
    echo "Usage:"
    echo "    $0 [parameters] ..."
    echo
    echo "For the ease of use, you can use:"
    echo "    $0 guess [parameters] ..."
    echo "This variant expect the following folder structure:"
    echo " - tsf-printing-ts"
    echo " - test-environment"
    echo
}

function run_fail() {
    echo "Error:" $@ >&2
    exit 1
}

if [ -f /tmp/te.dhcpd.leases ] ; then
    sudo rm /tmp/te.dhcpd*.leases
fi

is_docker="0"
if [ "$1" == "docker" ] ; then
    is_docker="1"
    shift 1
fi

if [ "$1" == "guess" ] ; then
    . ${TS_TOPDIR}/scripts/env.sh
    shift 1
fi

if [ "$is_docker" == "1" ] ; then
    export TE_DOCKER_FILE_DIR="${TS_TOPDIR}/scripts/docker/build"
    export TE_DOCKER_TAG="print-ts-build"
    # The sibling directory as well as the suite and TE, because
    # conf/external.yml tells you to point a library's url at the
    # checkout you are working in and git clones a filesystem path as
    # happily as a URL - but only if that path exists inside the
    # container too. Without this the advice in external.yml is true
    # outside Docker and quietly false within it.
    export TE_DOCKER_MOUNT_PATHS="${TS_TOPDIR}:${TE_BASE}:${TS_TOPDIR}/.."
    export TE_DOCKER_WORK_DIR="$(pwd)"
    # git refuses to touch a repository whose directory belongs to
    # another user, and everything under a bind mount does: the
    # container runs as the host uid, the files carry whatever the
    # mount driver reports, and the two need not agree. The Builder
    # clones the external repositories under build/, so without this
    # it stops with "detected dubious ownership" before anything is
    # compiled. Passed as environment rather than written into a
    # gitconfig because the container's home directory is part of the
    # image and does not survive it.
    export GIT_CONFIG_COUNT=1
    export GIT_CONFIG_KEY_0="safe.directory"
    export GIT_CONFIG_VALUE_0="*"
    export TE_DOCKER_ENV="TE_BASE:TS_TOPDIR:GIT_CONFIG_COUNT:GIT_CONFIG_KEY_0:GIT_CONFIG_VALUE_0"
    ${TS_TOPDIR}/scripts/docker_env.sh ${TS_TOPDIR}/scripts/run.sh $@
    exit $?
fi

if [ -d "${TS_TOPDIR}/../test-environment" ] ; then
    export TE_BASE="${TS_TOPDIR}/../test-environment"
fi

if test -z "${TE_BASE}" ; then
    if test -e dispatcher.sh ; then
        export TE_BASE="${RUNDIR}"
    elif test -e "${TS_TOPDIR}/dispatcher.sh" ; then
        export TE_BASE="${TS_TOPDIR}"
    else
        show_usage
        run_fail "Path to TE sources MUST be specified in TE_BASE"
    fi
fi

usage() {
    cat <<EOF
USAGE: run.sh [run.sh options] [dispatcher.sh options]
Options:
    --cfg=<CFG>             Configuration to be used
EOF
${TE_BASE}/dispatcher.sh --help
exit 1
}

TS_OPTS=""
TS_CFG="localhost"
while test -n "$1" ; do
    case $1 in
        --help)
            usage
            ;;
        --cfg=*)
            TS_CFG=${1#--cfg=}
            ;;
        *)
            TS_OPTS+="$1 "
            ;;
    esac
    shift 1
done

# The scheduler of the agent, which is this container.
# ippeveprinter on CUPS 2.4 refuses to start without DNS-SD, even with
# "-r off", so Avahi - and the D-Bus it needs - run too.
if [ -x /usr/sbin/cupsd ] && ! pgrep -x cupsd >/dev/null ; then
    sudo mkdir -p /run/dbus
    pgrep -x dbus-daemon >/dev/null || sudo dbus-daemon --system --fork
    pgrep -x avahi-daemon >/dev/null || sudo avahi-daemon -D --no-drop-root
    sudo /usr/sbin/cupsd
fi

source "${TE_BASE}/scripts/lib.meta"

te_meta_test_suite tsf-printing-ts ATS
te_meta_add "CFG" "$TS_CFG"

test -z "${TS_CFG}" || TS_OPTS+="--opts=run.conf.${TS_CFG} "

echo "Config is ${TS_CFG}"

if test -z "${TE_BUILD}" ; then
    if test "${TS_TOPDIR}" = "${RUNDIR}" ; then
        TE_BUILD="${TS_TOPDIR}/build"
        mkdir -p build
    else
        TE_BUILD="${RUNDIR}"
    fi
    export TE_BUILD
fi

if [ "${TE_WORKSPACE_DIR}" == "" ] ; then
    export TE_WORKSPACE_DIR="${TE_BUILD}/workspace"
fi

TS_CONF_DIRS=
TS_CONF_DIRS+="\"${TS_TOPDIR}\"/conf:"

TS_DEFAULT_OPTS=

TS_DEFAULT_OPTS+="--conf-dirs=${TS_CONF_DIRS} "
TS_DEFAULT_OPTS+="--external=external.yml "
TS_DEFAULT_OPTS+="--build-parallel "
TS_DEFAULT_OPTS+="--trc-db=\"${TS_TOPDIR}\"/conf/trc.xml "
TS_DEFAULT_OPTS+="--trc-tag=trc_test1 --trc-tag=trc_test2 "

export TE_TMP="${TS_TOPDIR}/tmp"
mkdir -p "${TE_TMP}"

export TE_TS_SRC="${TS_TOPDIR}/tsf-printing-ts"

eval "${TE_BASE}/dispatcher.sh ${TS_DEFAULT_OPTS} ${TS_OPTS} " \
    "--script=scripts/ta-def"
if [ "$?" == "0" ] ; then
    echo OK
else
    echo FAIL
fi

mkdir -p ${TS_TOPDIR}/meson-logs
rm -rf ${TS_TOPDIR}/meson-logs/*
cp -R /tmp/te_ws*/build/meson-logs/* ${TS_TOPDIR}/meson-logs/
