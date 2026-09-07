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
| P6–P27 | ⬜ 未开始 | — | — |

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
| 55 | 59-net-tcp.md | practice | 4,078 | 2 | 2 | 3 | 3 | P0 |

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
