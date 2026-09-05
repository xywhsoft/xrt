# XRT 协议模糊测试

`fuzz/*.c` 是普通确定性回归与 Clang/libFuzzer 共用的入口。`tools/test_protocol_fuzz.py`
从模块清单解析每个目标的源码、裁剪宏和平台链接闭包，并以 libFuzzer、ASan、UBSan 构建。

## 持久语料

发布关键目标的种子保存在源码树中：

| 目标 | 目录 | 主要边界 |
|---|---|---|
| TLS record/handshake | `fuzz/corpus/tls_protocol` | 截断记录头、长度边界、握手消息 |
| X.509/ASN.1 DER | `fuzz/corpus/x509_asn1` | 合法 Ed25519 证书、DER 长度与时间 |
| 网络地址 | `fuzz/corpus/net_address` | IPv4、IPv6、scope、端口和畸形文本 |
| HTTP/1 | `fuzz/corpus/http1_protocol` | 分帧歧义、Expect、TE 和 chunked |
| WebSocket | `fuzz/corpus/websocket_protocol` | mask、控制帧、压缩位和扩展协商 |

驱动会把持久语料复制到 `out/fuzz/<platform>/<target>/corpus`，再叠加目标内建种子。
工作目录可由 libFuzzer 扩充，但 CI 和本地短跑都不会直接修改源码树。
Sanitizer 构建使用 `-fno-builtin-memcpy`，避免 Clang 把明确允许未对齐的固定尺寸
`memcpy` 展开成带类型对齐检查的访问；alignment sanitizer 本身仍保持启用。

## 发布门禁

Linux CI 对全部目标各执行 20000 轮，发布候选执行更长门禁：

```text
python tools/test_protocol_fuzz.py --runs 100000
```

只复核本轮新增的三个发布目标可执行：

```text
python tools/test_protocol_fuzz.py tls x509 net-address --runs 20000
```

## 崩溃回流

`tls-state` 目标使用真实 TLS 1.3 PSK+DHE 握手创建新会话，一部分输入从握手中途开始，另一部分从认证后的 READY 开始。输入控制分片、读写、背压、KeyUpdate、ticket、关闭、EOF、密文损坏和带有效 AEAD 的畸形后握手消息；同时断言队列预算、终态不可复活和逻辑分配回到基线。每条输入最多 4096 字节、256 个操作，drive 使用小预算。种子保存在工具的目标配置中；普通模块/单头测试执行固定种子与 200 条确定性随机轨迹。

```text
python tools/test_protocol_fuzz.py tls-state --runs 20000
python tools/build.py --compiler gcc --suite tls_state_fuzz_tests --no-examples
```

TLS 外部互操作通过 `python tools/test_tls_interop.py` 对接 Python `ssl` 链接的 OpenSSL，打印实际版本，使用临时 CA 和短期证书，不访问公网或关闭证书验证。覆盖 TLS 1.2/1.3、RSA/P-256/P-384/Ed25519、AES-128/AES-256/ChaCha20、双端角色、分片、ALPN、数据、KeyUpdate、HRR 和认证关闭。Python `cryptography` 只用于测试生成证书和 RSA 差分，不是 XRT 运行依赖。

崩溃工件写入 `out/fuzz/<platform>/<target>/artifacts`，CI 失败时保留 14 天。处理流程为：

1. 用目标程序直接读取 `crash-*` 或 `timeout-*` 工件，确认能够稳定复现。
2. 使用 libFuzzer 的 `-minimize_crash=1` 与 `-exact_artifact_path=<path>` 生成最小样本。
3. 修复根因，并把最小样本放入对应的 `fuzz/corpus/<target>`；若语义更适合明确断言，
   同时或改为普通回归用例。
4. 重新执行目标短跑和所属模块、单头、裁剪测试，确认样本不再失败。

不得把未经确认的大型工作语料、包含私密数据的输入或未最小化崩溃直接提交到 corpus。
