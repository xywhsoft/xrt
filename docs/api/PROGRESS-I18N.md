# API 参考文档多语言进度（en / ru）

登记 API 参考页（`wwwroot/book/ref-*.html`）的翻译状态与续作批次。
书稿翻译见 `docs/book/PROGRESS.md`；本文件只管 `docs/api/`。

## 机制（2026-09-14 建立）

- 译文源：`docs/api/<lang>/<name>.md`，与 zh 源同名同构；代码块与符号序列必须一致。
- 生成：`python tools/gen_web_api.py --lang en`（ru 同理）→ `wwwroot/<lang>/book/ref-*.html`；
  翻译页带本语言导航/页脚/标签，语言菜单深链已译页、未译回退 `?lang=` 提示条。
- 门禁（生成即校验，失败即退出）：符号序列与全部代码块必须与 zh 逐块一致。
- hreflang：存在译本的 ref 页自动注入 zh-CN/en/ru/x-default 四链（zh 与译本页同步）。
- sitemap：`tools/i18n/gen_sitemap.py` 已扩展——有译本的 ref 页按已译语言互指。
- 目录页：en/ru `api.html` 中已译模块卡片改本地直链 `book/ref-*.html`（手工同步）。
- 术语：沿用 `tools/i18n/glossary.json`（103 条）。

## 进度

| 批次 | 文档 | en | ru | 状态 |
|---|---|---|---|---|
| 批 0（试点） | jsonl、xsonl | ✅ | ✅ | 2026-09-14 完成，结构校验全绿 |

**当前：2 / 87 文档已译（en 2、ru 2）。**

## 建议批次（按数据层→基础层→高频模块优先）

- 批 1 数据层补全：value、json、xson（4,867 / 1,844 / 2,080 行，量大可再拆）
- 批 2 高频基础：string、error、memory、time、logger
- 批 3 并发层：coroutine、future、channel、task、cancel、sync
- 批 4 网络层：net、tcp、udp、http1、websocket、tls*
- 批 5 其余模块按模块名字典序分批收尾

## 注意

- zh 源 `稳定契约 / 设计契约 / 裁剪与依赖` 等 h2 段自 2026-09-14 起渲染为契约卡单元
  （此前生成器静默丢弃；译文沿用同一机制，无需特殊处理）。
- 翻译时保持：代码块逐字节不变；`### \`symbol\`` 标题不变；h4 用对应语言键
  （Parameters / Return value / Errors / Example；Параметры / Возвращаемое значение / Ошибки / Пример）。
