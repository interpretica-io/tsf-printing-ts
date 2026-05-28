# tsf-printing-ts

A Test Environment suite that exercises
[tsf-printing](https://github.com/interpretica-io/tsf-printing) (`tapi_print`)
against the Test Agent it runs on.

| Test | What it checks |
|---|---|
| `detect` | the print system is found (CUPS), the capability table, the printer list and default |
| `ipp` | IPP straight to a printer: capabilities, Validate-Job, a job's options read back from the printer, hold/cancel on a slow printer, refusing options a backend cannot pass |
| `cups_queue` | a queue from add to remove: default, pause with a reason, reject → `EPERM`, capabilities, removed → `ENOENT` |
| `cups_jobs` | a file printed with options, read back from queue and printer, hold/release, cancel, cancel-all |
| `audit` | a planted clear-text queue and an anonymous IPP printer reported through tsf-cybersec |

Every test starts its own `ippeveprinter` virtual printer on the agent, so
nothing uses paper and the findings are known before the scanner runs.

## Running it

Needs Docker and `test-environment` as a sibling directory:

```bash
./scripts/run.sh docker guess --cfg=localhost
```

`conf/external.yml` names the `tsf-*` repositories the Builder clones. While
developing a library, point its `url` at your local checkout and set `ref`
to your branch. Requires a real Test Agent with `ta_rpcprovider` (started
from `conf/rcf.conf`/`conf/builder.conf`), because every call reads what a
command printed.

Verified green against CUPS 2.4 on Debian 12.
