# XNET Compare Sweep Summary

Generated: 2026-03-14 08:18:57 +0800

- Platform: linux
- Source dir: /opt/xrt/dev/bench/sweeps/linux_unpinned_20260314_b
- Logs found: 6
- Logs used: 6
- Logs skipped: 0
- Matrix TCP: 500 x 64B
- Matrix TLS: 100 x 64B
- Matrix UDP: 8000 x 128B
- Matrix pressure: messages=512 size=1024 pause_ms=250 drain_timeout_ms=120000

| Metric | Samples | Avg | Min | Max | CV % |
|---|---:|---:|---:|---:|---:|
| xnet-v1 TCP msg/s | 6 | 64785.5 | 44428.5 | 112981.1 | 34.8 |
| xnet-v2 TCP total msg/s | 6 | 58256.5 | 49324.6 | 60743.1 | 6.9 |
| xnet-v2 TCP steady msg/s | 6 | 58257.4 | 49325.4 | 60743.9 | 6.9 |
| xnet-v2 TLS total msg/s | 6 | 40054.5 | 37928.1 | 43649.9 | 4.3 |
| xnet-v2 TLS steady msg/s | 6 | 40056.4 | 37929.9 | 43652.1 | 4.3 |
| xnet-v1 UDP send pps | 6 | 343198.8 | 334082.4 | 350687.2 | 1.7 |
| xnet-v2 UDP send pps | 6 | 1367019.0 | 1317768.9 | 1387259.6 | 1.8 |
| xnet-v2 queue drain ms | 6 | 933.1 | 372.4 | 1631.8 | 55.8 |

| Incomplete benchmark | Count |
|---|---:|
| xnet-v1 tls | 6 |
