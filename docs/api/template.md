# Template

模板模块把“编译一次、重复渲染”作为基本模型。`xtemplate` 是不可变引用对象，可以跨线程共享；每次渲染的作用域、预算、输出和外部解析状态互相独立。模板源、路径、参数和输出都使用明确长度，允许嵌入零字节。

## 模块

- `XRT_MODULE_TEMPLATE_CORE`：文本、字符串值、数字和时间输出，编译对象、节点检查、流式与字符串渲染。
- `XRT_MODULE_TEMPLATE_CONTROL`：表达式、条件、范围循环、容器遍历和循环控制。
- `XRT_MODULE_TEMPLATE_COMPOSE`：本地定义、外部模板解析、包含、原样块和环检测；依赖 `map`。
- `XRT_MODULE_TEMPLATE_EXTENSION`：不可变扩展注册表、函数、语句、解析块和原样块；依赖 compose。
- `XRT_MODULE_TEMPLATE_FILE`：从文件读取完整模板并编译；只依赖 core 与 `file_whole`，不强制引入控制、组合或扩展层。

可以同时启用多个顶层模块。例如 `template_extension,template_file` 同时提供完整语言和文件入口。裁剪时依赖由清单展开，缺失直接依赖会在头文件或构建阶段失败，不会静默降级。

## 默认预算

所有默认值都是有限值，调用方可以通过配置进一步收紧：

| 宏 | 默认值 | 约束对象 |
| --- | ---: | --- |
| `XTEMPLATE_SOURCE_DEFAULT` | 16 MiB | 模板源字节数 |
| `XTEMPLATE_NODES_DEFAULT` | 1,000,000 | 编译节点数 |
| `XTEMPLATE_PATH_SEGMENTS_DEFAULT` | 1,000,000 | 全部路径段数 |
| `XTEMPLATE_PATH_DEPTH_DEFAULT` | 64 | 单条路径深度 |
| `XTEMPLATE_EXPRESSIONS_DEFAULT` | 1,000,000 | 表达式数 |
| `XTEMPLATE_BLOCK_DEPTH_DEFAULT` | 64 | 编译块嵌套深度 |
| `XTEMPLATE_EXPRESSION_DEPTH_DEFAULT` | 128 | 表达式嵌套深度 |
| `XTEMPLATE_ARGUMENTS_DEFAULT` | 1,000,000 | 全部扩展参数数 |
| `XTEMPLATE_CALL_ARGUMENTS_DEFAULT` | 256 | 单次扩展调用参数数 |
| `XTEMPLATE_OUTPUT_DEFAULT` | 16 MiB | 单次渲染输出字节数 |
| `XTEMPLATE_STEPS_DEFAULT` | 10,000,000 | 单次渲染执行步数 |
| `XTEMPLATE_RENDER_DEPTH_DEFAULT` | 64 | 运行时节点嵌套深度 |
| `XTEMPLATE_LOOP_DEFAULT` | 1,000,000 | 单次渲染循环次数 |
| `XTEMPLATE_INCLUDE_DEPTH_DEFAULT` | 32 | 外部包含深度 |

`xrtTemplateConfigInit` 初始化默认分隔符与编译预算，`xrtTemplateRenderConfigInit` 初始化默认作用域与渲染预算。配置结构必须先初始化，再覆盖所需字段；不要依赖全零结构的含义。

## 编译与所有权

`xrtTemplateCompile` 使用默认配置编译 `xstrview`。`xrtTemplateCompileConfig` 接受显式 `xtemplateconfig`：

- `Open`、`Close` 替换默认开始与结束标记。
- `MaxSourceBytes`、`MaxNodes`、`MaxPathSegments` 和 `MaxPathDepth` 约束核心编译产物。
- control 启用后，`MaxExpressions`、`MaxBlockDepth` 和 `MaxExpressionDepth` 约束表达式与块。
- extension 启用后，`Registry` 绑定不可变注册表，`MaxArguments` 和 `MaxCallArguments` 约束扩展参数。

编译会复制模板源及需要长期持有的元数据。调用返回后，输入 `xstrview` 与配置本身都可以失效。`xrtTemplateRef` 增加引用，`xrtTemplateRelease` 释放引用；最终释放时同时释放模板持有的注册表引用。

`xrtTemplateCompileFile` 使用默认配置读取并编译文件，`xrtTemplateCompileFileConfig` 使用配置中的 `MaxSourceBytes` 作为读取上限。文件内容按原始字节处理，不删除 BOM，也不做编码转换。读取失败保留 `xrt.file` 错误，编译失败使用 `xrt.template` 错误；临时文件缓冲在编译返回前释放。

## 基础语法

默认开始和结束标记是 `{` 与 `}`。重复开始标记输出一个字面开始标记，例如 `{{` 输出 `{`。

| 写法 | 含义 |
| --- | --- |
| `{$path}` | 输出字符串或可直接表示为文本的标量 |
| `{%path}` | 输出数字 |
| `{%path:04d}` | 使用 XRT 数字格式输出数字 |
| `{&path:%F}` | 使用 XRT 时间格式输出时间 |

路径默认从 `Current` 开始，支持对象键、点号、数组索引和负索引。`this`、`root`、`global` 是显式根；control 还提供 `loop`。默认查找不会从 Current 隐式回退到 Root 或 Global，跨作用域访问必须写出根名，避免同名字段改变含义。

Template 是通用文本引擎，不知道输出将进入 HTML 文本、HTML 属性、JavaScript、CSS、
Shell、SQL 还是其他上下文。`{$path}` 按原字节输出，不自动执行 HTML 或其他上下文
转义。只有已经由应用验证或转义的数据才能直接进入这些敏感位置；需要统一策略时，
应通过只读扩展函数集中完成，并为每种输出上下文使用不同扩展。JSON 序列化和 SQL
参数绑定不能用模板转义代替。

HTML 文本与带引号属性可以按需组合独立的 `html_escape` 模块。应用可以在写入
`xvalue` 前调用 `xrtHtmlEscape`，也可以注册只读模板扩展并在扩展内部调用它；
Template 本身不依赖该模块，也不会替调用方猜测输出上下文。详见
[HTML 文本转义](html.md)。

## 表达式与控制

表达式支持 `null`、布尔值、有符号整数字面量、浮点数、带引号字符串、路径和括号；输入 `xvalue` 也可提供完整 `uint64`。逻辑运算支持 `not` / `!`、`and` / `&&`、`or` / `||`，并执行短路求值。比较支持 `=`、`==`、`!=`、`~=`、`>`、`<`、`>=`、`<=`；有符号整数、无符号整数与浮点数执行精确的混合比较，NaN 不会被当成普通有序值。

| 写法 | 含义 |
| --- | --- |
| `{?expr:true-text:false-text}` | 行内条件；文本中的冒号写为 `\:` |
| `{#if:expr}...{#elseif:expr}...{#else}...{#end}` | 条件块 |
| `{#for:start:end}...{#end}` | 包含两端的整数范围 |
| `{#for:start:end:step}...{#end}` | 显式步长范围；步长为零是错误 |
| `{#foreach:expr}...{#end}` | 遍历数组、整数映射、集合或对象 |
| `{#break}` / `{#continue}` | 控制最近一层循环 |

`for` 未给出步长时根据起止方向选择 `1` 或 `-1`；显式步长方向不可能到达终点时循环为空。`foreach` 为迭代建立稳定快照，并把 Current 临时替换为当前项。

循环体可以读取 `loop.value`、`loop.index`、`loop.number`、`loop.key`、`loop.first`、`loop.last` 和 `loop.depth`。`index` 从零开始，`number` 从一开始。

## 组合

`{#define:'name'}...{#end}` 创建当前模板内的不可变定义。定义支持前向引用，同一名称不能重复。`{#include:expression}` 先查找本地定义，再调用 `xtemplateresolvefn` 查找外部模板。

解析回调的 `*pTemplate` 进入回调前为 `NULL`。成功且仍为 `NULL` 表示未找到；回调写入的任何非空引用都由渲染器接管，即使回调随后返回 `false`，渲染器也会释放该引用。外部模板按精确身份检测递归环，并受 `MaxIncludeDepth` 限制。

`{#raw}...{#end}` 原样输出主体，不解析其中的模板语法。需要在 raw 主体中放置看似结束标记的文本时，重复开始标记可以阻止它被识别为结束节点。

## 扩展

`xtemplateregistry` 是不可变、引用计数的扩展注册表。`xtemplateextensionfn` 是统一
扩展调用函数类型，`xtemplateextensiondrop` 是注册项用户数据的析构函数类型。
注册表最终释放时才调用析构函数。

`xtemplateextension` 描述一个扩展：

| 字段 | 含义 |
| --- | --- |
| `Name` | 明确长度名称；创建注册表时复制 |
| `Type` | 调用形态 |
| `MinArguments` / `MaxArguments` | 编译时参数数量约束 |
| `Call` | 渲染回调 |
| `Data` | 独立用户数据 |
| `Drop` | 注册表最终释放时的数据析构函数 |

`xtemplateextensiontype` 有四个明确值：

| 值 | 语法与主体 |
| --- | --- |
| `XTEMPLATE_EXTENSION_FUNCTION` | `{@name:args}`，行内函数 |
| `XTEMPLATE_EXTENSION_STATEMENT` | `{#name:args}`，无主体语句 |
| `XTEMPLATE_EXTENSION_BLOCK` | `{#name:args}...{#end}`，主体预编译为节点 |
| `XTEMPLATE_EXTENSION_RAW_BLOCK` | `{#name:args}...{#end}`，主体保留原始字节 |

`xrtTemplateRegistryCreate` 校验并复制全部描述和名称。只有创建完整成功后，注册表才接管每一项 `Data`；失败时仍由调用方处理所有数据。函数和语句使用独立名称空间，同名函数与语句可以同时注册。`xrtTemplateRegistryRef` 和 `xrtTemplateRegistryRelease` 管理不可变注册表引用。

参数用顶层冒号分隔，解析器会识别引号、转义和括号内的冒号。`name=expression` 是命名参数，其余是位置参数；命名参数不能重复，比较表达式可以使用 `==` 避免与命名符号混淆。表达式在模板编译时预编译，不在每次渲染时重新解析。

扩展回调通过 `xtemplatecall` 使用当前调用：

- `xrtTemplateCallName`、`xrtTemplateCallData` 返回扩展名称与描述中的用户数据。
- `xrtTemplateCallArgumentCount` 返回参数数。
- `xrtTemplateCallArgument` 按零基索引返回 `xtemplateargview`。
- `xrtTemplateCallFind` 查找命名参数；位置参数不会以空名称参与匹配。
- `xrtTemplateCallEval` 在当前作用域求值参数，结果写入 `xtemplatevalue`。
- `xrtTemplateCallWrite` 向共享 writer 写入一个明确长度分片。
- `xrtTemplateCallRender` 渲染解析块主体；可以调用零次或多次。
- `xrtTemplateCallRenderCurrent` 使用临时 Current 渲染解析块主体。
- `xrtTemplateCallRaw` 返回原样块主体；其他类型返回空视图。
- `xrtTemplateCallCurrent`、`xrtTemplateCallRoot`、`xrtTemplateCallGlobal` 返回借用作用域值。

`xtemplateargview` 的 `Index` 是稳定句柄，`Name` 和 `Source` 借用模板。`xtemplatevalue` 的 `Type` 决定 `Value`、`Bool`、`Integer`、`Unsigned`、`Float`、`Text` 或 `Time` 中哪些字段有效；文本和值仍由输入数据或模板拥有。`xtemplatecall` 及其返回视图只在当前回调期间有效，不得保存到回调外。

回调返回 `false` 时，如果已经设置 `xrt.template` 错误，该错误直接传播；其他错误会作为 cause 包装为 `XTEMPLATE_ERROR_CALLBACK` 并补充调用位置；未设置错误时创建 callback 错误。成功回调产生的临时错误会被丢弃，并恢复进入回调前的线程错误。`break` 和 `continue` 不能越过扩展回调边界。

注册表与模板都不可变且可跨线程共享。共享扩展的 `Data` 和 `Call` 必须由扩展实现保证并发只读或自行同步。

## 渲染

`xtemplaterenderconfig` 的 `Root`、`Current`、`Global` 建立三个显式作用域；`MaxOutputBytes` 和 `MaxSteps` 限制所有层共享的总输出与执行步数。control 的 `MaxDepth`、`MaxLoopIterations`，compose 的 `Resolve`、`ResolveData`、`MaxIncludeDepth` 只在对应模块启用时出现。

`xtemplaterenderflag` 当前定义 `XTEMPLATE_STRICT_UNDEFINED`，把缺失路径从空输出
提升为 `XTEMPLATE_ERROR_UNDEFINED`。未启用严格模式时，缺失的普通输出为空；
类型错误、格式错误和控制表达式错误仍然失败。

`xrtTemplateWrite` 是基础流式入口。`xtemplatewritefn` 借用当前输出分片；回调返回 `false` 立即停止。已经交给 writer 的分片无法回滚。

`xrtTemplateRenderTo` 把结果追加到 `xstrbuf`，本次调用具有事务性：任何失败都会撤销本次追加，调用前已有内容保持不变。`xrtTemplateRender` 是常见路径 helper，把同一 `xvalue` 同时作为 Root 和 Current，返回零结尾字符串；调用方使用 `xrtFree` 释放，并可通过 `pSize` 取得包含嵌入零字节的精确长度。

同一 `xtemplate` 可以并发渲染，但每次渲染的配置、writer、输出构建器和数据访问必须独立。传入的 `xvalue` 不会被模板修改；调用方必须保证渲染期间数据有效，并保证共享数据可并发读取。

## 检查

`xrtTemplateSource` 返回模板持有的原始源视图。`xrtTemplateNodeCount` 返回全部编译节点数，包括控制块内部节点。`xrtTemplateNode` 按索引填写 `xtemplatenodeview`：

- `Type` 使用 `xtemplatenodetype`。核心值是 `XTEMPLATE_NODE_TEXT`、
  `XTEMPLATE_NODE_OUTPUT`；control 增加 `XTEMPLATE_NODE_INLINE_IF`、
  `XTEMPLATE_NODE_IF`、`XTEMPLATE_NODE_FOR`、`XTEMPLATE_NODE_FOREACH`、
  `XTEMPLATE_NODE_BREAK`、`XTEMPLATE_NODE_CONTINUE`；compose 增加
  `XTEMPLATE_NODE_DEFINE`、`XTEMPLATE_NODE_INCLUDE`、`XTEMPLATE_NODE_RAW`；
  extension 增加 `XTEMPLATE_NODE_EXTENSION`。
- `Output` 使用 `xtemplateoutputtype`，通过 `XTEMPLATE_OUTPUT_TEXT`、
  `XTEMPLATE_OUTPUT_NUMBER`、`XTEMPLATE_OUTPUT_TIME` 区分字符串、数字和时间输出。
- `Location` 提供字节范围和行列。
- `Source`、`Expression`、`Format` 以及 compose 启用后的 `Name` 都是模板拥有的借用视图。

这些检查 API 提供稳定的只读结构信息，不公开内部 AST 指针，也不要求维护另一套序列化格式。

## 错误

`xtemplateerror` 定义 `xrt.template` 域中的稳定模板错误代码：

| 值 | 含义 |
| --- | --- |
| `XTEMPLATE_ERROR_CONFIG` | 配置、分隔符或注册描述无效 |
| `XTEMPLATE_ERROR_SYNTAX` | 模板或表达式语法错误 |
| `XTEMPLATE_ERROR_LIMIT` | 编译或渲染预算耗尽 |
| `XTEMPLATE_ERROR_UNDEFINED` | 严格模式下路径缺失 |
| `XTEMPLATE_ERROR_TYPE` | 值类型与操作不匹配 |
| `XTEMPLATE_ERROR_FORMAT` | 数字或时间格式无效 |
| `XTEMPLATE_ERROR_ITERATE` | 值不可遍历或迭代失败 |
| `XTEMPLATE_ERROR_WRITE` | writer 拒绝输出且未提供更具体错误 |
| `XTEMPLATE_ERROR_CALLBACK` | 外部扩展回调失败 |
| `XTEMPLATE_ERROR_INCLUDE` | 本地或外部包含解析失败 |
| `XTEMPLATE_ERROR_CYCLE` | 检测到模板包含环 |

`xtemplatelocation.Offset` 和 `Size` 是零基字节范围，`Line` 和 `Column` 从一开始。`xrtTemplateErrorLocation` 从模板错误中读取位置；没有位置或错误域不匹配时返回 `false`。内存不足保持统一 `XERR_MEMORY`，不为了包装错误再次分配。

## 示例

- `examples/template/core/main.c`：字符串、数字、时间的基础编译和输出。
- `examples/template/control/main.c`：条件、容器遍历、范围循环与 `break/continue`。
- `examples/template/compose/main.c`：前向定义、本地包含与外部解析器。
- `examples/template/extension/main.c`：四类扩展与参数求值。
- `examples/template/file/main.c`：一行式文件模板编译。
