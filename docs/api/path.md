# Path API

## 设计契约

路径体系分成三个可独立裁剪的层次：

- `XRT_FEATURE_PATH`：纯词法解析、分解、拼接、清理和相对路径，不访问文件系统。
- `XRT_FEATURE_PATH_SYSTEM`：工作目录、绝对路径、用户目录、临时目录和程序位置。
- `XRT_FEATURE_PATH_SAFE`：归档条目和静态资源名称的跨平台 UTF-8 词法校验。

`path_system` 和 `path_safe` 都依赖 `path` 与 `unicode`。纯词法层只依赖字符串模块，因此 URL、归档、构建工具和跨平台协议可以明确选择 POSIX 或 Windows 语义，而不引入系统调用。

全部返回 `str` 的函数都返回拥有字符串，调用方使用 `xrtFree` 释放。`xpathparts` 中的视图全部借用输入，输入失效后不可继续使用。失败返回 `NULL` 或 `false` 并保留结构化错误；谓词返回 `false` 时，如果需要区分“不是目标类型”和“参数错误”，应检查 `xrtGetError()`。

路径文本优先使用 UTF-8。词法层只禁止嵌入零字节，不强制路径必须是有效 UTF-8，以便操作本机可表达但非 Unicode 的 POSIX 文件名；Windows 系统层严格转换 UTF-8/UTF-16；POSIX 系统查询返回操作系统原始路径字节，调用方需要知道当前文件名编码；安全条目层要求严格有效的 UTF-8。

## 路径风格

```c
typedef enum xpathstyle {
	XPATH_NATIVE = 0,
	XPATH_POSIX,
	XPATH_WINDOWS
} xpathstyle;
```

- `XPATH_NATIVE` 在 Windows 上等价于 `XPATH_WINDOWS`，其他平台等价于 `XPATH_POSIX`。
- `XPATH_POSIX` 只把 `/` 视为分隔符，根为 `/`。
- `XPATH_WINDOWS` 同时接受 `/` 与 `\`，输出统一使用 `\`。

显式风格只决定词法规则，不假装目标操作系统存在对应驱动器、UNC 共享或文件。需要操作系统解释路径时使用 `path_system`。

```c
char xrtPathSep(void);
char xrtPathListSep(void);
```

`xrtPathSep` 返回本机目录分隔符。`xrtPathListSep` 返回环境路径列表分隔符，Windows 为 `;`，POSIX 为 `:`。

## 根和分解

```c
typedef enum xpathroot {
	XPATH_ROOT_NONE = 0,
	XPATH_ROOT_POSIX,
	XPATH_ROOT_WINDOWS,
	XPATH_ROOT_DRIVE_RELATIVE,
	XPATH_ROOT_DRIVE,
	XPATH_ROOT_UNC,
	XPATH_ROOT_DEVICE
} xpathroot;

typedef enum xpathflag {
	XPATH_FLAG_ROOTED = 0x01,
	XPATH_FLAG_ABSOLUTE = 0x02,
	XPATH_FLAG_TRAILING_SEPARATOR = 0x04
} xpathflag;
```

Windows 根必须明确区分：

| 输入 | 根类型 | 带根 | 完整绝对 |
| --- | --- | --- | --- |
| `name` | `NONE` | 否 | 否 |
| `C:name` | `DRIVE_RELATIVE` | 是 | 否 |
| `\name` | `WINDOWS` | 是 | 否 |
| `C:\name` | `DRIVE` | 是 | 是 |
| `\\server\share\name` | `UNC` | 是 | 是 |
| `\\?\C:\name` | `DEVICE` | 是 | 是 |

`C:name` 依赖驱动器 C 的当前目录，`\name` 依赖当前驱动器，所以二者都不能被 `xrtPathIsAbs` 误判成完整绝对路径。

```c
typedef struct xpathparts {
	xstrview Root;
	xstrview Parent;
	xstrview Name;
	xstrview Stem;
	xstrview Ext;
	xpathroot RootKind;
	uint32 Flags;
} xpathparts;

bool xrtPathParse(xstrview Path, xpathstyle Style, xpathparts* pParts);
```

`xrtPathParse` 零分配地分解路径。失败不修改 `*pParts`。`Ext` 包含前导点；只按最后一个点分割扩展名；`.gitignore` 的 `Stem` 是完整名称，`Ext` 为空。尾部分隔符表示目录语法，不产生虚假的空名称。

```c
xpathparts Parts;

if ( xrtPathParse(XRT_STR_LITERAL("archive/data.tar.gz"),
	XPATH_POSIX, &Parts) ) {
	/* Name=data.tar.gz, Stem=data.tar, Ext=.gz */
}
```

需要逐段处理路径时，不必重新扫描或分配字符串：

```c
typedef enum xpathcomponentkind {
	XPATH_COMPONENT_ROOT = 1,
	XPATH_COMPONENT_CURRENT,
	XPATH_COMPONENT_PARENT,
	XPATH_COMPONENT_NORMAL
} xpathcomponentkind;

typedef struct xpathcomponent {
	xstrview Text;
	xpathcomponentkind Kind;
} xpathcomponent;

typedef struct xpathiter {
	xstrview Path;
	size_t Position;
	size_t RootSize;
	xpathstyle Style;
	uint32 State;
} xpathiter;

bool xrtPathIterInit(xpathiter* pIterator,
	xstrview Path, xpathstyle Style);
bool xrtPathNext(xpathiter* pIterator, xpathcomponent* pComponent);
```

迭代器先返回根，再依次返回 `.`、`..` 和普通名称；重复分隔符不会制造空组件。组件视图借用原输入，`xpathiter` 的公开字段只用于栈上保存状态，不由调用方修改。初始化失败不修改迭代器；`xrtPathNext` 返回 `false` 表示遍历结束，迭代器或输出参数无效时会设置统一参数/状态错误。

## 常用分解函数

```c
str xrtPathName(cstr sPath);
str xrtPathStem(cstr sPath);
str xrtPathExt(cstr sPath);
str xrtPathParent(cstr sPath);
bool xrtPathIsAbs(cstr sPath);
bool xrtPathIsRoot(cstr sPath);
bool xrtPathIsRooted(cstr sPath);
bool xrtPathIsLocal(xstrview Path, xpathstyle Style);
```

四个字符串函数按本机风格解析并返回拥有副本。没有对应字段时返回已分配的空字符串，不用静态空串制造所有权例外。`xrtPathIsAbs` 只接受完整绝对路径；`xrtPathIsRoot` 只接受 `/`、`C:\`、完整 UNC 根等完整文件系统根；`xrtPathIsRooted` 还接受 Windows 驱动器相对和根相对路径。路径谓词都是纯词法判断，不查询路径是否存在。

`xrtPathIsLocal` 判断路径能否安全拼入任意基目录：路径必须非空、不能带根，清理过程中的 `..` 不能越过起点。Windows 风格还拒绝驱动器语法、备用数据流和设备保留名。它适合先检查归档子路径、静态资源键和用户提供的相对名称；它只提供词法包含保证，目录中的符号链接或挂载点仍可能改变最终位置。

```c
str sName = xrtPathName("logs/server.log");
str sExt = xrtPathExt("logs/server.log");

xrtFree(sName);
xrtFree(sExt);
```

## 清理和拼接

```c
str xrtPathClean(xstrview Path, xpathstyle Style);
str xrtPathBuild(const xstrview* arrParts, size_t iCount, xpathstyle Style);
str xrtPathJoin(cstr sLeft, cstr sRight);
```

`xrtPathClean` 是纯词法操作：

- 合并重复分隔符。
- 删除 `.` 段。
- 在不越过根的前提下折叠 `..`。
- 保留相对路径开头无法折叠的 `..`。
- 空路径清理为 `.`。
- Windows 设备命名空间原样复制，不对其特殊语义做破坏性清理。

它不访问文件系统，不解析符号链接，不判断目标是否存在，也不改变大小写。

`xrtPathBuild` 拼接任意数量的路径并清理结果。后续带根项会替换之前内容；Windows 根相对项（`\name`）会保留左侧已有的驱动器或 UNC 卷前缀。驱动器相对前缀 `C:` 与普通名称拼接后仍是 `C:name`，不会被错误升级为 `C:\name`。零项结果为 `.`。`xrtPathJoin` 是按本机风格拼接两个零结尾字符串的常用 Helper。三者都使用动态容量，没有旧版 4094 字节限制；构建结果会原地清理，常见拼接只需要一块动态缓冲。

```c
xstrview arrParts[] = {
	XRT_STR_LITERAL("var"),
	XRT_STR_LITERAL("cache"),
	XRT_STR_LITERAL("../run/app.pid")
};
str sPath = xrtPathBuild(arrParts, 3, XPATH_POSIX);

/* sPath == "var/run/app.pid" */
xrtFree(sPath);
```

## 相对路径

```c
str xrtPathRelative(xstrview Base, xstrview Target, xpathstyle Style);
str xrtPathRel(cstr sBase, cstr sTarget);
```

`xrtPathRelative` 把 `Base` 当作目录，纯词法计算到 `Target` 的相对路径。两端先清理；根类型或根值不同时返回 `XPATH_ERROR_ROOT`。相同路径返回 `.`。

Windows 风格的根和路径段按 ASCII 大小写不敏感比较，POSIX 风格逐字节比较。该规则只表达常规 Windows 词法语义，不查询单个目录是否启用了大小写敏感属性。

相对输入开头无法折叠的 `..` 表示未知父目录。若 `Base` 比 `Target` 退得更深，例如从 `../../a` 到 `../b`，纯词法信息不足以写出正确结果，函数返回 `XPATH_ERROR_ROOT`，而不是生成一个无法满足重新拼接逆关系的字符串。Windows 设备命名空间会关闭普通点段解析，因此也不参与普通相对路径计算。

`xrtPathRel` 属于系统层，先按操作系统规则把两端转成绝对路径，再调用纯词法原语。它同样不解析符号链接，因此比较的是路径表达，不是文件身份。

```c
str sRel = xrtPathRelative(XRT_STR_LITERAL("/srv/app"),
	XRT_STR_LITERAL("/srv/data/file"), XPATH_POSIX);

/* sRel == "../data/file" */
xrtFree(sRel);
```

## 修改名称

```c
str xrtPathWithName(cstr sPath, cstr sName);
str xrtPathWithExt(cstr sPath, cstr sExtension);
```

`xrtPathWithName` 替换末级名称，并允许新名称包含相对路径段；新名称带根时按 `xrtPathBuild` 规则替换旧父路径。`xrtPathWithExt` 只替换最后一个扩展名，非空扩展名可省略前导点，空扩展名删除扩展名；扩展名不得包含路径分隔符。

```c
str sPath = xrtPathWithExt("archive/data.tar.gz", "zip");

/* sPath == "archive/data.tar.zip" */
xrtFree(sPath);
```

## 系统路径

启用 `XRT_FEATURE_PATH_SYSTEM` 后提供：

```c
str xrtPathCwd(void);
bool xrtPathSetCwd(cstr sPath);
str xrtPathAbs(cstr sPath);
str xrtPathReal(cstr sPath);
str xrtPathRel(cstr sBase, cstr sTarget);
str xrtPathHome(void);
str xrtPathTemp(void);
str xrtPathExecutable(void);
str xrtPathAppDir(void);
```

- `xrtPathCwd` 动态读取当前工作目录，不依赖 `MAX_PATH` 或 `PATH_MAX`。
- `xrtPathSetCwd` 修改进程级当前目录，会影响其他线程，库代码不应把它当作局部状态。
- `xrtPathAbs` 使用操作系统规则解释驱动器相对等本机路径，但不要求目标存在；空路径表示当前工作目录。
- `xrtPathReal` 要求目标存在，跟随符号链接和 Windows 重解析点，返回操作系统确认的物理绝对路径。Windows 结果可能保留 `\\?\` 设备前缀；POSIX 结果由 `realpath` 解析。
- `xrtPathHome` 返回当前用户目录，不用当前目录伪装缺失结果。
- `xrtPathTemp` 返回系统临时目录；它不是一个已经安全创建的临时对象。
- `xrtPathExecutable` 返回当前可执行文件的绝对 UTF-8 路径。
- `xrtPathAppDir` 返回可执行文件所在目录，不等同于当前工作目录。

Windows 系统调用使用宽字符 API 和严格 UTF-8 转换。动态查询不使用固定长度缓冲。系统查询失败会在错误中保留原始 Win32 或 `errno` 代码。

`xrtPathReal` 适合比较已存在对象的物理位置、诊断链接和做目录树操作的前置校验，但它本身不是安全沙箱：查询完成后路径仍可能被其他线程或进程替换。需要抵抗恶意并发替换时，应使用后续文件层的目录句柄相对操作和禁止跟随链接选项。

## 安全条目

需要在 percent 解码、归档流或其他分块输入中逐段检查时，可以直接使用同一套
可移植段状态机：

```c
#define XPATH_SAFE_SEGMENT_STORAGE_SIZE 40u

typedef union xpathsafesegment {
	uint64 Alignment;
	uint8 Storage[XPATH_SAFE_SEGMENT_STORAGE_SIZE];
} xpathsafesegment;

void xrtPathSafeSegmentInit(xpathsafesegment* pState);
bool xrtPathSafeSegmentFeed(xpathsafesegment* pState, uint8 iValue);
bool xrtPathSafeSegmentFinish(const xpathsafesegment* pState);
```

状态机不分配内存，`Feed` 接收已经解码的字节，`Finish` 统一检查空段、点段、
尾随点或空格和 Windows 设备保留名。UTF-8 完整性和路径分隔符仍由外层流解析器
处理；因此它是段级原语，不替代完整入口检查。

```c
bool xrtPathIsSafeEntry(xstrview Path, bool bDirectory);
```

该函数检查一个归档、包或静态资源条目是否为跨 Windows/POSIX 可移植的相对 UTF-8 路径。它拒绝：

- 空路径、绝对路径、驱动器路径、反斜杠和空段。
- `.`、`..` 与父目录穿越。
- 嵌入零字节、无效 UTF-8、控制字符和 Windows 禁用字符。
- 尾随点或空格。
- `CON`、`NUL`、`COM1`、`LPT1` 等 Windows 设备保留名及其扩展名形式。

`bDirectory` 为 `true` 时允许末尾 `/`，但不要求必须带末尾 `/`；为 `false` 时末尾 `/` 被拒绝。

```c
bool bImage = xrtPathIsSafeEntry(
	XRT_STR_LITERAL("assets/icons/app.png"), false);
bool bTraversal = xrtPathIsSafeEntry(
	XRT_STR_LITERAL("../secret.txt"), false);
```

这是词法入口检查，不是完整文件系统沙箱。攻击者仍可能借助目标目录内已有符号链接或挂载点逃逸。安全解包和静态文件服务还必须使用后续文件模块提供的“相对可信目录句柄打开、禁止跟随符号链接、验证最终对象”能力。

## 错误

```c
typedef enum xpatherror {
	XPATH_ERROR_FORMAT = 1,
	XPATH_ERROR_OVERFLOW,
	XPATH_ERROR_ROOT,
	XPATH_ERROR_SYSTEM
} xpatherror;
```

路径错误域为 `xrt.path`：

- `FORMAT`：嵌入零字节、无末级名称或非法扩展名等格式错误。
- `OVERFLOW`：路径长度或容量计算不能表达。
- `ROOT`：相对路径两端根不兼容。
- `SYSTEM`：系统查询失败，`SystemCode` 保存平台错误码。

内存分配失败继续使用统一的 `XERR_MEMORY`，参数错误使用统一参数错误，不重复制造路径私有错误类别。

## 旧版迁移

| 旧版能力 | 新 API | 处理 |
| --- | --- | --- |
| `xrtPathParse` | `xrtPathParse` | 保留名称，改为零分配视图和明确根类型 |
| `xrtPathJoin` | `xrtPathJoin` / `xrtPathBuild` | 移除固定缓冲，增加多段与显式风格 |
| `xrtPathNormalize` | `xrtPathClean` | 明确为纯词法清理 |
| `xrtPathAbs` | `xrtPathAbs` | 动态系统查询，严格错误和 UTF 转换 |
| `xrtPathRelative` | `xrtPathRelative` / `xrtPathRel` | 拆分纯词法和系统绝对化两层 |
| `xrtPathGetNameExt` | `xrtPathName` | 使用简短统一命名 |
| `xrtPathGetName` | `xrtPathStem` | 避免“Name 是否含扩展名”的歧义 |
| `xrtPathGetExt` | `xrtPathExt` | 扩展名包含前导点 |
| `xrtPathGetDir` | `xrtPathParent` | 使用明确的父路径语义 |
| `xrtPathIsSafeArchive` | `xrtPathIsSafeEntry` | 补齐 UTF-8、设备名和可移植字符边界 |
| `xrtPathRandom` | 后续原子临时文件/目录 API | 删除只生成名称却承诺唯一性的 TOCTOU 设计 |

旧版 `xrtPathRandom` 不进入路径层。安全临时资源必须由文件层一次系统调用或带排他创建循环完成“选名并创建”，不能先猜一个不存在的路径再打开。

## 完整示例

- `examples/path/basic/main.c`：分解、修改名称和扩展名。
- `examples/path/system/main.c`：当前目录、用户目录、临时目录和程序目录。
- `examples/path/safe/main.c`：归档与静态资源入口校验。
