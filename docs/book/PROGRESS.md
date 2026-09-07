# PROGRESS — 教程重写进度记录

> 每阶段开始/结束时更新本文件；上下文被压缩后从这里恢复现场。

## 状态总览

| 阶段 | 状态 | 完成章 | 讲解字数 |
| --- | --- | --- | --- |
| P0 基础设施 + 金标准 | ✅ 完成 | 2 | 10,093 |
| P1 卷一 起步与核心 | ✅ 完成 | 8 | 34,150 |
| P2 卷二全部 + 卷三前半 | ✅ 完成 | 7 | 28,790 |
| P3 卷三后半 + 选型章 | ✅ 完成 | 7 | 27,804 |
| P4 卷四前半 | ✅ 完成 | 6 | 22,441 |
| P5 卷四后半 | ✅ 完成 | 7 | 30,235 |
| P6 卷五前半 | ✅ 完成 | 6 | 24,273 |
| P7 卷五后半 | ✅ 完成 | 8 | 34,240 |
| P8 卷六前半 | ✅ 完成 | 8 | 32,911 |
| P9 卷六后半 | ✅ 完成 | 2 | 11,210 |
| P10 卷七前半 | 🔄 进行中 | 0 | — |
| P11–P27 | ⬜ 未开始 | — | — |

## 章节明细

| 章 | 文件 | 类型 | 字数 | 程序 | 图示 | 坑 | 练习 | 阶段 |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| 1 | 01-vol1-intro.md | intro | 2,050 | 0 | 1 | 0 | 0 | P1 |
| 2 | 02-intro.md | practice | 4,054 | 2 | 1 | 2 | 3 | P1 |
| 3 | 03-first.md | concept | 6,001 | 2 | 2 | 3 | 3 | P1 |
| 4 | 04-error.md | concept | 6,015 | 2 | 2 | 3 | 3 | P0 |
| 5 | 05-memory.md | practice | 4,002 | 2 | 1 | 2 | 3 | P1 |
| 6 | 06-memory-debug.md | practice | 4,018 | 2 | 1 | 2 | 3 | P1 |
| 7 | 07-temp.md | practice | 4,007 | 2 | 1 | 2 | 3 | P1 |
| 8 | 08-core-trim.md | practice | 4,003 | 2 | 1 | 2 | 3 | P1 |
| 10 | 10-math.md | practice | 4,250 | 2 | 1 | 2 | 3 | P2 |
| 11 | 11-random.md | practice | 4,079 | 2 | 1 | 2 | 3 | P2 |
| 12 | 12-hash-xid.md | practice | 4,086 | 3 | 1 | 2 | 3 | P2 |
| 13 | 13-array.md | practice | 4,300 | 2 | 1 | 2 | 3 | P2 |
| 14 | 14-buffer.md | practice | 4,035 | 2 | 1 | 2 | 3 | P2 |
| 15 | 15-stack.md | practice | 4,003 | 2 | 1 | 2 | 3 | P2 |
| 16 | 16-list-slotmap.md | practice | 4,037 | 2 | 1 | 2 | 3 | P2 |
| 55 | 66-net-tcp.md | practice | 4,078 | 2 | 2 | 3 | 3 | P0 |

## P9 阶段记录

- P8 吸收了原 P9 计划中的 channel/future/executor 三章，P9 实际执行：ch59 取消与结构化
  并发重写 + 新增 ch60 调度器实战组合章（卷六收官）。卷六 11 章成型（50-60）。
- 全书第八次重编号（52 文件，ch60 之后 +1）。
- 门禁修正：WaitCancel→WaitUntilCancel 真名、xrtChannel 裸词改 Create 形态。
- ch60 是第三个组合章（6,208 字/4 坑/双完整程序）：三零指标（泵零阻塞/池零等待/
  消息零锁）、装配三决策、停机双通道、五项检查清单、卷七映射预告、三级迭代路径。

## P8 阶段记录

- 卷六前半 8 章（插入卷六导言 ch50 与取消体系专章 ch53，全书第七次重编号 60 文件）。
- 门禁拦下并修正：xrtThreadWait 无出参（返回 xwaitresult、值经原子/Future 回传）、
  取消语义精确化（Requested 含祖先链查询/Watch 至多同步一次/Triggered 查监听——三 API 分工）、
  调度器族全面重写（SchedCreate→CoSchedCreate + CoGo/Post + Run/Poll/Step + Sleep/Park/Wake——
  泵族三形态按头文件实际划分）、Future 三态→四态（RESOLVED/FAILED/CANCELLED/CLOSED——
  Reject/Resolve/Close 三入口）、xchannelresult TrySend、
  Spawn 签名(配置)、Line 返回 uint32、xson 品牌词。
- 卷六主线确立：取消贯穿（53 立地基→58 各章方言）+ 三层装配线（原语/协作/结构化）。

## P7 阶段记录

- 卷五后半 8 章（P6 预留的导言位 ch36 本章写就；signal 从卷六归位卷五 ch48；
  新增调试组合章 ch49 收官）。全书第六次重编号（61 文件），卷五 14 章成型。
- 门禁拦下：IsSafe→IsSafeEntry(视图+bDirectory)、xrtPathJoin varargs→两段 cstr、
  锁两档(Lock/LockRange)签名修正、xrtTempFile→xrtFileTemp、异步模型全面重写
  （回调制→任务池+Future 真实形态：AsyncFileOpen/ReadAt/WriteAt）、
  xrtSignalWatch→On/Once 族、VisitLive 返回值、xmemstats 无 CurrentBytes
  （改用 MallocBytes-FreeBytes）。
- 组合章型第二次落地（ch49，6,098 字/4 坑）：体检与尸检同构、观测经济学、
  诊断剧本、反模式警示（观测不能替代设计）。

## P6 阶段记录

- 卷五前半 6 章（插入卷五导言 ch36，全书第五次重编号 72 文件；导言本身属 P7 批次待写——本阶段 6 章指 logger×2/console/io/time/env）。
- 门禁拦下：xrtIoLineReader→xlinereader+Next 三态、env 三条路径实为两条（Get/Lookup，
  GetDefault 不在头文件）、xrtTimeAdd 签名出参序、printf 范例真实输出对齐、
  SplitAt 参数序修正。

## P5 阶段记录

- 卷四后半 7 章：regex 归位卷四（原卷十一 ch88 → ch30）、value/json/xson/template 重写、
  新增组合章 ch35（composition 型首章：6000 字/2 程序/5 坑）。
- 全书第四次重编号（77 文件）：regex 前插 + text-pipeline 尾插，卷四扩为 12 章。
- 门禁拦下：xrtValueRef→xrtValueRetain、xrtValueObjectNew→xrtValueObject、
  FindAdvance→Next、StringSub→String、模板 {+%}→真实语法、xson 补品牌词。

## P4 阶段记录

- 卷四前半 6 章（插入卷四导言 ch24，全书第三次重编号，82 文件位移）。
- 门禁拦下并修正：xrtStrStartsWith→xrtStrStarts、xrtStrMatch→xrtStrGlob、
  XRT_ENCODING_*→XENCODING_*、XUTF_IGNORE 不存在（仅 STRICT/REPLACE）、
  BOM 是 bool 参数非标志、xrtBase64EncodeUrl→字母表参数、xrtTimeNow→xrtNow。
- 经验沉淀：diagram 围栏内容不计讲解字数——扩字数必须改正文段落。
- stream_tour 示例预期输出有第 4 行（config corrupt rejected），term 已对齐。

## P3 阶段记录

- 卷三扩为 11 章（插入卷三导言 ch13、容器选型章 ch23）；全书第二次重编号（91 文件）。
- 行文章号引用按映射脚本统一迁移（13..21 → +1，≥22 → +2）。
- 修复 renumber_book.py 的 md 匹配缺陷（后缀 glob → 精确旧文件名），避免 core-trim 被误改名。
- 批次：ch18-20（映射/集合/AVL）、ch21-22（队列/池）、ch13+ch23（导言/选型收官）。

## P2 阶段记录

- 批次 1（卷二）：ch10 数学 / ch11 随机 / ch12 哈希与 XID；顺带修复仓库示例
  examples/hash/variants/main.c 预期输出注释缺第三行的问题。
- 批次 2（卷三前半）：ch13 数组 / ch14 缓冲 / ch15 栈族 / ch16 链表与 slot_map。
- 本阶段无新增章节，无重编号；门禁全绿后双仓库各两次提交。
- checker 白名单补充：家族前缀 xrtMath、产品名 XID（BRAND_TOKENS）。
- 经验沉淀：初稿字数普遍落在目标的 70-80%，需按 1.3 倍余量起稿。

## 遗留项

- ch82-xregex 旧重定向页仍指向 ch85-regex，属 P5 卷四 regex 归位时处理。
