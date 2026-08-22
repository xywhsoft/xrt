# xregex

`xregex` 是基于 XRT 的正则表达式扩展库。它提供字面量转义、不可变编译对象、
可复用 matcher、捕获、替换、拆分和多模式集合，并继续使用 XRT 的分配器、
字符串、Unicode 与结构化错误体系。

正则实现和 BBRE 不再进入 XRT 核心清单、核心 umbrella 或 `single/xrt.h`。
需要完整能力时可以选择整个扩展：

```c
#define XREGEX_MODULE_XREGEX
#include <xregex.h>
```

也可以只选择 `XREGEX_MODULE_REGEX_CORE`、`XREGEX_MODULE_REGEX_MATCH`、
`XREGEX_MODULE_REGEX_REPLACE`、`XREGEX_MODULE_REGEX_SPLIT` 或
`XREGEX_MODULE_REGEX_SET`。生成的 `single/xregex.h` 保留相同裁剪边界：

```c
#define XREGEX_MODULE_XREGEX
#define XREGEX_IMPLEMENTATION
#include "single/xregex.h"
```

仓库内使用独立扩展清单完成构建、回归与发布检查：

```text
python tools/build.py --manifest extlibs/xregex/config/modules.json --suite xregex
python tools/check_release_maturity.py --manifest extlibs/xregex/config/modules.json --product xregex --min-size-profiles 2
python tools/package.py --manifest extlibs/xregex/config/modules.json --suite xregex --kind static --verify
python tools/package.py --manifest extlibs/xregex/config/modules.json --suite xregex --kind shared --verify
python tools/measure_size.py --config extlibs/xregex/config/size_profiles.json --manifest extlibs/xregex/config/modules.json --profiles * --rebuild
python tools/measure_performance.py --config extlibs/xregex/config/performance_profiles.json --manifest extlibs/xregex/config/modules.json --profiles regex --smoke
```

BBRE 的许可保存在 `src/third_party/bbre/LICENSE.md`。扩展只包含 XRT 的公开头，
不读取 `src/internal`，也不在扩展内复制内存、字符串或 Unicode 实现。
