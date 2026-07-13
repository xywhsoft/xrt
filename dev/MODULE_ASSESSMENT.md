# XRT 模块评估报告

状态：Active
语言：中文
最后更新：2026-07-07

## 0. 评估方法与口径

### 0.1 评估对象

按耦合度将 xrt 的 ~69 个 `lib/*.h` 模块划分为 8 个批次，逐批产出诊断报告：

| 批次 | 模块 |
|---|---|
| **A. 基础运行时**（本轮） | base / os / string / charset / time / hash / math / xid / type |
| B. 文件与路径 | file / file_async / path |
| C. 内存体系 | bsmm / memunit / mempool_fs / mempool / memglobal / memdebug_* |
| D. 数据结构 | buffer / array / array_point / stack(_dyn) / list / queue / dict / set / avltree / typed_container / channel |
| E. 并发与异步 | thread / coroutine（+ xrt.h 中的 future/task/wait-source） |
| F. 网络传输 | network / xnet_* / nettls / xnet_proxy |
| G. 应用协议 | xhttp / xhttpd / xws / xweb / xhttp_util / xurl / xcodec_* |
| H. 结构化数据与文本 | value / json / jnum / xson / template / regex / crypto / logger |

### 0.2 每个模块的评估维度

- **现状诊断**：实现质量、性能瓶颈、潜在 bug、代码异味
- **缺失能力**：对比 ROADMAP 与 xlang `std.*` 缺口
- **优化建议**：具体可落地的改造点，标注优先级（P0/P1/P2）
- **xlang 对齐点**：与 `std.core/str/time/hash/math/encode/types` 等的映射
- **风险与依赖**：是否会牵动已冻结模块或跨模块改造
- **P0 主干接入点**：context / cancel / deadline、统一错误模型、统一生命周期在本模块的预留位置

### 0.3 优先级标注口径

- **P0**：影响 ROADMAP 三大主干，或本模块自身的关键性能/正确性缺陷，应优先处理
- **P1**：明显影响易用性或与 xlang 标准库对齐的缺口
- **P2**：清理性、增强性、可推迟

### 0.4 冻结边界提醒

参考 `XRT_API_FREEZE_PLAN.md`：
- runtime attach / future-task / 结构化并发为 **A 类（建议冻结）**，本批次涉及它们的优化只能"内部改、表面不动"
- 网络应用层 / 内存 shared 表面为 **C 类（暂不冻结）**，可更激进

---

## 1. 批次 A：基础运行时总览

### 1.1 整体观察

批次 A 是整个 xrt 的"地基"。它们的特点是：

- **代码量集中**：`string.h` 2125 行、`time.h` 1650 行、`type.h` 1572 行、`charset.h` 948 行
- **API 表面相对稳定**：除了 `type.h` 还在快速演进，其他模块函数名已多年未大改
- **性能与正确性问题并存**：`time.h` 的解码算法是 O(years)，`string.h` 的 case-insensitive 搜索每次都分配两份副本
- **P0 主干缺位**：错误模型仍是"单字符串线程槽"，没有 context/cancel/deadline 接入点

### 1.2 批次 A 关键发现（摘要）

| 模块 | 最关键问题 | 优先级 |
|---|---|---|
| base | 错误模型只有"单字符串"，无错误码/级别/来源/上下文 | P0 |
| os | 仅 3 个函数，无 subprocess 编排能力 | P1 |
| string | case-insensitive 搜索每次分配两份副本；`xrtStrLastIndexOf` 是 O(n·m) | P0 |
| charset | 缺 UTF-8 合法性校验（overlong/代理区码点），有安全风险 | P0 |
| time | `xrtDecodeSerial` 用线性循环 O(400) 找年份，应用闭合公式 O(1) | P0 |
| hash | 缺流式（init/update/final）API；无 CRC32 | P1 |
| math | `log2/exp2/cbrt/hypot` 都手写实现，未利用 CRT 内建 | P1 |
| xid | 自定义格式，不兼容 UUID v7/ULID；`xrtDecodeXID` 无错误返回 | P2 |
| type | `xrt_callable` 引用计数非原子；缺 move 语义；无类型注册中心 | P1 |

### 1.3 P0 主干接入点（批次 A 全局）

ROADMAP 三大主干在批次 A 的具体落点：

**统一执行上下文（context/cancel/deadline）**
- 暂不直接落在批次 A，但 `time.h` 的 `xrtTimer/xrtSleep/xrtNow` 是 deadline 计算的源头。建议预留 `xrtDeadlineCreate(timeout)` / `xrtDeadlineExpired(dl)` 的内部 helper（不暴露给业务），后续供网络、协程、subprocess 共用。

**统一错误模型**
- 落点在 `base.h`。当前 `xrtSetError(const void* sError, bool bFree)` 仅有"消息字符串 + 是否需要释放"二维信息。
- 建议方向（仅标注，不在本轮重构）：
  - 保留 `xrtSetError` 作为快速路径（兼容现有调用）
  - 在 `xrtThreadData` 增加 `xrtErrorContext` 槽：`{int code; const char* module; const char* file; int line; str message; void* attach;}`
  - 新增 `xrtSetErrorEx(code, module, ...)` 写结构化错误，`xrtGetErrorEx()` 读
  - 错误码采用 `(module_id << 16) | reason_code` 的分层方案，模块号集中注册

**统一资源生命周期**
- 落点在 `type.h` 的 `xrt_type_ops`。当前 `init/copy/move/drop` 中 `move` 全部为 NULL。
- 建议方向：为 `string/array` 等容器类添加 `move` 实现，让 vector 扩容时不走 copy+drop。

---

## 2. base.h

**规模**：618 行 ｜ **职责**：内存分配包装、线程级临时内存（TempArena）、错误槽、内存遥测

### 2.1 现状诊断

**实现质量：中上。** TempArena 是高质量的线性分配器设计（spill block + 常规 block 分离），`__xrtMemTelemetry*` 用原子计数做了无锁遥测，整体清晰。

**性能问题**
- `__xrtTempArenaEnsureCurrent` 在找可用 block 时**线性遍历链表**（base.h:274-294）。常规场景 block 数量少（1-3 个）影响不大，但若临时内存频繁 spill，链表会变长。建议维护 `pCurrent` 的"最近使用"指针即可，或限制常规 block 数量上限。
- `xrtTempMemory` 每次调用都查 `xrtThreadGetCurrent()` 并做两次 `if == 0` 初始化检查（base.h:404-409）。可用 `__thread` / `__declspec(thread)` 缓存 thread data 指针减少一次 TLS 查询（需确认现有 `xrtThreadGetCurrent()` 是否已做这层缓存）。

**正确性问题**
- `xrtSetErrorU16/U32`（base.h:480-498）**先释放入参再转换**，语义混乱：调用者传 `bFree=TRUE` 时，入参被 `xrtFree(sError)` 释放，但转换结果通过 `xrtSetError(..., TRUE)` 又标记为需释放。如果转换失败（返回 NULL），原入参已被释放但错误槽未设置，造成"错误丢失"。
- TempArena 的 `iUsed`/`iCapacity` 是 `uint32`（base.h: 经由 `xrtTempArenaBlock`），单 block 上限 4GB。在 64 位平台做大块临时缓冲时可能不够，但 spill block 路径会绕过此限制，影响有限。

**代码异味**
- `OnError` 回调（base.h:462）只接收字符串，没有级别/码/来源。和"统一错误模型"的缺口一致。
- `xrtSetError` 在 `pThreadData == NULL` 时仍会调用 `xCore.OnError`，但跳过线程槽写入——这是合理的，但未在注释中说明"未 attach 的线程调用 set error 不会持久化"。

### 2.2 缺失能力

- **错误级别**：无 trace/debug/info/warn/error/fatal 区分（与 `std.log` 对齐缺口）
- **错误码**：仅有字符串，无法程序化判断错误类型
- **错误来源**：无法定位是哪个模块/文件/行抛的错
- **结构化附加数据**：无法把 HTTP 状态码、系统 errno、原始 buffer 等附在错误上
- **错误栈**：嵌套调用链无法累积上下文（Go 的 `errors.Wrap` 模式）

### 2.3 优化建议

| # | 建议 | 优先级 | 改动范围 |
|---|---|---|---|
| base-1 | 在 `xrtThreadData` 增加 `xrtErrorContext` 结构槽，保留 `xrtSetError` 作为薄包装写入"消息+默认码" | P0 | base.h + type 定义 |
| base-2 | 新增 `xrtSetErrorEx(int code, const char* module, int line, const char* fmt, ...)` 与 `xrtGetLastErrorEx(xrtErrorContext*)` | P0 | base.h，不动旧 API |
| base-3 | 修复 `xrtSetErrorU16/U32` 的"先 free 后 convert"顺序问题，失败时不释放入参 | P0 | base.h 局部 |
| base-4 | 错误模块号注册表（XRT_MOD_BASE / XRT_MOD_NET / XRT_MOD_HTTP ...） | P1 | 新增小头文件 |
| base-5 | TempArena 的 `iUsed/iCapacity` 改为 `size_t`，单 block 支持超过 4GB | P2 | base.h + type |
| base-6 | 内存遥测增加"对齐 padding 浪费字节数"统计 | P2 | base.h |

### 2.4 xlang 对齐点

- `std.error` / `std.log` / `std.trace` 三个标准库都依赖结构化错误。`base.h` 是它们的 C 层根基。
- 建议错误对象设计参考 `xvalue`（已支持 error 类型），让错误可以直接 box 进 xvalue 在 xlang 侧传播。

### 2.5 风险与依赖

- runtime attach 模型是 **A 类冻结**，错误槽扩展必须**只增不改 public contract**：`xrtSetError/xrtGetError/xrtClearError` 三个函数签名和行为必须保持。
- 改动会影响**所有模块**（所有 `xrtSetError` 调用点）。建议新 API 走 `Ex` 后缀，旧调用点分批迁移。

---

## 3. os.h

**规模**：123 行 ｜ **职责**：进程启动（xrtRun / xrtStart / xrtChain）

### 3.1 现状诊断

**实现质量：低。** 这是 xrt 全库最薄的模块之一，只覆盖了"启动一个程序并（可选）等它结束"这一最低能力。

**正确性问题**
- `xrtChain` 在 Linux 的返回分支（os.h:113-118）只判断 `WIFEXITED`，**未处理 `WIFSIGNALED`**（被信号杀死时返回 -1，丢失"被哪个信号杀"的信息）。
- `xrtRun` 在 Linux 用 `fork() + execl("/bin/sh", "sh", "-c", sPath)`（os.h:30-38），**shell 注入风险**：若 `sPath` 来自不可信输入，攻击者可注入 `; rm -rf /`。应提供 `xrtRunEx` 直接 execvp 数组形式。
- `xrtRun` 在 Windows 用 `CREATE_NEW_CONSOLE`，无法被父进程捕获 stdout/stderr。
- 无 `xrtKill`/`xrtTerminate` 能力，拿到 process handle 后无法主动结束。

**代码异味**
- 三个函数都重复了"UTF-8→UTF-16 + CreateProcessW"模板（Windows 分支），应抽出 `__xrtSpawnWin`。
- `(void)iSize;` 在 Linux 分支显式丢弃参数，但函数签名仍接受 `iSize`，对调用者造成"这个参数有意义"的误导。

### 3.2 缺失能力

按 ROADMAP §4.5「子进程 / 工具执行层」的完整清单，缺：

- 启动子进程（✓ 已有，但过于简陋）
- 管道读取 `stdout/stderr` ✗
- 提供 `stdin` ✗
- timeout ✗
- exit code（部分：只有 `xrtChain` 返回）
- working dir ✗
- env override ✗
- 取消运行中的进程 ✗

### 3.3 优化建议

| # | 建议 | 优先级 | 改动范围 |
|---|---|---|---|
| os-1 | 修复 `xrtChain` Linux 分支处理 `WIFSIGNALED` | P0 | os.h 局部 |
| os-2 | 新增 `xrtSubprocess` 模块（建议单独成 `subprocess.h`，而非塞进 os.h），覆盖 stdin/stdout/stderr 管道、timeout、working dir、env、exit code、信号退出 | P1 | 新模块 |
| os-3 | 新增 `xrtRunArray(cmd, argv, ...)` 走 execvp，避免 shell 注入 | P1 | os.h 或 subprocess.h |
| os-4 | 新增 `xrtTerminateProcess(handle, exitCode)` | P1 | os.h |
| os-5 | 抽出 `__xrtSpawnWin` 内部 helper，消除三函数重复 | P2 | os.h |
| os-6 | `xrtStart` 在 Linux 走 `xdg-open`，macOS 缺 `open` 分支、Linux 桌面环境无 xdg-open 时缺降级 | P2 | os.h |

### 3.4 xlang 对齐点

- 对应 `std.process` / `std.sys.process` / `std.sys.wait`。
- subprocess 是 AI Agent 场景"工具执行"的核心能力（LLM 调工具→子进程→捕获输出），优先级应高于普通工具库。

### 3.5 风险与依赖

- 子进程管道 + timeout **强依赖 P0 的 context/deadline 主干**。建议先做 P0 主干，再做 subprocess。
- Windows 管道实现复杂（IOCP vs 同步管道），需要决定是否与 xnet 引擎集成。

---

## 4. string.h

**规模**：2125 行 ｜ **职责**：UTF-8 字符串操作（查找/替换/分割/格式化/编码/相似度）

### 4.1 现状诊断

**实现质量：中。** UTF-8 感知做得到位（`__xrtUtf8CharLenSafe`、字符下标↔字节偏移转换），但**多处性能实现停留在教科书级别**。

**性能问题（严重）**

1. **case-insensitive 搜索的"复制两份小写副本"模式**（string.h:441-449, 484-491, 557-565, 1000-1001）：
   ```c
   str sText1 = xrtLCase(sText, 0, FALSE);    // 全串复制
   str sText2 = xrtLCase(sSubText, 0, FALSE); // 全串复制
   sSub = memmem(sText1, iSize, sText2, iSubSize);
   ```
   每次调用都做 **O(n) 内存分配 + 2x 遍历**。HTTP header 解析、配置匹配等热路径会被严重拖慢。

2. **`xrtStrLastIndexOf` 用朴素 `for(i=0; ...; ++i) memcmp`**（string.h:492-496），最坏 **O(n·m)**。应该从右向左扫，或用 Boyer-Moore-Horspool / Sunday 算法。

3. **`xrtStrCount` 同样是朴素扫描**（string.h:527-534）。

4. **`xrtLCase/xrtUCase` 跳过 UTF-8 多字节字符**（string.h:363-385），意味着 `xrtLCase("İSTANBUL")` 不会做任何土耳其语特殊折叠。这在 ASCII 场景下足够，但**与 `std.str` 的 Unicode 语义存在缺口**。

5. **`xrtReplace` 用 `strncpy` 复制**（string.h:852-855），`strncpy` 在源长度 >= n 时不写 `\0`，且对中间 `\0` 处理奇怪。应换 `memcpy`。

6. **`xrtStrSim`（Levenshtein）分配 `int* dp`**（string.h:2058），小字符串频繁分配。建议短串走栈缓冲（如 `len2 <= 256`）。

**正确性问题**

1. `xrtLCase/xrtUCase` 处理 UTF-8 5/6 字节序列（string.h:377-383），但 RFC 3629 已限制 UTF-8 最多 4 字节。这部分代码无害但过时，且与 `charset.h` 的处理不一致（charset.h 把 5/6 字节替换为 FFFD）。

2. `xrtSplit` 的 `bSrcRevise=TRUE` 路径（string.h:1344-1351）直接 `pData[i] = 0` 写入用户传入的 buffer。如果该 buffer 来自只读段（如字符串字面量 `"a,b,c"`），会段错误。当前文档注释提到"会破坏原数据"，但应增加只读检测或在文档明确禁止传字面量。

3. `xrtFormat`（string.h:791-810）的 `vsnprintf(NULL, 0, ...)` 两次调用，第二次 `vsnprintf` 在某些 MSVC 老版本上行为不一致。当前代码已用 `sRet[iSize] = 0` 兜底，问题不大。

**代码异味**
- 多个函数对 `sText == xCore.sNull` 的处理不统一：有的当 NULL 处理，有的当空字符串处理。
- `xrtStrByteAt`（string.h:931-939）的命名误导——名字说返回 byte，注释说"获取指定字节"，但实际用 `__xrtStrNormalizeCharIndex` 把 index 当字符下标处理。

### 4.2 缺失能力

- **流式/SIMD 加速的 UTF-8 长度计算**：`xrtStrCharLen` 是逐字节扫描，长字符串（>1KB）成本高。ASCII 部分可 SIMD（参考 simdjson 的 UTF-8 validator）。
- **StringBuilder / 字符串构建器**：当前拼接只能 `xrtStrJoin(3, a, b, c)`，多次拼接每次都重新分配。应有 `xrtStrBuilder` 增量 append。
- **Unicode case folding**：仅 ASCII。`std.str` 标准要求完整 Unicode 大小写。
- **标准化（NFC/NFD/NFKC/NFKD）**：完全缺失。AI 文本处理场景常用。
- **整数解析的边界**：`xrtStrToI64/xrtStrToNum` 的实现没看到，需验证溢出行为。
- **split 的 limit/max**：`xrtSplit` 不能限制最多分几段。

### 4.3 优化建议

| # | 建议 | 优先级 | 改动范围 |
|---|---|---|---|
| str-1 | **重写 case-insensitive 搜索**：用 ASCII 大小写折叠 + memmem，或字符级比较函数 `__xrtStrEqIC(a,b,n)`，消除"两份副本" | P0 | string.h 多处 |
| str-2 | `xrtStrLastIndexOf` 改为**从右向左扫描**或 BMH 算法 | P0 | string.h 局部 |
| str-3 | `xrtReplace` 中 `strncpy` → `memcpy` | P0 | string.h 局部 |
| str-4 | 抽象一个 `__xrtStrFindIC(haystack, needle)` 内部 helper，被 find/indexof/count/contains 复用 | P1 | string.h 重构 |
| str-5 | 新增 `xrtStrBuilder`（append/appendf/concat/build），用于 HTTP header、JSON 序列化等场景 | P1 | string.h 或新模块 |
| str-6 | `xrtStrSim` 短串走栈（`int buf[256]`），长串才 malloc | P2 | string.h |
| str-7 | `xrtSplit` 增加 `iMaxSplit` 参数（`xrtSplitEx`），旧 `xrtSplit` 包装为 max=0 | P2 | string.h |
| str-8 | 移除 5/6 字节 UTF-8 死代码，统一与 charset.h 一致 | P2 | string.h |
| str-9 | 文档明确 `bSrcRevise=TRUE` 时 buffer 必须可写 | P2 | string.h 注释 |
| str-10 | `xrtStrByteAt` 重命名为 `xrtStrByteAtByChar`，或修正实现为真·字节访问 | P2 | string.h + 公开头 |

### 4.4 xlang 对齐点

- `std.str` 是 xlang 中最高频的标准库之一。当前 xrt 的 ASCII case 折叠 + 缺失 normalization 会让 xlang 在国际化场景（多语言 LLM 输出、跨语言文本处理）受限。
- `std.fmt` 对应 `xrtFormat`，需要支持命名参数、宽度/精度控制——当前的 `xrtIntFormat/xrtNumFormat` 是良好起点，但与 `xrtFormat`（走 vsnprintf）风格分裂，应统一。

### 4.5 风险与依赖

- string.h 被**所有模块**使用，任何 API 变更影响巨大。建议：
  - 重写 `xrtFindStr`/`xrtStrLastIndexOf` 的**内部实现**，不动签名（属于内部优化）
  - 新增能力（StrBuilder、SplitEx）走新 API
- 性能修复 str-1/str-2/str-3 可以单独成 PR，互不影响。

---

## 5. charset.h

**规模**：948 行 ｜ **职责**：UTF-8/16/32 互转、BOM/编码探测、跨编码转换

### 5.1 现状诊断

**实现质量：中。** 编码转换核心算法正确，BOM/编码探测的启发式合理。但**UTF-8 校验不严格**，存在安全风险。

**安全问题（重要）**

1. **`xrtUTF8to16/xrtUTF8to32` 不拒绝非法 UTF-8**：
   - 不检查 **overlong encoding**（如用 2 字节编码 U+007F，应只用 1 字节）
   - 不检查 **代理区码点**（U+D800-DFFF 编码成 UTF-8 是非法的，但这里会接受）
   - 不检查 **超出 U+10FFFF 的码点**
   - 5/6 字节序列（charset.h:109-117, 177-194）甚至被解码出来，远超 Unicode 上限

   这意味着攻击者可以构造特殊 UTF-8 字符串绕过 security filter（"UTF-8 smuggling"）。在 HTTP/JSON 解析场景是真实风险。

2. `xrtIsUTF8`（charset.h:750-773）检查过于宽松，同样不拒绝 overlong 和代理区。

3. `xrtConvCharset` 在 Windows 调 `WideCharToMultiByte` 不传 `WC_ERR_INVALID_CHARS` / `MB_ERR_INVALID_CHARS` 标志，错误字符被静默丢弃或替换。

**性能问题**
- 编码转换走"两遍扫描"（先算长度，再转换），合理。但**未对 ASCII 子集做 SIMD 加速**。UTF-8 转 UTF-16 时，若整段都是 ASCII，可用 SIMD 一次处理 16/32 字节（参考 simdjson `utf8_to_utf16`）。
- `xrtConvCharset` 的"20 种排列组合 if/else"（charset.h:544-628）可改为**表驱动**：`static const struct { int in; int out; conv_fn fn; } table[]`，O(1) 查表。

**正确性问题**
- `xrtUTF8to16` 中 `iExtraBytes < 3` 分支判断（charset.h:34）写得晦涩：实际含义是"1-3 extra bytes（即 2-4 字节 UTF-8 字符）编码为 1 个 UTF-16 unit"。注释里把"字节数"和"extra bytes"混用，建议改写。
- `xrtDetectCharset` 对 UTF-16/32 的代理区/范围检查（charset.h:832-872）逻辑正确但变量命名混乱（`bNoUTF16LE` 等）。

### 5.2 缺失能力

- **流式/增量编码转换**：当前必须整体转换，大文件/网络流必须缓冲全部数据。
- **Linux 下的 OEM/系统 ANSI 编码**：非 Windows 平台 `XRT_CP_OEM` 强制为 UTF-8（charset.h:530-531），但 GBK/Shift-JIS 等仍常见。可考虑内置最小 ICU-like 表，或留作扩展。
- **Latin-1 (ISO-8859-1)**：HTTP header 历史编码，缺失。
- **错误处理 callback**：转换遇到非法字节时，应允许调用者选择"替换/跳过/中止"。

### 5.3 优化建议

| # | 建议 | 优先级 | 改动范围 |
|---|---|---|---|
| cs-1 | **加严 UTF-8 校验**：拒绝 overlong encoding、代理区码点、> U+10FFFF；5/6 字节序列直接当非法 | P0 | charset.h 多处 |
| cs-2 | 提供 `xrtUTF8Validate(str, size, size_t* bad_offset)`，让 HTTP/JSON 解析器在入口校验 | P0 | charset.h |
| cs-3 | `xrtConvCharset` Windows 分支加 `WC_ERR_INVALID_CHARS` 等严格标志 | P1 | charset.h |
| cs-4 | `xrtConvCharset` 改为表驱动 | P2 | charset.h |
| cs-5 | ASCII 子集 SIMD 加速（utf8→utf16 快路径） | P2 | charset.h |
| cs-6 | 流式编码转换 API（state + feed + finish） | P1 | charset.h 或新模块 |
| cs-7 | 移除 5/6 字节 UTF-8 解码，统一替换为 FFFD | P1 | charset.h |

### 5.4 xlang 对齐点

- `std.encode` 包含 utf8 相关编码解码。`xrtUTF8Validate` 是 std.encode 的基础能力。
- 严格的 UTF-8 校验是 JSON 解析器（`json.h`）安全的前提——JSON spec 明确要求 UTF-8 必须 valid。

### 5.5 风险与依赖

- 加严 UTF-8 校验**可能让原本"宽容"接受的输入变成拒绝**。需要在版本说明里明确"breaking 行为变更"。
- `json.h`、`xhttp_util.h` 等都依赖 charset 转换，需联合验证。

---

## 6. time.h

**规模**：1650 行 ｜ **职责**：xtime（秒级，公元 0 年为原点）类型、日期时间分解/合成、格式化/解析、时区

### 6.1 现状诊断

**实现质量：中下。** 这是批次 A 里**最值得重构**的模块。算法层面有重大性能问题，时区处理有正确性 bug。

**性能问题（严重）**

1. **`xrtDay/xrtMonth/xrtYear/xrtDecodeSerial` 用线性循环找年份**（time.h:213-221, 255-263, 291-298, 348-356）：
   ```c
   for ( int i = 0; i < 400; i++ ) {
       uint64 iSec = xrtDaysInYear(i) * XRT_TIME_DAY;
       if ( iYearMod >= iSec ) { iYearMod -= iSec; iYear++; }
       else break;
   }
   ```
   每次 decode 都要做 **O(400) 次循环 + 闰年判断**。对于日志、HTTP 时间戳等高频场景，这是灾难性的。

   **正确做法**：用闭合公式（Howard Hinnant 的 `civil_from_days` 算法，O(1)）：
   ```c
   // 把 days-from-epoch 转换为 (y/m/d)，纯算术
   int64 z = days + 719468;
   int64 era = z >= 0 ? z/146097 : (z-146096)/146097;
   int64 doe = z - era*146097;   // [0, 146097)
   int64 yoe = (doe - doe/1460 + doe/36524 - doe/146096) / 365;
   ...
   ```

2. `xrtNow/xrtDate/xrtTime/xrtNowStr/xrtDateStr/xrtTimeStr/xrtNowUTC` **每个都独立调 `time(NULL) + localtime_r`**（time.h:416-501）。如果业务同时需要 date 和 time，会做 2 次 syscall。应有 `xrtNowParts(struct tm*)`。

3. `xrtTimeFormat` 分配固定 256 字节缓冲（time.h:1421-1422），且循环里有 `pos < bufSize - 32` 守卫，**超长字面量会静默截断**。

**正确性问题（严重）**

1. **时区处理完全错误**（time.h:846-886）：
   - `xrtTimezoneOffset()` 返回**当前时刻**的偏移（含 DST）。
   - `xrtUTCToLocal(utc) = utc + xrtTimezoneOffset()`：对一个**夏令时切换前**的旧时间戳，会用**当前 DST 状态**的偏移转换，结果错误。
   - 例如：北京 1 月 1 日某时刻（无 DST，中国历史上曾有 DST，已废除，但美国/欧洲仍有）。`xrtUTCToLocal` 对美国 1 月某时刻会用 7 月（夏令时）的偏移。
   - **正解**：转换任意时间戳必须基于该时间戳本身的 `mktime`/`localtime_r`，不能复用当前偏移。

2. `xrtDateSerial` 的负年份（公元前）路径（time.h:120-129）：
   ```c
   iYear = llabs(iYear);
   for ( uint64 i = 0; i < iYearMod; i++ ) { iDate += xrtDaysInYear((int)i) * XRT_TIME_DAY; }
   ```
   这里 `(int)i` 把 0..iYearMod 当年份传入 `xrtDaysInYear`——**和公元后路径混了**。公元前 5 年的算法应该是"从公元 0 年往回扣 5 个 year"，但循环里 `xrtDaysInYear(i)` 是把 i 当年份算闰年，含义错乱。

3. `xrtWeekday`（time.h:311-316）返回 `iDay % 7`，但**没有定义 epoch（公元 1 年 1 月 1 日）是星期几**。`xrtDecodeSerial` 也是同样的取模逻辑。需要验证：公元 1-01-01 实际是星期一，那么 `iDay % 7` 的结果 0 对应星期一才对。当前文档/测试未明确。

4. `xrtStrToTime` 与 `xrtTryStrToTime`（time.h:915-1129）**大量重复代码**（~200 行几乎复制粘贴）。`xrtTryStrToTime` 多了边界检查，`xrtStrToTime` 没有——若解析失败，`xrtStrToTime` 会返回 0（=公元 0-01-01），无法区分"真解析出 0"和"解析失败"。应共享逻辑。

5. `xrtTimeParse` 在 `year==0` 时自动用当前年份（time.h:1614）——副作用，应文档说明或加 flag。

### 6.2 缺失能力

- **纳秒/毫秒精度**：`xtime` 是秒级 `int64`，`xrtTimer()` 返回 double 高精度但**与 xtime 体系割裂**。应有 `xrtNowNS()` 或在 xtime 旁定义 `xdatetime`（秒+纳秒）。
- **时区名/数据库**：缺 `Asia/Shanghai`、`America/New_York` 等时区名支持。完整 IANA tzdata 体积大，但可支持系统时区名 + 当前偏移。
- **DST 感知**：当前完全无 DST 信息。
- **单调时钟**：`xrtTimer()` 用 `CLOCK_MONOTONIC`（Linux）/ QPC（Win），但没有显式的 `xrtMonotonicNow()` 让用户表达"我要的是单调时钟不是墙钟"。
- **ISO 8601 duration/interval**：`P1Y2M3DT4H5M6S` 完全缺失。
- **RFC 3339 / RFC 2822 专用 helper**：虽然 `xrtTimeFormat` 的格式串能表达，但缺少专用快速函数。

### 6.3 优化建议

| # | 建议 | 优先级 | 改动范围 |
|---|---|---|---|
| time-1 | **`xrtDecodeSerial` 改用 Hinnant `civil_from_days` 闭合公式**，O(1) | P0 | time.h 重写核心 |
| time-2 | **修复时区转换**：`xrtUTCToLocal(t)` 改为 `mktime(gmtime_r(...))` 路径，基于 t 自身而非"当前偏移" | P0 | time.h |
| time-3 | 验证并文档化 `xrtWeekday` 的 epoch 假设（公元 1-01-01 = 星期一） | P0 | time.h + 测试 |
| time-4 | 修复 `xrtDateSerial` 公元前路径的循环逻辑 | P0 | time.h |
| time-5 | `xrtStrToTime` 与 `xrtTryStrToTime` 合并为单一内部函数 + 严格性 flag | P1 | time.h |
| time-6 | 新增 `xrtNowParts(struct tm*)`，其他 now* 函数复用，减少 syscall | P1 | time.h |
| time-7 | 新增 `xrtMonotonicNow()` 显式单调时钟 | P1 | time.h |
| time-8 | 新增 `xrtNowNS()/xrtNowMS()` 返回纳秒/毫秒精度的墙钟 | P1 | time.h |
| time-9 | `xrtTimeFormat` 改为动态扩容缓冲，避免 256 字节截断 | P2 | time.h |
| time-10 | 新增 ISO 8601 duration 解析/格式化（`PnYnMnDTnHnMnS`） | P2 | time.h |
| time-11 | 新增 RFC 3339 / RFC 2822 专用 helper | P2 | time.h |
| time-12 | 内部预留 deadline helper（`xrtDeadlineFromTimeout`/`xrtDeadlineExpired`）供 P0 主干复用 | P1 | time.h 内部 |

### 6.4 xlang 对齐点

- `std.time` / `std.clock`：`std.time` 对应墙钟，`std.clock` 对应单调时钟/高精度计时。当前 `xrtTimer()` 实际是 `std.clock`，命名应澄清。
- ISO 8601 是现代 API/JSON 时间交换的标准格式（LLM API、数据库、HTTP 都用），缺失会显著降低 xlang 标准库的可用性。

### 6.5 风险与依赖

- `xrtDecodeSerial` 重写有**正确性回归风险**，必须有充分的边界测试（公元前、1582 格里高利历切换点、闰秒附近、远未来日期）。
- 时区修复影响所有依赖 `xrtUTCToLocal` 的代码（HTTP Date header、TLS 证书有效期、日志时间戳等），需联合回归。

---

## 7. hash.h

**规模**：1253 行 ｜ **职责**：32 位（nmhash32x）/64 位（rapidhash）非加密哈希

### 7.1 现状诊断

**实现质量：高。** vendored 的 nmhash32x 和 rapidhash 都是高质量实现，带 SSE2/AVX2/AVX512 自动向量化。xrt 提供了三档 64 位变体（full/micro/nano），覆盖 HPC 和嵌入式场景。

**潜在问题**

1. **`rapid_secret` 非 static**（hash.h:758）：`RAPIDHASH_CONSTEXPR uint64_t rapid_secret[8]`。在 C 模式下 `RAPIDHASH_CONSTEXPR = static const`，没问题；但若 xrt.h 被 C++ TU 包含且 `RAPIDHASH_CONSTEXPR = constexpr`（C++ 模式），会导致**链接期多重定义**。xrt 主语言是 C，影响小，但 single-head 分发到 C++ 项目时会出问题。

2. **默认 seed 硬编码**：`xrtHash32` 用 `HASH32_SEED`，`xrtHash64` 用 0。zero seed 在 rapidhash 设计上是合法的，但**易被预测**——如果用作哈希表的抗碰撞种子，攻击者可构造大量同哈希字符串触发 DoS（hash flooding）。

3. **`rapid_mum` 在 GCC x86_64 走 `__uint128_t`**，无问题；但在 32 位平台走手动 64x64→128 拆解（hash.h:814-824），性能会显著下降。32 位支持应明确为"可用但非最优"。

**缺失能力（重要）**

1. **流式/增量哈希 API**：当前只有 `xrtHash32(ptr, len)` 一次性接口。HTTP body、JSON 大对象、文件校验等场景需要 `init → update(N 次) → final`。
2. **CRC32**：网络（PNG/zip/gzip）、Ethernet 都用 CRC32。rapidhash 不能替代 CRC（CRC 是错误检测，不是哈希）。
3. **字符串/整数专用快速哈希**：`xrtHashStr(s)` = `xrtHash64(s, strlen(s))`；`xrtHashInt64(v)` = 整数 finalizer（如 `SplitMix64`）。这些是 dict/set 的高频入口，应作为内联 helper。
4. **抗碰撞种子生成**：`xrtHashRandomSeed()` 返回密码学随机的 64 位种子，用于哈希表初始化。

### 7.2 优化建议

| # | 建议 | 优先级 | 改动范围 |
|---|---|---|---|
| hash-1 | 修复 `rapid_secret` 在 C++ 模式的链接问题（强制 `static const` 或挪到 .c） | P1 | hash.h |
| hash-2 | 新增流式 API：`xrtHash32State/xrtHash32Update/xrtHash32Final` 和 64 位版本 | P1 | hash.h |
| hash-3 | 新增 `xrtCRC32/CRC32C`（Castagnoli），用于网络/压缩/存储校验 | P1 | hash.h 或新模块 |
| hash-4 | 新增内联 helper：`xrtHashStr`、`xrtHashInt64`、`xrtHashInt32` | P1 | hash.h |
| hash-5 | 新增 `xrtHashRandomSeed()`（基于 xrtRand64 + getpid + 时间） | P2 | hash.h |
| hash-6 | 文档明确：非加密哈希，不能用于密码学场景；抗 flood 需用随机种子 | P2 | hash.h 注释 |

### 7.3 xlang 对齐点

- `std.hash`：对应 hash32/hash64。流式 API 是 std.hash 的标配。
- CRC32 在 xlang 标准库定位待澄清（是否进 `std.hash` 还是单独 std.crc）。建议先在 xrt 提供，xlang 侧再决定映射。

### 7.4 风险与依赖

- 流式 API 对 nmhash32x/rapidhash 来说需要从原项目引入 `__xxhash_stream` 类似代码，注意许可证（rapidhash 是 BSD-2，OK）。
- `dict.h`、`set.h` 等容器依赖哈希函数，新增 helper 不影响现有调用。

---

## 8. math.h

**规模**：527 行 ｜ **职责**：PCG 随机数 + 数值辅助（min/max/clamp/sign/log2/exp2/hypot/...）

### 8.1 现状诊断

**实现质量：中。** PCG 部分干净（pcg32 + 双流拼接 pcg64），数值辅助覆盖合理。但**多处重复造轮子**，未利用 CRT/编译器内建。

**性能问题**

1. `xrtMathLog2`（math.h:429-432）走 `log(value) / log(2.0)`——**两次 log + 一次除法**。应直接用 `log2(value)`（C99 标准函数，所有支持的编译器都有）。
2. `xrtMathExp2`（math.h:437-440）走 `pow(2.0, value)`，应用 `exp2(value)`。
3. `xrtMathCbrt`（math.h:487-496）手写 `pow(value, 1.0/3.0)`，应用 `cbrt(value)`（更精确，且处理负数无需分支）。
4. `xrtMathHypot`（math.h:501-526）手写，应用 `hypot(x, y)`（CRT 实现通常更精确，避免中间溢出）。
5. `xrtMathIsNaN/IsInf/IsFinite` 用 `value != value` 等技巧（math.h:405-424），可读性差。C99 `isnan/isinf/isfinite` 在所有目标平台都可用。

**正确性问题**
- `xrtRandRangeEx`（math.h:154-178）用拒绝采样消除模偏差，正确。但 `threshold = -iRange % iRange` 在 `iRange == 0` 时除零（仅 `min == max` 时 iRange==1，所以实际不会触发，但仍建议加保护）。
- `xrtIntApprox` 的"百分比模式"对 `maxAbs == 0` 直接返回 TRUE（math.h:278），意味着 `approx(0, 100, percent=0.05)` 会返回 TRUE——若 tol>0 这显然不对。应改为返回 `diff <= tol`。

**设计问题**
- PCG64 用两个 pcg32 流拼接（math.h:73-74），状态空间 128 位但输出 64 位。这比真正的 pcg64（128-bit state, 64-bit output, single stream）在统计品质上略弱，但实际够用。
- `xrtRandSeedThread`（math.h:63-75）用固定异或常量派生两个流（`seed ^ 0x5851...`），种子之间相关性可能不够独立。

### 8.2 缺失能力

- **分布函数**：`xrtRandGauss`（Box-Muller）/`xrtRandExp`（指数分布）/`xrtRandUniformDouble`（[0,1) 浮点）。AI/仿真场景常用。
- **游戏/AI 常用数值工具**：`lerp`、`smoothstep`、`moveTowards`、`wrap`、`map_range`。
- **位运算工具**：`nextPow2`、`popcount`、`clz/ctz`（编译器内建 `__builtin_popcount`/`__builtin_clz` 已有，应包装为跨平台 API）。
- **整数运算安全**：`mulOverflow`/`addOverflow`（编译器内建 `__builtin_mul_overflow`）。

### 8.3 优化建议

| # | 建议 | 优先级 | 改动范围 |
|---|---|---|---|
| math-1 | `xrtMathLog2/Exp2/Cbrt/Hypot` 改用 C99 内建函数；老 CRT 用 `#ifdef` 退化 | P1 | math.h |
| math-2 | `xrtMathIsNaN/IsInf/IsFinite` 改用 `isnan/isinf/isfinite` | P1 | math.h |
| math-3 | 修复 `xrtIntApprox` 的 `maxAbs==0` 边界 | P0 | math.h 局部 |
| math-4 | 新增 `xrtRandDouble()` 返回 [0,1) 浮点 | P1 | math.h |
| math-5 | 新增 `xrtRandGauss(mu, sigma)` | P2 | math.h |
| math-6 | 新增 `xrtMathLerp/smoothstep/nextPow2/popcount/clz` | P2 | math.h |
| math-7 | `xrtRandRangeEx` 的 `iRange==0` 保护 | P2 | math.h |

### 8.4 xlang 对齐点

- `std.math` / `std.rand`：分布函数是 std.rand 的标配。
- `std.math` 的常量（PI/E 等）建议集中在 math.h 头部（已有 `XRT_MATH_PI`）。

### 8.5 风险与依赖

- 改用 C99 内建函数需确认 TCC 支持（TCC 对 C99 math 支持有限，可能需 `#ifdef __TINYC__` 退化路径）。

---

## 9. xid.h

**规模**：77 行 ｜ **职责**：自定义 XID（192-bit：Tick+Time+Addr+Rand）

### 9.1 现状诊断

**实现质量：低。** 这是个早期设计的"分布式唯一 ID"，存在多个问题。

**问题**
1. **`RandStringDefaultTemplate` 复用做 base64 编码**（xid.h:7）：`xrtBase64Encode(pXID, 24, RandStringDefaultTemplate)` 用了"0-9A-Za-z-_" 这 64 字符表。但标准 base64 字符表是 `A-Za-z0-9+/`，这里用 URL-safe 风格（`-_`）但**没有 padding 处理**，输出可能与其他 base64 解码器不兼容。
2. **`xrtDecodeXID` 无错误返回**（xid.h:13-16）：非法输入返回 `xrtBase64Decode` 的结果，但若解码失败返回 `xCore.sNull`，调用者把它 cast 成 `xid`（结构体指针）后**解引用会段错误**。应返回 NULL 并设置错误。
3. **`xCore.LocalAddr` 来源不明**：从命名推测是 MAC 地址派生，但容器/虚拟机/云主机 MAC 可能重复。分布式场景不能依赖 MAC 唯一性。
4. **`Tick` 派生方式平台不一致**（xid.h:29-41）：Windows 用 QPC.LowPart（32 位截断），Linux 用 `(tv_sec & 3) << 30 | tv_nsec`。同一时刻两个平台生成的 Tick 完全不同。
5. **时钟回拨未处理**：若 `xrtNow()` 因 NTP 调整回拨，`Time` 字段可能比上次小，破坏单调性。
6. **`xrtMakeXID` 分配 24 字节但结构是 4×8=32 字节**（xid.h:23）：`xrtMalloc(24)` 但 `xid` 是 `{Tick, Time, Addr, Rand}` 4 个字段。若每字段 8 字节，应是 32 字节。需查 `xid` 类型定义（在 type 头）。**可能是真实 bug**。

### 9.2 缺失能力

- **标准 ID 格式**：UUID v4（随机）/UUID v7（时间排序，2024 标准）/ULID（时间排序+随机）。生态兼容性比自定义 XID 重要得多。
- **雪花算法（Snowflake）**：分布式自增 ID，需要 machine ID 分配。
- **可排序 ID**：ULID/UUID v7 的核心价值是"按生成时间排序"，对数据库索引友好。
- **字符串形式紧凑**：当前 base64 编码 32 字节 = 44 字符（含 padding）；UUID hex 形式 16 字节 = 36 字符（带横线）。

### 9.3 优化建议

| # | 建议 | 优先级 | 改动范围 |
|---|---|---|---|
| xid-1 | **验证 `xrtMakeXID` 的 `xrtMalloc(24)` 与 `xid` 结构体大小是否匹配**（疑似 bug） | P0 | xid.h |
| xid-2 | `xrtDecodeXID` 失败返回 NULL + 设置错误，不返回 sNull | P0 | xid.h |
| xid-3 | 决策方向：保留 XID 还是迁移到 UUID v7/ULID？建议**新增** UUID v7/ULID 生成，XID 标记为 legacy | P1 | 新模块或 xid.h |
| xid-4 | 若保留 XID：统一跨平台 Tick 派生（用 `xrtTimer()` 的纳秒 + monotonic） | P1 | xid.h |
| xid-5 | 时钟回拨检测：单调时钟作为 fallback | P1 | xid.h |
| xid-6 | `LocalAddr` 改为运行时可配置（避免依赖 MAC） | P2 | xid.h + 初始化 |

### 9.4 xlang 对齐点

- xlang 标准库未明确 ID 模块，但 LLM/AI 场景（会话 ID、消息 ID、trace ID）对 sortable unique ID 需求强。建议作为 `std.xid` 或扩展库。

### 9.5 风险与依赖

- xid-1 若确认为 bug，是**真实内存越界**，必须立即修。
- 迁移到 UUID v7 不影响现有 XID 调用者（新 API）。

---

## 10. type.h

**规模**：1572 行 ｜ **职责**：运行时类型描述（xrt_type_desc）、类型操作（ops）、callable、调用帧/结果

### 10.1 现状诊断

**实现质量：中上。** 这是批次 A 里**架构最现代**的模块，明确为 xlang 桥接设计。`xrt_type_desc + ops + callable + call_frame/result` 的层次清晰，能支撑 xlang 的类型方法、box/unbox、跨语言调用。

**正确性问题（重要）**

1. **`xrt_callable->RefCount` 非原子**（type.h:1497, 1526-1548）：
   ```c
   pCallable->RefCount = 1;
   ...
   pCallable->RefCount++;   // 非原子
   ```
   如果 callable 被跨线程持有（如 future 的回调），并发 AddRef/Unref 是**数据竞争**（UB）。应改用 `__xrtAtomicAddFetch`/`__xrtAtomicSubFetch`，参考 `base.h` 的遥测已用原子操作。

2. **`__xrtTypeBoxFuture` 的引用计数路径复杂**（type.h:540-556）：
   - 先 `xFutureAddRef`，再 `xvoCreateHandle`；
   - 若 `xvoCreateHandle` 失败，回滚 `xFutureRelease`。
   - 逻辑正确，但**未处理 box 失败时的并发**（如果 future 在 box 期间被释放，AddRef 后的状态可能已无效）。需确认 future 释放是否延迟到 refcount==0。

3. **缺 move 语义**（type.h:586-694 所有 ops 的 `.move = NULL`）：
   - 对 `string` 类型，vector 扩容时只能 `copy + drop`，多一次 malloc/free。
   - 应实现 `__xrtTypeMoveString`：直接指针搬迁 + 置空源。

4. **`__xrtTypeHashString` 每次都 `strlen + xrtHash64`**（type.h:269-276）：
   - 字符串作为 dict key 时，每次查找都重算哈希。
   - 应在字符串对象上缓存哈希（xvalue 已有此优化？需确认；C 端 `str` 裸指针无元数据）。

**设计问题**

1. **类型注册中心缺失**：所有内置类型用 `static const xrt_type_desc __xrtTypeXxx`，外加 `xrtTypeInt()/xrtTypeString()` getter。但用户自定义类型如何注册？如何保证跨 TU 的 TypeId 唯一性？当前 `TypeId` 是 `XRT_TYPE_KIND_*` 枚举，自定义类型只能用名字字符串 fallback（`xrtTypeSame` 在 TypeId==0 时按名匹配），这不利于性能。
2. **方法查找线性扫描**（type.h:1048-1071）：`xrtTypeFindMethod` 是 O(n) memcmp。当前只有 `toString`，无影响；但 xlang 类型方法会很多，应改哈希表。
3. **类型转换表硬编码**（type.h:1090-1140）：`__xrtTypeCanExplicitConvert` 是大 switch。新增类型需改这里，违反开闭原则。建议每类型自带 `can_convert_to` 列表。

### 10.2 缺失能力

- **类型注册 API**：`xrtTypeRegister(const xrt_type_desc*)` + 全局表，让外部模块/扩展库注册新类型。
- **TypeId 分配器**：原子递增的 64 位 ID，保证全局唯一。
- **复合类型**：record/class 的字段表、继承关系、虚函数表。当前 `__xrtTypeRecord` 只有名字，无字段（type.h:909-920）。
- **泛型实例化缓存**：`array<int>` vs `array<string>` 应有缓存，避免每次重新构造 type_desc。
- **属性/事件**：xlang 类型方法已有，但属性（getter/setter）和事件（observer）未在 type_desc 中体现。

### 10.3 优化建议

| # | 建议 | 优先级 | 改动范围 |
|---|---|---|---|
| type-1 | **`xrt_callable->RefCount` 改为原子操作** | P0 | type.h |
| type-2 | 为 `string` 实现 `move`；后续给 `array/list/dict` 补 move | P1 | type.h |
| type-3 | 新增类型注册中心 `xrtTypeRegister/xrtTypeFindByName`，TypeId 改为原子分配的 64 位 | P1 | type.h + 新模块 |
| type-4 | `xrtTypeFindMethod` 改为按名字哈希（首次扫描时建表） | P2 | type.h |
| type-5 | `__xrtTypeCanExplicitConvert` 改为每类型自带 `convertible_to` 表 | P2 | type.h |
| type-6 | `__xrtTypeHashString` 与 `str` 缓存哈希联动（需 xvalue 配合） | P2 | type.h + value.h |
| type-7 | record 类型补字段表（`xrt_field_desc[]`） | P1（依赖 xlang 进度） | type.h |

### 10.4 xlang 对齐点

- `std.types` / 语言内建 `type`：type.h 是它们的 C 层映射。
- xlang v5 的 class/record 需要字段表 + 方法表 + 继承链。type.h 当前有方法表，缺字段表和继承。
- callable 对应 xlang 的 `function` 类型，refcount 是跨线程 future 回调的基础。

### 10.5 风险与依赖

- refcount 改原子是**纯内部**改动，不动 API。
- 类型注册中心是新增 API，旧代码不受影响，但需要 xlang 侧配合。
- record 字段表与 xlang v5 class 实现强耦合，应与 xlang 团队协同。

---

## 11. 批次 A 总结与建议执行顺序

### 11.1 立即修复（P0，正确性/安全/数据竞争）

| 模块 | 修复点 | 工作量 |
|---|---|---|
| time | `xrtDecodeSerial` 闭合公式重写 + 时区转换 bug + 公元前路径 + weekday epoch 文档 | 中 |
| string | case-insensitive 搜索重写 + `xrtStrLastIndexOf` 算法 + `xrtReplace` strncpy→memcpy | 中 |
| charset | UTF-8 严格校验（overlong/代理区/超限） | 中 |
| base | `xrtSetErrorU16/U32` free 顺序 bug | 小 |
| math | `xrtIntApprox` 的 maxAbs==0 边界 | 小 |
| xid | 验证 `xrtMalloc(24)` vs `xid` 结构体大小；`xrtDecodeXID` 错误返回 | 小 |
| type | `xrt_callable` refcount 原子化 | 小 |

### 11.2 高优先级（P1，能力补齐 + xlang 对齐）

- base：结构化错误模型（`xrtErrorContext` + `xrtSetErrorEx`）
- os：subprocess 模块（依赖 P0 主干）
- string：StrBuilder、Unicode case folding
- charset：流式编码转换、Windows 严格标志
- time：`NowParts/MonotonicNow/NowNS`、ISO 8601 duration
- hash：流式 API、CRC32、HashStr/HashInt helper
- math：C99 内建函数迁移、RandDouble/Gauss
- xid：UUID v7/ULID
- type：类型注册中心、move 语义、record 字段表

### 11.3 中优先级（P2，清理/增强）

- string：StrSim 栈优化、SplitEx、5/6 字节死代码清理
- charset：表驱动转换、SIMD ASCII 快路径
- time：TimeFormat 动态缓冲、RFC 3339/2822 helper
- hash：随机种子生成、文档化非加密警告
- math：游戏/AI 数值工具、位运算包装
- xid：跨平台 Tick 统一、时钟回拨处理
- type：方法哈希表、转换表外置

### 11.4 与 P0 三大主干的衔接

执行完批次 A 的 P0 修复后，建议按以下顺序推进主干：

1. **统一错误模型**（base.h 扩展）：批次 A 内部完成，不影响其他批次。
2. **统一执行上下文**（context/cancel/deadline）：依赖 time.h 的 deadline helper，批次 A 内可预留接口。
3. **统一资源生命周期**：依赖 type.h 的 move 语义，批次 A 内可启动。

三条主干准备就绪后，再进入批次 E（并发/异步），让网络、协程、subprocess 共享主干。

---

## 附录 A：评估方法学备注

- 本报告基于 `lib/*.h` 当前源码静态审查 + 测试用例 + ROADMAP/FREEZE 文档对齐分析，未做动态基准测试。
- 性能判断基于算法复杂度与实现模式，未给出具体数字（需后续 benchmark 验证）。
- "风险与依赖"列出的跨模块影响需在实际改动时通过 grep + 测试回归确认。
- 后续批次（B-H）将沿用本报告的维度和优先级口径。
