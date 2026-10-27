# xredis public API reference

此文件由 `tools/generate_api_reference.py` 从 `extlibs/xredis/config/modules.json` 与公共头生成。
不要手工维护第二份符号清单。主题语义、状态机、所有权、错误和示例见
[../../README.md](../../README.md)；每个声明的精确契约以链接的公共头中文注释为准。

当前登记 `2` 个函数、`7` 个常量或宏、
`1` 个公共类型。

## `extlibs/xredis/include/xredis/command.h`

[查看带契约注释的公共头](../../include/xredis/command.h)

### 函数 (2)

- `xredisCommandSize`
- `xredisCommandWrite`

### 常量与宏 (5)

- `XREDIS_COMMAND_ARGUMENT`
- `XREDIS_COMMAND_CAPACITY`
- `XREDIS_COMMAND_OVERFLOW`
- `XREDIS_COMMAND_OVERLAP`
- `XREDIS_FEATURE_XREDIS_COMMAND`

### 类型 (1)

- `xrediscommanderror`

## `extlibs/xredis/include/xredis/export.h`

[查看带契约注释的公共头](../../include/xredis/export.h)

### 常量与宏 (2)

- `XREDIS_BUILD_SHARED`
- `XREDIS_USE_SHARED`
