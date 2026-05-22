# XNET Compare Sweep Summary

Generated: 2026-03-14 07:26:44 +08:00

- Platform: windows
- Source dir: D:\Git\xrt\dev\bench\sweeps\windows_fresh_20260314_owner_b
- Logs found: 3
- Logs used: 3
- Logs skipped: 0
- Matrix TCP: 500 x 64B
- Matrix TLS: 100 x 64B
- Matrix UDP: 8000 x 128B
- Matrix pressure: messages=512 size=1024 pause_ms=250 drain_timeout_ms=120000

| Metric | Samples | Avg | Min | Max | CV % |
|---|---:|---:|---:|---:|---:|
| xnet-v1 TCP msg/s | 3 | 90.7 | 85.8 | 96.9 | 5.1 |
| xnet-v2 TCP total msg/s | 3 | 82,486.5 | 81,674.0 | 83,654.0 | 1.0 |
| xnet-v2 TCP steady msg/s | 3 | 82,487.9 | 81,675.3 | 83,655.4 | 1.0 |
| xnet-v1 TLS msg/s | 3 | 72.7 | 69.9 | 78.0 | 5.2 |
| xnet-v2 TLS total msg/s | 3 | 60,511.1 | 59,641.0 | 62,019.4 | 1.8 |
| xnet-v2 TLS steady msg/s | 3 | 60,514.8 | 59,644.5 | 62,023.2 | 1.8 |
| xnet-v1 UDP send pps | 3 | 316,297.0 | 313,104.2 | 322,159.8 | 1.3 |
| xnet-v2 UDP send pps | 3 | 1,716,567.6 | 1,710,351.9 | 1,722,096.7 | 0.3 |
| xnet-v2 queue drain ms | 3 | 15.2 | 14.8 | 15.5 | 1.9 |