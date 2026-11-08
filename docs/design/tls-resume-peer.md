# TLS 恢复凭据的证书链与当前策略

`XTLS_VERIFY_RESUME_PEER_V1` 是创建时固定的验证器选项。通过
`xrtTlsVerifierCreateFlagsV1` 或 `xrtTlsVerifierCreateOwnedFlagsV1` 传入；
原有工厂仍等价于 flags=0，配置结构及 owned V1 策略布局不变。

客户端签发队列中的每张票据共享实际完整握手保存的不可变证书链，不复制 DER，
不保存“上次验证成功”的布尔结论，也不保留旧验证器。服务端选择恢复 PSK 后，
客户端用当前验证器、有效验证名、当前验证时间重新调用完整验证入口；当前信任、
路径、名称、附加策略及由策略执行的撤销检查都重新生效。失败保持 checked
握手失败，不能继续进入 Ready；PSK Finished 仍按原状态机验证。

仅从 parts 创建的凭据没有证书链。启用该选项的客户端不提供此 PSK，改走完整
握手；ResumeOnly 则在创建时明确拒绝。`PeerIdentity` 仍是不透明调用方元数据，
绝不当作证书或信任证明。持有 PSK 的调用方可显式使用
`xrtTlsResumeWithPeerCertificatesV1` 创建一个带 DER 链的新凭据；导入只复制／
解析数据，不作认证承诺，恢复时依然重新验证。计数／字节上限必须显式传入，
0 代表调用方主动关闭该项预算，超限不截断。原凭据不被修改。

链采用原完整握手那一块精确分配，恢复会话及后继票据继续共享它。最后一个
引用释放后才销毁；无语言捕获、回调或旧决策。只编入基础 TLS_RESUME 时不
引入 peer 字段／客户端依赖。启用 CLIENT_RESUME 时，64 位常见 ABI 的票据
增加一个指针、链增加一个引用计数（含对齐）；复验本身有明确计算与回调成本，
不声称零开销。显式导入需要一块 DER 链快照和一份独立凭据复制。

## 验证

```text
python tools/test_tls_resume_peer.py --root <frozen-native-root> --out <new-evidence-directory> --tcc <tcc>
```

仅运行正常固定材料的内存握手、真实票据签发／恢复、策略变化、显式导入与
128 个分配故障位置、共享指针和释放账本。没有 socket、畸形线路、外部网络
或攻击测试。Windows GCC O0/O2、TCC 和 Linux 同配置加 ASan/UBSan，分别
验证模块化／单头。2026-10-26 最终接口的 14 个配置、28 个构建／运行阶段
通过，证据 `F:/xlang-validation/five-groups-20261026/p7-tls-resume-peer/`
中的 `native-win-r2` 和 `native-linux-r2`；r1 为收紧 DER 导入接口前的保留轮。

这不是 xlang shipping 或完整 TLS／五组联合验收结论。两侧已有时间合同
不变，不添加 0-RTT 或客户端证书认证协议功能。
