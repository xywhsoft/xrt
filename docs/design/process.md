# Process 设计

## 目标

Process 是 XRT 调用外部程序、编排本地工具和承载宿主系统命令的统一底座。设计同时满足以下约束：

- 直接执行默认不经过 Shell，参数不需要调用方拼接或转义。
- 基础进程对象不自动累计 stdout/stderr，不为每个对象预留固定缓冲。
- 同步、Future 和协程使用同一个退出状态与等待语义。
- 一次性执行提供安全的有界捕获，流式场景可以直接消费真实管道。
- Windows 与 POSIX 的进程组、环境、UTF-8、句柄继承和启动失败都具有明确契约。
- 功能按真实依赖裁剪，不以虚假的函数表解耦平台、线程、文件或 Future。



## 分层

| 模块 | 职责 | 关键依赖 |
|---|---|---|
| `process` | 配置、启动、真实标准流、等待、退出状态、停止 | Thread、Mutex、Cond、Unicode |
| `process_open` | 使用系统默认关联程序打开文件路径或 URI | Process；Windows 额外链接 Shell32 |
| `process_run` | 并发排空、有界捕获、输入、Deadline、Cancel、常用 Helper | Process、Buffer、Cancel |
| `process_pipeline` | 并发启动并用真实 OS pipe 连接多个阶段 | Process Run |
| `process_future` | 把进程退出映射为 Future，供协程直接 Await | Process、Future |
| `process_terminal` | Windows ConPTY 与 POSIX PTY | Process |
| `process_file` | 把 XRT 文件安全映射为标准流 HANDLE 配置 | Process、File |

基础层公开真实管道能力，高层只组合它们，不维护另一套进程实现。



## 命令模型

`XPROCESS_EXEC` 是默认模式。`Program` 指定程序，`Args` 只包含 `argv[1...]`，`Arg0` 允许少数工具覆盖 `argv[0]`。Windows 实现必须按照 Microsoft CRT 规则构造命令行，POSIX 直接传递 `argv`。

`XPROCESS_SHELL` 只接受 `Command`，由平台选择 `cmd.exe /D /S /C` 或 `/bin/sh -c`。自定义 PowerShell、Python 或其他解释器应按普通 EXEC 配置程序和参数，避免 Process 猜测解释器方言。

默认关联程序属于 `process_open` 辅助层，不是 Shell 模式。Windows 把目标交给系统 Shell 关联处理器；macOS 直接执行 `/usr/bin/open`；其他 POSIX 桌面直接执行 `xdg-open`。POSIX 目标始终作为独立参数传入，不能通过字符串拼接重新引入命令解释器。打开请求通常不能提供稳定进程句柄，因此公共契约只返回系统是否接受请求，不伪造等待或退出状态。



## 标准流

标准流配置具有五种模式：

- `INHERIT`：复制父进程对应标准流。
- `PIPE`：创建父子管道，父进程通过 Read/Write/Close 使用。
- `NULL`：连接平台空设备。
- `HANDLE`：Spawn 复制调用方借用的原生句柄，不接管其所有权。
- `MERGE`：只允许 stderr 使用，令 stderr 与最终 stdout 指向同一目标。

Process 对象不保存输出快照。直接读取会消费管道，单个流同一时刻只允许一个读取者；stdin 同一时刻只允许一个写入者。需要自动并发排空时使用 Process Run。

Windows 创建进程时使用 `PROC_THREAD_ATTRIBUTE_HANDLE_LIST` 限定可继承句柄，不能因为某个标准流需要继承就泄露进程中的其他可继承句柄。POSIX 创建的内部 fd 全部设置 close-on-exec，并提升到 `0/1/2` 之外；HANDLE 模式在 Spawn 内复制借用 fd，避免调用方标准流已关闭、重定向源互换或 Spawn 返回后关闭原 fd 时产生别名和生命周期错误。

Terminal 是标准流的替代传输层，不是第六种单流重定向模式。ConPTY/PTY 把 stdin、stdout、stderr 连接到同一终端会话，父端只暴露输入和合并输出。这样既保留交互程序需要的 TTY 行为，也避免把不可实现的“独立终端 stderr”伪装成普通管道。普通 PIPE/HANDLE 配置在 Terminal 模式下不参与启动。



## 生命周期

Spawn 成功后对象立即处于 RUNNING。内部等待引用负责回收平台进程，因此调用方可以在运行时释放最后一个外部引用：父端标准流会关闭，子进程继续运行并最终被回收，不会产生 POSIX zombie，也不会被隐式强杀。

多个线程可以同时等待和读取状态。退出状态只发布一次，随后保持不可变。后台等待失败作为 `xrt.process` 结构化错误保存在对象中，等待者会在自己的错误上下文看到同一原因链。

进程组默认开启，因为超时、取消和服务器退出时通常需要完整收容后代进程。`KillTree` 只以 XRT 创建的组或 Windows Job 为边界，不扫描系统进程表。



## 捕获与内存

Run 为 stdout 和 stderr 分别创建读取任务，避免一个管道填满后阻塞另一个管道或子进程。默认每个流最多保留 16 MiB，且只按实际内容动态分配。

达到上限时有三种显式策略：

- `ERROR`：停止执行并返回限制错误。
- `KEEP_FIRST`：继续排空但只保留前部。
- `KEEP_LAST`：使用滑动窗口保留尾部。

输出回调与捕获相互独立。即使保留窗口已满，回调仍能看到后续块；回调返回 false 会触发统一停止。stdout 与 stderr 回调可能在两个读取线程并发发生，调用方必须自行串行化共享状态。



## Deadline、取消与停止

Run 只接受绝对单调 Deadline，避免多层相对超时重复扣减。Cancel 和 Deadline 都是正常控制流：结果的 `Wait` 分别为 `XWAIT_CANCELLED` 或 `XWAIT_TIMEOUT`。

收口顺序固定为：

1. `Interrupt`
2. 等待 `StopGrace`
3. `Terminate`
4. 再等待 `StopGrace`
5. `KillTree`，失败时退回 `Kill`
6. 等待进程和全部输出读取结束

停止原因写入最终 Process Status，不把“发出停止请求”伪装成进程已经退出。



## Pipeline

Pipeline 必须先创建 N - 1 条 OS pipe，再启动全部阶段。中间 stdout 直接连接下一段 stdin，不经过父进程完整缓存，因此具备并发、流式和背压语义。旧版“执行一段、完整捕获、再把字节写给下一段”的串行实现不属于 Pipeline。

启动任一阶段失败时，已经启动的阶段全部按进程树收口。等待使用一个共享 Deadline。结果保存每一段退出状态、末段 stdout 和带阶段归属的 stderr；不为中间 stdout 产生无意义副本。



## 平台策略

Windows 使用宽字符 API、精确句柄列表、挂起启动后绑定 Job，再恢复主线程。环境块按 Windows 不区分大小写规则覆盖并排序，所有公共输入保持 UTF-8。ConPTY 入口按运行时解析，以便旧 SDK 与 TinyCC 仍可编译；进程退出时关闭伪控制台，使合并输出可靠进入 EOF。

非 Terminal POSIX 路径避免在 fork 子进程中调用内存分配与环境修改函数。Terminal 路径在父进程预构建参数和环境，子进程只建立 session 与 controlling terminal、重定向 fd、切换目录并 exec。Linux PTY 在从端关闭后可能以 `EIO` 表示终端 EOF，公共 Read 将其归一化为零。



## 旧版资产取舍

保留并迁移：

- Windows CRT 参数转义边界。
- UTF-8 工作目录、环境和命令。
- 使用系统默认程序打开文件和 URI，但改为无歧义的 `xrtProcessOpen()`。
- 启动失败阶段与系统错误。
- stdin EOF、stdout/stderr 并发排空。
- 进程组超时收容、PTY/ConPTY 和 Future 生命周期测试。

不原样迁移：

- 每个 PIPE 自动建立永久捕获缓冲。
- 每个输出流同时缓存、回调和事件复制的多重数据路径。
- 受 Network 裁剪宏控制的 Process Future。
- 串行完整缓存模拟的 Pipeline。
- 在多线程 POSIX 程序 fork 后调用 `clearenv`、`putenv` 的路径。

旧版 `GetStdout/GetStderr`、`ReadSince`、快照序号和输出事件环被明确退役。它们会让每个 Process 隐式持有输出、重复复制同一字节，并把持续流消费变成轮询；底层替代路径是直接 `xrtProcessRead()`，一次性替代路径是有界 `xrtProcessRun()`，持续观察使用 Run 输出回调或由调用方在线程、任务中消费原始管道。

旧版 `ResultCopy` 不再保留。当前 Result 的输出所有权单一且可见，需要跨生命周期保存时由调用方使用 Buffer 或字节复制能力显式决定代价。旧版 Future 返回 Process 指针并混合对象所有权，现改为共享 Future 持有不可变 `xprocessstatus` 快照；文件异步转存案例则由 `xrtProcessFile()` 直接重定向，避免先完整捕获再复制到文件。

旧版文档中 `GetStdout/GetStderr` 的“借用指针”描述与实际分配快照行为不一致，部分示例还引用了已经漂移的字段。2.0 文档只描述当前头文件的真实所有权和裁剪契约，不延续这些文档错误。



## 发布门槛

- 参数：空参数、空字符串、空格、引号、反斜杠、UTF-8、超长参数。
- 默认程序：空目标、非法 UTF-8、缺少关联程序或桌面启动器、OOM、Windows Shell32 链接与单头裁剪。
- 环境：继承、覆盖、删除、空值、重复名、UTF-8、空环境。
- 标准流：继承、NULL、PIPE、HANDLE、stderr 合并、提前关闭、部分写入、双流大输出。
- 生命周期：多等待者、提前释放、非零退出、信号退出、后台等待失败、进程树。
- Run：零输入、大输入、16 MiB 边界、三种溢出策略、回调失败、Deadline、Cancel。
- Pipeline：真实并发、背压、大于管道容量的数据、阶段启动失败、阶段非零退出、共享超时。
- Terminal：TTY 检测、尺寸修改、交互式 Shell、平台不支持路径。
- 资源失败：Process、Run、Pipeline、Future 在每个分配点注入 OOM，错误通道和结果析构保持可用。
- 工具链：Windows GCC x64、TinyCC x86、单头和最小裁剪；Linux GCC/Clang 验证普通管道、关闭 `0/1/2`、信号与 PTY；macOS Clang 验证 kqueue 无关的 Process、信号与 PTY。
- 稳定性：Windows 与 POSIX 分别进行长期重复执行、并发双流排空、提前释放和进程树收口；所有平台在退出后检查线程、句柄、fd 与内存归零。
