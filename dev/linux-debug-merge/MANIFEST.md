# Manifest

Base commit:

- `0c1b065ce4ef95fbff91f5c5d9528a235bf8eb58`

Recorded items:

- `patches/tracked_linux_debug.patch`
- `snapshot/dev/bench/xnet_v1/bench_v1_common.h`
- `snapshot/dev/bench/xnet_v1/bench_echo_tcp_v1.c`
- `snapshot/dev/bench/xnet_v1/bench_echo_tls_v1.c`
- `snapshot/dev/bench/xnet_v1/bench_udp_burst_v1.c`

Linux validation commands used during this round:

```bash
gcc dev/test_xhttp2_only.c xrt.c -I . -O2 -pthread -o dev/test_xhttp2_only_linux
./dev/test_xhttp2_only_linux

gcc dev/test_xnet2_stage.c xrt.c -I . -O2 -pthread -o dev/test_xnet2_stage_linux
./dev/test_xnet2_stage_linux
```

Linux benchmark smoke commands used during this round:

```bash
gcc dev/bench/xnet2/bench_echo_tcp.c xrt.c -I . -O2 -pthread -o dev/bench/xnet2/bench_echo_tcp_linux
gcc dev/bench/xnet2/bench_echo_tls.c xrt.c -I . -O2 -pthread -o dev/bench/xnet2/bench_echo_tls_linux
gcc dev/bench/xnet2/bench_udp_burst.c xrt.c -I . -O2 -pthread -o dev/bench/xnet2/bench_udp_burst_linux
gcc dev/bench/xnet2/bench_send_queue_pressure.c xrt.c -I . -O2 -pthread -o dev/bench/xnet2/bench_send_queue_pressure_linux
gcc dev/bench/xnet2/bench_idle_conn.c xrt.c -I . -O2 -pthread -o dev/bench/xnet2/bench_idle_conn_linux
```

Legacy comparison commands used during this round:

```bash
gcc dev/bench/xnet_v1/bench_echo_tcp_v1.c xrt.c -I . -O2 -pthread -o dev/bench/xnet_v1/bench_echo_tcp_v1_linux
gcc dev/bench/xnet_v1/bench_echo_tls_v1.c xrt.c -I . -O2 -pthread -o dev/bench/xnet_v1/bench_echo_tls_v1_linux
gcc dev/bench/xnet_v1/bench_udp_burst_v1.c xrt.c -I . -O2 -pthread -o dev/bench/xnet_v1/bench_udp_burst_v1_linux
```

Host detail:

- Debian 13 host at `/opt/xrt`
- `xnet-v2` Linux validation done in a normal Debian 13 container
- `xnet-v1` Linux TCP/TLS comparison required `docker run --privileged`
