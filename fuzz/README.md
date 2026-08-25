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

驱动会把持久语料复制到 `out/fuzz/<platform>/<target>/corpus`，再叠加目标内建种子。
工作目录可由 libFuzzer 扩充，但 CI 和本地短跑都不会直接修改源码树。

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

崩溃工件写入 `out/fuzz/<platform>/<target>/artifacts`，CI 失败时保留 14 天。处理流程为：

1. 用目标程序直接读取 `crash-*` 或 `timeout-*` 工件，确认能够稳定复现。
2. 使用 libFuzzer 的 `-minimize_crash=1` 与 `-exact_artifact_path=<path>` 生成最小样本。
3. 修复根因，并把最小样本放入对应的 `fuzz/corpus/<target>`；若语义更适合明确断言，
   同时或改为普通回归用例。
4. 重新执行目标短跑和所属模块、单头、裁剪测试，确认样本不再失败。

不得把未经确认的大型工作语料、包含私密数据的输入或未最小化崩溃直接提交到 corpus。
