# 生产验收

验收冻结 XTP 自有生产源码为 `cf24b015`（与后续测试改动之间无 include/src 变化）。XS 固定快照及 Windows/Linux 宿主 ABI 证据见 XS 的 `docs/xtp3-validation-2026-11-17.json`。验收工具和测试另按实际 Git 提交、源码及可执行文件哈希记录。

整体生产资格只有所有必需项完成后才通过。尚在运行的任务保持 running，缺少环境保持 pending；进程退出、伪造时长、仅生成测试脚本均不能算通过。

| 项目 | 条件 | 当前证据 |
| --- | --- | --- |
| 协议与宿主 | 核心安全/配额/OOM/生命周期、C99/模块/单头、x86/大端、宿主与嵌套 TCC | 既有功能验收通过 |
| 可重复性能 | 四种形态、每种100万RPC、7次重复、固定CPU；MAD≤20%、中心范围≤30%，拒绝/超时零 | Windows 通过，P99中心范围最大约1.66% |
| 路径故障 | Noise认证、在途RPC/PUSH、源端口重绑定/回切、500ms黑洞、未认证路径丢弃、无重复执行 | 真实UDP实验室测试通过；不代表运营商真网 |
| 实际连接恢复 | TLS1.3及测试前端认证，部分响应断线后续接；认证、身份或状态证明失败不得重执行业务 | Windows/Linux模块与单头通过；有效续接20轮，失败返回UNKNOWN |
| 持续连接 | 连续72小时真实时钟、同批连接/Session、多worker；每轮静止、分配账本一致，退出全部释放 | 待运行完成，不提前签发通过 |
| 目标部署环境 | 明确的目标设备、部署端点及网络；移动场景需真实Wi-Fi/蜂窝/NAT/休眠唤醒 | 当前ADB未连接设备，端点/设备信息待提供 |

## 性能门限

```text
python extlibs/xtp/bench/measure_qualification.py --compiler gcc --cpu 4 --report out/xtp3-production/p99-windows.json
```

使用 `config/qualification_profiles.json`，只固定当前测试进程及子进程的CPU亲和性，不修改系统电源计划，结束时恢复原亲和性。若CPU不在可用集合中，选择首个允许的CPU，报告记录实际值。7次重复、每次100万目标，保留首次分配和全部操作；既有20%/30%质量门限保持不变。旧的未固定亲和性、2万目标的失败收据保留，不与新实验混为相同条件。

## 持续连接与分配核对

先构建专用探针（PATH中提供GCC，Linux sanitizer提供Clang）。构建包含Delivery代码路径，长稳流量默认是普通RPC，恢复故障由专门测试覆盖。启用sanitizer时同时使用 `-fno-sanitize-recover=all`。这个专用探针的测试分配器有记录头和每轮静止检查，因此吞吐包含诊断开销，不替代普通容量探针。

```text
python extlibs/xtp/bench/build_production.py --compiler gcc --output out/xtp3-production/production-full.exe
python extlibs/xtp/bench/build_production.py --compiler clang --sanitizers --output out/xtp3-production/production-full-asan
python extlibs/xtp/bench/production_soak.py --exe out/xtp3-production/production-full.exe --seconds 259200 --connections 8 --workers 2 --mode 1 --count 2000 --report out/xtp3-production/full-tls-72h.json
python extlibs/xtp/bench/production_soak.py --exe out/xtp3-production/production-full-asan --seconds 259200 --connections 8 --workers 2 --mode 3 --count 2000 --report out/xtp3-production/full-wss-72h-asan.json
```

mode0/1/2/3分别为TCP/TLS1.3/WS/WSS。报告路径必须是新的，避免覆盖历史证据。最长7天，默认72小时。每轮在所属worker检查 pending、incoming、tx 和 retained 均清空，实际自有分配字节与Session账本一致；分配计数和总完成数用64位，持续运行不因32位计数回绕而假通过。退出在原生退休后Close/Destroy，并断言自有分配块及字节归零。

子进程每秒发布检查点；父进程每两秒采样OS内存、追加JSONL事件并原子更新状态。默认RSS硬上限3GiB，300秒预热后的静止自有内存增长余量为每peer128KiB；记录150秒无完成进度与总时长加180秒退出宽限。阈值在运行前写入报告，不随结果放宽。OS内存包含网络/运行时/sanitizer，和XTP账本不同。

通过须真实运行满要求时长、子进程成功、进度单调、账本/内存条件满足且最终释放通过。sanitizer即使恢复后返回0也判失败。中断、断电或监督器崩溃留下的running状态不能当成通过。子进程日志、事件日志、可执行文件、源码和运行参数均记录哈希。

记录器的反例测试：

```text
python extlibs/xtp/tests/test_production_soak.py
```

覆盖假冒三天时长、非零退出、恢复型sanitizer报错、重复进度和RSS超限；它们必须留下失败证据。

## 网络切换的证据边界

`test_xrudp_path.c` 使用实际UDP socket及Noise握手；保留同一XTP/RUDP实例，在可靠消息已发出、对端开始处理后更换客户端源端口，并切回旧映射。随后注入500ms双向黑洞和未认证第三方报文。检查路径证明、消息完成、业务调用次数和密钥轮换，搭配原有丢包/RTT/MTU矩阵。

这证明实验室路径与NAT映射语义，不能代替实体设备、运营商路由、系统休眠或真正公网端点。当前已找到ADB，但 `adb devices -l` 没有设备；外部设备/端点未提供前，相应项保持pending。库的连接与认证控制面仍由宿主管理，不因验收而加入新的XTP线路命令或路由层。

`test_delivery_native.c` 通过真实TLS1.3、固定测试证书pin和新鲜随机凭据的测试前端认证后才Bind。将RESPONSE只发送7字节后断开物理连接，隔离旧事件、Unbind、真实退休旧TLS，再以原Session和经验证状态续接，检查旧Ticket失效与业务不重复。另验证错误凭据、实际销毁服务端XTP状态及错误身份声明均不重新执行。该测试前端不是生产登录服务，也没有新增XTP命令。

```text
python tools/build.py --manifest extlibs/xtp/config/modules.json --suite xtp_delivery_native_tests --compiler gcc --cflag=-O2
python extlibs/xtp/bench/fuzz_qualification.py --compiler clang --seconds 600 --report out/xtp3-production/fuzz-600.json
```

模糊测试依次对Codec/Reader/Session/Delivery运行，使用独立有效/非法线路种子、固定随机种子及ASan/UBSan。实际运行满每项时长、退出0、无sanitizer诊断才通过；保留语料、失败输入、编译器、源码/单头及二进制哈希。有限时间模糊测试不能证明所有输入安全。
