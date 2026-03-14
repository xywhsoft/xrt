# XNET Compare Sweep Summary

Generated: 2026-03-14 07:59:19 +0800

- Platform: linux
- Source dir: /opt/xrt/dev/bench/sweeps/linux_taskset_20260314_a
- Logs found: 3
- Logs used: 3
- Logs skipped: 0
- Matrix TCP: 500 x 64B
- Matrix TLS: 100 x 64B
- Matrix UDP: 8000 x 128B
- Matrix pressure: messages=512 size=1024 pause_ms=250 drain_timeout_ms=120000

| Metric | Samples | Avg | Min | Max | CV % |
|---|---:|---:|---:|---:|---:|
| xnet-v1 TCP msg/s | 3 | 60981.6 | 48943.4 | 83277.7 | 25.9 |
| xnet-v2 TCP total msg/s | 3 | 60854.1 | 58572.9 | 63877.3 | 3.7 |
| xnet-v2 TCP steady msg/s | 3 | 60855.0 | 58573.7 | 63878.4 | 3.7 |
| xnet-v2 TLS total msg/s | 3 | 42422.9 | 39794.9 | 45521.9 | 5.6 |
| xnet-v2 TLS steady msg/s | 3 | 42425.0 | 39796.5 | 45524.6 | 5.6 |
| xnet-v1 UDP send pps | 3 | 270707.3 | 261227.9 | 276063.8 | 2.5 |
| xnet-v2 UDP send pps | 3 | 955671.2 | 614047.2 | 1380883.6 | 33.3 |
| xnet-v2 queue drain ms | 3 | 372.5 | 372.2 | 372.9 | 0.1 |

| Incomplete benchmark | Count |
|---|---:|
| xnet-v1 tls | 3 |
