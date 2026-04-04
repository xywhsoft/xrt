# xrt extlibs

`extlibs` 用于放置这类扩展库：

- 不是 `xrt` 最核心的通用能力
- 但在特定业务场景里很有用
- 适合做成 header-only，直接被 `xsbase.h` 集成

当前约定：

- 每个扩展库使用独立目录，例如 `tcc/extlibs/xsmtp/`
- 对外主入口统一命名为 `inline_x*.h`
- 头文件应尽量自洽，不依赖宿主项目私有模块
- 优先复用 `xrt` 现有基础设施，例如：
  - 内存分配
  - 编码工具
  - 加密/TLS
  - 时间与网络抽象

当前已接入：

- `xsmtp`
  - 文件：`xsmtp/xsmtp.h`
  - 能力：SMTP/SMTPS/STARTTLS、AUTH PLAIN/LOGIN、UTF-8 邮件头、text/html 邮件体
