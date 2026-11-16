# 功能验收与部署资格

XTP3 的线路、Codec/Reader、三种消息模型、四种 Command/Params 组合、参数按位访问、Session、可靠恢复、自动聚合和承载桥接已经实现。它不创建连接、线程、路由或持久队列；认证、网络事件调度及断线状态证明由宿主提供。这些边界是接口设计，不是遗漏的内部实现。

## 可重建检查

从仓库根目录运行：

```text
python tools/build.py --manifest extlibs/xtp/config/modules.json --suite xtp_tests,xtp_delivery --compiler gcc --cflag=-O2
python tools/build.py --manifest extlibs/xtp/config/modules.json --suite xtp_native_tests,xtp_wss_tests,xtp_xrudp_tests --compiler gcc --cflag=-O2
python tools/build.py --manifest extlibs/xtp/config/modules.json --suite xtp_tests,xtp_delivery --compiler clang --cflag=-std=c99 --cflag=-O1 --cflag=-fsanitize=address,undefined --ldflag=-fsanitize=address,undefined
```

Windows x86 使用 `i686-w64-mingw32-gcc` 和 `--cflag=-D_WIN32_WINNT=0x0601`。本次模块化在 `-O2` 通过；模块/单头全套在 `-O0` 通过。该工具链的 `-O2` 单头测试触发 GCC maybe-uninitialized 告警，`-O1` 还触发测试 key 格式化的 format-truncation 告警；原日志保留，没有关闭这些诊断或声称该组合全绿。生产源码和 Windows x64/PowerPC64 的 `-O2` 验证另列。

Linux PowerPC64 大端使用真实交叉编译及 QEMU 执行，包含独立黄金字节、状态机、恢复和单头测试：

```sh
QEMU_LD_PREFIX=/usr/powerpc64-linux-gnu python3 tools/build.py --manifest extlibs/xtp/config/modules.json --suite xtp_tests,xtp_delivery --compiler powerpc64-linux-gnu-gcc --cflag=-O2 --runner qemu-ppc64
```

宿主有真实 PowerPC64/QEMU 用户态依赖时才运行这个命令。模拟执行证明字节序与对应 ABI 下的行为，不代表该 CPU 上的原生吞吐。

## 容量与持续连接

```text
python extlibs/xtp/bench/measure_capacity.py --compiler gcc --count 500 --repeats 3 --report out/xtp3-bench/capacity-windows.json
python extlibs/xtp/bench/soak.py --exe out/xtp3-bench/multi.exe --seconds 300 --connections 8 --workers 4 --mode 1 --count 2000 --report out/xtp3-bench/soak-windows.json
```

capacity 工具构建 xrudp 和 multi 探针。xrudp 使用真实 UDP、Noise IK、测试静态公钥 pin 和地址 cookie，覆盖 RPC/PUSH/NOTIFY、可用 Body 大小、在途窗口和注入丢失/RTT；NOTIFY 完成只代表本地下层受理，接收数另列。实际游戏的 LATEST 快照、资源独立流、乱序/重复和 MTU 下降仍由游戏故障测试验证。

multi 使用同一个网络引擎，每条连接配一个临时端口的 listener，两端固定在同一个 worker；独立连接跨 worker 并行。所有连接完成初始化后统一开始，包含 1/8/32 连接与 1/2/4 worker、TCP/TLS/WS。每个连接都保留完成计数及尾延迟，不能把多个进程或单连接单 worker 的成绩当成跨核扩展。

soak 的位置参数由工具管理；同一批连接和 Session 在多轮间保持，重复使用统计数组，持续时间可配置至 24 小时。JSON 保存可执行文件哈希、退出状态、所有最终输出、累计完成数、XTP 内存峰值以及每两秒 OS 进程内存。OS 内存包含测量数组、网络依赖、运行时和可能的 sanitizer，不把它等同于 XTP 自有内存。多分钟运行检查持续压力和退出回收，不能冒充多日长稳。

## 需要部署环境的验收

本地已完成的功能和容量验证不包含实体 Android/iOS、运营商移动网络、NAT 切换、休眠唤醒及多日长稳；当前环境未发现可用 adb/已连接移动设备。这些需要设备、真实网络和部署参数。内核唤醒与每一层实际复制的动态归因也没有由 writer 次数代替；当前只明确计入 XTP 聚合复制、宿主暂存、原生字节/事件及分配计数。

部署前应冻结 XRT/宿主来源、身份验证与恢复证明交换方式、配额和超时，使用上述工具保留对应长稳收据，并在真实客户端执行 Wi-Fi/蜂窝切换、断网重连、进程终止、服务端状态丢失与 MTU 下降。缺失状态应得到 UNKNOWN/STATE_LOST，可靠命令不得在身份或状态证明失败后静默重跑。此清单不引入新的 XTP API 或线路协议。
