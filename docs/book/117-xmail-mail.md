---
num: 117
slug: xmail-mail
title: 邮件消息与 MIME
volume: 卷十一 其他扩展库
type: practice
lead: 消息视图、CRLF 纪律、multipart 游标、编码词与传输编码——邮件数据的全部原语，零树零复制。
api: xmail-mail, xmail-mail_message, xmail-mail_multipart
---

## 导读

xmail 系列开篇：**数据层**——邮件是什么、怎么读。设计立场贯穿全部模块：**不建对象树、不复制报文、不强迫 builder**——`xrtMailMessageParse` 产借用视图（字段数组+正文边界），multipart 是流式游标，编码词是"边界解析+可插拔字符集"。具体四块：**核心**（`mail_core`：`xrt.mail` 错误域、CRLF 规范化、边界语法谓词）；**消息视图**（`mail_message`：严格 CRLF 解析+传输编码解码正文）；**multipart**（`mail_multipart`：零分配 MIME 游标+三标记构建）；**编码词与编码**（`mail_word`：RFC 2047 的 `=?utf-8?B?...?=` 词、`mail_codec`：QP/Base64 的 MIME 行宽版）。三个协议章（SMTP/POP3/IMAP）与组合章全部建立在这层。

## 引入

邮件是人类网络上最古老的"文档格式"——1982 年的 RFC 822 血统加上 1996 年的 MIME 扩展，四十年的兼容包袱都在字段区里：**CRLF 是铁律**（裸 LF 的报文在很多服务器上直接拒绝）、**编码词**（`Subject: =?utf-8?B?...?=`——非 ASCII 在 ASCII 头里的转义）、**multipart**（boundary 分隔的部件树——附件的结构）、**传输编码**（QP/Base64——二进制在 ASCII 通道里的形态）。每一条都有历史实现的宽容/严格分歧，而宽容正是注入的温床（头字段注入是邮件伪造的经典路径）。

xmail 的答案是**严格解析+零树视图**：CRLF 严格（`MailCrlfWrite` 规范化、解析器要求严格 CRLF）、编码词严格（拒绝空字符集/非法标记/控制字节/超 75 字节——注入字节当场拒）、multipart 严格（只接受**物理行首完整匹配**的 boundary——拒绝裸换行伪造）、视图零复制（借用输入，"高级对象树不会堵住原始报文路径"——要树自己组，要原始字节永远直通）。

## 概念

### 核心：错误域、CRLF 与边界

`mail_core` 提供三件基础设施：`xrt.mail` 稳定错误域（配置/线路/字段/编码/字符集/地址/MIME/协议/限制各有独立的稳定错误码；底层 XRT Base64/网络错误**原样传播**——最具体原因直达调用方）；`xrtMailCrlfWrite/Crlf`（CRLF 规范化——两段式/分配式双形态，明确长度**保留零字节**、重叠拒绝）；`xrtMailBoundaryValid`（不依赖随机数/容器/解析器的**纯语法谓词**——70 字节上限的边界合法性；随机生成在独立 `mail_id` 模块）。

### 消息视图：解析与正文解码

```diagram flow
- 解析：MailMessageParse(报文, 头预算, 头数预算, &视图)
  ——字段区严格 CRLF、必须空行分隔；视图全部借用输入
- 字段：MailMessageHeader(名, 序号)——ASCII 大小写不敏感；重复字段保留不合并（Received 合法重复）
- 传输编码：MailMessageTransfer——唯一 Content-Transfer-Encoding；
  缺失=7BIT、重复或未知=错误
- 正文：MailMessageBody(Write)——7bit/8bit/binary 原样；QP/Base64 解码（复用 mail_codec）
```

预算默认（`HEADER_BYTES_DEFAULT/HEADERS_DEFAULT`）防恶意超大头区；`SIZE_MAX` 显式取消——预算是显式决策不是默认无限。**不自动解释字符集/Content-Type/multipart**——这三样由 `mail_word`/`mail_param`/`mail_multipart` 继续（组合自由：你只要正文就停在这里，要 MIME 结构就再走一层）。

### multipart：流式游标与三标记

读侧：`xmailmultipartcursor` 保存借用 Source/Boundary/Preamble/Epilogue 与推进位置；`MultipartNext` 逐 part 产 `xmailmultipartview`（Headers/Body 借用原输入）。**严格性**：只接受物理行首**完整匹配**的 boundary（`--b\r\n`）；拒绝裸换行、非法 part 字段、缺失关闭分隔线、预算溢出。**不解码 part 正文**——每 part 的传输编码各自声明各自解（消息视图按 part 复用）。写侧：`xmailmultipartmark` 三标记（FIRST/NEXT/CLOSE）经 `MultipartMarkWrite` 产可直接发送的分隔片段——**调用方把字段和正文直接写网络流，避免总报文拼接**（大附件不占内存——第 107 章流式正文的邮件版）。

### 编码词：RFC 2047 的边界与字符集策略

`=?charset?B/Q?text?=` 形态的编码词是 ASCII 头里非 ASCII 的通道。三层设计：**视图层**（`MailWordParse`：零分配读边界——Charset/Language/Encoding/正文视图；RFC 2231 的 `charset*language` 拆开）——接入 xmail 未内置字符集的**插槽**（GB18030/Big5/Shift_JIS 等大映射表不进默认构建：取边界+原始正文、应用自带转换器）；**编码**（`WordEncodeWrite`：UTF-8 输入——纯 ASCII 原样、非 ASCII 或可误识别 `=?` 的编成 ≤75 字节词、**只在 UTF-8 标量边界分片**；Base64 默认/Q 编码 phrase 安全集）；**解码**（`WordDecodeWrite`：混合文本+编码词；相邻可解码词间空白/折叠忽略、其余折叠规范一空格；严格模式内置 UTF-8/ASCII/Latin-1/CP1252 别名并**拒绝可注入的控制字节**；RELAXED 模式未知字符集保留原文不猜）。

### 传输编码：QP 与 Base64 的 MIME 版

`mail_codec` 在 XRT 核心（第 28 章）之上的 MIME 特化：**QP**（`QpWrite`——行宽默认 76、软换行前预留 `=`、行尾空格/制表符始终编码；BINARY 模式 CR/LF 当普通字节、TEXT 模式统一 CRLF；解码严格拒绝残缺转义、支持同址收缩）；**Base64**（`Base64Write`——按最多 57 字节一组直调核心**不建完整副本**、每行 CRLF 结束、行宽 4..76 的 4 倍数；解码只额外允许 MIME 空白、拒绝非规范填充与非零尾位）。

## 示例

### 第一个完整程序：解析与正文解码

下面的程序来自 `examples/mail/message`——最小邮件的读取闭环：

```embed path="extlibs/xmail/examples/mail/message/main.c" title="extlibs/xmail/examples/mail/message/main.c"
```

```term
$ gcc -O1 -DXRT_MODULE_ALL -I extlibs/xmail/single -include xmail.h impl.c extlibs/xmail/examples/mail/message/main.c -lws2_32 -liphlpapi
headers=2 body=hello
```

**刚才发生了什么。** ① 输入是三行头+空行+base64 正文的完整报文——`MailMessageParse` 一次解析：`HeaderCount=2`、视图借用输入（零复制零分配）。② `MailMessageTransfer` 读出唯一编码声明 BASE64；`MailMessageBody` 解码 `aGVsbG8=` 为 `hello`——**按报文声明解码**而不是猜。③ 两行输出就是数据层的最小闭环：结构（头数）+内容（解码正文）。要继续？`MailMessageHeader` 查字段、multipart 游标拆部件、编码词解 Subject——每一步都是显式调用，没有隐藏的"自动全部解析"。

### 第二个完整程序：multipart 游标

第二个程序来自 `examples/mail/multipart`——部件遍历：

```embed path="extlibs/xmail/examples/mail/multipart/main.c" title="extlibs/xmail/examples/mail/multipart/main.c"
```

```term
$ gcc -O1 -DXRT_MODULE_ALL -I extlibs/xmail/single -include xmail.h impl.c extlibs/xmail/examples/mail/multipart/main.c -lws2_32 -liphlpapi
body=hello
```

**刚才发生了什么。** ① 输入 `--b\r\nContent-Type: text/plain\r\n\r\nhello\r\n--b--\r\n`：boundary `b`、一个 part（头+正文）、关闭分隔线。② `MultipartCursorInit` 绑定源与边界（零预算取默认 part 上限）→ `MultipartNext` 产首个 part——`Part.Body` 借用原输入的 `hello`。③ 循环形态与 DER/HTTP 帧/查询串的游标完全同构（Init→Next→ITEM/END）——第 N 次见到"偏移迭代、借用视图"模式。真实部件树=外层消息视图+multipart 游标+每 part 递归（multipart 里嵌 multipart 是附件内嵌邮件的形态）——递归深度是你显式的循环，不是库的自动树。

## 契约

- **严格 CRLF**：解析要求严格 CRLF；`CrlfWrite` 两段式（重叠拒绝/容量原子）；明确长度保留零字节。
- **预算显式**：头字节/头数/part 数默认有限；`SIZE_MAX` 是显式取消不是缺省无限。
- **视图借用**：消息/部件/编码词视图全部借用输入——输入存活期约束（第 15 章通则）。
- **重复字段保留**：按序查找不合并——Received 等合法重复原样可见。
- **传输编码唯一**：缺失=7BIT；重复/未知=错误（不猜）。
- **multipart 严格**：物理行首完整匹配；拒裸换行/非法字段/缺关闭线/超预算；不解码 part 正文。
- **流式构建**：三标记产分隔片段——字段正文直写网络流，无总报文拼接。
- **编码词三层**：Parse（零分配边界+字符集插槽）/Encode（UTF-8 输入、标量边界分片、≤75B）/Decode（严格拒注入字节；RELAXED 保留不猜）。
- **大字符集不内置**：GB18030/Big5/Shift_JIS 经视图层接应用转换器——不拖映射表。
- **QP/Base64**：MIME 行宽版；57 字节分组直调核心；严格解码（同址支持）；错误原样传播。
- **边界谓词**：`BoundaryValid` 纯语法、随机生成独立模块。

## 避坑

### 坑 1：拿视图出了输入作用域

症状：解码后的部件正文偶发乱码崩溃——报文缓冲是局部变量，视图带出去了。

原因：全部视图借用输入（本章第三次强调）——报文的生命周期决定视图的生命周期。要长期保存就复制（`MailMessageBody` 的分配式入口已经是复制形态）。

```c bad
bytes load_and_parse(cstr sRaw) {
	xmailmessageview V;
	xrtMailMessageParse(sRaw, 0, 0, &V);
	return V 的某个视图;   /* 报文在栈上：返回即悬空 */
}
```

```c good
/* 谁持有报文谁持有视图——结构体绑两者 */
typedef struct { str sRaw; xmailmessageview View; } loadedmail;
bool load(cstr sPath, loadedmail* pOut) {
	pOut->sRaw = read_file(sPath);
	return xrtMailMessageParse(pOut->sRaw, 0, 0, &pOut->View);
}
```

### 坑 2：组装邮件忘了 CRLF（发裸 LF）

症状：本地测试通过、真实服务器拒收或头字段被并入正文——严格服务器把裸 LF 当非法。

原因：邮件线路是 CRLF 铁律；你的 `"\n"` 在本地宽容解析下"碰巧"能过。规范在发送前过 `MailCrlfWrite`。

```c bad
snprintf(Buf, "Subject: hi\n\nbody\n");   /* 裸 LF */
send(Buf);
```

```c good
/* 生成后规范化（或直接全用 \r\n 写） */
xrtMailCrlfWrite(Normalized, sizeof(Normalized), Raw, RawSize, &n);
send(Normalized, n);
```

### 坑 3：boundary 出现在正文里（分隔符碰撞）

症状：附件内容恰好含 `--boundary` 行——部件被"劈开"，解析结果错乱。

原因：boundary 的唯一性是构建方的责任（RFC 的 70 字节上限就是为了随机不碰撞）。固定短 boundary（如示例的 `b`）只能测试用——生产用 `mail_id` 的随机生成+`BoundaryValid` 校验。

```c bad
#define BOUNDARY "b"   /* 固定短值：附件含 "--b" 即碰撞 */
```

```c good
char Boundary[XMAIL_BOUNDARY_MAX + 1];
mail_boundary_random(Boundary);   /* mail_id 随机生成 */
assert(xrtMailBoundaryValid(view(Boundary)));
```

## 练习

### 基础：解码矩阵

构造四种传输编码（7bit/8bit/QP/Base64）的报文，逐一解析+解码并对比。验收标准：四种正文往返一致；重复编码字段被拒。

### 进阶：两部件邮件遍历

text/plain + text/html 的 multipart 报文：外层消息视图→multipart 游标→每 part 的类型字段（`mail_param`）与正文各自解码。验收标准：两部件类型与正文正确；关闭分隔线缺失的变体被拒。

### 挑战：编码词往返

含中文 Subject+发件人名的邮件：`WordEncode`（UTF-8 输入）→ 组装 → 解析 → `WordDecode` 往返。多词长 Subject（超 75 字节分片）。验收标准：往返逐字节一致；分片点在 UTF-8 标量边界（无切断字符）；RELAXED 模式对未知字符集词保留原文。

## 速查

| 知识点 | 速查 |
| --- | --- |
| 设计立场 | 零树零复制零 builder——视图借用、树自己组、原始报文直通 |
| 核心 | xrt.mail 错误域/CRLF 规范化（保留零字节）/边界纯语法谓词 |
| 消息视图 | 严格 CRLF+空行分隔；头按序查不合并；预算显式 |
| 传输编码 | 唯一声明；缺失=7BIT；QP/Base64 复用 codec |
| multipart | 游标借视图；物理行首完整匹配；三标记流式构建 |
| 编码词 | Parse（字符集插槽）/Encode（标量边界 ≤75B）/Decode（拒注入） |
| 大字符集 | 视图层接应用转换器——映射表不进默认构建 |
| QP/Base64 | MIME 行宽；57 字节分组直调；严格解码同址支持 |
| 错误传播 | 底层 Base64/网络错误原样——最具体原因直达 |
| 递归结构 | 部件树的深度是显式循环不是自动树 |
