# XRUDP 首次发布验收矩阵

更新：2026-10-19。当前仍处于开发验收，不能据此宣称生产验收完成。
本文将需求、实际证据和剩余门槛分开记录；追加测试或文档提交不能替代运行时版本的验证。

## 验证对象

- XRUDP 运行时：`3d55ec41e13f0884ffe840d44a57a0189d9310f3`。其后的接入、配额与压力提交新增测试和文档，`src`、`include` 与该运行时无差异。
- XS 主线：`0014ed930f3afb9bce9f0a640168d3fef4bdb4bf`，已有单独的 `class:xrudp` 服务回归。
- mdo 产品 SDK：`a6906e202e051b6bc624f16ec131b88892963457`。保留产品需要的 xllm/session/xwork 等接口，不能用 XS 主线整库覆盖它。
- 本次独立交付检查的 mdo 提交：`2da7ad5928a35867f517d679f4b6e294bbaded51`。
- 核心单头使用提交中的 `015f5b19…` / `6421baab…`，没有纳入工作区内其他任务的文件根目录改动。

## 需求与证据

| 需求 | 已有实现及验证范围 | 证据入口或剩余边界 |
| --- | --- | --- |
| 纯 C/XRT、无新增运行依赖 | 模块化与单头、Windows GCC、Linux Clang、TCC 宿主已验证 | `README.md`、`implementation-status.md`；开发用独立 Python 对端不是运行依赖 |
| 可靠多字节流、消息流 | 流额度、FIN/最终偏移、RESET/STOP、退休、可靠排空与关闭；独立实现对照及故障状态覆盖 | `out/xrudp-wire-interop/congestion-wire-acceptance.json`，2,080 个当前运行时进程，全部正常加入 Engine |
| 游戏数据报 | 过期、去旧、替换、受限权重与共享拥塞/资源预算；核心、原生和状态模型有回归 | `protocol.md` 的数据报契约、`implementation-status.md`；应用同步/预测算法仍由调用者实现 |
| 固定安全配置与同版互通 | Noise IK、公钥 pin、应用域、Cookie、密钥确认/轮换、防重放；独立线路四种平台/后端组合 | 同上独立线路收据，含 3,520 个未认证变异；这不是外部密码学审计结论 |
| 恢复、拥塞、路径 MTU | NewReno/RTT/PTO/pacing、持续拥塞证据、迁移重建、MTU 增长/黑洞回退 | `out/xrudp-wire-interop/congestion-acceptance.json` 及完整线路收据；持续拥塞修复位于 `3d55ec41` |
| 资源与关闭所有权 | 有界连接/半开/逻辑字节、保留关闭句柄、跨线程 Future、失败分配和取消/加入 | `out/xrudp-endpoint-quota/acceptance.json`，16 进程、1,024 次压力拒绝/恢复、3,072 次压力关闭、32 次 Engine 加入 |
| 未认证接入压力 | 固定限速桶、全局/每 IP 限速、恶意 CID/来源、合法流持续进展、内存/句柄上界 | `endpoint-admission-validation.md`、`ingress-pressure-validation.md`；压力收据含 8 进程、388,352 个实际发送数据报，最差流比率 0.9581；仅代表测得负载 |
| 同套接字发现及打洞 | STUN、IPv4/IPv6、候选重试、10 秒尝试、250ms 双族竞速、取消落败连接 | 核心/原生发现、打洞与 mdo rendezvous 回归；两个独立 NAT 的实测待完成 |
| mdo 直连优先及权限 | 进程内原生端点；认证一次性会合、公钥/owner/runtime/nonce/只读绑定；HTTP、实时订阅、业务收据、LAN/中继回退 | 当前 SDK 公网 Windows/Linux 正反向各通过一次，均跨 65 秒空闲；一侧 NAT 加公网服务器，不等于双 NAT |
| XS 独立协议服务 | TCP 风格监听/连接/默认字节流、运行代保留/切换/回滚及生命周期 | `D:/GIT/xserver/docs/xrudp-xrt-2026-10-19.md`；mdo 无须通过该服务建立端点 |
| 已提交 mdo 独立构建 | 仅从 Git 提交检出 2,561 个文件，无 7 个未跟踪传输源文件；Windows/Linux 六扩展宿主、打包启动/内置 TCC、Home 排他及崩溃后重开均通过 | `D:/GIT/mdo/.build/committed-source-audit-20261019/acceptance.json`；两平台再通过双族 NAT/IPv6 黑洞回退、HTTP/live/收据、只读、一次性票据、中继和注销回归 |
| 实机及长期运行 | 当前运行时 9 组真实时钟测试在运行；历史 18 组单列为旧运行时基线 | 未达到 72 小时；Android ARM64 QA 包构建/签名/16KiB 静态校验通过，实体设备执行待完成 |

## 本轮源码抽查

`src/network/rudp_endpoint.c` 先做报文结构检查，再分发已知 CID；未知 CID 在握手 Cookie/DH 前经过固定大小限速表。监听入口先预留连接与逻辑字节预算，失败路径归还；`src/crypto/rudp_handshake.c` 在分配握手对象和执行 Noise 前校验地址 Cookie。HELLO 的 Retry 不创建 peer。

已认证 peer 的 `XRUDP_AUTH_FAILED` 只计入无效报文，不关闭现有连接。迁移必须经过已认证路径处理，CID 或 STUN 映射本身不授予权限。关闭先释放运行态资源并发布最终快照，保留句柄仍计入端点预算，最后一次 Destroy 才释放句柄和端点引用。

mdo 的 `rendezvous.inc.c` 将 owner/runtime/peer/nonce/mode 纳入绑定并限制一次消费；`rudp.inc.c` 的 Engine、命令、定时器、消息和探测均有独立预算与取消期限，销毁等待端点终结并加入 Engine。抽查与既有动态验证未确认新运行时缺陷；不将抽查描述成完整安全审计或形式证明。

## 尚未满足的发布门槛

1. 实体 Android ARM64：实际原生/Future 执行、直连/中继、前后台与取消生命周期。注册表、ELF 或 APK 静态校验不能替代执行。
2. 两个相互独立 NAT 的设备：双方实际映射、角色反转、失败回退与正常收尾。现有本地 NAT 模拟及公网服务器场景不能替代。
3. 当前运行时至少 72 小时混合长稳：保留现有来源、二进制和原始起始时钟；中断、超出采样间隔或更换运行时须如实失败/重算，不能沿用旧运行时成绩。
4. 休眠恢复及最终安全/交付复核：区分进程暂停、实际系统休眠、Android 后台/Doze；当前记录不声称已完成系统级休眠恢复。测试、文档继续变化时需复核最终冻结来源。

本次 mdo 构建产物是 server 开发检查包：Linux 使用本机 WSL ABI，版本元数据取自已提交源码；不作为 GUI、发行 ABI、正式版本号或安装发布的验收。工作区其他任务的 152 个 mdo 路径和 15 个 XRT 路径未纳入提交检查。没有安装、发布或重启生产网站。
