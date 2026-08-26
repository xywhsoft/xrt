# XRT 示例索引

此文件由 `tools/generate_example_index.py` 从 `config/modules.json` 生成，
不要手工维护第二份示例清单。构建器会按所属模块的真实依赖闭包编译并运行示例。

当前共登记 `287` 个可运行示例。

## asn1 (2)

- [asn1/der](../examples/asn1/der/main.c) - `asn1_der`
- [asn1/pem](../examples/asn1/pem/main.c) - `pem`

## charset (4)

- [charset/detect](../examples/charset/detect/main.c) - `charset_detect`
- [charset/transcode](../examples/charset/transcode/main.c) - `charset`
- [charset/unicode](../examples/charset/unicode/main.c) - `unicode`
- [charset/unicode_text](../examples/charset/unicode_text/main.c) - `unicode_text`

## codec (3)

- [codec/base64](../examples/codec/base64/main.c) - `codec_base64`
- [codec/hex](../examples/codec/hex/main.c) - `codec_hex`
- [codec/percent](../examples/codec/percent/main.c) - `codec_percent`

## compress (2)

- [compress/deflate](../examples/compress/deflate/main.c) - `deflate`
- [compress/inflate](../examples/compress/inflate/main.c) - `inflate`

## concurrency (32)

- [concurrency/cancel](../examples/concurrency/cancel/main.c) - `cancel`
- [concurrency/channel](../examples/concurrency/channel/main.c) - `channel`
- [concurrency/channel_cancel](../examples/concurrency/channel_cancel/main.c) - `channel_cancel`
- [concurrency/channel_coroutine](../examples/concurrency/channel_coroutine/main.c) - `channel_coroutine`
- [concurrency/channel_select](../examples/concurrency/channel_select/main.c) - `channel_select`
- [concurrency/channel_select_cancel](../examples/concurrency/channel_select_cancel/main.c) - `channel_select_cancel`
- [concurrency/condition](../examples/concurrency/condition/main.c) - `cond`
- [concurrency/coroutine](../examples/concurrency/coroutine/main.c) - `coroutine`
- [concurrency/coroutine_event](../examples/concurrency/coroutine_event/main.c) - `coroutine_event`
- [concurrency/coroutine_lifecycle](../examples/concurrency/coroutine_lifecycle/main.c) - `coroutine_scheduler`
- [concurrency/coroutine_scheduler](../examples/concurrency/coroutine_scheduler/main.c) - `coroutine_scheduler`
- [concurrency/deadline](../examples/concurrency/deadline/main.c) - `wait`
- [concurrency/executor](../examples/concurrency/executor/main.c) - `executor`
- [concurrency/future](../examples/concurrency/future/main.c) - `future`
- [concurrency/future_combine](../examples/concurrency/future_combine/main.c) - `future_combine`
- [concurrency/future_continue](../examples/concurrency/future_continue/main.c) - `future_continue`
- [concurrency/future_coroutine](../examples/concurrency/future_coroutine/main.c) - `future_coroutine`
- [concurrency/once](../examples/concurrency/once/main.c) - `once`
- [concurrency/report](../examples/concurrency/report/main.c) - `concurrency_report_example`
- [concurrency/rwlock](../examples/concurrency/rwlock/main.c) - `rwlock`
- [concurrency/semaphore](../examples/concurrency/semaphore/main.c) - `sem`
- [concurrency/spin](../examples/concurrency/spin/main.c) - `spin`
- [concurrency/sync](../examples/concurrency/sync/main.c) - `mutex`
- [concurrency/task_coroutine](../examples/concurrency/task_coroutine/main.c) - `task_coroutine`
- [concurrency/task_group](../examples/concurrency/task_group/main.c) - `task_group`
- [concurrency/task_group_coroutine](../examples/concurrency/task_group_coroutine/main.c) - `task_group_coroutine`
- [concurrency/task_group_pool](../examples/concurrency/task_group_pool/main.c) - `task_group_pool`
- [concurrency/task_group_scope](../examples/concurrency/task_group_scope/main.c) - `task_group`
- [concurrency/task_pool](../examples/concurrency/task_pool/main.c) - `task_pool`
- [concurrency/thread](../examples/concurrency/thread/main.c) - `thread`
- [concurrency/thread_key](../examples/concurrency/thread_key/main.c) - `thread_key`
- [concurrency/worker](../examples/concurrency/worker/main.c) - `concurrency_worker_example`

## console (1)

- [console/output](../examples/console/output/main.c) - `console`

## containers (19)

- [containers/array](../examples/containers/array/main.c) - `array`
- [containers/avl](../examples/containers/avl/main.c) - `avl`
- [containers/avl_tree](../examples/containers/avl_tree/main.c) - `avl_tree`
- [containers/block_stack](../examples/containers/block_stack/main.c) - `block_stack`
- [containers/buffer](../examples/containers/buffer/main.c) - `buffer`
- [containers/fixed_stack](../examples/containers/fixed_stack/main.c) - `fixed_stack`
- [containers/int_map](../examples/containers/int_map/main.c) - `int_map`
- [containers/list](../examples/containers/list/main.c) - `list`
- [containers/map](../examples/containers/map/main.c) - `map`
- [containers/ptr_array](../examples/containers/ptr_array/main.c) - `ptr_array`
- [containers/ptr_fixed_stack](../examples/containers/ptr_fixed_stack/main.c) - `ptr_fixed_stack`
- [containers/ptr_stack](../examples/containers/ptr_stack/main.c) - `ptr_stack`
- [containers/queue_mpmc](../examples/containers/queue_mpmc/main.c) - `queue_mpmc`
- [containers/queue_mpsc](../examples/containers/queue_mpsc/main.c) - `queue_mpsc`
- [containers/queue_spsc](../examples/containers/queue_spsc/main.c) - `queue_spsc`
- [containers/set](../examples/containers/set/main.c) - `set`
- [containers/set/owned](../examples/containers/set/owned/main.c) - `set`
- [containers/slot_map](../examples/containers/slot_map/main.c) - `slot_map`
- [containers/stack](../examples/containers/stack/main.c) - `stack`

## core (5)

- [core/atomic](../examples/core/atomic/main.c) - `atomic`
- [core/error](../examples/core/error/main.c) - `core`
- [core/error_format](../examples/core/error_format/main.c) - `error_format`
- [core/memory](../examples/core/memory/main.c) - `core`
- [core/reference](../examples/core/reference/main.c) - `core`

## crypto (30)

- [crypto/aes](../examples/crypto/aes/main.c) - `crypto_aes`
- [crypto/aes_gcm](../examples/crypto/aes_gcm/main.c) - `crypto_aes_gcm`
- [crypto/chacha20](../examples/crypto/chacha20/main.c) - `crypto_chacha20`
- [crypto/chacha20_poly1305](../examples/crypto/chacha20_poly1305/main.c) - `crypto_chacha20_poly1305`
- [crypto/core](../examples/crypto/core/main.c) - `crypto_core`
- [crypto/ecdsa_p256](../examples/crypto/ecdsa_p256/main.c) - `crypto_ecdsa_p256_sign_der`
- [crypto/ecdsa_p384](../examples/crypto/ecdsa_p384/main.c) - `crypto_ecdsa_p384_sign_der`
- [crypto/ed25519](../examples/crypto/ed25519/main.c) - `crypto_ed25519_keypair`
- [crypto/ed25519_sign](../examples/crypto/ed25519_sign/main.c) - `crypto_ed25519_sign`
- [crypto/ed25519_verify](../examples/crypto/ed25519_verify/main.c) - `crypto_ed25519_verify`
- [crypto/hkdf_sha256](../examples/crypto/hkdf_sha256/main.c) - `crypto_hkdf_sha256`
- [crypto/hkdf_sha512](../examples/crypto/hkdf_sha512/main.c) - `crypto_hkdf_sha512`
- [crypto/hmac_sha256](../examples/crypto/hmac_sha256/main.c) - `crypto_hmac_sha256`
- [crypto/hmac_sha512](../examples/crypto/hmac_sha512/main.c) - `crypto_hmac_sha512`
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
- [crypto/x25519](../examples/crypto/x25519/main.c) - `crypto_x25519_keypair`
- [crypto/x448](../examples/crypto/x448/main.c) - `crypto_x448_keypair`

## data (4)

- [data/buffer_base64](../examples/data/buffer_base64/main.c) - `buffer_base64`
- [data/buffer_hex](../examples/data/buffer_hex/main.c) - `buffer_hex`
- [data/json](../examples/data/json/main.c) - `json`
- [data/xson](../examples/data/xson/main.c) - `xson`

## file (19)

- [file/async](../examples/file/async/main.c) - `file_async`
- [file/async_manage](../examples/file/async_manage/main.c) - `file_async_manage`
- [file/async_whole](../examples/file/async_whole/main.c) - `file_async_whole`
- [file/basic](../examples/file/basic/main.c) - `file`
- [file/dir_async](../examples/file/dir_async/main.c) - `dir_async`
- [file/dir_temp](../examples/file/dir_temp/main.c) - `dir_temp`
- [file/directory](../examples/file/directory/main.c) - `dir`
- [file/fifo](../examples/file/fifo/main.c) - `file_fifo`
- [file/link](../examples/file/link/main.c) - `file_link`
- [file/lock](../examples/file/lock/main.c) - `file_lock`
- [file/map](../examples/file/map/main.c) - `file_map`
- [file/report](../examples/file/report/main.c) - `time_path_file_example`
- [file/root](../examples/file/root/main.c) - `file_root`
- [file/temp](../examples/file/temp/main.c) - `file_temp`
- [file/text](../examples/file/text/main.c) - `file_text`
- [file/tree](../examples/file/tree/main.c) - `file_tree`
- [file/tree_async](../examples/file/tree_async/main.c) - `file_tree_async`
- [file/walk](../examples/file/walk/main.c) - `file_walk`
- [file/whole](../examples/file/whole/main.c) - `file_whole`

## hash (3)

- [hash/hash32](../examples/hash/hash32/main.c) - `hash32`
- [hash/hash64](../examples/hash/hash64/main.c) - `hash64`
- [hash/keyed](../examples/hash/keyed/main.c) - `hash_keyed`

## http (14)

- [http/base](../examples/http/base/main.c) - `http`
- [http/connection](../examples/http/connection/main.c) - `http_connection`
- [http/decode](../examples/http/decode/main.c) - `http_decode`
- [http/encoding](../examples/http/encoding/main.c) - `http_encoding`
- [http/expect](../examples/http/expect/main.c) - `http_expect`
- [http/host](../examples/http/host/main.c) - `http_host`
- [http/http1](../examples/http/http1/main.c) - `http1_head`
- [http/http1_body](../examples/http/http1_body/main.c) - `http1_body`
- [http/http1_message](../examples/http/http1_message/main.c) - `http1_message`
- [http/param](../examples/http/param/main.c) - `http_param`
- [http/target](../examples/http/target/main.c) - `http_target`
- [http/te](../examples/http/te/main.c) - `http_te`
- [http/trailer](../examples/http/trailer/main.c) - `http_trailer`
- [http/upgrade](../examples/http/upgrade/main.c) - `http_upgrade_write`

## id (2)

- [id/xid](../examples/id/xid/main.c) - `xid`
- [id/xid_batch](../examples/id/xid_batch/main.c) - `xid`

## io (4)

- [io/buffer](../examples/io/buffer/main.c) - `io_buffer`
- [io/file](../examples/io/file/main.c) - `io_file`
- [io/line](../examples/io/line/main.c) - `io_line`
- [io/memory](../examples/io/memory/main.c) - `io`

## logging (10)

- [logging/async](../examples/logging/async/main.c) - `logger_async`
- [logging/console](../examples/logging/console/main.c) - `logger_console`
- [logging/core](../examples/logging/core/main.c) - `logger_core`
- [logging/file](../examples/logging/file/main.c) - `logger_file`
- [logging/file_json](../examples/logging/file_json/main.c) - `logger_file_json`
- [logging/file_text](../examples/logging/file_text/main.c) - `logger_file_text`
- [logging/format_json_buffer](../examples/logging/format_json_buffer/main.c) - `logger_format_json_buffer`
- [logging/format_text_buffer](../examples/logging/format_text_buffer/main.c) - `logger_format_text_buffer`
- [logging/json](../examples/logging/json/main.c) - `logger_format_json`
- [logging/printf](../examples/logging/printf/main.c) - `logger_printf`

## math (8)

- [math/helpers](../examples/math/helpers/main.c) - `math`
- [math/near](../examples/math/near/main.c) - `math`
- [math/random](../examples/math/random/main.c) - `random`
- [math/random_secure](../examples/math/random_secure/main.c) - `random_secure`
- [math/random_secure_text](../examples/math/random_secure_text/main.c) - `random_secure_text`
- [math/random_text](../examples/math/random_text/main.c) - `random_text`
- [math/thread_random](../examples/math/thread_random/main.c) - `random_default`
- [math/thread_random_text](../examples/math/thread_random_text/main.c) - `random_text_default`

## memory (7)

- [memory/debug](../examples/memory/debug/main.c) - `memory_debug`
- [memory/debug_report](../examples/memory/debug_report/main.c) - `memory_debug_report`
- [memory/memory_pool](../examples/memory/memory_pool/main.c) - `memory_pool`
- [memory/pool](../examples/memory/pool/main.c) - `pool`
- [memory/pool_page](../examples/memory/pool_page/main.c) - `pool_page`
- [memory/stats](../examples/memory/stats/main.c) - `memory_stats`
- [memory/temp](../examples/memory/temp/main.c) - `temp_memory`

## network (34)

- [network/address](../examples/network/address/main.c) - `net`
- [network/buffer](../examples/network/buffer/main.c) - `net_buffer`
- [network/dns](../examples/network/dns/main.c) - `net_dns`
- [network/engine](../examples/network/engine/main.c) - `net_engine`
- [network/frame_length](../examples/network/frame_length/main.c) - `net_frame_length`
- [network/frame_line](../examples/network/frame_line/main.c) - `net_frame_line`
- [network/interface](../examples/network/interface/main.c) - `net_interface`
- [network/local_info](../examples/network/local_info/main.c) - `net_interface_text`
- [network/port_epoll](../examples/network/port_epoll/main.c) - `net_port_epoll`
- [network/port_iocp](../examples/network/port_iocp/main.c) - `net_port_iocp`
- [network/port_kqueue](../examples/network/port_kqueue/main.c) - `net_port_kqueue`
- [network/port_select](../examples/network/port_select/main.c) - `net_port_select`
- [network/port_uring](../examples/network/port_uring/main.c) - `net_port_uring`
- [network/proxy_dial](../examples/network/proxy_dial/main.c) - `net_proxy_dial_socks5_tests`
- [network/proxy_dial_http_connect](../examples/network/proxy_dial_http_connect/main.c) - `net_proxy_dial_http_connect_tests`
- [network/proxy_http_connect](../examples/network/proxy_http_connect/main.c) - `net_proxy_http_connect`
- [network/proxy_socks5](../examples/network/proxy_socks5/main.c) - `net_proxy_socks5`
- [network/resolver](../examples/network/resolver/main.c) - `net_resolver`
- [network/resolver_future](../examples/network/resolver_future/main.c) - `net_resolver_future`
- [network/socket](../examples/network/socket/main.c) - `net_socket`
- [network/socket_tcp](../examples/network/socket_tcp/main.c) - `net_socket`
- [network/task](../examples/network/task/main.c) - `task_net`
- [network/task_group](../examples/network/task_group/main.c) - `task_group_net`
- [network/tcp](../examples/network/tcp/main.c) - `net_tcp`
- [network/tcp_dial](../examples/network/tcp_dial/main.c) - `net_tcp_dial_future`
- [network/tcp_dial_sync](../examples/network/tcp_dial_sync/main.c) - `net_tcp_dial_sync`
- [network/tcp_future](../examples/network/tcp_future/main.c) - `net_tcp_future`
- [network/tcp_server](../examples/network/tcp_server/main.c) - `net_tcp_server`
- [network/tcp_server_sync](../examples/network/tcp_server_sync/main.c) - `net_tcp_server_sync`
- [network/tcp_sync](../examples/network/tcp_sync/main.c) - `net_tcp_sync`
- [network/udp](../examples/network/udp/main.c) - `net_udp`
- [network/udp_errors](../examples/network/udp_errors/main.c) - `net_udp_sync`
- [network/udp_future](../examples/network/udp_future/main.c) - `net_udp_future`
- [network/udp_sync](../examples/network/udp_sync/main.c) - `net_udp_sync`

## number (3)

- [number/float](../examples/number/float/main.c) - `number_float`
- [number/format](../examples/number/format/main.c) - `number_format`
- [number/integer](../examples/number/integer/main.c) - `number_integer`

## path (3)

- [path/basic](../examples/path/basic/main.c) - `path`
- [path/safe](../examples/path/safe/main.c) - `path_safe`
- [path/system](../examples/path/system/main.c) - `path_system`

## process (8)

- [process/capture](../examples/process/capture/main.c) - `process_run`
- [process/file](../examples/process/file/main.c) - `process_file`
- [process/future](../examples/process/future/main.c) - `process_future`
- [process/open](../examples/process/open/main.c) - `process_open`
- [process/pipeline](../examples/process/pipeline/main.c) - `process_pipeline`
- [process/signal](../examples/process/signal/main.c) - `signal`
- [process/stream](../examples/process/stream/main.c) - `process`
- [process/terminal](../examples/process/terminal/main.c) - `process_terminal`

## string (6)

- [string/basic](../examples/string/basic/main.c) - `string`
- [string/builder](../examples/string/builder/main.c) - `string`
- [string/distance](../examples/string/distance/main.c) - `unicode_distance`
- [string/format](../examples/string/format/main.c) - `string_format`
- [string/glob](../examples/string/glob/main.c) - `string_glob`
- [string/split](../examples/string/split/main.c) - `string_split`

## system (1)

- [system/environment](../examples/system/environment/main.c) - `environment`

## template (5)

- [template/compose](../examples/template/compose/main.c) - `template_compose`
- [template/control](../examples/template/control/main.c) - `template_control`
- [template/core](../examples/template/core/main.c) - `template_core`
- [template/extension](../examples/template/extension/main.c) - `template_extension`
- [template/file](../examples/template/file/main.c) - `template_file`

## text (1)

- [text/html_escape](../examples/text/html_escape/main.c) - `html_escape`

## time (5)

- [time/basic](../examples/time/basic/main.c) - `time`
- [time/clock](../examples/time/clock/main.c) - `time`
- [time/format](../examples/time/format/main.c) - `time_text`
- [time/local](../examples/time/local/main.c) - `time_local`
- [time/protocol](../examples/time/protocol/main.c) - `time_text`

## tls (17)

- [tls/auth_messages](../examples/tls/auth_messages/main.c) - `tls_auth_messages_write`
- [tls/cipher_backends](../examples/tls/cipher_backends/main.c) - `tls_cipher_backends_examples`
- [tls/client_resume](../examples/tls/client_resume/main.c) - `tls_client_resume_example`
- [tls/context](../examples/tls/context/main.c) - `tls_context`
- [tls/dial](../examples/tls/dial/main.c) - `tls_stream_dial_example`
- [tls/dial_future](../examples/tls/dial_future/main.c) - `tls_stream_dial_future_example`
- [tls/identity](../examples/tls/identity/main.c) - `tls_identity_builtin`
- [tls/key_exchange](../examples/tls/key_exchange/main.c) - `tls_key_exchange_p256`, `tls_key_exchange_p384`, `tls_key_exchange_tests`, `tls_key_exchange_x25519`, `tls_key_exchange_x448`
- [tls/messages](../examples/tls/messages/main.c) - `tls_messages_write`
- [tls/negotiate](../examples/tls/negotiate/main.c) - `tls_negotiate`
- [tls/policy](../examples/tls/policy/main.c) - `tls_policy`
- [tls/record](../examples/tls/record/main.c) - `tls`
- [tls/resume](../examples/tls/resume/main.c) - `tls_resume`
- [tls/server](../examples/tls/server/main.c) - `tls_server_tests`
- [tls/stream](../examples/tls/stream/main.c) - `tls_stream_tests`
- [tls/stream_future](../examples/tls/stream_future/main.c) - `tls_stream_future`
- [tls/verify](../examples/tls/verify/main.c) - `tls_verify`

## value (9)

- [value/basic](../examples/value/basic/main.c) - `value`
- [value/collections/batch](../examples/value/collections/batch/main.c) - `value_collection`
- [value/collections](../examples/value/collections/main.c) - `value_collection`
- [value/containers/indexed](../examples/value/containers/indexed/main.c) - `value_container`
- [value/containers/lifo](../examples/value/containers/lifo/main.c) - `value_container`
- [value/containers](../examples/value/containers/main.c) - `value_container`
- [value/graph](../examples/value/graph/main.c) - `value_graph`
- [value/handle](../examples/value/handle/main.c) - `value`
- [value/ownership](../examples/value/ownership/main.c) - `value`

## websocket (10)

- [websocket/close](../examples/websocket/close/main.c) - `websocket_close`
- [websocket/deflate](../examples/websocket/deflate/main.c) - `websocket_deflate`
- [websocket/deflater](../examples/websocket/deflater/main.c) - `websocket_deflater`
- [websocket/extension](../examples/websocket/extension/main.c) - `websocket_extension`
- [websocket/frame](../examples/websocket/frame/main.c) - `websocket_frame`
- [websocket/handshake](../examples/websocket/handshake/main.c) - `websocket_handshake`
- [websocket/inflater](../examples/websocket/inflater/main.c) - `websocket_inflater`
- [websocket/message](../examples/websocket/message/main.c) - `websocket_message`
- [websocket/stream_ref](../examples/websocket/stream_ref/main.c) - `websocket_stream_ref`
- [websocket/upgrade](../examples/websocket/upgrade/main.c) - `websocket_upgrade`

## x509 (16)

- [x509/crl](../examples/x509/crl/main.c) - `x509_crl`
- [x509/crl_policy](../examples/x509/crl_policy/main.c) - `x509_crl_policy_rsa_tests`
- [x509/crl_profile](../examples/x509/crl_profile/main.c) - `x509_crl_profile`
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
- [x509/verify](../examples/x509/verify/main.c) - `x509_verify_rsa`
