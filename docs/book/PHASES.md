# PHASES — 教程重写阶段计划与执行手册

本文件是全书重写的阶段事实来源。每个阶段由一条提示词开启（模板见文末），
提示词只带阶段号，范围与验收以本文件为准。进度记录在 `docs/book/PROGRESS.md`。

**总量目标**：全书约 158 章 / 讲解约 90 万字（现状 100 章 / 22.8 万字）。
**总量测算**：剩余约 156 章；按难度折算（容器文本类 ×1、并发网络类 ×1.5、安全扩展项目类 ×2）
约 250 当量章；每会话稳定产出 10–14 当量章 → **26 个写作阶段 + 1 个收尾阶段 = 27 个阶段提示词**。

## 阶段表

| 阶段 | 范围 | 章数 | 状态 |
| --- | --- | --- | --- |
| P0 | 基础设施：SPEC + 生成器 + 门禁 + 金标准章（03、52） | 2 | ✅ 完成 |
| P1 | 卷一：ch01/02 重写、memory 拆 2 章、temp、新增 core 章（版本/ABI/模块查询/单头原理）、卷导言 | 8 | ✅ 完成 |
| P2 | 卷二全部 + 卷三前半：math/random/hash-xid + array/buffer/stack/list-slotmap | 9 | ⬜ |
| P3 | 卷三后半 + 新增：map/set/avl/queue/pool + 容器选型决策章 + 卷导言 | 7 | ⬜ |
| P4 | 卷四前半：string/number/charset/codec/compress | 6 | ⬜ |
| P5 | 卷四后半：value/json/xson/template + regex 归位本卷 + JSON+模板组合章 | 7 | ⬜ |
| P6 | 卷五前半：logger×2/console/io/time/env | 6 | ⬜ |
| P7 | 卷五后半：path/file×2/dir/file-async/signal + 调试与诊断组合章 | 7 | ⬜ |
| P8 | 卷六前半：thread-sync（含 once）/coroutine×3 + 取消体系专章 | 8 | ⬜ |
| P9 | 卷六后半：channel/future/executor/task×2 + 调度器实战章 | 8 | ⬜ |
| P10 | 卷七前半：net-addr/port/buf/dns/file/frame + 事件模型与 C10K 思路章 | 7 | ⬜ |
| P11 | 卷七后半：tcp-adv/udp/proxy/misc/interface + netbuf 深入章 | 6 | ⬜ |
| P12 | 卷八·密码基础：hash/aead/asym + 密码工程纪律章 + 卷导言 | 5 | ⬜ |
| P13 | 卷八·证书链：der-pem/x509/x509-verify + 信任链与吊销章 | 4 | ⬜ |
| P14 | 卷八·TLS 上：tls-client/tls-identity + 握手时序图解章 + 验证策略章 | 4 | ⬜ |
| P15 | 卷八·TLS 下：tls-server/tls-stream + 会话恢复章 | 3 | ⬜ |
| P16 | 卷九·HTTP 核心：http/http1/headers/decode + 分帧图解章 | 5 | ⬜ |
| P17 | 卷九·WebSocket：ws-frame/ws-stream/http-upgrade + 双向通信组合章 | 4 | ⬜ |
| P18 | 卷十·xhttp 客户端：easy/runtime/redirect/cache/url+query 重组 | 6 | ⬜ |
| P19 | 卷十·xhttp 服务端与高级：server/middleware + SSE/流式上传/连接池深潜/重试策略新章 | 6 | ⬜ |
| P20 | 卷十一·xws：连接管理/组播广播/引用发送/压缩/服务端路由/运行时 | 6 | ⬜ |
| P21 | 卷十一·xssh：传输与包层/kex/hostkey/认证/通道/端口转发/客户端运行时 | 7 | ⬜ |
| P22 | 卷十一·xmail：SMTP/IMAP/POP3/MIME/组合收发 | 5 | ⬜ |
| P23 | 卷十一·xruntime：类型系统/对象图/动态调用/typed 容器 | 4 | ⬜ |
| P24 | 卷十二·工程实践：build/trim/package/testing/embed/perf + 性能分析方法章 + OOM 与故障注入测试章 | 8 | ⬜ |
| P25 | 卷十三·项目上：CLI 工具/配置服务/聊天服务（设计+实现） | 5 | ⬜ |
| P26 | 卷十三·项目下：下载器/WebSocket 服务 + 全书收官 | 5 | ⬜ |
| P27 | 收尾：index.html 目录全量重建、卷导言核对、全站链接校验、漂移总报告、CI 接线 | — | ⬜ |

## 每阶段执行流程（模板引用此处）

1. 读 `BOOK_SPEC.md`、本文件对应阶段行、金标准章（04/55）、`PROGRESS.md`。
2. 列本阶段章节清单（含新增/拆分/合并），涉及新增时先更新 `order.json`
   并在 `wwwroot/book/index.html` 对应卷分组插入条目。
3. 分批执行，每批 3–5 章：
   a. `python tools/gen_web_book.py material <slug>` 取素材包，**读素材原文**后动笔；
   b. 写 md → `python tools/gen_web_book.py build` → `python tools/check_book.py check`；
   c. 修复全部 FAIL（禁止调低红线），全绿后两仓库各 commit 一次。
4. 阶段结束：`check_book.py report` 度量表贴给用户；更新 `PROGRESS.md`（各章指标、阶段状态、
   遗留项）；输出阶段总结。

## 漂移红线（任何阶段不得豁免）

- FAIL 章不算完成；check 红线只允许在规范变更（先改 BOOK_SPEC）后调整。
- 连续 3 章低于类型目标 80% → 停止推进，回金标准校准后再继续。
- 不得手改 `wwwroot/book/ch*.html`；一切经 `docs/book/*.md` 与生成器。
- 每章的 API 行为必须来自素材包原文（examples/docs/api/头文件），不得凭记忆断言。

## 阶段提示词模板

```
执行 XRT 教程重写阶段 P{n}。

前置，按顺序完成，禁止跳过：
1. 读 D:\GIT\xrt\docs\book\BOOK_SPEC.md —— 源格式规范与门禁红线
2. 读 D:\GIT\xrt\docs\book\PHASES.md 中 P{n} 的阶段定义与执行流程
3. 读金标准章 docs/book/04-error.md 与 docs/book/55-net-tcp.md，以其深度、结构、语气为基准
4. 在 docs/book/PROGRESS.md 标记 P{n} 开始

执行纪律（防漂移，逐条遵守）：
- 每章动笔前运行 python tools/gen_web_book.py material <slug> 获取素材包；
  examples 源码与 docs/api 契约必须先读原文，禁止凭记忆描述 API 行为
- 每批 3-5 章：写 md → python tools/gen_web_book.py build → python tools/check_book.py check
  → 修复所有 FAIL → 全绿后 commit（D:\GIT\xrt 与 D:\GIT\home\host\xrt 两仓库）→ 下一批
- check 报 FAIL 的章不算完成；禁止调低 check_book.py 红线来"通过"
- 新增/拆分章节时先更新 docs/book/order.json，并在 wwwroot/book/index.html 对应卷分组插入条目
- 阶段结束：运行 python tools/check_book.py report 贴出度量表，更新 PROGRESS.md，
  commit 并输出阶段总结（完成章数、字数、程序/图示/避坑统计、遗留项）

范围：只做 P{n} 定义的内容；时间不够宁可少完成一批，不可降低质量标准。
```
