# 时间、路径与文件使用指南

时间、路径和文件是多数上层模块都会经过的系统边界。各入口的完整契约见 [Time API](../api/time.md)、[Path API](../api/path.md) 和 [File API](../api/file.md)；本指南说明三者如何组合。

## 区分时钟

- `xrtNow` 返回 Unix 微秒绝对时间，适合记录事件、生成时间文本和协议时间。
- `xrtClock` 返回单调微秒计数，适合耗时、期限和超时预算，不受系统日历时间校准影响。
- `xrtTimer` 是浮点秒便利入口，适合展示和简单测量；精确预算优先使用整数 `xrtClock`。
- `xrtSleep` 和 `xrtSleepUs` 阻塞当前线程，不是异步等待。任务和协程应使用自身的可取消等待入口。

不要用两次 `xrtNow` 计算可靠耗时。管理员校时、NTP 校准或虚拟机时间变化都可能让日历时间跳变。

## 选择路径根

`xrtPathAppDir` 返回可执行文件所在目录，适合定位随程序发布的只读资源，但该目录安装后可能不可写。`xrtPathCwd` 是进程级可变状态，库代码不应假设它稳定。

临时工件使用 `xrtFileTemp` 或 `xrtDirTemp` 排他创建；用户文件从调用方配置、`xrtPathHome` 或平台约定的应用数据位置开始。XRT 不替业务猜测持久数据目录。

两个片段用 `xrtPathJoin`，多个视图片段用 `xrtPathBuild`。解析、清理和拼接只处理路径语法，不证明目标存在，也不构成文件系统沙箱。处理归档条目或 URL 映射时先用 `xrtPathIsSafeEntry`，再使用目录句柄相对文件 API 抵抗并发替换和链接跳转。

## 区分二进制与文本

二进制内容使用 `xrtFileReadAllLimit`、`xrtFileWriteAll` 或 `xrtFileWriteAtomic`，并始终按显式长度处理。文本 API 的内存表示固定为 UTF-8；`xrtFileReadTextLimit` 可以在转码前限制源文件字节数。

读取 `XENCODING_UNKNOWN` 时只根据 BOM 和严格检测选择 UTF-8、UTF-16 或 UTF-32，不会猜测本地代码页。自己生成的文件应明确选择编码；新文本通常使用不带 BOM 的 UTF-8。

普通整文件写入失败时可能留下部分目标。配置、状态快照和其他不能暴露半份内容的文件应使用 `xrtFileWriteAtomic` 或 `xrtFileWriteTextAtomic`。原子替换保证普通并发读者看到旧版本或完整新版本，不等于断电后目录项必然持久化。

## 常见流程

1. 选择与用途匹配的根目录，不默认写入可执行文件目录。
2. 使用路径 API 构建目标；需要父目录时显式创建。
3. 对外部文件设置大小上限，再决定二进制或文本入口。
4. 用 `xrtNow` 记录事件时间，用 `xrtClock` 测量流程耗时。
5. 重要整文件结果使用原子写入，并根据业务决定是否额外同步目录元数据。
6. 释放拥有型路径、文本和字节缓冲，关闭句柄后再清理临时对象。

同步整文件 helper 适合配置、CLI、启动加载和小型工件。文件可能很大、主线程不能阻塞或需要取消时，应进入 [异步文件 API](../api/file_async.md)，同时保留相同的路径、大小上限、所有权和错误规则。

## 示例

[运行报告示例](../../examples/file/report/main.c) 在排他临时目录中构建带 UTC 时间的文件名，以原子方式写入 UTF-8 报告，限长读回验证，并用单调时钟统计整个流程。独立的 [时间示例](../../examples/time/basic/main.c)、[路径示例](../../examples/path/basic/main.c)、[整文件示例](../../examples/file/whole/main.c) 和 [文本示例](../../examples/file/text/main.c) 展示各层更多入口。
