# XRT 扩展库

`extlibs/` 中的扩展建立在 XRT 公共 API 之上。每个扩展必须先获得所需核心模块；扩展单头只包含自身代码。正式实现不使用核心 `src/internal/`。

## 开发范式

全部扩展使用相同目录和仓库根目录工具：

```text
extlibs/<product>/
├── config/                    模块依赖、性能与体积配置
│   └── modules.json           源码、头文件、测试、示例、文档的唯一清单
├── include/<product>.h        公共聚合入口
├── include/<product>/         公共 API 与生成的 features.h
├── src/<system>/              独立编译的实现文件
├── src/internal/              本库私有声明与结构
├── tests/                     模块化测试、single/ 与 package/ 消费测试
├── examples/                  使用公共 API 的示例
├── bench/                     性能基准
└── docs/api/reference.md      按公共头生成的 API 参考
```

`modules.json` 的 `depends` 声明核心及其他扩展模块；跨扩展依赖用 `dependency_manifests` 引入。需要内存诊断等额外能力的测试使用测试套件模块声明，不加入产品依赖。公开 API 使用 `XRT_API`，适用于统一静态库和动态库交付。

所有 CI 位于根目录 `.github/workflows/`。各扩展使用 `tools/build.py`、`tools/package.py`、`tools/generate_extension_features.py`、`tools/amalgamate.py`、`tools/generate_api_reference.py`、`tools/measure_performance.py` 和 `tools/measure_size.py`。构建与测量的临时产物进入根目录 `out/`，统一打包的交付产物进入根目录 `release/`。

认证故障注入与覆盖率测试中的 `tests/support/implementation.c` 是测试专用源码包含入口，供替换调用与故障注入使用；生产构建按清单独立编译 `src/` 中的文件。

例如，从仓库根目录执行：

```sh
python tools/build.py --compiler gcc --manifest extlibs/xllm/config/modules.json --suite xllm --jobs 4
python tools/package.py --compiler gcc --manifest extlibs/xllm/config/modules.json --suite xllm --kind static --verify
python tools/generate_extension_features.py --check extlibs/xllm/config/modules.json
python tools/check_release_maturity.py --release --manifest extlibs/xllm/config/modules.json
python tools/measure_performance.py --config extlibs/xllm/config/performance_profiles.json --smoke
python tools/measure_size.py --config extlibs/xllm/config/size_profiles.json --kind single
```

## 当前扩展

| 产品 | 主要能力 |
| --- | --- |
| xruntime | 类型化运行时、动态值与语言运行时支撑 |
| xhttp | HTTP 高层客户端和服务端 |
| xws | WebSocket 客户端和服务端 |
| xmail | MIME 内容层与邮件传输基座 |
| xsmtp | SMTP 客户端 |
| xpop3 | POP3 客户端 |
| ximap | IMAP 客户端 |
| xssh | 分层 SSH 协议 |
| xacme | ACME 证书申请与管理 |
| xjwt | JWT/JWS 签发、验证与 JWKS |
| xoauth2 | OAuth2 客户端与 OIDC 应用组合支持 |
| xllm | 单次模型调用、流式响应与提供方适配 |
| xllm-session | 会话账本、预算、压缩与持久化 |
| xwork | Agent 循环、工具执行与审批 |

`xllm-session` 的清单产品和模块标识是 `xllm_session`（C 标识符），目录和公共头仍为 `xllm-session`。依赖链为 `xwork → xllm-session → xllm → xrt`。`xoauth2` 与 `xjwt` 各自依赖核心，OIDC 示例与测试在应用层组合两者。

## 单头发布布局

根目录 `single/extlibs/<product>.h` 和 `<product>_decl.h` 分别提供实现与声明，均只展开所属产品代码。先选择最外层扩展的 `*_MODULE_*`，按依赖顺序处理 features，提供核心头后再提供依赖扩展和本扩展。具体组合示例见 [构建说明](../docs/BUILD.md)。

```sh
python tools/amalgamate.py --all
python tools/amalgamate.py --all --check
```
