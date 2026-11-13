# TLS dial performance evidence

The developing TLS dial peer has a verified local benchmark workload. Production performance acceptance remains **false**. The [full report](../tests/results/tls-dial-performance-20261114.json) retains every sample, spread, CPU/allocation observation and exact artifact identity.

Reproduce the current profiles with the canonical runner (choose the installed compiler and architecture):

```sh
python tools/measure_performance.py --compiler gcc --arch native --manifest extlibs/xrpc/config/modules.json --config extlibs/xrpc/config/performance_profiles.json --profiles xrpc_tls,xrpc_tls_dial --warmups 1 --repeats 7 --report out/tls-dial-performance.json
```

For parent/off/on comparison and CPU calibration, preserved scripts, original parent source/header blobs, ordered link commands and binaries live in `out/service-extensions/xrpc-tls-dial-perf-20261114/`. No branches or old worktrees are required. The parent object occupies the same link-list position as its replacement.

Hot samples exclude setup/handshake, validate every echoed byte and use one TLS13 fixture-pinned connection, Engine2 and the shared Server defaults. Window32 is a barrier window. Window1 reports P50/P95/P99/P99.9. Cold numeric/prepared/localhost samples include complete factory/call/result/both-peer retirement, explicit TLS13/nonresumed/ALPN/verification-count and reply validation. Host construction/final joins are outside. TLS record/ciphertext copies are included; there is no end-to-end zero-copy claim.

| Host | Payload B | Window | Parent calls/s | Off calls/s | On calls/s | Rate spread percent parent/off/on |
| --- | ---: | ---: | ---: | ---: | ---: | --- |
| windows | 7 | 1 | 11000.507 | 11040.739 | 10835.469 | 10.032 / 7.913 / 5.404 |
| windows | 7 | 32 | 61328.623 | 65418.054 | 64292.35 | 22.56 / 7.031 / 17.651 |
| windows | 16384 | 1 | 2238.881 | 2231.34 | 2242.551 | 2.477 / 2.312 / 2.986 |
| windows | 16384 | 32 | 5725.494 | 5915.669 | 5892.345 | 26.218 / 20.484 / 33.316 |
| wsl | 7 | 1 | 5027.241 | 4993.75 | 5173.165 | 9.615 / 13.51 / 23.453 |
| wsl | 7 | 32 | 37457.462 | 38134.826 | 37348.621 | 29.092 / 18.716 / 8.124 |
| wsl | 16384 | 1 | 1779.472 | 1795.738 | 1765.548 | 6.374 / 7.594 / 15.945 |
| wsl | 16384 | 32 | 5359.41 | 5491.677 | 5520.001 | 11.287 / 26.795 / 5.304 |

This first unpinned cohort has substantial noise. Only Windows16KiB/window1 meets the <5% full-range/median throughput screening bound for all three variants; all its rate comparisons remain within the planned5% loss bound. Its P99 spread fails the10% screening bound. All other hot throughput/P99 comparisons are inconclusive. Seven samples, healthy loopback traffic and closed-loop quantiles do not certify production tails. P99.9 at512 hot/100 cold observations is the sample maximum.

Short Windows CPU observations can be zero/coarse. Separate longer batches each observed >100ms process CPU on both hosts; these are resolution calibration, with allocation statistics disabled. They do not replace any original samples or qualify CPU regressions. XRT allocation traffic is recorded separately; some window profiles differ and no zero-extra-allocation claim is made. Other-process/kernel allocation and copy/synchronization/syscall counts are outside this slice.

The generic benchmark identity now includes the CPU helper, direct fixture/test helper and TLS service helper as applicable, plus negotiated TLS version/cipher and routing mode facts. The canonical parser validated all210 steady observations and its17 existing contract tests pass.

Controlled duration/pinning, fixed-arrival/queue-wait, worker/connection scale, native Linux/72h, broader trust/resumption/fault matrices and the complete original four-product gates remain open.
