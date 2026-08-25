# XRT 扩展库深度审计核验与修复方案

核验日期：2026-08-25  
审计基线：`master aff1ac5a`  
核验范围：`xregex`、`xws`、`xmail`、`xssh`、`xruntime`、`xhttp`

## 1. 结论

第二份审计报告有较高价值，尤其准确发现了 `xregex` 第三方核心中的三个发布阻断问题。但是，报告对部分代码的构建边界、调用方持有关系、协议分层和显式配置语义判断不足，导致 `xws`、`xmail`、`xhttp` 和 `xssh` 中若干问题被明显放大。

经当前源码、模块清单、调用链和定向运行核验，结论调整如下：

| 级别 | 数量 | 结论 |
| --- | ---: | --- |
| P0 发布阻断 | 3 | 均位于 `xregex`：集合位图崩溃、捕获槽扩容失控、非法重复区间进入断言 |
| P1 应在发布前修复 | 5 | `xregex` 标志与位图清理、`xws` Close 后投递、`xws` 路由 Origin 策略缺口、`xruntime` 空 Value 追踪、`xregex` 裁剪测试闭包 |
| P2 功能/健壮性改进 | 多项 | Regex 边界语义、Mail 字符集互操作、SSH future 生命周期、Runtime 防御性检查、HTTP 文档与策略说明 |
| 不成立或明显夸大 | 12 项以上 | 包括 WebSocket 压缩 Writer 普通背压终止连接、关闭定时器 UAF、Mail STARTTLS 未初始化读取、当前 xmail 关闭证书验证、Cookie flag 隐式失效等 |

当前版本不能因为常规测试通过就直接发布：`xregex` 的 P0 问题具有明确可达路径，其中 32 模式集合匹配已经在本机复现为访问冲突。

## 2. xregex

### 2.1 P0：集合位图初始化崩溃，确认属实

位置：`extlibs/xregex/src/third_party/bbre/bbre.c:3513-3522`

`b` 的类型是位图缓冲区指针的指针。当前代码使用：

```c
*b[i] = 0;
```

表达式被解释为 `*(b[i])`，而不是 `(*b)[i]`。同时循环次数使用 `(size + BBRE_BITS_PER_U32) / BBRE_BITS_PER_U32`，在 `size` 恰好为位宽整数倍时还会额外处理一个元素。因此 32 个模式就会进入第二次循环并访问错误地址。

本机使用 32 个已成功创建的模式构造集合并执行匹配，进程退出码为 `0xC0000005`。现有 `regex_set` 测试只覆盖 3 个模式，无法发现该问题。

修复不能只改报告提出的单行，还必须同时修正元素数量：

```c
size_t iWords = (size + BBRE_BITS_PER_U32 - 1) / BBRE_BITS_PER_U32;

for (i = 0; i < iWords; i++)
    (*b)[i] = 0;
```

还应将位设置表达式中的 `1 << bit` 改为无符号类型移位，避免第 31 位上的有符号移位未定义行为。

### 2.2 P0：捕获槽扩容公式失控，确认属实

位置：`bbre.c:3401-3417`

`slots_alloc` 记录的是元素容量，当前后续扩容却再次乘以 `per_thrd`：

```c
size_t new_alloc = (s->slots_alloc ? s->slots_alloc * 2 : 16) * s->per_thrd;
```

捕获组较多时，这不是普通的保守扩容，而是乘法级内存放大。修复应统一容量单位，并对所有乘加执行溢出检查：

1. 计算 `needed = (slots_size + 1) * per_thrd`。
2. 初始容量按 `16 * per_thrd` 建立。
3. 后续只对现有元素容量做倍增，不再乘 `per_thrd`。
4. 若倍增不足则直接增长到 `needed`。
5. 所有计算先检查 `SIZE_MAX` 边界，失败返回 OOM/limit 错误，不允许回绕。

需要加入捕获组数量与输入长度的矩阵测试，并记录峰值分配量，确保内存增长符合既定复杂度上界。

### 2.3 P0：`{m,n}` 未验证 `m <= n`，确认属实

位置：解析器 `bbre.c:1339-1376`，编译器 `bbre.c:2920`

解析阶段没有建立 `min <= max` 不变量，编译阶段只用 `assert(min <= max)` 防御。结果是调试构建可能终止进程，关闭断言后又可能接受非法表达式并改变语义。

应在解析完成、写入 AST 之前返回稳定的语法错误。编译器断言可以保留为内部不变量检查，但不能承担外部输入校验。测试必须同时覆盖 Debug、Release、`{3,2}`、`{0,0}`、开放上界和最大值边界。

### 2.4 P1：标志解析和位图清理，确认属实

- `bbre.c:1439-1461` 的 `u` 分支没有设置当前 `flag`，`(?i-u)` 会错误清除 `i`。应把字符到标志位的映射集中为一个无状态函数，再统一执行 set/clear，避免分支残留状态。
- `bbre.c:3527-3530` 将位图元素数直接作为 `memset` 字节数，只清除了约四分之一空间。应使用 `bbre_buf_size(*b) * sizeof(**b)`，或让缓冲 API 提供明确的 byte-size 接口。

这两项应与 P0 位图修复放在同一个提交中，避免位图行为继续依赖未清区域碰巧不参与结果。

### 2.5 P2：边界和兼容语义，基本属实

- 恰好达到最大数字位数的重复计数被拒绝，需要修正先递增后拒绝的边界顺序。
- `[a-]` 是否接受需要形成明确兼容策略；若目标对齐现代常见正则，应按字面 `-` 处理。
- `[z-a]` 当前被静默规范化成 `[a-z]`，建议直接报语法错误，避免输入错误被隐藏。
- 重复命名捕获组和组名中的 NUL 当前不会形成直接内存安全问题，因为实现使用长度视图和索引捕获；但查找语义不清晰。建议拒绝重复名称，并明确公开 API 是否允许嵌入 NUL。

### 2.6 新发现：裁剪 OOM 测试闭包不成立

使用 `xregex` manifest 构建 `regex_match,regex_set` 时，`extlibs/xregex/tests/regex/test_regex_oom.c:11` 调用了当前裁剪闭包中不可见的 `xrtStrViewN`，在 `-Werror` 下编译失败。

这不是产品运行时缺陷，但说明扩展库的裁剪 release gate 没有闭合。优先方案是在测试中直接构造 `xstrview`，不要为了测试便利给 Regex 引入 String 功能依赖。

## 3. xws

### 3.1 Origin：存在高级入口策略缺口，但不是底层协议高危漏洞

位置：`extlibs/xws/src/websocket/server.c:422-612`、`server_router.c:210-221`、`include/xrt/websocket_http.h:97-106`

报告正确指出现有服务器配置没有 Origin 策略，固定路由入口也没有授权回调。但需要修正两点：

1. WebSocket 协议层不能无条件拒绝缺少 Origin 的请求，原生客户端通常不发送 Origin。
2. 分阶段低级 API 允许调用方在 `xrtWsServerCheck` 和 Accept 之间检查原始 HTTP 请求，因此并非“应用无法实现 Origin 校验”。真正缺口位于开箱即用的 router/server 便利层。

建议分层实现：

- `xrtWsServerCheck` 继续只负责协议有效性，不绑定浏览器安全策略。
- 增加无分配 Origin 解析、规范化和 same-origin 比较 helper。
- router/server config 增加 `ANY`、`SAME_HOST_OR_ABSENT`、`ALLOWLIST`、`CALLBACK` 四种策略。
- Web 服务器便利入口默认 `SAME_HOST_OR_ABSENT`：浏览器提交 Origin 时必须匹配 Host，原生客户端缺省 Origin 仍可工作；`Origin: null` 默认拒绝。
- 授权回调接收完整握手视图，可同时处理 Cookie、Bearer、子协议和业务 ACL。

`OPTIONS 204` 本身不是 CORS 授权，也不会让浏览器绕过 WebSocket Origin 规则；应删除或收紧无意义的自动 OPTIONS 行为，但不应把它当作漏洞根因。

### 3.2 P1：Close 后仍可能投递数据，确认属实

位置：`connection.c:1822`、`connection.c:2106-2165`

收到 Close 后连接进入 `CLOSING`，读取驱动仍可继续处理同一接收批次中的后续帧，而 `FrameBegin` 没有用 `CloseReceived` 阻止新的应用数据事件。这是协议事件顺序错误；如果应用在 Close 回调后释放会话状态，会放大为应用层生命周期问题。

修复原则：

- Close 帧完成后立即停止应用数据帧投递。
- 丢弃同一缓冲区中 Close 后的字节，不再触发 Message/Frame 回调。
- 只保留发送 Close 响应、排空必要控制输出和关闭传输的状态机动作。
- 增加 `Close + Text`、`Close + fragmented data`、TLS、permessage-deflate 四组同包测试。

### 3.3 压缩 Writer 遇普通 AGAIN 即杀连接，不成立

位置：`connection.c:3896-3913`、`connection.c:4011-4107`，契约位于 `include/xrt/websocket_runtime.h:531-536`

当前实现会在推进 deflater 之前完成长度、预算和发送容量预检。普通容量不足会返回可重试状态，且不会推进 Writer；只有预检之后出现编码故障或违反提交不变量的传输故障才会终止连接。此时压缩上下文已经改变，终止是合理的防御行为。

没有找到报告所称“正常瞬时背压进入 Abort”的可达路径。建议增加故障注入断言，证明预检与提交之间不会产生正常 AGAIN；在没有复现前，不应为 deflater 增加昂贵的中途快照/回滚机制。

### 3.4 关闭定时器失败路径 UAF，不成立

位置：`connection.c:1304-1331`、调用方 `connection.c:1349-1417`

定时器路径自己持有一个引用，调用方在整个关闭操作期间另持有一个强引用。调度失败后释放定时器引用不会销毁调用方仍在使用的对象，因此不存在报告描述的 UAF。

可以把错误捕获和记录移到释放前，让持有关系更容易审计，但这是可读性改进，不是安全修复。

### 3.5 其余低危项核验

- 创建失败漏 `xrtSpinUnit`：属生命周期对称性问题；当前 `SpinUnit` 只清 magic，不释放系统资源。建议补齐清理，但不是资源泄漏。
- `Rejected++` 无锁：操作提交完成前对象尚未发布，未发现并发读取该字段的路径，数据竞争结论不成立；可移入锁内以保持统一不变量。
- Pause 回退：独立影响较低；修复 Close 后投递后风险基本消失。建议进入关闭态后不再撤销 pause。
- 忽略 `xrtNetBufInit(&Buffer, NULL)`：对非空栈对象调用不会失败，报告结论不成立。
- 客户端 URL 未拒绝 userinfo：属实。WebSocket URI 不应接受 userinfo，应与 fragment 一起在配置阶段拒绝。

## 4. xmail

### 4.1 `verify_peer:false` 不是当前树内实现的高危缺陷

现代实现位于 `extlibs/xmail/src/{smtp,pop3,imap,transport}`，`mail_net.c:127-136` 对安全传输模式要求非空 TLS verifier。报告引用的 `extlibs/xmail/xmail_xlang.c`、`xmail.h` 和 `build_test.bat` 依赖已经不存在的 `../xsmtp`、`../xpop3`、`../ximap`、`../xmail_mime` 与 `../../singlehead/xrt.h`，也不在 `modules.json` 的 source/test 清单中。

因此：

- `verify_peer:false` 和全局 JSON 返回缓冲的问题在遗留文件中确实存在。
- 它们不是当前 xmail 构建产物中的可达缺陷，更不是现代实现的 TLS 关闭路径。
- `modules.json:21-22` 将 xlang 标记为 `integrated` 与当前源码事实不符。

发布前应把根目录遗留绑定和旧测试整体移入 `dev/archive/mail_legacy_20260816` 或删除，并把 xlang 集成状态改为 `pending`。未来重写绑定时直接面向现代 xmail API；不迁移模糊的 `verify_peer:false`，若确需不安全模式，应使用名称明显、只能显式开启的独立配置。

### 4.2 STARTTLS 读取未初始化 Reply，不成立

- SMTP 在 `smtp_client.c:402` 明确对 `Reply` 执行 `memset`。
- POP3 在进入 STLS 分支前已经成功读取服务器 greeting 到同一个 `Reply`，所以失败分支读取的是已定义的前一响应，不是未初始化内存。

POP3 复用 greeting 作为失败分支状态虽然不够直观，但原始命令错误已经存在。可以在发出 STLS 前重置 Reply，改善诊断确定性；不应把它列为 UB 修复。

### 4.3 MIME 与邮件互操作：属于功能补充，不是当前内存安全缺陷

- RFC 2231 低级 API 有意返回解码字节、Charset 和 Language 的借用视图，不承诺转码。应保留这个无损底层接口，另加 `DecodeUtf8` 高级 helper 和严格/宽松策略。
- RFC 2047 高级解码当前只接受 UTF-8 与 ASCII，确实影响旧邮件互操作。应接入 XRT 字符集转换器或可注入转换回调，优先覆盖 ISO-8859-1、Windows-1252、GB18030、Big5、Shift_JIS 等常见集合。
- 报告中的 RFC 2047 `charset*lang` 结论混淆了 RFC 2231 参数语法，不应据此修改 encoded-word 语法。
- IMAP literal 低层是流式、无拥有分配的协议层，不应强加 64MB 固定上限；拥有式/聚合 helper 应带可配置总预算。
- obs-zone、两位年份、重复 singleton 头和 64KB 响应行属于严格模式与现实邮件兼容之间的策略。建议保留严格模式，再增加 relaxed 模式和诊断标志。
- `arrDecoded[64]` 在当前 RFC 2047 长度限制下没有确认溢出，但容量与协议上限存在隐式耦合。应改为由协议常量推导，并加静态断言。

## 5. xssh

审计报告“零高中危”的总体结论成立。五个低危项中，只有两项值得进入改进清单：

1. Channel 被 remove/discard 时，应主动通知 future 管理器，将引用该 channel/local-id 的 waiter 完成为 CLOSED 或 CANCELLED，不能让它一直等到整个连接关闭。当前匹配使用短路和 ID，不构成 UAF，但存在等待存活期契约缺口。
2. SSH wire 协议中的端口字段本来就是 `uint32`，低级编解码不应收窄；TCP 高级转发 helper 应拒绝大于 65535 的端口，同时保留 0 表示动态分配。

其余结论调整如下：

- known_hosts 对同一主机保留多个算法密钥是合法模型，`CHANGED` 只比较同算法不能直接认定为缺陷。
- ChannelOpen failure reason 已从 `ssh_client.c:828-831` 进入 notice，并由 `ssh_client_future.c:712` 写入错误，报告所称“吞错误码”不成立；可继续补充 description/language 作为可观测性增强。
- 收到无对应请求的 reply 时严格断开连接是有效协议策略，不应改成静默忽略。

## 6. xruntime

### 6.1 P1：空 Value 槽追踪失败，确认属实

位置：`extlibs/xruntime/src/runtime/runtime_value.c:1217-1229`

空 `xvalue` 是合法的未设置状态，追踪它应该成功且不产生子边。当前代码返回 `(pOwned != NULL) && trace(...)`，导致空槽让整个对象图追踪失败。外层会设置泛化错误，因此报告中的“完全不设错”表述不精确，但真正根因被丢失。

应改为：

```c
if (pOwned == NULL)
    return true;

return xrtValueGraphTrace(...);
```

并加入对象包含空 Value 字段、空/非空混合字段和循环图三类测试。

### 6.2 祖先数组和迭代终态：建议防御性加固，当前严重度被高估

- `runtime_field.c:293-296`、`351-354` 的局部数组填充没有本地边界判断，但两个公开入口在此前已经通过 `xrtTypeValidate` 或有界计数验证限制了深度和环。当前没有可达溢出路径。仍建议补本地边界检查，防止未来重排调用顺序后失守。
- `typed_value.c:1038/1376/1723/1997` 没有在迭代结束后区分 EOF 与错误。不过这些调用使用本地、稳定、无重叠的快照迭代器，正常状态下未发现中途失败路径。若直接读取 TLS 全局错误，还会受调用前残留错误影响。长期应给迭代器增加显式 status API，再由四个构造器统一检查；当前按 P2 健壮性处理。

## 7. xhttp

### 7.1 `ALLOW_UNVERIFIED_DOMAIN` 不是隐藏 footgun

`XCOOKIE_JAR_ALLOW_UNVERIFIED_DOMAIN` 的公开注释明确说明：没有 PSL 回调时也允许 Domain Cookie。默认 `Flags = 0`，实现于 `cookie_jar.c:1013-1023` 严格 fail-closed。开启 flag 后再因为缺少 PSL 回调拒绝，会让这个显式 escape hatch 完全失去意义。

结合 XRT 的灵活性原则，建议保留该能力，但改名为更直接的 `XCOOKIE_JAR_ALLOW_DOMAIN_WITHOUT_PSL`，文档在配置点明确标出 supercookie 风险。若决定彻底禁止，则应删除 flag，而不是让 flag 名义开启、实际无效。

### 7.2 其余条目

- Cookie 请求上下文默认 same-site 是普通 HTTP 客户端 helper 的明确默认值，不是浏览器上下文推断。文档应强调跨站模拟必须显式传入 site context。
- Multipart 完整缓冲解析器接收调用方已有缓冲；流式 parser 本身不分配并维护计数。默认 `SIZE_MAX` 不是该层的内存放大漏洞。客户端/服务器上游 body budget 仍必须保留。
- `RFC 10025` 是贯穿 Cookie 头文件、源码、文档和生成单头文件的机械性错引，应统一改为 `RFC 6265`，然后重新生成 single header。

## 8. 修复批次

### 批次 A：阻断崩溃和资源失控

1. 修复 xregex bitmap 指针、字数计算、清零字节数和无符号移位。
2. 修复 save-slot 容量单位、增长算法和乘法溢出。
3. 在解析期拒绝 `{m,n}` 中 `m > n`。
4. 增加 31/32/33/63/64/65 模式集合测试、捕获槽内存预算测试、Debug/Release 非法量词测试。

### 批次 B：协议和对象生命周期契约

1. xws 收到 Close 后停止所有应用数据事件，并覆盖同包尾随帧。
2. xws router 增加 Origin/授权策略；低级 handshake checker 保持协议中立。
3. xruntime 空 Value trace 返回成功。
4. xssh channel remove/discard 主动结束相关 future。

### 批次 C：边界、裁剪和互操作

1. 修复 xregex flag、计数和字符类边界，并修正 OOM 测试裁剪闭包。
2. 清理 xmail 根目录遗留实现并纠正 xlang 集成状态。
3. 为 xmail 增加可选 UTF-8 转码 helper、relaxed 解析和拥有式预算。
4. 拒绝 WebSocket URL userinfo；补压缩 Writer 故障注入测试。
5. 补 runtime 局部边界防御和显式迭代终态。
6. 修正 xhttp RFC 编号与 Cookie 策略文档。

## 9. Release gate

修复完成后至少需要通过：

- Windows GCC/MSVC Debug 与 Release：六个扩展库 manifest 的模块测试、裁剪测试和单头文件测试。
- WSL GCC/Clang Debug 与 Release：同一组协议和边界用例。
- ASan/UBSan：xregex 集合、量词、捕获；xws Close 尾随数据；xmail MIME 变异；xruntime 对象图；xssh channel/future 生命周期。
- OOM/fault injection：xregex slot growth、xws 压缩预检/提交边界、xmail 字符集输出、xruntime Value graph。
- 协议定向：WebSocket Origin 策略矩阵、缺失 Origin 的原生客户端、Close+data 同包、TLS/deflate 组合。
- 构建边界：每个修复模块独立 manifest closure，不允许测试通过聚合 suite 偶然获得未声明依赖。

## 10. 本次验证记录

以下 Windows GCC x64 定向套件通过：

- xregex：`regex_set` 常规集合测试通过；32 模式定向复现发生 `0xC0000005`。
- xws：`websocket_connection/test_connection_protocol.c`、`websocket_writer_deflate/test_writer_deflate.c`、`websocket_server_router_tests/test_server_router.c` 通过。
- xmail：`mail_word`、`mail_param`、`smtp_client_tls_runtime_tests`、`pop3_client_tls_runtime_tests` 通过。
- xruntime：`runtime_value_trace`、`runtime_field`、`typed_value` 通过。
- xhttp：`cookie_jar`、`multipart` 通过。
- xssh：`ssh_client_future`、`ssh_known_host_db`、`ssh_client_forward` 通过。

这些回归证明普通路径基线健康，但不覆盖本报告新增的定向边界；因此不能用“现有测试全过”否定源码中已确认的可达问题。
