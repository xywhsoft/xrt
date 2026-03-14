# XNET Compare Sweep Summary

Generated: 2026-03-14 07:19:45 +0800

- Platform: linux
- Source dir: ./dev/bench/sweeps/linux_fresh_20260314_owner
- Logs found: 3
- Logs used: 3
- Logs skipped: 0
- Matrix TCP: 500 x 64B
- Matrix TLS: 100 x 64B
- Matrix UDP: 8000 x 128B
- Matrix pressure: messages=512 size=1024 pause_ms=250 drain_timeout_ms=120000

| Metric | Samples | Avg | Min | Max | CV % |
|---|---:|---:|---:|---:|---:|
| xnet-v1 TCP msg/s | 2 | 89973.9 | 60928.4 | 119019.4 | 32.3 |
| xnet-v2 TCP total msg/s | 3 | 53589.9 | 37258.1 | 61970.4 | 21.6 |
| xnet-v2 TCP steady msg/s | 3 | 53590.7 | 37258.5 | 61971.3 | 21.6 |
| xnet-v2 TLS total msg/s | 3 | 42665.3 | 38511.7 | 45489.2 | 7.0 |
| xnet-v2 TLS steady msg/s | 3 | 42667.7 | 38513.4 | 45492.4 | 7.0 |
| xnet-v1 UDP send pps | 3 | 305578.9 | 225033.6 | 350083.2 | 18.7 |
| xnet-v2 UDP send pps | 3 | 1298514.9 | 1066753.9 | 1431868.0 | 12.7 |
| xnet-v2 queue drain ms | 3 | 510.3 | 372.5 | 584.4 | 19.1 |

| Incomplete benchmark | Count |
|---|---:|
| xnet-v1 tcp | 1 |
| xnet-v1 tls | 3 |
