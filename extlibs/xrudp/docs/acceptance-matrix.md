# XRUDP 首次发布验收矩阵

更新：2026-10-22。当前仍处于开发验收，不能据此宣称生产验收完成。
本文将需求、实际证据和剩余门槛分开记录；追加测试或文档提交不能替代运行时版本的验证。

## 验证对象

- 下列 2026-10-19 矩阵是固定 `3d55ec41` 的历史基线。2026-10-22 复核时，mdo 产品 SDK 已锁定 XS `216f5445163ca30c815ad7d5d2da3b71cb2f6401`、XRUDP `0c46aca7519c5513c51b15de61d73d90ebca79b6`，核心仍为 `3d55ec41`。认证连通性检查等后续改动不能仅凭旧基线认定完成长期验收。
- XRUDP 运行时：`3d55ec41e13f0884ffe840d44a57a0189d9310f3`。其后的接入、配额与压力提交新增测试和文档，`src`、`include` 与该运行时无差异。
- XS 主线：`0014ed930f3afb9bce9f0a640168d3fef4bdb4bf`，已有单独的 `class:xrudp` 服务回归。
- mdo 产品 SDK：`a6906e202e051b6bc624f16ec131b88892963457`。保留产品需要的 xllm/session/xwork 等接口，不能用 XS 主线整库覆盖它。
- 本次独立交付检查的 mdo 提交：`2da7ad5928a35867f517d679f4b6e294bbaded51`。
- 后续 Android 真机使用已提交的 mdo `e6865f3c4fe889b12cad3db26a3555b32a2e4bd0`（0.2.0）；运行时和产品 SDK 不变。设备记录提交为 mdo `e5650615`、`f2813515`。
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
| 同套接字发现及打洞 | STUN、IPv4/IPv6、候选重试、10 秒尝试、250ms 双族竞速、取消落败连接 | 已实测手机移动网络与电脑不同公网映射、双向握手超时及自动中继回退；两个独立 NAT 的正向直连仍未证明 |
| mdo 直连优先及权限 | 进程内原生端点；认证一次性会合、公钥/owner/runtime/nonce/只读绑定；HTTP、实时订阅、业务收据、LAN/中继回退 | 当前 SDK 公网 Windows/Linux 正反向各通过一次，均跨 65 秒空闲；一侧 NAT 加公网服务器，不等于双 NAT |
| XS 独立协议服务 | TCP 风格监听/连接/默认字节流、运行代保留/切换/回滚及生命周期 | `D:/GIT/xserver/docs/xrudp-xrt-2026-10-19.md`；mdo 无须通过该服务建立端点 |
| 已提交 mdo 独立构建 | 仅从 Git 提交检出 2,561 个文件，无 7 个未跟踪传输源文件；Windows/Linux 六扩展宿主、打包启动/内置 TCC、Home 排他及崩溃后重开均通过 | `D:/GIT/mdo/.build/committed-source-audit-20261019/acceptance.json`；两平台再通过双族 NAT/IPv6 黑洞回退、HTTP/live/收据、只读、一次性票据、中继和注销回归 |
| 进程暂停恢复 | Windows/Linux、IPv4/IPv6、3 秒及 65 秒八项通过；短暂停恢复既有直连/live/中继，超过 60 秒期限后关闭旧直连并重建；继续完成 Unit/撤销/注销 | mdo `tests/test_remote_process_pause.py`，`.build/xrudp-process-pause/acceptance.json`；同一子进程内的两个端点同时暂停，网站仍运行，不代表单端或物理系统休眠 |
| 浏览器自动回退 | 当前 SDK 与干净 mdo 源码在真实 Edge 155 中完成双族 UDP 黑洞后回退 LAN，以及 LAN 失败后回退中继；实际写入无重复，关闭后原生计数为零 | 同上统一收据，2 个浏览器场景；无头浏览器验证不代表原生 WebView 外壳或发行 ABI |
| Android 真机 | ARM64 shell UID 下 IPv4/IPv6 × Select/Epoll 四项 Future；APK JNI/TCC 单独执行；双向直连、冻结恢复、65 秒持续后台、实际 Edge/WebView 自动中继回退、强制 Doze 恢复通过 | mdo `tests/acceptance/xrudp-android/acceptance.json`、`docs/build-records/2026-10-19-xrudp-android-device.md`；设备 4 KiB 页；永久 idle 白名单由用户启用，强制 Doze 期间保持亮屏供电，不等于物理系统休眠 |
| 长期运行 | `3d55ec41` Linux 四组已经完成连续 72 小时和正常排空；Windows 四组及 mdo 业务组约 28.1 小时后明确失败。另从当前主线 `1df26c67` 独立启动八组原生、当前产品 SDK 启动一组业务长稳 | [2026-10-22 长稳复核与重跑](long-soak-validation.md)；新九组仍待实际 72 小时及最终排空，历史通过与失败时长均不跨来源累计 |

## 本轮源码抽查

20 个运行时翻译单元的 Clang 路径分析及未屏蔽告警复核见 [静态分析记录](static-analysis-validation.md)。两条候选空指针路径与当前 opaque 状态不变量矛盾，未确认新运行时缺陷；这项检查不替代完整生产验收。

`src/network/rudp_endpoint.c` 先做报文结构检查，再分发已知 CID；未知 CID 在握手 Cookie/DH 前经过固定大小限速表。监听入口先预留连接与逻辑字节预算，失败路径归还；`src/crypto/rudp_handshake.c` 在分配握手对象和执行 Noise 前校验地址 Cookie。HELLO 的 Retry 不创建 peer。

已认证 peer 的 `XRUDP_AUTH_FAILED` 只计入无效报文，不关闭现有连接。迁移必须经过已认证路径处理，CID 或 STUN 映射本身不授予权限。关闭先释放运行态资源并发布最终快照，保留句柄仍计入端点预算，最后一次 Destroy 才释放句柄和端点引用。

mdo 的 `rendezvous.inc.c` 将 owner/runtime/peer/nonce/mode 纳入绑定并限制一次消费；`rudp.inc.c` 的 Engine、命令、定时器、消息和探测均有独立预算与取消期限，销毁等待端点终结并加入 Engine。抽查与既有动态验证未确认新运行时缺陷；不将抽查描述成完整安全审计或形式证明。

## 尚未满足的发布门槛

1. Android 物理系统挂起或自然熄屏 Doze：已经通过的 UID 冻结、持续后台和强制 DeviceIdle 策略，不能替代整机休眠；真实 16 KiB 页设备仍未执行。
2. 两个相互独立 NAT 的正向直连：移动网络已实测双方映射、角色反转、失败回退与正常收尾，但端口依赖映射下两方向都回退中继，不能算作双 NAT 直连成功。
3. 当前交付来源至少 72 小时混合长稳：Linux 的 `3d55ec41` 四组已完成，但 Windows 原生及 mdo 业务验收尚未通过；保留来源、二进制和原始起始时钟，中断、时钟偏差或更换运行时须如实失败/重算，不能沿用其他来源成绩。
4. 休眠恢复及最终安全/交付复核：区分进程暂停、实际系统休眠、Android 后台/Doze；当前记录不声称已完成系统级休眠恢复。测试、文档继续变化时需复核最终冻结来源。

独立构建检查中的 mdo 产物是 server 开发检查包：Linux 使用本机 WSL ABI，版本元数据取自已提交源码；不作为 GUI、发行 ABI、正式版本号或安装发布的验收。后续仅安装独立 Android QA 包，未覆盖手机原 mdo 或修改其 Home。工作区其他任务的 152 个 mdo 路径和 15 个 XRT 路径未纳入提交检查。没有公开发布或重启生产网站。
