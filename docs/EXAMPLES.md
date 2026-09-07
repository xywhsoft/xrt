# XRT 示例索引

此文件由 `tools/generate_example_index.py` 从 `config/modules.json` 生成，
不要手工维护第二份示例清单。构建器会按所属模块的真实依赖闭包编译并运行示例。

当前共登记 `410` 个可运行示例。

## asn1 (5)

- [asn1/decode_tour](../examples/asn1/decode_tour/main.c) - `asn1_der`
- [asn1/der](../examples/asn1/der/main.c) - `asn1_der`
- [asn1/encode_tour](../examples/asn1/encode_tour/main.c) - `asn1_der`
- [asn1/pem](../examples/asn1/pem/main.c) - `pem`
- [asn1/pem_tour](../examples/asn1/pem_tour/main.c) - `asn1_der`

## charset (8)

- [charset/detect](../examples/charset/detect/main.c) - `charset_detect`
- [charset/transcode](../examples/charset/transcode/main.c) - `charset`
- [charset/transcode_tour](../examples/charset/transcode_tour/main.c) - `charset`
- [charset/unicode](../examples/charset/unicode/main.c) - `unicode`
- [charset/unicode_text](../examples/charset/unicode_text/main.c) - `unicode_text`
- [charset/utf16_32](../examples/charset/utf16_32/main.c) - `charset`
- [charset/utf8_edit](../examples/charset/utf8_edit/main.c) - `charset`
- [charset/utf8_search](../examples/charset/utf8_search/main.c) - `charset`

## codec (4)

- [codec/base64](../examples/codec/base64/main.c) - `codec_base64`
- [codec/hex](../examples/codec/hex/main.c) - `codec_hex`
- [codec/percent](../examples/codec/percent/main.c) - `codec_percent`
- [codec/tour](../examples/codec/tour/main.c) - `codec_hex`

## compress (3)

- [compress/deflate](../examples/compress/deflate/main.c) - `deflate`
- [compress/inflate](../examples/compress/inflate/main.c) - `inflate`
- [compress/stream_tour](../examples/compress/stream_tour/main.c) - `deflate`

## concurrency (41)

- [concurrency/bridge_tour](../examples/concurrency/bridge_tour/main.c) - `future_bridge`
- [concurrency/cancel](../examples/concurrency/cancel/main.c) - `cancel`
- [concurrency/channel](../examples/concurrency/channel/main.c) - `channel`
- [concurrency/channel_cancel](../examples/concurrency/channel_cancel/main.c) - `channel_cancel`
- [concurrency/channel_coroutine](../examples/concurrency/channel_coroutine/main.c) - `channel_coroutine`
- [concurrency/channel_select](../examples/concurrency/channel_select/main.c) - `channel_select`
- [concurrency/channel_select_cancel](../examples/concurrency/channel_select_cancel/main.c) - `channel_select_cancel`
- [concurrency/channel_tour](../examples/concurrency/channel_tour/main.c) - `channel`
- [concurrency/condition](../examples/concurrency/condition/main.c) - `cond`
- [concurrency/coroutine](../examples/concurrency/coroutine/main.c) - `coroutine`
- [concurrency/coroutine_event](../examples/concurrency/coroutine_event/main.c) - `coroutine_event`
- [concurrency/coroutine_lifecycle](../examples/concurrency/coroutine_lifecycle/main.c) - `coroutine_scheduler`
- [concurrency/coroutine_scheduler](../examples/concurrency/coroutine_scheduler/main.c) - `coroutine_scheduler`
- [concurrency/coroutine_tour](../examples/concurrency/coroutine_tour/main.c) - `coroutine`
- [concurrency/deadline](../examples/concurrency/deadline/main.c) - `wait`
- [concurrency/executor](../examples/concurrency/executor/main.c) - `executor`
- [concurrency/executor_tour](../examples/concurrency/executor_tour/main.c) - `executor`
- [concurrency/future](../examples/concurrency/future/main.c) - `future`
- [concurrency/future_combine](../examples/concurrency/future_combine/main.c) - `future_combine`
- [concurrency/future_continue](../examples/concurrency/future_continue/main.c) - `future_continue`
- [concurrency/future_coroutine](../examples/concurrency/future_coroutine/main.c) - `future_coroutine`
- [concurrency/future_tour](../examples/concurrency/future_tour/main.c) - `future`
- [concurrency/once](../examples/concurrency/once/main.c) - `once`
- [concurrency/queue_tour](../examples/concurrency/queue_tour/main.c) - `queue`
- [concurrency/report](../examples/concurrency/report/main.c) - `concurrency_report_example`
- [concurrency/rwlock](../examples/concurrency/rwlock/main.c) - `rwlock`
- [concurrency/semaphore](../examples/concurrency/semaphore/main.c) - `sem`
- [concurrency/spin](../examples/concurrency/spin/main.c) - `spin`
- [concurrency/sync](../examples/concurrency/sync/main.c) - `mutex`
- [concurrency/sync_tour](../examples/concurrency/sync_tour/main.c) - `sync`
- [concurrency/task_coroutine](../examples/concurrency/task_coroutine/main.c) - `task_coroutine`
- [concurrency/task_group](../examples/concurrency/task_group/main.c) - `task_group`
- [concurrency/task_group_coroutine](../examples/concurrency/task_group_coroutine/main.c) - `task_group_coroutine`
- [concurrency/task_group_pool](../examples/concurrency/task_group_pool/main.c) - `task_group_pool`
- [concurrency/task_group_scope](../examples/concurrency/task_group_scope/main.c) - `task_group`
- [concurrency/task_pool](../examples/concurrency/task_pool/main.c) - `task_pool`
- [concurrency/task_tour](../examples/concurrency/task_tour/main.c) - `task`
- [concurrency/thread](../examples/concurrency/thread/main.c) - `thread`
- [concurrency/thread_key](../examples/concurrency/thread_key/main.c) - `thread_key`
- [concurrency/thread_tour](../examples/concurrency/thread_tour/main.c) - `thread`
- [concurrency/worker](../examples/concurrency/worker/main.c) - `concurrency_worker_example`

## console (2)

- [console/output](../examples/console/output/main.c) - `console`
- [console/variants](../examples/console/variants/main.c) - `console`

## containers (27)

- [containers/array](../examples/containers/array/main.c) - `array`
- [containers/array_tour](../examples/containers/array_tour/main.c) - `array`, `ptr_array`
- [containers/avl](../examples/containers/avl/main.c) - `avl`
- [containers/avl_tour](../examples/containers/avl_tour/main.c) - `avl`
- [containers/avl_tree](../examples/containers/avl_tree/main.c) - `avl_tree`
- [containers/block_stack](../examples/containers/block_stack/main.c) - `block_stack`
- [containers/buffer](../examples/containers/buffer/main.c) - `buffer`
- [containers/buffer_tour](../examples/containers/buffer_tour/main.c) - `buffer`
- [containers/fixed_stack](../examples/containers/fixed_stack/main.c) - `fixed_stack`
- [containers/int_map](../examples/containers/int_map/main.c) - `int_map`
- [containers/int_map_tour](../examples/containers/int_map_tour/main.c) - `map`
- [containers/list](../examples/containers/list/main.c) - `list`
- [containers/list_tour](../examples/containers/list_tour/main.c) - `list`
- [containers/map](../examples/containers/map/main.c) - `map`
- [containers/map_tour](../examples/containers/map_tour/main.c) - `map`
- [containers/ptr_array](../examples/containers/ptr_array/main.c) - `ptr_array`
- [containers/ptr_fixed_stack](../examples/containers/ptr_fixed_stack/main.c) - `ptr_fixed_stack`
- [containers/ptr_stack](../examples/containers/ptr_stack/main.c) - `ptr_stack`
- [containers/queue_mpmc](../examples/containers/queue_mpmc/main.c) - `queue_mpmc`
- [containers/queue_mpsc](../examples/containers/queue_mpsc/main.c) - `queue_mpsc`
- [containers/queue_spsc](../examples/containers/queue_spsc/main.c) - `queue_spsc`
- [containers/set](../examples/containers/set/main.c) - `set`
- [containers/set/owned](../examples/containers/set/owned/main.c) - `set`
- [containers/set_tour](../examples/containers/set_tour/main.c) - `set`
- [containers/slot_map](../examples/containers/slot_map/main.c) - `slot_map`
- [containers/slot_map_tour](../examples/containers/slot_map_tour/main.c) - `slot_map`
- [containers/stack](../examples/containers/stack/main.c) - `stack`

## core (8)

- [core/allocator_tour](../examples/core/allocator_tour/main.c) - `memory_debug`
- [core/atomic](../examples/core/atomic/main.c) - `atomic`
- [core/atomic_tour](../examples/core/atomic_tour/main.c) - `atomic`
- [core/error](../examples/core/error/main.c) - `core`
- [core/error_format](../examples/core/error_format/main.c) - `error_format`
- [core/memory](../examples/core/memory/main.c) - `core`
- [core/reference](../examples/core/reference/main.c) - `core`
- [core/version_limits](../examples/core/version_limits/main.c) - `core`

## crypto (35)

- [crypto/aead_tour](../examples/crypto/aead_tour/main.c) - `crypto_aes_gcm`
- [crypto/aes](../examples/crypto/aes/main.c) - `crypto_aes`
- [crypto/aes_gcm](../examples/crypto/aes_gcm/main.c) - `crypto_aes_gcm`
- [crypto/chacha20](../examples/crypto/chacha20/main.c) - `crypto_chacha20`
- [crypto/chacha20_poly1305](../examples/crypto/chacha20_poly1305/main.c) - `crypto_chacha20_poly1305`
- [crypto/core](../examples/crypto/core/main.c) - `crypto_core`
- [crypto/ecdh_tour](../examples/crypto/ecdh_tour/main.c) - `crypto_x25519_keypair`
- [crypto/ecdsa_p256](../examples/crypto/ecdsa_p256/main.c) - `crypto_ecdsa_p256_sign_der`
- [crypto/ecdsa_p384](../examples/crypto/ecdsa_p384/main.c) - `crypto_ecdsa_p384_sign_der`
- [crypto/ed25519](../examples/crypto/ed25519/main.c) - `crypto_ed25519_keypair`
- [crypto/ed25519_sign](../examples/crypto/ed25519_sign/main.c) - `crypto_ed25519_sign`
- [crypto/ed25519_verify](../examples/crypto/ed25519_verify/main.c) - `crypto_ed25519_verify`
- [crypto/hash_tour](../examples/crypto/hash_tour/main.c) - `crypto_sha512`
- [crypto/hkdf_sha256](../examples/crypto/hkdf_sha256/main.c) - `crypto_hkdf_sha256`
- [crypto/hkdf_sha512](../examples/crypto/hkdf_sha512/main.c) - `crypto_hkdf_sha512`
- [crypto/hmac_sha256](../examples/crypto/hmac_sha256/main.c) - `crypto_hmac_sha256`
- [crypto/hmac_sha512](../examples/crypto/hmac_sha512/main.c) - `crypto_hmac_sha512`
- [crypto/kdf_tour](../examples/crypto/kdf_tour/main.c) - `crypto_hkdf_sha256`
- [crypto/md5](../examples/crypto/md5/main.c) - `crypto_md5`
- [crypto/p256](../examples/crypto/p256/main.c) - `crypto_p256_keypair`
- [crypto/p384](../examples/crypto/p384/main.c) - `crypto_p384_keypair`
- [crypto/pbkdf2_sha256](../examples/crypto/pbkdf2_sha256/main.c) - `crypto_pbkdf2_sha256`
- [crypto/pbkdf2_sha512](../examples/crypto/pbkdf2_sha512/main.c) - `crypto_pbkdf2_sha512`
- [crypto/poly1305](../examples/crypto/poly1305/main.c) - `crypto_poly1305`
- [crypto/rsa_pkcs1](../examples/crypto/rsa_pkcs1/main.c) - `crypto_rsa_pkcs1_sign`
- [crypto/rsa_pss](../examples/crypto/rsa_pss/main.c) - `crypto_rsa_pss_sign`
- [crypto/session](../examples/crypto/session/main.c) - `crypto_session_example`
- [crypto/sha1](../examples/crypto/sha1/main.c) - `crypto_sha1`
- [crypto/sha224](../examples/crypto/sha224/main.c) - `crypto_sha224`
- [crypto/sha256](../examples/crypto/sha256/main.c) - `crypto_sha256`
- [crypto/sha512](../examples/crypto/sha512/main.c) - `crypto_sha512`
- [crypto/sha512_256](../examples/crypto/sha512_256/main.c) - `crypto_sha512_256`
- [crypto/sign_tour](../examples/crypto/sign_tour/main.c) - `crypto_ed25519_sign`
- [crypto/x25519](../examples/crypto/x25519/main.c) - `crypto_x25519_keypair`
- [crypto/x448](../examples/crypto/x448/main.c) - `crypto_x448_keypair`

## data (6)

- [data/buffer_base64](../examples/data/buffer_base64/main.c) - `buffer_base64`
- [data/buffer_hex](../examples/data/buffer_hex/main.c) - `buffer_hex`
- [data/json](../examples/data/json/main.c) - `json`
- [data/json_tour](../examples/data/json_tour/main.c) - `json`
- [data/xson](../examples/data/xson/main.c) - `xson`
- [data/xson_tour](../examples/data/xson_tour/main.c) - `xson_core`

## environment (1)

- [environment/variants](../examples/environment/variants/main.c) - `environment`

## error (1)

- [error/tour](../examples/error/tour/main.c) - `core`

## file (24)

- [file/async](../examples/file/async/main.c) - `file_async`
- [file/async_manage](../examples/file/async_manage/main.c) - `file_async_manage`
- [file/async_tour](../examples/file/async_tour/main.c) - `file_async`
- [file/async_whole](../examples/file/async_whole/main.c) - `file_async_whole`
- [file/basic](../examples/file/basic/main.c) - `file`
- [file/dir_async](../examples/file/dir_async/main.c) - `dir_async`
- [file/dir_temp](../examples/file/dir_temp/main.c) - `dir_temp`
- [file/dir_tour](../examples/file/dir_tour/main.c) - `file`
- [file/directory](../examples/file/directory/main.c) - `dir`
- [file/fifo](../examples/file/fifo/main.c) - `file_fifo`
- [file/io_tour](../examples/file/io_tour/main.c) - `file`
- [file/link](../examples/file/link/main.c) - `file_link`
- [file/link_tour](../examples/file/link_tour/main.c) - `file`
- [file/lock](../examples/file/lock/main.c) - `file_lock`
- [file/map](../examples/file/map/main.c) - `file_map`
- [file/report](../examples/file/report/main.c) - `time_path_file_example`
- [file/root](../examples/file/root/main.c) - `file_root`
- [file/root_tour](../examples/file/root_tour/main.c) - `file`
- [file/temp](../examples/file/temp/main.c) - `file_temp`
- [file/text](../examples/file/text/main.c) - `file_text`
- [file/tree](../examples/file/tree/main.c) - `file_tree`
- [file/tree_async](../examples/file/tree_async/main.c) - `file_tree_async`
- [file/walk](../examples/file/walk/main.c) - `file_walk`
- [file/whole](../examples/file/whole/main.c) - `file_whole`

## hash (4)

- [hash/hash32](../examples/hash/hash32/main.c) - `hash32`
- [hash/hash64](../examples/hash/hash64/main.c) - `hash64`
- [hash/keyed](../examples/hash/keyed/main.c) - `hash_keyed`
- [hash/variants](../examples/hash/variants/main.c) - `hash_keyed`

## html (1)

- [html/variants](../examples/html/variants/main.c) - `html_escape`

## http (22)

- [http/base](../examples/http/base/main.c) - `http`
- [http/connection](../examples/http/connection/main.c) - `http_connection`
- [http/connection_cursor](../examples/http/connection_cursor/main.c) - `http_connection`
- [http/decode](../examples/http/decode/main.c) - `http_decode`
- [http/decode_tour](../examples/http/decode_tour/main.c) - `http_decode`
- [http/encoding](../examples/http/encoding/main.c) - `http_encoding`
- [http/expect](../examples/http/expect/main.c) - `http_expect`
- [http/field_tour](../examples/http/field_tour/main.c) - `http`
- [http/host](../examples/http/host/main.c) - `http_host`
- [http/http1](../examples/http/http1/main.c) - `http1_head`
- [http/http1_body](../examples/http/http1_body/main.c) - `http1_body`
- [http/http1_message](../examples/http/http1_message/main.c) - `http1_message`
- [http/method_tour](../examples/http/method_tour/main.c) - `http`
- [http/param](../examples/http/param/main.c) - `http_param`
- [http/param_tour](../examples/http/param_tour/main.c) - `http`
- [http/small_fields](../examples/http/small_fields/main.c) - `http`
- [http/target](../examples/http/target/main.c) - `http_target`
- [http/te](../examples/http/te/main.c) - `http_te`
- [http/token_tour](../examples/http/token_tour/main.c) - `http`
- [http/trailer](../examples/http/trailer/main.c) - `http_trailer`
- [http/upgrade](../examples/http/upgrade/main.c) - `http_upgrade_write`
- [http/validate_tour](../examples/http/validate_tour/main.c) - `http`

## http1 (2)

- [http1/head_tour](../examples/http1/head_tour/main.c) - `http1_head`
- [http1/parse_buffer](../examples/http1/parse_buffer/main.c) - `http1_net`

## id (2)

- [id/xid](../examples/id/xid/main.c) - `xid`
- [id/xid_batch](../examples/id/xid_batch/main.c) - `xid`

## io (5)

- [io/buffer](../examples/io/buffer/main.c) - `io_buffer`
- [io/file](../examples/io/file/main.c) - `io_file`
- [io/line](../examples/io/line/main.c) - `io_line`
- [io/memory](../examples/io/memory/main.c) - `io`
- [io/stream_tour](../examples/io/stream_tour/main.c) - `io`

## logging (13)

- [logging/async](../examples/logging/async/main.c) - `logger_async`
- [logging/console](../examples/logging/console/main.c) - `logger_console`
- [logging/core](../examples/logging/core/main.c) - `logger_core`
- [logging/file](../examples/logging/file/main.c) - `logger_file`
- [logging/file_json](../examples/logging/file_json/main.c) - `logger_file_json`
- [logging/file_text](../examples/logging/file_text/main.c) - `logger_file_text`
- [logging/format_json_buffer](../examples/logging/format_json_buffer/main.c) - `logger_format_json_buffer`
- [logging/format_text_buffer](../examples/logging/format_text_buffer/main.c) - `logger_format_text_buffer`
- [logging/json](../examples/logging/json/main.c) - `logger_format_json`
- [logging/logger_tour](../examples/logging/logger_tour/main.c) - `logger_core`
- [logging/printf](../examples/logging/printf/main.c) - `logger_printf`
- [logging/ring_async](../examples/logging/ring_async/main.c) - `logger_ring`
- [logging/sink_tour](../examples/logging/sink_tour/main.c) - `logger_core`

## math (10)

- [math/helpers](../examples/math/helpers/main.c) - `math`
- [math/near](../examples/math/near/main.c) - `math`
- [math/random](../examples/math/random/main.c) - `random`
- [math/random_secure](../examples/math/random_secure/main.c) - `random_secure`
- [math/random_secure_text](../examples/math/random_secure_text/main.c) - `random_secure_text`
- [math/random_text](../examples/math/random_text/main.c) - `random_text`
- [math/random_tour](../examples/math/random_tour/main.c) - `random`
- [math/thread_random](../examples/math/thread_random/main.c) - `random_default`
- [math/thread_random_text](../examples/math/thread_random_text/main.c) - `random_text_default`
- [math/tour](../examples/math/tour/main.c) - `math`

## memory (8)

- [memory/debug](../examples/memory/debug/main.c) - `memory_debug`
- [memory/debug_report](../examples/memory/debug_report/main.c) - `memory_debug_report`
- [memory/fail_inject](../examples/memory/fail_inject/main.c) - `memory_debug`
- [memory/memory_pool](../examples/memory/memory_pool/main.c) - `memory_pool`
- [memory/pool](../examples/memory/pool/main.c) - `pool`
- [memory/pool_page](../examples/memory/pool_page/main.c) - `pool_page`
- [memory/stats](../examples/memory/stats/main.c) - `memory_stats`
- [memory/temp](../examples/memory/temp/main.c) - `temp_memory`

## network (50)

- [network/addr_tour](../examples/network/addr_tour/main.c) - `net`
- [network/address](../examples/network/address/main.c) - `net`
- [network/buf_tour](../examples/network/buf_tour/main.c) - `net_buffer`
- [network/buffer](../examples/network/buffer/main.c) - `net_buffer`
- [network/dns](../examples/network/dns/main.c) - `net_dns`
- [network/engine](../examples/network/engine/main.c) - `net_engine`
- [network/engine_tour](../examples/network/engine_tour/main.c) - `net_engine`
- [network/file_tour](../examples/network/file_tour/main.c) - `net_file`
- [network/frame_length](../examples/network/frame_length/main.c) - `net_frame_length`
- [network/frame_line](../examples/network/frame_line/main.c) - `net_frame_line`
- [network/interface](../examples/network/interface/main.c) - `net_interface`
- [network/interface_tour](../examples/network/interface_tour/main.c) - `net_interface`
- [network/local_info](../examples/network/local_info/main.c) - `net_interface_text`
- [network/port_epoll](../examples/network/port_epoll/main.c) - `net_port_epoll`
- [network/port_iocp](../examples/network/port_iocp/main.c) - `net_port_iocp`
- [network/port_kqueue](../examples/network/port_kqueue/main.c) - `net_port_kqueue`
- [network/port_select](../examples/network/port_select/main.c) - `net_port_select`
- [network/port_tour](../examples/network/port_tour/main.c) - `net_port`
- [network/port_uring](../examples/network/port_uring/main.c) - `net_port_uring`
- [network/proxy_dial](../examples/network/proxy_dial/main.c) - `net_proxy_dial_socks5_tests`
- [network/proxy_dial_http_connect](../examples/network/proxy_dial_http_connect/main.c) - `net_proxy_dial_http_connect_tests`
- [network/proxy_http_connect](../examples/network/proxy_http_connect/main.c) - `net_proxy_http_connect`
- [network/proxy_socks5](../examples/network/proxy_socks5/main.c) - `net_proxy_socks5`
- [network/proxy_tour](../examples/network/proxy_tour/main.c) - `net_proxy`
- [network/resolve_tour](../examples/network/resolve_tour/main.c) - `net_resolver`
- [network/resolver](../examples/network/resolver/main.c) - `net_resolver`
- [network/resolver_future](../examples/network/resolver_future/main.c) - `net_resolver_future`
- [network/socket](../examples/network/socket/main.c) - `net_socket`
- [network/socket_tcp](../examples/network/socket_tcp/main.c) - `net_socket`
- [network/socket_tour](../examples/network/socket_tour/main.c) - `net_socket`
- [network/task](../examples/network/task/main.c) - `task_net`
- [network/task_group](../examples/network/task_group/main.c) - `task_group_net`
- [network/tcp](../examples/network/tcp/main.c) - `net_tcp`
- [network/tcp_dial](../examples/network/tcp_dial/main.c) - `net_tcp_dial_future`
- [network/tcp_dial_sync](../examples/network/tcp_dial_sync/main.c) - `net_tcp_dial_sync`
- [network/tcp_dial_tour](../examples/network/tcp_dial_tour/main.c) - `net_tcp`
- [network/tcp_future](../examples/network/tcp_future/main.c) - `net_tcp_future`
- [network/tcp_server](../examples/network/tcp_server/main.c) - `net_tcp_server`
- [network/tcp_server_sync](../examples/network/tcp_server_sync/main.c) - `net_tcp_server_sync`
- [network/tcp_server_tour](../examples/network/tcp_server_tour/main.c) - `net_tcp_server`
- [network/tcp_stream_tour](../examples/network/tcp_stream_tour/main.c) - `net_tcp`
- [network/tcp_sync](../examples/network/tcp_sync/main.c) - `net_tcp_sync`
- [network/udp](../examples/network/udp/main.c) - `net_udp`
- [network/udp_batch](../examples/network/udp_batch/main.c) - `net_udp`
- [network/udp_errors](../examples/network/udp_errors/main.c) - `net_udp_sync`
- [network/udp_future](../examples/network/udp_future/main.c) - `net_udp_future`
- [network/udp_introspect](../examples/network/udp_introspect/main.c) - `net_udp`
- [network/udp_multicast](../examples/network/udp_multicast/main.c) - `net_udp`
- [network/udp_send_tour](../examples/network/udp_send_tour/main.c) - `net_udp`
- [network/udp_sync](../examples/network/udp_sync/main.c) - `net_udp_sync`

## number (4)

- [number/float](../examples/number/float/main.c) - `number_float`
- [number/format](../examples/number/format/main.c) - `number_format`
- [number/integer](../examples/number/integer/main.c) - `number_integer`
- [number/variants](../examples/number/variants/main.c) - `number_format`

## path (4)

- [path/basic](../examples/path/basic/main.c) - `path`
- [path/safe](../examples/path/safe/main.c) - `path`, `path_safe`
- [path/system](../examples/path/system/main.c) - `path`, `path_system`
- [path/tour](../examples/path/tour/main.c) - `path`

## process (10)

- [process/capture](../examples/process/capture/main.c) - `process_run`
- [process/file](../examples/process/file/main.c) - `process_file`
- [process/future](../examples/process/future/main.c) - `process_future`
- [process/open](../examples/process/open/main.c) - `process_open`
- [process/pipeline](../examples/process/pipeline/main.c) - `process_pipeline`
- [process/signal](../examples/process/signal/main.c) - `signal`
- [process/signal_tour](../examples/process/signal_tour/main.c) - `signal`
- [process/stream](../examples/process/stream/main.c) - `process`
- [process/terminal](../examples/process/terminal/main.c) - `process_terminal`
- [process/tour](../examples/process/tour/main.c) - `process`

## stack (1)

- [stack/tour](../examples/stack/tour/main.c) - `stack`

## string (16)

- [string/basic](../examples/string/basic/main.c) - `string`
- [string/builder](../examples/string/builder/main.c) - `string`
- [string/builder_tour](../examples/string/builder_tour/main.c) - `string`
- [string/case](../examples/string/case/main.c) - `string`
- [string/compare](../examples/string/compare/main.c) - `string`
- [string/distance](../examples/string/distance/main.c) - `unicode_distance`
- [string/dup_join](../examples/string/dup_join/main.c) - `string`
- [string/edit](../examples/string/edit/main.c) - `string`
- [string/find](../examples/string/find/main.c) - `string`
- [string/format](../examples/string/format/main.c) - `string_format`
- [string/format_tour](../examples/string/format_tour/main.c) - `string`
- [string/glob](../examples/string/glob/main.c) - `string_glob`
- [string/iterators](../examples/string/iterators/main.c) - `string`
- [string/list](../examples/string/list/main.c) - `string`
- [string/pad_trim](../examples/string/pad_trim/main.c) - `string`
- [string/split](../examples/string/split/main.c) - `string_split`

## system (1)

- [system/environment](../examples/system/environment/main.c) - `environment`

## template (6)

- [template/compose](../examples/template/compose/main.c) - `template_compose`
- [template/control](../examples/template/control/main.c) - `template_control`
- [template/core](../examples/template/core/main.c) - `template_core`
- [template/extension](../examples/template/extension/main.c) - `template_extension`
- [template/file](../examples/template/file/main.c) - `template_file`
- [template/tour](../examples/template/tour/main.c) - `template_core`

## text (8)

- [text/html_escape](../examples/text/html_escape/main.c) - `html_escape`
- [text/pattern](../examples/text/pattern/main.c) - `pattern`
- [text/pattern_tour](../examples/text/pattern_tour/main.c) - `pattern`
- [text/regex](../examples/text/regex/main.c) - `regex`, `regex_match`
- [text/regex_replace](../examples/text/regex_replace/main.c) - `regex`, `regex_replace`
- [text/regex_set](../examples/text/regex_set/main.c) - `regex`, `regex_set`
- [text/regex_split](../examples/text/regex_split/main.c) - `regex`, `regex_split`
- [text/regex_tour](../examples/text/regex_tour/main.c) - `regex`

## time (8)

- [time/basic](../examples/time/basic/main.c) - `time`
- [time/calendar_tour](../examples/time/calendar_tour/main.c) - `time`
- [time/clock](../examples/time/clock/main.c) - `time`
- [time/format](../examples/time/format/main.c) - `time_text`
- [time/local](../examples/time/local/main.c) - `time_local`
- [time/protocol](../examples/time/protocol/main.c) - `time_text`
- [time/range_tour](../examples/time/range_tour/main.c) - `time`
- [time/text_parse](../examples/time/text_parse/main.c) - `time_text`

## tls (25)

- [tls/auth_messages](../examples/tls/auth_messages/main.c) - `tls_auth_messages_write`
- [tls/cipher_backends](../examples/tls/cipher_backends/main.c) - `tls_cipher_backends_examples`
- [tls/client_resume](../examples/tls/client_resume/main.c) - `tls_client_resume_example`
- [tls/context](../examples/tls/context/main.c) - `tls_context`
- [tls/dial](../examples/tls/dial/main.c) - `tls_stream_dial_example`
- [tls/dial_future](../examples/tls/dial_future/main.c) - `tls_stream_dial_future_example`
- [tls/extension_tour](../examples/tls/extension_tour/main.c) - `tls_negotiate`
- [tls/handshake_extra](../examples/tls/handshake_extra/main.c) - `tls_stream_listener`
- [tls/identity](../examples/tls/identity/main.c) - `tls_identity_builtin`
- [tls/key_exchange](../examples/tls/key_exchange/main.c) - `tls_key_exchange_p256`, `tls_key_exchange_p384`, `tls_key_exchange_tests`, `tls_key_exchange_x25519`, `tls_key_exchange_x448`
- [tls/listener_tour](../examples/tls/listener_tour/main.c) - `tls_stream_listener`
- [tls/message_tour](../examples/tls/message_tour/main.c) - `tls_messages`
- [tls/messages](../examples/tls/messages/main.c) - `tls_messages_write`
- [tls/negotiate](../examples/tls/negotiate/main.c) - `tls_negotiate`
- [tls/policy](../examples/tls/policy/main.c) - `tls_policy`
- [tls/record](../examples/tls/record/main.c) - `tls`
- [tls/resume](../examples/tls/resume/main.c) - `tls_resume`
- [tls/resume_tour](../examples/tls/resume_tour/main.c) - `tls_resume`
- [tls/server](../examples/tls/server/main.c) - `tls_server_tests`
- [tls/session_tour](../examples/tls/session_tour/main.c) - `tls_session`
- [tls/stream](../examples/tls/stream/main.c) - `tls_stream_tests`
- [tls/stream_future](../examples/tls/stream_future/main.c) - `tls_stream_future`
- [tls/stream_tour](../examples/tls/stream_tour/main.c) - `tls_stream`
- [tls/verify](../examples/tls/verify/main.c) - `tls_verify`
- [tls/writer_tour](../examples/tls/writer_tour/main.c) - `tls_negotiate`

## value (13)

- [value/array_tour](../examples/value/array_tour/main.c) - `value`
- [value/basic](../examples/value/basic/main.c) - `value`
- [value/collections/batch](../examples/value/collections/batch/main.c) - `value_collection`
- [value/collections](../examples/value/collections/main.c) - `value_collection`
- [value/containers/indexed](../examples/value/containers/indexed/main.c) - `value_container`
- [value/containers/lifo](../examples/value/containers/lifo/main.c) - `value_container`
- [value/containers](../examples/value/containers/main.c) - `value_container`
- [value/graph](../examples/value/graph/main.c) - `value_graph`
- [value/handle](../examples/value/handle/main.c) - `value`
- [value/iter_weak](../examples/value/iter_weak/main.c) - `value`
- [value/object_tour](../examples/value/object_tour/main.c) - `value`
- [value/ownership](../examples/value/ownership/main.c) - `value`
- [value/set_tour](../examples/value/set_tour/main.c) - `value`

## websocket (13)

- [websocket/close](../examples/websocket/close/main.c) - `websocket_close`
- [websocket/deflate](../examples/websocket/deflate/main.c) - `websocket_deflate`
- [websocket/deflater](../examples/websocket/deflater/main.c) - `websocket_deflater`
- [websocket/extension](../examples/websocket/extension/main.c) - `websocket_extension`
- [websocket/extension_tour](../examples/websocket/extension_tour/main.c) - `websocket_handshake`
- [websocket/frame](../examples/websocket/frame/main.c) - `websocket_frame`
- [websocket/handshake](../examples/websocket/handshake/main.c) - `websocket_handshake`
- [websocket/inflater](../examples/websocket/inflater/main.c) - `websocket_inflater`
- [websocket/message](../examples/websocket/message/main.c) - `websocket_message`
- [websocket/stream_ref](../examples/websocket/stream_ref/main.c) - `websocket_stream_ref`
- [websocket/stream_tour](../examples/websocket/stream_tour/main.c) - `websocket_stream`
- [websocket/upgrade](../examples/websocket/upgrade/main.c) - `websocket_upgrade`
- [websocket/upgrade_tour](../examples/websocket/upgrade_tour/main.c) - `websocket_upgrade`

## x509 (19)

- [x509/cert_tour](../examples/x509/cert_tour/main.c) - `x509_parse`
- [x509/crl](../examples/x509/crl/main.c) - `x509_crl`
- [x509/crl_policy](../examples/x509/crl_policy/main.c) - `x509_crl_policy_rsa_tests`
- [x509/crl_profile](../examples/x509/crl_profile/main.c) - `x509_crl_profile`
- [x509/crl_tour](../examples/x509/crl_tour/main.c) - `x509_crl`
- [x509/distribution](../examples/x509/distribution/main.c) - `x509_distribution`
- [x509/identity](../examples/x509/identity/main.c) - `x509_identity`
- [x509/inspect](../examples/x509/inspect/main.c) - `x509_parse`
- [x509/name](../examples/x509/name/main.c) - `x509_name`
- [x509/name_constraints](../examples/x509/name_constraints/main.c) - `x509_name_constraints`
- [x509/path](../examples/x509/path/main.c) - `x509_path_rsa_tests`
- [x509/path_build](../examples/x509/path_build/main.c) - `x509_path_build_rsa_tests`
- [x509/profile](../examples/x509/profile/main.c) - `x509_profile`
- [x509/signature](../examples/x509/signature/main.c) - `x509_signature`
- [x509/store](../examples/x509/store/main.c) - `x509_store`
- [x509/store_file](../examples/x509/store_file/main.c) - `x509_store_file`
- [x509/store_system](../examples/x509/store_system/main.c) - `x509_store_system`
- [x509/store_tour](../examples/x509/store_tour/main.c) - `x509_store`
- [x509/verify](../examples/x509/verify/main.c) - `x509_verify_rsa`
