# XTP3 生产验收与跨线程交接手册

> 更新：2026-11-17，时区 Asia/Shanghai。用户确认正式长稳为**连续24小时**，随后要求本线程先跳过测试、由其他线程验收。本文件是执行方法和交接依据，不是生产通过证书。本次文档整理不启动或重跑测试。

## 1. 接手结论与当前现场

功能开发已按确认方案完成；生产验收尚未全部完成。不要把“功能已实现”“某次局部测试通过”“整体可发布”混成同一结论。

| 内容 | 当前现场与使用方式 |
| --- | --- |
| XTP生产源码基准 | `cf24b0155bc5fabc958d5d10baf3e6e36d8e96bd`；后续验收工具开发未修改 `extlibs/xtp/include` / `src`。接手时必须再次比较，不能仅相信本表 |
| 已提交的验收工具阶段 | `8a56b8db`：验收门限/记录器/UDP迁移；`2d791bc9`：真实TLS续接/模糊入口；`4641cd2e`：逐消息内容校验；`e67a4541`：系统分配器诊断变体。以接手时实际HEAD及本文件所在提交为准 |
| XS集成 | `D:/GIT/xserver`，集成提交 `c8547ec9441ba3757d063e9ee57494e252247b3d`，vendored XTP固定于上述 `cf24b015`；核心及其他扩展有各自固定版本 |
| 已有性能证据 | 四种形态，每种100万次RPC、7次重复，固定CPU，原有质量门限通过；最大P99中心范围约1.66%。原始收据见 [生产P99记录](measurements/2026-11-17-production-p99-windows.json) |
| 已有故障证据 | 实际UDP重绑定/回切/500ms黑洞/未认证路径/密钥轮换，以及实际TLS部分响应断线续接；详见 [实验室收据](measurements/2026-11-17-production-lab.json) |
| 原72小时长稳 | 两项均已按用户指令停止，约运行12.4小时；未执行最终完整回收，不是24小时通过，也不是72小时通过。原报告的非零退出是人工终止，须结合 `out/xtp3-production/user-stop.json` 分类 |
| 新24小时长稳 | 尚未完成。必须新建运行目录、重新连续计时；不能把已中断的12.4小时拼接到新运行 |
| 模糊测试 | 旧默认XRT分配器的4项600秒运行已完成。当前工具默认 `--allocator system`；系统分配器的短预检不等于正式600秒验收，接手时检查报告里的分配器及命令 |
| 系统诊断 | 已采集部分任务时钟、上下文切换和系统调用计数；sched_wakeup为零且正对照也为零，尚未验证其有效性，不能称为“零唤醒”或完整复制归因 |
| 真机/部署端点 | 此前ADB盘点无连接设备，未获得正式设备/端点信息；这是历史盘点，接手线程须重新核实 |

历史方法、体积和功能覆盖分别见 [qualification.md](qualification.md)、[performance.md](performance.md)、[coverage.md](coverage.md)。设计与语义以 [XTP3设计](../../../docs/design/xtp3.md)、[Session](session.md)、[Delivery](delivery.md)、[承载](transport.md)、[xrudp](xrudp.md) 为准。

### 1.1 给接手线程的直接指令

可将下面整段交给其他线程：

```text
在 D:/GIT/xrt/master 接手 XTP3 生产验收，先完整阅读
extlibs/xtp/docs/production-acceptance.md 和根目录 AGENTS.md。
用户确认长稳为连续24小时；上一线程已停止旧72小时任务。
先核对HEAD、工作区、源码/工具/依赖和现有证据，不清理其他线程改动。
按本文G0-G7执行，所有新运行使用独立目录；保留失败和中断记录。
性能测量保持串行、固定条件，不与长稳/编译/模糊测试并发。
不要修改阈值以制造通过，不把短预检、QEMU、回环或实验室NAT当作真机公网资格。
每阶段报告来源、命令、退出码、通过/失败/未执行项及证据位置。
测试通过后按明确路径分阶段提交；不推送远端、不进行发布。
任何必需项未完成时，整体状态保持未完成，不签发生产通过结论。
```

## 2. 验收范围与判定状态

### 2.1 验收边界

本验收覆盖协议库、组合承载和XS集成；路由、业务schema、业务幂等、持久队列、登录/授权服务、媒体编解码、抖动缓冲不属于XTP新增功能。

明确三种模型的成功含义：

- REQUEST：RESPONSE只表达应用返回的结果，不等于跨进程恰好一次业务效果。
- PUSH：成功ACK表示接收端回调接管/进入应用有界内存队列，不表示落盘或业务已执行完。
- NOTIFY：本地SENT表示下层受理；远端收到数必须另外统计。故障网络中丢失允许，不能伪造回执或当作可靠消息。
- Delivery：只对显式Replay且身份、状态、期限、预算均有效的操作恢复；健康可靠链路不再叠加XTP超时重传。状态丢失须UNKNOWN/STATE_LOST，不能静默重跑。

### 2.2 状态字典

| 状态 | 含义 | 可计为通过 |
| --- | --- | --- |
| passed | 对声明的源码、平台和参数，完整执行且所有门限满足 | 仅该项可以 |
| failed | 断言、协议不符、诊断、计数、资源或门限失败 | 否 |
| running | 仍运行或尚未正常退出 | 否 |
| pending | 等待执行、设备、端点或必需资料 | 否 |
| interrupted | 人工停止、重启、断电、监督器退出 | 否 |
| skipped / out_of_scope | 用户明确排除，须写明理由、影响和适用范围 | 否，不能替代原必需项 |
| partial / invalid | 指标无有效对照、环境不可比、证据不完整等 | 否 |

原始工具可能将人工终止记为failed。保留原始内容，另写“操作员人工中断”的分类与理由，不改成passed；同样不能把真实崩溃包装成人工中断。

### 2.3 必需关卡

| 关卡 | 通过标准 |
| --- | --- |
| G0 来源与环境 | 源码/依赖/工具/二进制/配置可定位，有哈希，无影响结论的未解释变动 |
| G1 功能与交付形态 | 核心、单头、C99、目标ABI、包消费、所有声明的消息/API组合完整通过 |
| G2 安全与资源 | 畸形输入、配额、OOM、生命周期、重入、身份/状态失败通过；sanitizer及正式模糊测试无错误 |
| G3 实际连接故障 | 可靠消息在有效条件下完成、无重复业务；无效恢复不执行；NOTIFY按语义丢弃 |
| G4 可重复性能 | 质量门限、计数和基线比较均满足；部署绝对SLO必须事先声明 |
| G5 连续24小时 | 每个必需场景独立连续86400秒，稳定检查、内存预算、退出与释放均通过 |
| G6 目标设备/网络 | 声明范围内的实际设备、网络、宿主接入和故障切换通过 |
| G7 XS集成与最终签发 | 对实际固定快照构建宿主、外层/嵌套TCC及示例通过，汇总所有关卡和限制 |

发布范围不包含移动端时，可由用户在执行前明确排除相应设备项；执行者不能自行用回环或模拟器抵消尚未完成的移动部署要求。内核/全链路复制归因属于增强诊断；若部署性能合同要求它，须提升为必需项。

## 3. 执行前冻结和证据目录（G0）

默认直接在master工作，不新建分支/工作树。遵守 `AGENTS.md` 和 `docs/development/parallel-branch-integration.md`：不stash/reset/clean，不提交其他线程文件，不改写历史，不推送或发布。

Windows从 `D:/GIT/xrt` 执行以下准备；后续示例用 `$run` 指代**本次新目录**：

```powershell
Set-Location D:/GIT/xrt
$env:PATH = 'E:/software/w64devkit/bin;' + $env:PATH
$run = 'out/xtp3-acceptance/' + (Get-Date -Format 'yyyyMMdd-HHmmss')
New-Item -ItemType Directory -Path $run | Out-Null
git rev-parse HEAD > "$run/head.txt"
git branch --show-current > "$run/branch.txt"
git status --short > "$run/status-before.txt"
git diff --binary -- extlibs/xtp > "$run/xtp-working.patch"
gcc --version > "$run/compiler-windows.txt"
python --version > "$run/python-windows.txt"
git diff cf24b015 -- extlibs/xtp/include extlibs/xtp/src > "$run/production-source-diff.txt"
```

Linux从 `/mnt/d/GIT/xrt` 或实际仓库根目录执行，另建本次目录，以下用 `$RUN` 指代：

```sh
cd /mnt/d/GIT/xrt
RUN=out/xtp3-acceptance/$(date +%Y%m%d-%H%M%S)-linux
mkdir -p "$RUN"
git rev-parse HEAD > "$RUN/head.txt"
git status --short > "$RUN/status-before.txt"
clang --version > "$RUN/compiler-linux.txt"
uname -a > "$RUN/kernel.txt"
```

必须额外记录：CPU/核心数、OS版本、内存、后端、编译器完整版本、优化/链接/宏参数、亲和性、电源策略、是否虚拟机/WSL、并发负载、网络拓扑、Body/参数/在途/队列/期限、证书验证方式及部署端点。不要输出私钥、令牌或真实凭据。

冻结XRT以及xws/xrudp实际来源；XS核对 `lib/xrt_sources.lock.json`。HEAD相同但工作区不同，不是相同来源。若保留其他线程无关脏文件，写清路径、为什么不进入本次编译，仍保留实际编译输入的哈希。

正式长稳前固化源码、构建和参数。生产代码或实际链接依赖改变后，旧长稳不能为新二进制背书；重新构建并重启受影响关卡。纯文档更正不自动要求重跑，但必须说明差异。

## 4. 功能、安全、交付形态（G1、G2）

### 4.1 核心模块与单头

```powershell
python tools/build.py --manifest extlibs/xtp/config/modules.json --suite xtp_tests,xtp_delivery --compiler gcc --cflag=-O2 *> "$run/core-windows.log"
$code = $LASTEXITCODE
$code | Set-Content "$run/core-windows.exit"
if ($code -ne 0) { throw 'core validation failed; preserve the log' }
```

至少覆盖并逐项对照现有测试，而不是只看“有可执行文件”：

| 范围 | 必须观察的行为 |
| --- | --- |
| 线路 | 固定32字节头、独立黄金字节、LE字段、边界值，明确拒绝XTP1/2及非法类型/位/长度 |
| 四种形态 | CommandID/string与array/table独立组合；数字命令0合法；table保留线路顺序，table/array均支持ParamAt |
| Reader | 单字节/所有拆分点、多帧粘包、RECORD结束、EOF、PAUSE/Resume、准确Used、有界丢弃及接收绝对期限 |
| 关联 | 双向ID、请求/响应类型和lane匹配、发送状态、未知/重复/迟到结果，不完成错误操作 |
| 资源 | 有限pending/tx/reader/保留预算，处理业务前预留响应/ACK；OOM不部分发布、失败不接管输入 |
| 生命周期 | 借用视图、异步请求令牌、回调重入拒绝、分配器重入、旧绑定/旧Ticket失效、分轮关闭和恰好一次本地完成 |
| 聚合 | 同一可靠lane立即聚合、不等待凑包；跨帧短写、RECORD整批受理、OOM回退、回调修改队列、部分发送和预算正确 |
| 恢复 | 显式Replay、身份/状态/期限证明、BUSY退避、原ID/内容/截止时间、滚动完成窗口不淘汰活跃请求、缓存有限 |

非法STREAM前缀若无法可靠确定下一帧边界，允许按设计关闭；不能为“继续通信”扫描magic猜测边界。RECORD及可验证长度内的错误按Reader设计丢弃，不污染下一记录。

### 4.2 严格C99与sanitizer（Linux）

```sh
python3 tools/build.py --manifest extlibs/xtp/config/modules.json --suite xtp_tests,xtp_delivery --compiler clang --cflag=-std=c99 --cflag=-pedantic-errors --cflag=-O1 --cflag=-g --cflag=-fsanitize=address,undefined --cflag=-fno-sanitize-recover=all --ldflag=-fsanitize=address,undefined --no-examples > "$RUN/core-c99-asan.log" 2>&1
printf '%s\n' "$?" > "$RUN/core-c99-asan.exit"
```

通过要求：退出0、模块/单头目标均实际运行、无ASan/UBSan/LeakSanitizer诊断。若某项未编译或未运行，列为未执行，不用总入口的通过掩盖它。

### 4.3 32位、大端、静态/动态与C++

```powershell
python tools/build.py --manifest extlibs/xtp/config/modules.json --suite xtp_tests,xtp_delivery --compiler i686-w64-mingw32-gcc --cflag=-O2 --cflag=-D_WIN32_WINNT=0x0601 *> "$run/core-x86.log"
```

```sh
QEMU_LD_PREFIX=/usr/powerpc64-linux-gnu python3 tools/build.py --manifest extlibs/xtp/config/modules.json --suite xtp_tests,xtp_delivery --compiler powerpc64-linux-gnu-gcc --cflag=-O2 --runner qemu-ppc64 > "$RUN/core-ppc64.log" 2>&1
```

QEMU结果只证明对应字节序/ABI下的行为，不是该CPU原生吞吐或实体移动设备证据。交叉工具链/运行时缺失应报环境缺口。

```powershell
python tools/package.py --manifest extlibs/xtp/config/modules.json --suite xtp --kind static --compiler gcc --verify *> "$run/package-static.log"
python tools/package.py --manifest extlibs/xtp/config/modules.json --suite xtp --kind shared --compiler gcc --verify *> "$run/package-shared.log"
python tools/package.py --manifest extlibs/xtp/config/modules.json --suite xtp,xtp_native,xtp_xrudp --kind static --compiler gcc --verify *> "$run/package-full-static.log"
python tools/package.py --manifest extlibs/xtp/config/modules.json --suite xtp,xtp_native,xtp_xrudp --kind shared --compiler gcc --verify *> "$run/package-full-shared.log"
```

Windows/Linux包输出可能共用 `release/gnu/native`，必须串行，及时把本平台产物与packages.json另存到证据目录；不能把Linux库误链接到Windows消费者。C++11消费者源码为 `tests/package/test_consumer.cpp`。核心包示例：

```powershell
g++ -std=c++11 -Wall -Wextra -Werror -DXTP_MODULE_XTP -Iinclude -Iextlibs/xtp/include extlibs/xtp/tests/package/test_consumer.cpp release/gnu/native/xtp/static/libxtp.a release/gnu/native/xtp/static/libxrt.a -o "$run/consumer-static.exe"
& "$run/consumer-static.exe"
g++ -std=c++11 -Wall -Wextra -Werror -DXTP_MODULE_XTP -DXTP_USE_SHARED -DXRT_USE_SHARED -Iinclude -Iextlibs/xtp/include extlibs/xtp/tests/package/test_consumer.cpp release/gnu/native/xtp/shared/libxtp.dll.a release/gnu/native/xtp/shared/libxrt.dll.a -o "$run/consumer-shared.exe"
$env:PATH = (Resolve-Path release/gnu/native/xtp/shared).Path + ';' + $env:PATH
& "$run/consumer-shared.exe"
```

每条构建/运行分别保存退出码；编译失败时禁止继续运行上次遗留的同名exe。Linux应使用对应本机包，不照搬Windows导入库路径。

### 4.4 模糊测试与分配器覆盖

```sh
python3 extlibs/xtp/bench/fuzz_qualification.py --compiler clang --allocator system --seconds 600 --report "$RUN/fuzz-system-600.json" > "$RUN/fuzz-system-600.log" 2>&1
python3 extlibs/xtp/bench/fuzz_qualification.py --compiler clang --allocator xrt --seconds 600 --report "$RUN/fuzz-xrt-600.json" > "$RUN/fuzz-xrt-600.log" 2>&1
```

每次依次运行Codec/Reader/Session/Delivery，故一套约需40分钟加编译时间。分别保存结果，不将两种分配器混成同一配置。每目标必须真实运行满600秒，退出0，有最终统计、无崩溃/超时/内存诊断；保存语料及失败输入。固定随机种子和独立有效/非法线路种子已在工具中。

XRT默认小块堆会池化，ASan不一定逐块识别其内部子分配。system模式对XTP Reader/Session使用直接libc分配，获得逐块红区/释放检测；xrt模式保留真实默认分配器语义。两者互补，不能用其中之一声称所有底层网络/运行时分配均有逐块检测。有限轮次不证明所有输入安全。

失败先保存输入、源码、exe哈希和完整日志，再用报告中的目标二进制与单个失败输入复现；修复后复现原样例，再重跑受影响目标/语料。不删除失败语料以换取通过。

## 5. 真实承载与故障恢复（G3）

```powershell
python tools/build.py --manifest extlibs/xtp/config/modules.json --suite xtp_native_tests,xtp_wss_tests,xtp_xrudp_tests,xtp_delivery_native_tests --compiler gcc --cflag=-O2 *> "$run/transports-windows.log"
```

Linux使用Clang ASan/UBSan及 `-fno-sanitize-recover=all` 运行相同suite，并保留模块和单头结果。

| 场景 | 具体标准 |
| --- | --- |
| TCP/直连TLS/WS/WSS | 双向三模型按语义完成，正确短写/背压/记录边界，正常退休后释放宿主对象 |
| TLS安全 | 真实协商1.3；策略允许的ChaCha20-Poly1305/AES-128-GCM，验证对端身份；错误pin、TLS1.2拒绝且业务调用0 |
| WSS | TLS的HTTP ALPN、真实Upgrade/101检查、`xtp.v3`；两端旧/伪造子协议拒绝；HTTP头后余下明文交给xws，不丢首帧 |
| xrudp损失矩阵 | 0/1/5/10%损失 × 20/80/200ms RTT的12组合；可靠命令/资源完成、无重复业务，资源传输时快照仍可用 |
| 快照/MTU | 60Hz提交、LATEST及应用去旧；1200→512路径下降后较大NOTIFY本地拒绝，小快照及可靠流继续。注入时钟用例不是实际延迟成绩 |
| NAT/路径 | 实际UDP源端口切换、回切、500ms双向黑洞、未认证第三方报文；路径证明后可靠消息继续，业务不重复，密钥轮换仍可用 |
| TLS续接 | RESPONSE只发7字节后断开真实连接；旧事件隔离、Unbind、退休、认证后续接；20轮有效续接无重复业务、旧Ticket拒绝 |
| 失败续接 | 错误凭据、服务端XTP状态销毁、身份声明错误，不执行新一轮业务；已发送未决操作以UNKNOWN结束 |

`test_xrudp_path` 可直接带循环数1～1000，例如本机已构建Linux目标：

```sh
out/clang/native/xtp_xrudp_tests/test_xrudp_path 100 > "$RUN/udp-path-100.log" 2>&1
```

确认路径对应本轮编译产物后再执行，不能拿旧exe替代。固定网络矩阵中的操作/接收/快照期限见 [xrudp.md](xrudp.md)，不是核心默认值。

恢复的前提必须来自真实认证边界。测试前端的固定证书pin和新鲜随机凭据仅用于实验；`Authenticated=true` 是宿主声明，不能把设置该字段本身当作认证验证。真实服务端状态丢失后允许UNKNOWN，不能为追求“零错误”新建同名会话重跑。

## 6. 性能与容量（G4）

### 6.1 可重复性门限

```powershell
python extlibs/xtp/bench/measure_qualification.py --compiler gcc --cpu 4 --report "$run/p99-windows.json" *> "$run/p99-windows.log"
```

使用 `config/qualification_profiles.json`：四种Command/Params形态，每种每次100万RPC，7次重复；仅调整本测试进程及子进程亲和性，结束恢复，不修改系统电源计划。CPU4不可用时取第一个允许的CPU，报告实际值。性能测量应独占相应资源，不与编译、fuzz、长稳或其他负载并发。

固定标准：

- offered/submitted/completed均符合目标，rejected=0、timed_out=0。现有profile报告只保留吞吐、rejected、P99和峰值内存四项；程序内部另有完成数/服务端收到数的非零退出检查。它没有保存每次stdout，不能声称该报告含全部计数字段。正式验收须另留原始JSON计数证据，或在运行前补充记录器保存每次stdout并验证该工具改动。
- 每个纳入profile的指标 `relative_mad ≤ 0.20`，`relative_central_range ≤ 0.30`。
- 相对MAD为 `median(|样本−中位数|)/|中位数|`。7个样本的中心范围去掉最高、最低各一个，再以剩余最大减最小除以中位数；全部原始样本仍保留。中位数为0按工具实现判断，不能自行除0或删样本。
- 报告保留源身份、机器、CPU、编译器、flags、亲和性、电源策略、原始重复及质量失败。不得反复运行只挑最快/通过的一次。

旧2万目标/未固定亲和性的失败和新百万目标/固定亲和性的通过是不同实验；只能说明新条件下的可重复性，不能抹掉旧失败。

现有GCC/native构建的单形态原始JSON输出方法如下；shape取0、1、2、3分别执行并检查退出码，路径须确认是本轮构建产物：

```powershell
& out/performance/gnu/native/xtp/xtp_shape_0.exe 1000000 0 64 8 1 1 0 > "$run/shape-0-counts.json"
```

原始JSON需满足 offered=submitted=completed=1000000、rejected=timed_out=0。补充运行须记录实际CPU亲和性、exe哈希和参数，明确是**独立计数验证**，不是前面七次测量被丢弃stdout的补写；若要求七次性能样本和全部计数逐一对应，必须先完成stdout留存再正式重测。

### 6.2 容量矩阵和回归

```powershell
python extlibs/xtp/bench/measure_matrix.py --compiler gcc --count 5000 --repeats 3 --report "$run/matrix.json" *> "$run/matrix.log"
python extlibs/xtp/bench/measure_native.py --compiler gcc --count 2000 --repeats 3 --report "$run/native.json" *> "$run/native.log"
python extlibs/xtp/bench/measure_capacity.py --compiler gcc --count 500 --repeats 3 --report "$run/capacity.json" *> "$run/capacity.log"
```

分别覆盖本地Codec/Session、真实TCP/TLS/WS、真实UDP/Noise、共享引擎多连接多worker。--quick只能预检。原生WS容量从已Upgrade的流开始；WSS功能/长稳不是WSS同口径容量数据。每连接一个临时listener的多连接测试，也不代表单listener接入峰值。

计数要求：闭环可靠场景目标全完成且无超时；NOTIFY受理和实际接收分列；固定到达率所有 offered=accepted+rejected，窗口拒绝不能删掉，生成器滞后另报。每连接都记录，不能只报合计掩盖饥饿。

同机器/编译器/配置/亲和性/电源策略/benchmark指纹可比时，当前默认回归阈值：吞吐中位数不低于基线90%，低者更好指标不高于基线120%；零拒绝基线不能增加拒绝。配置针对某指标有专门limit时以冻结配置为准。环境或benchmark指纹不一致应判不可比，不拼接跨机器、不同Body或不同冷热状态数据。

部署绝对SLO必须在执行前填写：目标并发、Body/参数分布、到达率、吞吐下限、端到端P99/超时/拒绝上限及网络条件。未声明时只能完成库级容量及质量门限验收，不能凭某个回环数值签发生产容量承诺，更不能声称固定HTTP加速倍数。

### 6.3 体积、复制和系统诊断

```powershell
python tools/measure_size.py --compiler gcc --config extlibs/xtp/config/size_profiles.json --profiles xtp_codec,xtp_session,xtp_delivery --kind static --report "$run/size.json" *> "$run/size.log"
```

源码不设硬上限，优先简洁、安全、性能。分别报告UTF-8/LF自有include/src、生成物、目标码text/data/bss、依赖及运行内存，不能把目标码当源码或隐去依赖。

`BatchCopiedBytes`是聚合构建复制，包含构建后未发送快照；宿主暂存、Reader碎片复制、编码、原生writer及TLS处理另有成本。writer调用不等于系统调用，内核上下文切换不等于唤醒，统计memcpy调用也不等于全部编译后复制。

Linux x86_64可在已运行的本轮子进程上使用诊断入口：

```sh
python3 extlibs/xtp/bench/profile_runtime.py --run-report "$RUN/L2-wss-pool-24h.json" --seconds 30 --report "$RUN/kernel-counters.json" > "$RUN/kernel-counters.log" 2>&1
```

它只针对已知验收子进程/TID，采集聚合软件与系统调用计数，不采集消息载荷。当前环境的sched_wakeup零值未通过正对照，因此该项是partial/未验证，不能解释成零唤醒；这份诊断代码的最终修订也需接手线程先做正对照。没有内核权限/tracefs或线程集合变化时保留缺口。该工具不是完整memcpy指令归因器；若业务合同要求全部复制归因，必须额外制定并执行测量方案。

## 7. 连续24小时长稳（G5）

### 7.1 场景、构建与强制参数

24小时指**每个必需场景独立连续86400秒**，不是多个短运行相加，也不是三场景各8小时。默认执行下列三种组合；若目标明确不包含某分配器配置，可在运行前记录范围和理由，不能事后把失败排除。

| ID | 配置 | 时长/工作负载 |
| --- | --- | --- |
| L1 | Windows GCC -O2，默认XRT分配器，TLS1.3 | 86400秒；8连接、2worker、每方向每轮2000请求、Body64 B |
| L2 | Linux Clang ASan/UBSan，默认XRT分配器，WSS | 同上 |
| L3 | Linux Clang ASan/UBSan，XTP实例使用libc分配器，WSS | 同上，保证XTP小块分配的逐块检测 |

构建均包含Delivery编译路径，长稳业务是普通RPC；Replay恢复由G3及G2专项覆盖。它们不能证明在长稳中反复断线恢复，也不替代xrudp游戏故障矩阵。TCP/明文WS等实际部署配置还应增加对应场景。

```powershell
python extlibs/xtp/bench/build_production.py --compiler gcc --output "$run/L1.exe" *> "$run/L1-build.log"
python extlibs/xtp/bench/production_soak.py --exe "$run/L1.exe" --seconds 86400 --connections 8 --workers 2 --mode 1 --count 2000 --report "$run/L1-tls-pool-24h.json" *> "$run/L1-supervisor.log"
```

```sh
python3 extlibs/xtp/bench/build_production.py --compiler clang --sanitizers --output "$RUN/L2" > "$RUN/L2-build.log" 2>&1
python3 extlibs/xtp/bench/production_soak.py --exe "$RUN/L2" --seconds 86400 --connections 8 --workers 2 --mode 3 --count 2000 --report "$RUN/L2-wss-pool-24h.json" > "$RUN/L2-supervisor.log" 2>&1
python3 extlibs/xtp/bench/build_production.py --compiler clang --sanitizers --system-allocator --output "$RUN/L3" > "$RUN/L3-build.log" 2>&1
python3 extlibs/xtp/bench/production_soak.py --exe "$RUN/L3" --seconds 86400 --connections 8 --workers 2 --mode 3 --count 2000 --report "$RUN/L3-wss-system-24h.json" > "$RUN/L3-supervisor.log" 2>&1
```

Linux示例按顺序执行；资源足够时可在独立进程并行，但要记录并发负载。工具 `production_soak.py` 当前缺省仍是72小时，**必须显式传 `--seconds 86400`**。不要改原72小时运行文件来冒充24小时配置。

mode0/1/2/3分别为TCP/TLS1.3/WS/WSS。构建禁止NDEBUG；sanitizer构建启用 `-fno-sanitize-recover=all`。每条请求有完整64位序号、连接号、方向和确定性内容；服务端逐字节检查，客户端对照原Tag内容，防止全零payload掩盖串包和错误关联。

每轮在owner worker检查 pending/incoming/tx/retained清空，实际XTP分配与Session账本一致，计数64位；正常结束在原生退休后Close/Destroy，检查自有分配字节及块数归零。

### 7.2 固定通过标准

| 标准 | 数值与解释 |
| --- | --- |
| 时间 | 子进程真实seconds≥86400，监督器真实elapsed_seconds≥86400；不是注入时钟 |
| 完成 | 总完成=rounds×connections×2×count，无重复/串包/错误Tag/意外超时/断言；本探针不把拒绝藏进成功分母 |
| 进度 | 检查点完成数严格单调，`ledger_match=true`、`quiescent=true` |
| 资源 | 每进程RSS≤3072MiB；预热300秒后静止XTP实际分配不高于预热峰值+每peer128KiB（8连接×2peer总余量2MiB） |
| 存活 | 150秒无完成进度即失败；总时长加180秒仍未结束即失败 |
| 诊断 | 无ASan/UBSan/LeakSanitizer等错误，恢复后退出0的诊断也算失败 |
| 收尾 | 子进程退出0，存在最终summary，计数对账，Close/Destroy后自有分配归零；人工kill不能提供该证据 |

这些阈值由工具默认实现并写入报告。若目标资源预算要调整，必须在运行前说明并冻结，报告实际参数。RSS包含整个进程、网络/运行时/sanitizer、测量数组和校验payload；不能把它当作XTP独占内存。OS可能保留堆页，因此不能用“RSS未归零”代替真实分配泄漏判断；也不能只看RSS不看账本。

### 7.3 状态、监控和后台运行

每次 `--report foo.json` 同目录生成：

- `foo.json`：原子更新的状态、源码/命令/exe哈希、PID、内存尾部、最后检查点；结束补充退出码、summary和日志哈希。
- `foo.child.log`：子进程检查点、断言/诊断和最终摘要。
- `foo.events.jsonl`：逐项检查点及约两秒一次的OS内存采样。

长稳首次看到running后确认head/exe/required_seconds/command与本轮计划一致。每30～60分钟汇报一次进展；若其他阶段失败、没有进度、资源超限或源码被改，立即汇报，不等24小时结束。

后台执行时重定向stdout与stderr，保存监督器及子进程PID、启动时间和报告路径。Windows使用Start-Process必须 `-WindowStyle Hidden`；需要停止时先核对PID、可执行路径和父子关系，不按泛化进程名杀进程。不能以“进程还活着”作为验收通过。

### 7.4 记录器和故意损坏的负对照

```powershell
python extlibs/xtp/tests/test_production_soak.py *> "$run/recorder-negative-tests.log"
python extlibs/xtp/bench/build_production.py --compiler gcc --inject-test-corruption --output "$run/intentional-bad.exe" *> "$run/intentional-bad-build.log"
python extlibs/xtp/bench/production_soak.py --exe "$run/intentional-bad.exe" --seconds 5 --connections 2 --workers 2 --mode 1 --count 100 --report "$run/intentional-bad.json" *> "$run/intentional-bad-supervisor.log"
```

记录器5项测试应通过，含伪造时长、非零退出、恢复型sanitizer报错、重复进度和RSS超限。故意损坏的exe则应非零退出、报告failed，并在日志出现内容校验失败；这是“负对照通过”，不是可用于长稳的二进制。编译失败/进程未启动不能代替这次预期校验失败。正式长稳禁止 `--inject-test-corruption`。

## 8. 真机与目标部署环境（G6）

先写清设备型号/架构/OS、客户端/服务端源码及exe哈希、认证方式、端点、运营商/接入方式、MTU、队列/期限/重试/keepalive配置和业务SLO。固定测试证书、虚拟机、QEMU、回环、端口重绑定分别注明，不能冒充真实移动端。

移动/游戏部署建议以下默认次数，执行前按目标冻结：

| 操作 | 默认次数 | 判断要点 |
| --- | ---: | --- |
| Wi-Fi↔蜂窝切换 | 每方向20次 | 有效同连接路径由xrudp证明后继续；换承载时按认证续接，旧输入不得串入新绑定 |
| NAT地址/端口重绑定及回切 | 各20次 | 未验证路径不执行，验证后可靠消息按期限继续；不能假定所有NAT类型都支持打洞 |
| 断网/飞行模式 | 10次，分别覆盖短中断及超过操作期限 | 有效预算内恢复；超期允许TIMEOUT/UNKNOWN但只完成一次，不延长原期限 |
| 锁屏/后台/系统休眠 | 10次，含≥5分钟停顿 | 回来后时钟、期限、旧事件隔离正确；系统杀进程不伪称保留状态 |
| 服务进程/XTP状态丢失 | 至少3次 | 不能重用“状态仍保留”的证明，不静默重执行业务；应用事务另测 |
| 新身份、过期凭据、无效证书/恢复证明 | 每类至少3次 | 不进入业务处理；旧已发送未决结果UNKNOWN，不给虚假成功 |
| 正常24小时部署运行 | 按实际目标计划 | 业务SLO、资源和关闭/卸载满足部署要求，不以库级回环代替 |

短故障要求零丢失时，必须明确操作期限/寿命/最大尝试均足够且双方状态存活；超出契约不能硬性要求“所有请求最终成功”。NOTIFY可丢，快照必须去旧，不能因可靠资源流繁忙而补播已过期快照。任何产生业务效果的操作应通过应用唯一ID/审计日志对账，不只靠客户端计数。

无设备、无端点或没有授权切换网络时，记录pending和缺口；不要关闭用户日常设备网络或改全局系统策略来凑证据。

## 9. XS、最终产物与来源（G7）

在 `D:/GIT/xserver` 单独保存本轮证据；不直接修改 `lib/xtp` vendored源码：

```powershell
Set-Location D:/GIT/xserver
$env:PATH = 'E:/software/w64devkit/bin;' + $env:PATH
$xsrun = '.build/xtp3-acceptance/' + (Get-Date -Format 'yyyyMMdd-HHmmss')
New-Item -ItemType Directory -Path $xsrun | Out-Null
python tools/sync_xrt.py --check *> "$xsrun/snapshot.log"
python tools/build.py xtp --build-dir "$xsrun/build" --output "$xsrun/xs-xtp3.exe" *> "$xsrun/build.log"
python -X utf8 tools/test_extensions_runtime.py --exe "$xsrun/xs-xtp3.exe" xtp *> "$xsrun/runtime.log"
python -X utf8 tools/run_xtp3_example.py --exe "$xsrun/xs-xtp3.exe" *> "$xsrun/example.log"
python -X utf8 tools/test_extensions.py *> "$xsrun/registry.log"
python -X utf8 tools/test_xrt_sync.py *> "$xsrun/snapshot-tests.log"
python -X utf8 tools/test_sync_xrt.py *> "$xsrun/selective-sync-tests.log"
```

逐步检查退出码，构建失败不运行旧文件。Linux用独立目录和本机可执行文件重复宿主、runtime及示例；在XS仓库运行，不与Windows同输出目录并发。

应观察外层和嵌套TCC：四种形态、table ParamAt、三模型、聚合Frames、跨帧短写、配置/统计字段，以及回调返回结构和32字节Request/Output token按值传递。当前历史闭包为42个XTP导出、三个扩展共329导出；合法API变更后应按实际声明重新核对并更新收据，不能靠固定数字掩盖漏导入。XTP1/2符号/头不应重新出现。

如果生产源码确实需要更新，使用 `tools/sync_xrt.py --source ../xrt --ref <明确提交> --library xtp` 选择性同步，再构建/验证。确认核心、xws、xrudp等未被意外一起升级；否则按新依赖重新验收，不复用旧ABI通过结论。

脚本卸载前停止输入、加入异步工作、Close/Drive至CLOSED再Destroy。当前没有自动热重载所有权适配器；若部署要热重载，必须设计宿主停机/清理策略并单独验收。

## 10. 失败、中断及重测规则

1. 首先保存完整命令、退出码、源码/工具/二进制哈希、日志、语料/包序列、最后检查点及系统环境，不覆盖原目录。
2. 分类为产品缺陷、测试工具缺陷、环境/不可比、人工中断或缺少设备。分类须有证据，不凭猜测。
3. 原样例复现，优先最小化触发条件；不能先改超时、内存、输入范围、负载或阈值来消除失败。
4. 修复后先复现原失败并验证预期，再重跑受影响组合。未受影响项能否沿用，依据源码/依赖/flags及工作负载差异说明。
5. 长稳中断后新建目录重新连续24小时；不能合并中断段、改脚本虚构时间、手工填summary或把running改passed。
6. 若工具本身缺陷导致误报，保留误报和修复证据，再完整运行；不是简单擦掉失败记录。
7. 每个验证后的阶段按明确路径单独提交，保留其他线程文件与暂存内容。没有发布授权，不推送远端，不合并发布或替换生产服务。

## 11. 汇报规范与模板

### 11.1 阶段/长稳进展

每次至少写：北京时间、关卡、来源、当前状态、计划/实际时长、已完成操作、错误/拒绝/超时、内容/账本检查、RSS/自有内存、退出情况、证据路径、下一步及阻塞。不要只写“全绿”“运行正常”或只给瞬时QPS。

```text
时间：YYYY-MM-DD HH:mm:ss Asia/Shanghai
范围/来源：XRT <SHA>；XTP生产源 <SHA/差异>；XS <SHA>；exe <SHA256>
阶段：G5/L2，running，已真实运行 HH:MM:SS / 24:00:00
进度：完成 <N>；检查点 <N>；ledger_match/quiescent=<值>
错误：断言/诊断 <N>；拒绝/超时 <值或“此字段不由该工具直接提供”>
内存：RSS当前/峰值 <B>；静止XTP实际分配 <B>；预热峰值/余量 <B>
收尾：尚未退出，因此尚未验证最终全部释放，不能计为passed
证据：<status.json>、<child.log>、<events.jsonl>、<build.log>
阻塞/下一步：<设备/端点/预计后续动作>；不承诺早于真实24小时完成
```

### 11.2 最终人工报告

```text
结论：通过 / 未通过 / 未完成（限定具体平台与部署范围）
用户确认的标准：连续24小时；其他范围/排除项及确认依据
来源：XRT、XTP、XS、xws/xrudp及核心固定版本；配置/源码/exe哈希
环境：OS/CPU/编译器/后端/flags/亲和性/电源策略/虚拟化/设备/网络
G0-G7逐项表：状态、实际命令、退出码、关键指标、证据路径
长稳：每场景开始/结束、真实时长、完成数、预算、最终回收；人工中断另列
性能：全部重复、P50/P95/P99/P99.9/max、吞吐、拒绝/超时、质量与回归门限
恢复：有效和无效恢复、业务调用对账、UNKNOWN/STATE_LOST、无重复证明
安全：sanitizer、fuzz时长/运行数/语料、OOM/边界、认证与状态证明
限制：未执行项、不可比项、计数有效性、设备/网络缺口，不能证明的承诺
变更：新增/修复、相应复验和本地提交；是否有发布授权
复现：一份从构建到报告的完整命令列表；证据目录与SHA256清单
```

### 11.3 建议机器可读汇总

以下是**模板，不是已完成结果**。每个passed必须有对应真实证据；未完成必需项时overall不得为passed：

```json
{
  "schema": 1,
  "run_id": "<unique>",
  "timezone": "Asia/Shanghai",
  "overall": "pending",
  "required_continuous_seconds": 86400,
  "scope": {"platforms": [], "transports": [], "allocators": [], "devices": [], "exclusions": []},
  "source": {"xrt_head": "<sha>", "xtp_production": "<sha>", "xs_head": "<sha>", "lock_sha256": "<hash>"},
  "gates": {"G0": "pending", "G1": "pending", "G2": "pending", "G3": "pending", "G4": "pending", "G5": "pending", "G6": "pending", "G7": "pending"},
  "runs": [],
  "failures": [],
  "interruptions": [],
  "limitations": [],
  "evidence_sha256": {}
}
```

证据清单至少包括：源码差异和版本记录、构建日志、命令/退出码、配置、机器/网络说明、exe和库哈希、原始报告、所有重复样本、长稳JSON/日志/JSONL、语料与失败样例、设备/业务对账记录。大文件可以存受控归档，仓库提交索引、摘要和哈希；明确可访问的位置，不能只留临时文件名。

## 12. 本轮交接的限制

- 用户已要求本线程跳过测试；本次只整理文档与保存现场，不把文档检查当验收重跑。
- 两项原72小时运行已停止；现有原始failed/非零退出配合人工停止记录解读。没有新的24小时通过结果。
- 工具中系统分配器诊断、默认system模糊模式已做过短预检；正式长时运行仍需按本手册执行。
- 内核唤醒诊断未完成有效正对照，全链路复制指令未完整归因；不得把零计数或部分计数当全链路证明。
- 生产服务、真实设备、业务认证、幂等及部署SLO需由接手线程确认；没有这些资料，不签发无范围限制的“生产可用”。
