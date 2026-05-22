# XNET V1 Freeze Manifest

## Snapshot Metadata

- Freeze date: `2026-03-12`
- Source commit: `64be42690871b3d635e8f459932165fc16b33f70`
- Snapshot mode: copy-based

## Implementation Files

- `lib/network.h`
- `lib/netsock.h`
- `lib/netpoll.h`
- `lib/netloop.h`
- `lib/netproxy.h`
- `lib/nettcp.h`
- `lib/netudp.h`
- `lib/nettls.h`
- `lib/nethttp.h`
- `lib/netws.h`
- `lib/nethttpd.h`

## Test Reference Files

- `test/test_network.h`
- `test/test_netsock.h`
- `test/test_netloop.h`
- `test/test_netringbuf.h`
- `test/test_nettcp.h`
- `test/test_nettls.h`
- `test/test_nethttp.h`
- `test/test_nethttpd.h`
- `test/test_netws.h`

## Documentation Reference Files

- `docs/api-network.md`
- `docs/api-network.en.md`
- `docs/api-network-tls.en.md`
- `docs/api-netpoll.md`
- `docs/api-netpoll.en.md`
- `docs/api-nettcp.md`
- `docs/api-nettcp.en.md`
- `docs/api-netudp.md`
- `docs/api-netudp.en.md`
- `docs/api-nettls.md`
- `docs/api-nettls.en.md`
- `docs/api-nethttp.md`
- `docs/api-nethttp.en.md`

## Example Reference Files

- `examples/network/http_client_server/main.c`
- `examples/network/http_client_server/build.bat`
- `examples/network/http_client_server/build.sh`
- `examples/network/tcp_client_server/main.c`
- `examples/network/tcp_client_server/build.bat`
- `examples/network/tcp_client_server/build.sh`
- `examples/network/udp_broadcast/main.c`
- `examples/network/udp_broadcast/build.bat`
- `examples/network/udp_broadcast/build.sh`

## Top-Level Integration Snapshots

- `snapshot/xrt.h`
- `snapshot/xrt.c`
- `snapshot/test.c`
