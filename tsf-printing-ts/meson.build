# Copyright (C) 2025-2026 Interpretica Unipessoal Lda
project('tsf-printing-ts', 'c',
    version: '1.0.0',
    default_options: 'werror=true'
)
te_path = get_option('te_path')
te_libdir = get_option('te_libdir')
add_project_arguments(get_option('te_cflags').split(), language: 'c')
add_project_link_arguments(get_option('te_ldflags').split(), language: 'c')

cc = meson.get_compiler('c')

te_tests_info_sh = find_program(join_paths(te_path, 'te_tests_info.sh'))

test_deps = [
    dependency('threads'),
    dependency('openssl'),
    dependency('zlib'),
    cc.find_library('m'),
]

# The libraries under test come first: a dependant has to be named
# before what it depends on, and tapi_container, tapi_fuzz and tapi_tls
# are built on tapi_cybersec, which is built on tapi_kernel, which is
# built on tapi_devtool.
te_libs = [
    'tapi_print',
    'tapi_cybersec',
    'tapi_kernel',
    'tapi_devtool',
    'tapi_env',
    'tapi_job',
    'tapi_rpc',
    'rcfrpc',
    'tapi',
    'confapi',
    'conf_oid',
    'rcfapi',
    'rpcxdr',
    'rpc_types',
    'logger_ten',
    'ipc',
    'tools',
    'tapi_tad',
    'ndn',
    'asn',
    'logger_core',
]

foreach lib : te_libs
    test_deps += cc.find_library(lib, dirs: te_libdir)
endforeach

lib_dir = include_directories('lib')

# The line below would produce empty dependencies on systems
# having no tirpc - it is not a problem.
dep_tirpc = dependency('libtirpc', required: false)

test_deps += [ dep_tirpc ]

subdir('lib')

test_deps += declare_dependency(include_directories: lib_dir,
                                link_with: [ ts_lib ])


tests = [ 'prologue', 'epilogue' ]

package_dir = 'tsf-printing-ts'
package_tests_c = []

foreach test : tests
    test_exe = test
    test_c = test + '.c'
    package_tests_c += [ test_c ]
    executable(test_exe, test_c, install: true, install_dir: package_dir,
               dependencies: test_deps)
endforeach

install_data([ 'mainpage.dox', 'package.xml' ],
             install_dir: package_dir)

mydir = package_dir

# Unrolled rather than looped: subdir() takes a literal here, and each
# group resets package_dir and package_tests_c for the block below it.
package_dir = join_paths(mydir, 'print')
package_tests_c = []
subdir('print')
