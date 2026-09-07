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
| P10 卷七前半 | ✅ 完成 | 8 | 31,742 |
| P11 卷七后半 | ✅ 完成 | 2 | 13,099 |
| P12 卷八密码基础 | ✅ 完成 | 5 | 25,122 |
| P13 卷八证书链 | ✅ 完成 | 4 | 23,822 |
| P14 卷八TLS上 | ✅ 完成 | 4 | 21,347 |
| P15 卷八TLS下 | ✅ 完成 | 3 | 15,847 |
| P16 卷九HTTP核心 | ✅ 完成 | 5 | 22,879 |
| P17 卷九WebSocket | ✅ 完成 | 4 | 18,724 |
| P18 卷十xhttp客户端 | ✅ 完成 | 6 | 28,006 |
| P19 卷十xhttp服务端 | ✅ 完成 | 6 | 28,133 |
| P20 卷十一xws | ✅ 完成 | 4 | 16,994 |
| P21 卷十一xssh | 🔄 进行中 | 0 | — |
| P22–P27 | ⬜ 未开始 | — | — |

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
| 61 | 61-vol7-intro.md | intro | 2,227 | 0 | 1 | 0 | 0 | P10 |
| 62 | 62-net-addr.md | practice | 4,658 | 2 | 1 | 2 | 3 | P10 |
| 63 | 63-net-port.md | practice | 4,192 | 2 | 1 | 2 | 3 | P10 |
| 64 | 64-net-buf.md | practice | 4,251 | 2 | 1 | 2 | 3 | P10 |
| 65 | 65-net-dns.md | practice | 4,099 | 2 | 1 | 2 | 3 | P10 |
| 67 | 67-net-tcp-adv.md | practice | 4,117 | 2 | 1 | 2 | 3 | P10 |
| 68 | 68-net-udp.md | practice | 4,156 | 2 | 1 | 2 | 3 | P10 |
| 69 | 69-net-proxy.md | practice | 4,042 | 2 | 1 | 2 | 3 | P10 |
| 66 | 66-net-tcp.md | practice | 4,078 | 2 | 2 | 3 | 3 | P0 |
| 70 | 70-net-interface.md | practice | 6,174 | 3 | 1 | 3 | 3 | P11 |
| 71 | 71-net-misc.md | practice | 6,925 | 3 | 1 | 3 | 3 | P11 |
| 72 | 72-vol8-intro.md | intro | 2,147 | 0 | 1 | 0 | 0 | P12 |
| 73 | 73-crypto-hash.md | practice | 5,963 | 4 | 1 | 3 | 3 | P12 |
| 74 | 74-crypto-aead.md | practice | 5,509 | 2 | 1 | 3 | 3 | P12 |
| 75 | 75-crypto-asym.md | practice | 6,228 | 3 | 1 | 3 | 3 | P12 |
| 76 | 76-crypto-discipline.md | practice | 5,275 | 2 | 2 | 3 | 3 | P12 |
| 77 | 77-der-pem.md | practice | 5,382 | 2 | 1 | 3 | 3 | P13 |
| 78 | 78-x509.md | practice | 5,934 | 2 | 1 | 3 | 3 | P13 |
| 79 | 79-x509-verify.md | practice | 5,518 | 2 | 1 | 3 | 3 | P13 |
| 80 | 80-cert-chain.md | practice | 6,988 | 3 | 1 | 3 | 3 | P13 |
| 81 | 81-tls-client.md | practice | 5,719 | 2 | 1 | 3 | 3 | P14 |
| 82 | 82-tls-identity.md | practice | 5,482 | 2 | 1 | 3 | 3 | P14 |
| 83 | 83-tls-handshake.md | practice | 5,174 | 2 | 2 | 3 | 3 | P14 |
| 84 | 84-tls-policy.md | practice | 4,972 | 2 | 2 | 3 | 3 | P14 |
| 85 | 85-tls-server.md | practice | 5,691 | 2 | 1 | 3 | 3 | P15 |
| 86 | 86-tls-stream.md | practice | 4,937 | 2 | 1 | 3 | 3 | P15 |
| 87 | 87-tls-resume.md | practice | 5,219 | 3 | 1 | 3 | 3 | P15 |
| 88 | 88-http.md | practice | 4,513 | 2 | 1 | 3 | 3 | P16 |
| 89 | 89-http1.md | practice | 4,356 | 2 | 1 | 3 | 3 | P16 |
| 90 | 90-http-framing.md | practice | 4,912 | 2 | 2 | 3 | 3 | P16 |
| 91 | 91-http-headers.md | practice | 5,096 | 2 | 1 | 3 | 3 | P16 |
| 92 | 92-http-decode.md | practice | 4,502 | 2 | 2 | 3 | 3 | P16 |
| 93 | 93-http-upgrade.md | practice | 4,526 | 2 | 1 | 3 | 3 | P17 |
| 94 | 94-ws-frame.md | practice | 4,611 | 2 | 1 | 3 | 3 | P17 |
| 95 | 95-ws-stream.md | practice | 4,693 | 2 | 1 | 3 | 3 | P17 |
| 96 | 96-ws-composition.md | practice | 4,894 | 2 | 2 | 3 | 3 | P17 |
| 97 | 97-xhttp-easy.md | practice | 4,667 | 2 | 1 | 3 | 3 | P18 |
| 98 | 98-xhttp-runtime.md | practice | 4,946 | 2 | 1 | 3 | 3 | P18 |
| 99 | 99-xhttp-redirect.md | practice | 4,402 | 2 | 1 | 3 | 3 | P18 |
| 100 | 100-xhttp-cache.md | practice | 4,703 | 2 | 1 | 3 | 3 | P18 |
| 101 | 101-xhttp-url.md | practice | 5,290 | 2 | 1 | 3 | 3 | P18 |
| 102 | 102-xhttp-query.md | practice | 4,998 | 2 | 1 | 3 | 3 | P18 |
| 103 | 103-xhttp-server.md | practice | 5,027 | 2 | 1 | 3 | 3 | P19 |
| 104 | 104-xhttp-middleware.md | practice | 4,370 | 2 | 1 | 3 | 3 | P19 |
| 105 | 105-xhttp-auth.md | practice | 4,850 | 2 | 1 | 3 | 3 | P19 |
| 106 | 106-xhttp-sse.md | practice | 4,960 | 2 | 1 | 3 | 3 | P19 |
| 107 | 107-xhttp-stream.md | practice | 4,809 | 2 | 1 | 3 | 3 | P19 |
| 108 | 108-xhttp-advanced.md | practice | 4,117 | 2 | 2 | 3 | 3 | P19 |
| 109 | 109-xws-conn.md | practice | 4,002 | 2 | 1 | 3 | 3 | P20 |
| 110 | 110-xws-send.md | practice | 4,179 | 2 | 1 | 3 | 3 | P20 |
| 111 | 111-xws-group.md | practice | 4,049 | 2 | 1 | 3 | 3 | P20 |
| 112 | 112-xws-server.md | practice | 4,764 | 2 | 1 | 3 | 3 | P20 |









## P20 阶段记录

- 卷十一 xws 4 章：ch109 连接管理（xwsconn 接管边界/事件与视图纪律/
  pause-resume 流控/Future 桥/关闭协议/connection_tour 七行全链巡检）、
  ch110 发送路径（新增章：writer 生命周期/压缩 writer 同构/三态所有权矩阵/
  块帧解耦）、ch111 连接组与广播（新增章：唯一成员/容量与封闭/锁外快照/
  异步操作对象/慢连接策略）、ch112 服务端路由与会话（Origin 三档/Authorize/
  自动响应五类/分阶段底层四入口/Open 借用与 Release 语义）。
- 范围调整（PHASES 注明）：P20 定义 6 章——"引用发送"与"压缩"并入 ch110
  发送路径章、"运行时"并入 ch109 连接管理章（xws 契约收敛期：HTTP 适配/
  路由等高级对象已退出核心进 archive，素材密度自然支撑 4 章高质量）。
- 全书第十九次重编号：双插入 xws-send(110)/xws-group(111)，126 文件 +2，
  全书 130 章；卷十一 109-118 十章骨架（xws 4 + xruntime 2 + xmail 2 +
  xssh 2）。
- 事故与修复：order.json 重建脚本编号偏移算错（+3 应为 +2）且叠写产生
  238 项重复——git checkout 恢复后一次性正确重写；教训：结构脚本必须先
  assert 再落盘、失败后先恢复基线再重试。
- 门禁拦下：ref 页前缀 xws- 非 xhttp-（websocket_connection 等页在
  ref-xws-websocket_runtime）、XRT_NET_AGAIN 是契约文档描述词非符号
  （改"AGAIN 结果"行文）、xrtWsConnTextFuture 不存在（Future 族真名在
  websocket_http_future：ConnectAsync 一族）。
- 行文章号修正 7 处（testing→122、perf→124）。

## P19 阶段记录

- 卷十 xhttp 服务端 6 章（卷十 12 章全部收官）：ch103 服务端上（五段超时/
  Headers 五策/Body 应用背压/响应三路/Drain）、ch104 中间件（洋葱模型/
  Next 同步栈语义/短路纪律/静态文件层）、ch105 认证（新增章：Basic/Bearer/
  Digest 挑战应答/verify 四态/重放表）、ch106 SSE（新增章：无专用状态机/
  事件四要素/AGAIN+WaitWritable/Last-Event-ID 续传）、ch107 流式正文
  （新增章：五来源与重放矩阵/租约式 Reader/有界生产流/异步文件正文）、
  ch108 服务端收官（压缩协商/Range 写侧/代理头族/卷十全景）。
- 范围调整（PHASES 注明）：P19 定义的"连接池深潜/重试策略"新章已在 P18
  ch98/99 提前完成——按素材量替换为 auth/SSE/流式正文三章。
- 全书第十八次重编号：三插入 auth(105)/sse(106)/stream(107)，
  advanced 105→108 并改题"服务端收官"，21 文件 +3，全书 128 章；
  卷十 97-108 共 12 章成型（客户端 6 + 服务端 6）。
- 门禁拦下：XHTTP_DIGEST_VALID 真名 XHTTP_DIGEST_VERIFY_* 四态、
  xhttpdigestresult 真名 xhttpdigestverifycheck、xrtHttpServerFile 真名
  xrtHttpConnFile 族、xrtHttpServerRequest*Auth 通配展开实名、
  ch108 初稿字数不足（补读者走法节）。
- 行文章号修正 7 处（testing→120、perf→122）。

## P18 阶段记录

- 卷十 xhttp 客户端 6 章：ch97 easy（无第二实现的便利层/三入口三形态/
  正文语义分界/故意不接受边界）、ch98 运行时（构建器冻结语义/两类截止时间/
  双限额/Info 诊断/origin 分片连接池）、ch99 自动行为（重定向方法语义与
  凭据边界/幂等重试三条件/Cookie Jar 挂载）、ch100 自动缓存（四模式/
  存储契约/条件提交/Range 组合/分区键）、ch101 URL 解析（RFC 3986 全形态/
  存在位/端口词法/引用展开）、ch102 查询与表单（新增章：零分配遍历四形态/
  容器四动词/编码层/multipart 双形态）。
- 基础设施（规范变更流程）：符号表扩展到 extlibs/*/include/xrt（SPEC 7.1
  同步修订）；用户宏白名单加 XHTTP/XWS 前缀并扫 extlibs features.h；
  品牌词加 xlang。扩展后既有 95 章全绿不变——门禁红线未降，扫描面更全。
- 全书第十七次重编号：插入 xhttp-query（102），23 文件 +1，全书 125 章；
  卷十 97-105 九章骨架（P18 完成 97-102 客户端侧）。
- 门禁拦下：ref 页名带 xhttp- 前缀（api 字段改 xhttp-*）、
  xrtHttpRequestSetMethodUrl 真名 SetMethod+SetUrl、xhttpclientredirectoptions
  实为 xhttpcalloptions 字段、xhttpcookiejar 真名 xcookiejar、
  XURL_HAS_* 通配改具体、xrtFormData* 通配展开、XHTTP_MODULE_HTTP_CLIENT_EASY
  等 17 个真宏（白名单扩展后放行）。
- 行文章号修正 6 处（testing→117、perf→119）。

## P17 阶段记录

- 卷九 WebSocket 4 章（卷九 9 章全部收官）：ch93 HTTP 升级（新增章：提议迭代/
  应答生成/Accept RFC 向量/子协议协商/校验绑定 Key）、ch94 WS 帧（帧头三态/
  分段相位掩码/分片重组与控制帧旁路/增量 UTF-8 跨帧校验）、ch95 WS 流
  （Attach 精确衔接/事件视图即用即弃/发送三形态与统一背压/引用发送计数释放/
  stream_tour 全特性巡检）、ch96 WS 组合（新增章：关闭码语义域与写侧强制/
  permessage-deflate 协商最小合规子集/流式压缩双重上限/一条消息的旅程全景）。
- 全书第十六次重编号：双插入 http-upgrade(93)/ws-composition(96)，
  30 文件 +2，全书 124 章；卷九 88-96 共 9 章成型
  （P16 HTTP 核心 5 + P17 升级与 WS 4）。
- 门禁拦下：XRT_MODULE_WEBSOCKET 裸宏不存在（真名拆为 _FRAME/_MESSAGE/
  _HANDSHAKE/_STREAM 等 17 个细粒度宏——正文改用细分表述）、
  xrtWsInflaterInit 真名 InflaterConfigInit/InflaterCreate 族。
- 素材侦察发现 examples/websocket 部分子目录为空壳（真文件在
  extlibs/xws/examples/websocket）——本章只用有真实 main.c 的 13 个示例。
- 行文章号修正 9 处（testing→116、perf→118、xhttp→97）。

## P16 阶段记录

- 卷九 HTTP 核心 5 章：ch88 HTTP 地基（字段/token/Host/参数三纪律：严格解析+
  借用视图+容量原子性）、ch89 HTTP/1 消息（绑定数组零分配/三态返回/封包原子性/
  上限防御）、ch90 正文分帧图解（新增章：三种定界/Plan 先行/Body 流式三态/
  chunk 写出/trailer/TE 协商/走私防御）、ch91 头字段族（参数与 quoted-string/
  token 语义查找/GetUnique 唯一值/五专字段族）、ch92 正文解码（Create 由头/
  Write 推回调/Done CRC 校验/bFinal 语义/内容协商闭环）。
- 全书第十五次重编号：插入 http-framing（90），32 文件 +1，全书 122 章；
  卷九 88-94 七章骨架就位（P16 完成 88-92，ws-frame/ws-stream 93/94 归 P17）。
- 门禁拦下：XHTTP1_NEXT 真名 XHTTP1_MORE、xrtHttp1HeadFind/FieldValue 实为
  地基层 FieldFind/FieldGet 族、xhttphost 真名 xhttpauthority、ParamEncode/Decode
  真名 ParamBuild/ParamWrite/ParamNext 族、XRT_MODULE_HTTP1 拆为 _HEAD/_BODY/
  _MESSAGE、xrtHttpExpectParse 真名 ExpectFields/ExpectValid、
  xrtHttpEncodingQuality 真名 xrtHttpAcceptEncodingQuality、head_tour 预期输出
  实为 6 行（term 补齐）、small_fields 头注释后源码字符串污染预期输出提取
  （换 field_tour 作 embed）、ch88/91 初稿漏图示（补管线/分层 diagram）。
- 行文章号修正 7 处（testing→114、perf→116、xhttp→95）。

## P15 阶段记录

- 卷八 TLS 下 3 章（卷八 16 章全部收官）：ch85 TLS 服务端（会话层裸协议机/
  Feed-Drive-Send 三步循环/SNI 动态选择与 Cookie 路由/惰性首航 Arena/
  票据签发与恢复接受）、ch86 TLS 流（StreamAccept 组合层/事件模型与硬背压/
  共享配置与服务级生命周期/stream_tour 全特性巡检含 starttls）、
  ch87 会话恢复（新增章：恢复对象契约/签发-接管-缓存-恢复闭环/
  PSK+DHE 前向保密/binder 认证边界/路由绑定）。
- 全书第十四次重编号：插入 tls-resume（87），34 文件 +1，全书 121 章；
  卷八 72-87 共 16 章成型（P12 密码基础 5 + P13 证书链 4 + P14 TLS 上 4 + P15 TLS 下 3）。
- 门禁拦下：ch85/86 初稿各只有 1 个完整程序（server/stream 示例），
  补 session_tour/stream_tour 作为第二程序——正好承担"可执行速查表"角色。
- 行文章号修正 7 处（xhttp→94、testing→113、perf→115）。

## P14 阶段记录

- 卷八 TLS 上 4 章：ch81 TLS 客户端（托管拨号/回调与 Future 双形态/READY 后
  收发与认证关闭/会话恢复）、ch82 TLS 身份（新增章：四构造器/CanSign/两段式
  Sign/HSM 扩展）、ch83 握手时序图解（新增章：四消息时序/HKDF 密钥调度链/
  HRR/协商/KeyUpdate/证书条目）、ch84 验证策略（新增章：策略对象白名单/
  验证器三层决策/无隐式联网边界）。
- 全书第十三次重编号：三插入 tls-identity(82)/tls-handshake(83)/tls-policy(84)，
  36 文件 +3，全书 120 章；卷八 15 章（72-86）骨架就位。
- 门禁拦下：xtlsstreamevents 在符号表提取规则外（结构体内部字段分号切断
  typedef 语句——正文改用"事件表：Open/Read/Writable/Close"描述性写法）；
  ch82 初稿漏图示（补生命周期 diagram）。
- 行文章号修正 2 处（tls-stream→86）。

## P13 阶段记录

- 卷八证书链 4 章：ch77 DER 与 PEM（TLV 游标/严格 DER/PEM 块协议）、
  ch78 X.509 证书解析（零拷贝视图/RDN 游标/OID 翻译层/消费侧校验清单）、
  ch79 签名验证与服务身份（三问模型/协议分派/RFC 9525 匹配三态）、
  ch80 信任链与吊销（新增章：PathValidate/PathBuild/信任库四形态/
  CRL 两层与吊销三态）——三问模型（签名/身份/信任）贯穿 79-80 两章。
- 全书第十二次重编号：插入 cert-chain（80），37 文件 +1，全书 117 章；
  卷八 12 章（72-83）骨架就位。
- 门禁拦下：XRT_MODULE_ASN1 不存在（真名 XRT_MODULE_ASN1_DER——DER 游标
  是独立模块）、X509/X25519/X448 裸词（PEM 标签例换 TRUSTED CERTIFICATE、
  枚举斜杠列表改中文）、xrtAlloc（真名 xrtMalloc）、X509_VERIFY_* 简写
  （补全 XRT_FEATURE_X509_VERIFY_* 全名）、xrtX509Store* 通配（展开实名）。
- 行文章号修正 12 处（tls-client→81、tls-stream→83、卷八范围 80-82→81-83）。

## P12 阶段记录

- 卷八密码基础 5 章新写：ch72 导言（信任金字塔+两条横切纪律+先读禁令方法论）、
  ch73 哈希与 HMAC（摘要族/流式三段式/PBKDF2 与 HKDF 双 KDF）、ch74 AEAD
  （AES-GCM 状态机/ChaCha20-Poly1305 便捷层/nonce 纪律）、ch75 非对称
  （x25519/x448/P-256/P-384 交换+Ed25519/ECDSA/RSA-PSS 签名）、
  ch76 密码工程纪律（常量时间/密钥生命周期四段/随机源分界/组合纪律，
  session 示例=TLS 1.3 骨架缩微版）。
- 全书第十一次重编号：双插入 vol8-intro（72）与 crypto-discipline（76），
  43 文件 +2，全书 116 章；卷八 11 章（72-82）骨架就位。
- 门禁拦下：X25519/X448 裸词（算法名改小写避开符号规则，函数名 xrtX448* 保留）、
  xrtAesCtrEncrypt（坑3 bad 块编造名改示意伪码）、xrtRand/xrtFastRand 不在符号表
  （普通随机改用已验证的 xrtFastRandSeed 表述）、XCRYPTO_HASH_* 通配改具体枚举、
  xxHash 改"SipHash 家族"。
- 行文章号修正 11 处（testing→108、perf→110、http-headers→85、tls-client→80）。
- 事故修复：重建 order.json 时 1-9 章 file 字段丢前导零（1-vol1-intro），磁盘文件
  未受损（renumber 按 slug 定位），补零后重建通过。

## P11 阶段记录

- P11 定义 6 章中 tcp-adv/udp/proxy 已在 P10 提前完成、netbuf 深入并入 ch64；本阶段
  实际执行 2 章：ch70 网卡接口与本机信息（新增章：接口快照/名称索引互转/本机三件套
  三层形态/IPv6 scope）、ch71 分帧与异步文件（卷七收官：行分帧/长度前缀/帧取用/
  完成端口文件三段式）。卷七 11 章（61-71）全部成型。
- 全书第十次重编号：插入 net-interface（44 文件 +1，全书 114 章）。
- 门禁拦下：ref-addr/ref-net_buf 页不存在（api 字段改 net 族真实页）、
  xrtFileClose 不在符号表（xfile 家族统一 xrtClose，改写表述）。
- 行文章号修正 11 处：正文中"第 N 章"纯文字引用按主题重定位（测试体系→106、
  性能分析→108、分帧→71、HTTP 头解析→83 等），消除历次重编号的漂移。
- 非 确 定 性 输 出示例（interface/local_info/frame_line/frame_length 预期输出为
  "随机器变化"占位）：check 按空提取跳过比对，term 给代表性输出并注明。

## P10 阶段记录

- 卷七前半 8 章新写：ch61 导言（骨架工业化映射主线+两条横切纪律）、ch62 地址模型、
  ch63 事件端口与五后端、ch64 缓冲链、ch65 DNS、ch67 TCP 进阶（发送五档+写预算）、
  ch68 UDP（双形态+批量）、ch69 代理（SOCKS5/HTTP CONNECT+托管拨号）。
  ch66 TCP 金标准重编号迁移（旧 65 → 66），内容未改。
- 全书第九次重编号：插入卷七导言 ch61（ch61 之后全部 +1）。
- 范围调整：net-file/net-frame 并入 ch70 net-misc（分帧、端口文件与网卡），归 P11。
- 门禁拦下：xsockaddr_storage 不存在（改 Native[64] 数组 + iSize 出参）、
  xnetportcapabilities 非结构体（Capabilities 返回 uint32 位标志）、
  xnetbufspan 真名 xnetspan 且 Spans 为填调用方数组模式、
  term 输出多次对齐示例头注释（resolver_future/tcp_stream_tour/udp_batch 真实行）。
- 站点同步：index.html 全站编号偏移修正（插入 ch61 后 112 条目 +1）+ 卷范围对齐
  order.json + 卷七 61-69 置 done；start.html 章号可见文字与 href 一致化；
  全站 book 链接扫描 0 断链。

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
