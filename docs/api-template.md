# Template 模板引擎库

> XTE (X Template Engine) 轻量级模板引擎

[English](api-template.en.md) | [中文](api-template.md) | [返回索引](README.md)

---

## 📑 目录

- [模板语法](#模板语法)
- [表达式语法](#表达式语法)
- [数据结构](#数据结构)
- [词法分析](#词法分析)
- [路径解析](#路径解析)
- [表达式解析](#表达式解析)
- [模板解析](#模板解析)
- [模板生成](#模板生成)
- [高级特性](#高级特性)

---

## 模板语法

### Token类型

```c
// 最大支持参数数量
#define XTE_PARAM_MAXCOUNT  6

// 基础Token
#define XTE_TK_TEXT         0       // 文本内容
#define XTE_TK_COMMEN       1       // 注释: {! * }
#define XTE_TK_VAR          0x100   // 代入变量: {$ * : *}
#define XTE_TK_NUM          0x101   // 代入数字变量: {% * : *}
#define XTE_TK_TIME         0x102   // 代入时间变量: {& * : *}
#define XTE_TK_BOOL         0x103   // 根据逻辑代入值: {? * : * : *}
#define XTE_TK_ARR          0x104   // 代入数组: {* * : *}
#define XTE_TK_PROC         0x105   // 代入函数或流程: {@ * : * ...}
#define XTE_TK_SUBTEMPLATE  0x106   // 代入子模板: {= * : *}
#define XTE_TK_SYMBOL       0xFFFF  // 预定义符号: {# * : *}
#define XTE_MODE_BLOCK      0xFFFE  // 数据块采集模式（以 {#end} 结尾）

// Token 扩展编号
#define XTE_TK_INCLUDE      0x10000 // 引用外部文件: {#include *}
#define XTE_TK_DEFINE       0x10001 // 定义子模板: {#define *}
#define XTE_TK_SCRIPT       0x10002 // 脚本块
#define XTE_TK_IF           0x20000 // 判断语句: {#if *}
#define XTE_TK_ELSEIF       0x20001 // {#elseif *}
#define XTE_TK_ELSE         0x20002 // {#else}
#define XTE_TK_FOR          0x30000 // 循环语句: {#for *}
#define XTE_TK_FOREACH      0x30001 // 迭代循环语句: {#foreach *}
#define XTE_TK_BREAK        0x30002 // 跳出循环: {#break}
#define XTE_TK_CONTINUE     0x30003 // 继续下一轮: {#continue}
#define XTE_TK_END          0xFFFFFF // 语句结束: {#end}
#define XTE_TK_USER         0x1000000 // 用户自定义扩展起始编号

// 标识符类型
#define XTE_IDTPE_DEFAULT   0       // 单语句
#define XTE_IDTPE_BLOCK     1       // 独立语句块（以 {#end} 结尾）
```

---

### 基本语法

#### 变量替换

```
姓名：{$ name : 未知}
年龄：{% age : 0}
时间：{& createTime : YYYY-MM-DD}
```

**说明：**
- `{$ name : 未知}` - 字符串变量，默认值"未知"
- `{% age : 0}` - 数字变量，默认值0
- `{& time : format}` - 时间变量，指定格式

---

#### 条件判断

条件语句支持完整的表达式计算：

```
{#if:age > 18}
  用户已成年
{#elseif:age > 12}
  青少年
{#else}
  儿童
{#end}

{#if:score >= 90}
  A等级
{#elseif:score >= 60}
  及格
{#else}
  不及格
{#end}
```

**支持的运算符：**
- 比较: `=`, `!=`, `~=`(约等于), `>`, `<`, `>=`, `<=`
- 逻辑: `and`, `or`, `not`
- 括号: `(`, `)`

---

#### 计次循环 (for)

```
{#for:起始:结束:步长}
  {$ __index__}
{#end}

{#for:1:5:1}
  第 {% __index__ } 项
{#end}

{#for:10:1:-1}
  倒计时: {% __index__ }
{#end}
```

---

#### 迭代循环 (foreach)

```
{#foreach:users}
  {$ name} - {$ email}
{#end}

{#foreach:items}
  索引: {% __index__ }
  当前元素属性: {$ name}
{#end}
```

---

#### 循环控制 (break/continue)

在 `for` 和 `foreach` 循环中可以使用控制语句：

- `{#break}` - 立即跳出当前循环
- `{#continue}` - 跳过本次迭代，进入下一轮循环

```
{! for 循环中使用 break }
{#for:1:10:1}
  {#if:__index__ > 3}
    {#break}
  {#end}
  {%__index__} 
{#end}
输出: 1 2 3

{! for 循环中使用 continue }
{#for:1:5:1}
  {#if:__index__ = 3}
    {#continue}
  {#end}
  {%__index__},
{#end}
输出: 1,2,4,5, (跳过了3)

{! foreach 循环中使用 break }
{#foreach:items}
  {#if:__index__ = 2}
    {#break}
  {#end}
  {$ name},
{#end}

{! foreach 循环中使用 continue }
{#foreach:items}
  {#if:skip = 1}
    {#continue}
  {#end}
  [{$ name}]
{#end}
```

**注意：** `{#break}` 和 `{#continue}` 只能在循环内部使用，且仅影响最内层循环。

---

#### 内置变量

| 变量 | 说明 | 可用范围 |
|------|------|----------|
| `__self__` | 当前代入值 | 子模板内部 |
| `__index__` | 当前循环索引 | `for`、`foreach` 循环内部 |
| `__value__` | 当前迭代值 | `foreach` 循环内部 |
| `__key__` | 当前迭代键名（遍历表时） | `foreach` 循环内部 |

**示例：**
```
{#define item}
  当前值：{$ __self__}
{#end}

{#foreach:items}
  索引：{% __index__}，值：{$ name}
{#end}
```

---

#### 注释

```
{! 这是注释，不会输出 }
```

---

#### 标签内空格

模板标签内部支持空格，解析器会自动 trim：

```
{$  name  }           等价于 {$name}
{%  price  :  .2  }   等价于 {%price:.2}
{&  time  :  yyyy-mm-dd  }   等价于 {&time:yyyy-mm-dd}
```

**注意：** 起始符与类型符（如 `{$`、`{%`、`{#`）必须连写。

---

## 表达式语法

条件语句 `{#if}`、`{#elseif}` 支持完整的表达式计算。

### 运算符

| 运算符 | 说明 | 示例 |
|--------|------|------|
| `=` | 等于 | `age = 18` |
| `!=` | 不等于 | `status != "active"` |
| `~=` | 约等于（数字/字符串/时间） | `price ~= 100` |
| `>` | 大于 | `score > 60` |
| `<` | 小于 | `count < 10` |
| `>=` | 大于等于 | `level >= 5` |
| `<=` | 小于等于 | `age <= 65` |
| `and` | 逻辑与 | `age > 18 and vip` |
| `or` | 逻辑或 | `admin or manager` |
| `not` | 逻辑非 | `not disabled` |
| `(` `)` | 括号分组 | `(a or b) and c` |

### 字面量

```
数字：   100, 3.14, -50
字符串： "hello", 'world'
布尔值： true, false
```

### 变量路径

支持点号访问和数组索引：

```
user.name           嵌套属性访问
user.profile.age    多层嵌套
items[0]            数组索引
items[0].title      数组元素属性
```

### 表达式示例

```
{#if:age >= 18 and active}
  成年活跃用户
{#end}

{#if:score >= 90 or vip}
  优秀用户
{#end}

{#if:not disabled and (role = "admin" or role = "manager")}
  管理员
{#end}

{#if:user.profile.level > 5}
  高级用户
{#end}
```

---

## 数据结构

### XTE_IdentInfo - 标识符信息

用于定义自定义标识符。

```c
typedef struct {
    char* Ident;              // 标识符
    uint32 TokenIndex;        // 对应的 Token 编号
    unsigned short Type;      // 0 = 单语句、1 = 独立语句块(以 {#end} 结尾)
    unsigned short Size;      // 标识符长度
    unsigned short MinParamCount; // 最小参数数量
    unsigned short MaxParamCount; // 最大参数数量
    uint32 Hash;              // 标识符哈希值
} XTE_IdentInfo_Struct, *XTE_IdentInfo;
```

---

### XTE_TokenItem - Token 项

表示解析后的单个 Token。

```c
typedef struct {
    uint32 Type;                          // Token 定义编号
    char* Text;                           // 关联文本
    size_t Size;                          // 关联文本长度
    uint32 ParamCount;                    // 参数数量
    char* ParamText[XTE_PARAM_MAXCOUNT];  // 参数文本
    uint32 ParamSize[XTE_PARAM_MAXCOUNT]; // 参数长度
    XTE_IdentInfo IdentInfo;              // 标识符语句对应的标识符信息结构体指针
    uint32 RefLine;                       // 语句在源文件中所在行
    uint32 RefLinePos;                    // 语句在源文件中所在行的位置
    uint32 RefPos;                        // 语句在源文件中所在的位置
    uint32 RefSize;                       // 语句在源文件中的长度
} XTE_TokenItem_Struct, *XTE_TokenItem;
```

---

### XTE_TokenList - Token 列表

词法分析返回的结果结构体。

```c
typedef struct {
    int Success;              // 解析是否成功
    int ErrorCode;            // 错误代码（0=成功）
    const char* ErrorDesc;    // 错误描述
    uint32 ErrorLine;         // 错误行号
    uint32 ErrorLinePos;      // 错误行位置
    uint32 ErrorPos;          // 错误位置
    uint32 ErrorRefLine;      // 出错参考行
    uint32 ErrorRefLinePos;   // 出错参考行位置
    uint32 ErrorRefPos;       // 错误参考位置
    xarray_struct Tokens;     // Token 列表
} XTE_TokenList_Struct, *XTE_TokenList;
```

---

### XTE_LiteObject - 模板对象

编译后的模板对象结构体。

```c
typedef struct {
    int Success;              // 解析是否成功
    int ErrorCode;            // 错误代码（0=成功）
    const char* ErrorDesc;    // 错误描述
    uint32 ErrorLine;         // 错误行号
    uint32 ErrorLinePos;      // 错误行位置
    uint32 ErrorPos;          // 错误位置
    uint32 ErrorRefLine;      // 出错参考行
    uint32 ErrorRefLinePos;   // 出错参考行位置
    uint32 ErrorRefPos;       // 错误参考位置
    xarray Tokens;            // Token 列表
    xparray_struct Actions;   // 编译后的动作列表
    xdict_struct SubTemplates;// 子模板列表（哈希表）
} XTE_LiteStruct, *XTE_LiteObject;
```

---

## 词法分析

### xteCreateIdentList

创建关键字列表，用于自定义扩展。

**函数原型：**
```c
XXAPI xarray xteCreateIdentList();
```

**返回值：**
- 成功：关键字列表
- 失败：`NULL`

---

### xteDestroyIdentList

销毁关键字列表。

**函数原型：**
```c
XXAPI void xteDestroyIdentList(xarray objList);
```

---

### xteAddIdentToList

添加一个关键字到列表。

**函数原型：**
```c
XXAPI int xteAddIdentToList(
    xarray objList,
    char* sID,
    uint32 iSize,
    uint32 iIndex,
    uint32 iType,
    uint32 iMinParamCount,
    uint32 iMaxParamCount
);
```

**参数：**
- `objList` - 关键字列表
- `sID` - 标识符字符串
- `iSize` - 标识符长度（0 自动计算）
- `iIndex` - 对应的 Token 编号
- `iType` - 标识符类型（`XTE_IDTPE_DEFAULT` 或 `XTE_IDTPE_BLOCK`）
- `iMinParamCount` - 最小参数数量
- `iMaxParamCount` - 最大参数数量

**返回值：**
- 成功：新增项的索引
- 失败：`0`

**示例：**
```c
xarray identList = xteCreateIdentList();
xteAddIdentToList(identList, "myTag", 0, XTE_TK_USER, XTE_IDTPE_DEFAULT, 1, 3);
xteAddIdentToList(identList, "myBlock", 0, XTE_TK_USER + 1, XTE_IDTPE_BLOCK, 0, 1);
// 使用 identList 进行词法分析...
xteDestroyIdentList(identList);
```

---

### xteLexer

解析模板文件为 Token 列表。

**函数原型：**
```c
XXAPI XTE_TokenList xteLexer(
    char* sText,
    size_t iSize,
    xarray objIdentList,
    char* sBracket
);
```

**参数：**
- `sText` - 模板文本
- `iSize` - 长度（0 自动计算）
- `objIdentList` - 标识符列表（支持 `{#xxx}` 语法，可传 `NULL`）
- `sBracket` - 括号字符（如 `"{}"` 或 `NULL` 使用默认）

**返回值：**
- `XTE_TokenList` 结构体指针

**释放：** ✅ 需要 `xteLexerFree` 释放

---

### xteLexerFree

释放 Token 列表。

**函数原型：**
```c
XXAPI void xteLexerFree(XTE_TokenList arrToken);
```

---

### xteParseFromTokenList

将 Token 列表转换为模板对象。

**函数原型：**
```c
XXAPI XTE_LiteObject xteParseFromTokenList(XTE_TokenList objToks);
```

**参数：**
- `objToks` - Token 列表（调用后将被释放）

**返回值：**
- `XTE_LiteObject` 模板对象指针

**注意：** 传入的 `objToks` 会被函数释放，调用后不可再使用。

---

## 路径解析

### xteResolvePath

解析变量路径，支持点号嵌套和数组索引。

**函数原型：**
```c
XXAPI xvalue xteResolvePath(
    const char* path,
    size_t pathLen,
    xvalue tblVal,
    xvalue tblRoot,
    xvalue tblENV
);
```

**参数：**
- `path` - 路径字符串（如 `"user.profile.name"` 或 `"items[0].title"`）
- `pathLen` - 路径长度（传0则自动计算）
- `tblVal` - 当前作用域
- `tblRoot` - 根作用域（可传 `NULL`）
- `tblENV` - 环境变量（可传 `NULL`）

**返回值：**
- 成功：解析到的 `xvalue`
- 失败：`&XVO_VALUE_NULL`

**路径查找顺序：**
1. 先在 `tblVal` 中查找
2. 如未找到且 `tblRoot != NULL`，在 `tblRoot` 中查找
3. 如未找到且 `tblENV != NULL`，在 `tblENV` 中查找

**示例：**
```c
// 创建数据结构
xvalue data = xvoCreateTable();
xvalue user = xvoCreateTable();
xvalue profile = xvoCreateTable();

xvoTableSetText(profile, "name", 0, "张三", 0, FALSE);
xvoTableSetInt(profile, "age", 0, 25);
xvoTableSetValue(user, "profile", 0, profile, TRUE);
xvoTableSetValue(data, "user", 0, user, TRUE);

// 简单路径
xvalue v1 = xteResolvePath("user", 0, data, NULL, NULL);
// v1 -> user 表

// 点号访问
xvalue v2 = xteResolvePath("user.profile.name", 0, data, NULL, NULL);
str name = xvoGetText(v2);  // "张三"

// 数组索引
xvalue items = xvoCreateArray();
xvalue item0 = xvoCreateTable();
xvoTableSetText(item0, "title", 0, "第一项", 0, FALSE);
xvoArrayAppendValue(items, item0, TRUE);
xvoTableSetValue(data, "items", 0, items, TRUE);

xvalue v3 = xteResolvePath("items[0].title", 0, data, NULL, NULL);
str title = xvoGetText(v3);  // "第一项"

xvoUnref(data);
```

---

## 表达式解析

### xteExprEvalBool

解析并求值表达式，返回布尔结果。

**函数原型：**
```c
XXAPI int xteExprEvalBool(
    const char* expr,
    size_t len,
    xvalue tblVal,
    xvalue tblRoot,
    xvalue tblENV
);
```

**参数：**
- `expr` - 表达式字符串
- `len` - 表达式长度（传0则自动计算）
- `tblVal` - 当前作用域（用于变量查找）
- `tblRoot` - 根作用域（可传 `NULL`）
- `tblENV` - 环境变量（可传 `NULL`）

**返回值：**
- 真：非0值
- 假：0

**示例：**
```c
xvalue data = xvoCreateTable();
xvoTableSetInt(data, "age", 0, 25);
xvoTableSetText(data, "name", 0, "Alice", 0, FALSE);
xvoTableSetBool(data, "active", 0, TRUE);
xvoTableSetFloat(data, "score", 0, 85.5);

// 比较运算
int r1 = xteExprEvalBool("age = 25", 0, data, NULL, NULL);      // 1
int r2 = xteExprEvalBool("age > 18", 0, data, NULL, NULL);      // 1
int r3 = xteExprEvalBool("score >= 90", 0, data, NULL, NULL);   // 0

// 字符串比较
int r4 = xteExprEvalBool("name = \"Alice\"", 0, data, NULL, NULL); // 1
int r5 = xteExprEvalBool("name != \"Bob\"", 0, data, NULL, NULL);  // 1

// 逻辑运算
int r6 = xteExprEvalBool("age > 18 and active", 0, data, NULL, NULL);   // 1
int r7 = xteExprEvalBool("age < 18 or active", 0, data, NULL, NULL);    // 1
int r8 = xteExprEvalBool("not active", 0, data, NULL, NULL);            // 0

// 括号
int r9 = xteExprEvalBool("(age > 20) and (score > 80)", 0, data, NULL, NULL); // 1

// 字面量
int r10 = xteExprEvalBool("true", 0, NULL, NULL, NULL);   // 1
int r11 = xteExprEvalBool("100 > 50", 0, NULL, NULL, NULL); // 1

xvoUnref(data);
```

---

## 模板解析

### xteParse

解析模板文本

**函数原型：**
```c
XXAPI XTE_LiteObject xteParse(char* sText, size_t iSize, char* sBracket);
```

**参数：**
- `sText` - 模板文本
- `iSize` - 长度（0自动）
- `sBracket` - 括号字符（通常为 `"{}"` 或 `NULL` 使用默认）

**返回值：**
- 成功：模板对象指针
- 失败：`NULL`

**释放：** ✅ 需要 `xteParseFree` 释放

**示例：**
```c
str template_text = 
    "Hello {$ name}!\n"
    "{#if vip}\n"
    "  您是VIP用户\n"
    "{#end}";

XTE_LiteObject tpl = xteParse(template_text, 0, NULL);
if (tpl) {
    if (!tpl->Success) {
        printf("Parse error: %s\n", tpl->ErrorDesc);
        printf("Line: %u, Pos: %u\n", tpl->ErrorLine, tpl->ErrorPos);
    }
    xteParseFree(tpl);
}
```

---

### xteParseFree

释放模板对象

**函数原型：**
```c
XXAPI void xteParseFree(XTE_LiteObject objLite);
```

---

## 模板生成

### xteMakeActions

根据动作列表生成文档（底层函数）。

**函数原型：**
```c
XXAPI char* xteMakeActions(
    xparray arrAction,
    XTE_LiteObject objTemplate,
    xvalue tblVal,
    xvalue tblRoot,
    xvalue tblENV,
    xdict tblInclude,
    size_t* pRetSize
);
```

**参数：**
- `arrAction` - 动作列表
- `objTemplate` - 模板对象
- `tblVal` - 当前数据表
- `tblRoot` - 根数据表（用于子模板访问父级数据）
- `tblENV` - 环境变量表（可选，传 `NULL`）
- `tblInclude` - 包含的模板字典（可选，传 `NULL`）
- `pRetSize` - 输出：生成文本的长度

**返回值：**
- 成功：生成的文本
- 失败：`NULL`

**内存释放：** ✅ 需要 `xrtFree` 释放

---

### xteMake

根据模板生成文档

**函数原型：**
```c
XXAPI char* xteMake(
    XTE_LiteObject objTemplate,
    xvalue tblVal,
    xvalue tblENV,
    xdict tblInclude,
    size_t* pRetSize
);
```

**参数：**
- `objTemplate` - 模板对象
- `tblVal` - 数据表（xvalue Table 或 Array 类型）
- `tblENV` - 环境变量表（可选，传 `NULL`）
- `tblInclude` - 包含的模板字典（可选，传 `NULL`）
- `pRetSize` - 输出：生成文本的长度

**返回值：**
- 成功：生成的文本
- 失败：`NULL`

**内存释放：** ✅ 需要 `xrtFree` 释放

**示例：**
```c
// 准备模板
str template_text = 
    "用户信息：\n"
    "姓名：{$ name : 未知}\n"
    "年龄：{% age : 0}\n"
    "{#if vip}\n"
    "VIP等级：{% vipLevel}\n"
    "{#end}";

XTE_LiteObject tpl = xteParse(template_text, 0, NULL);
if (!tpl || !tpl->Success) {
    printf("Template parse error\n");
    return;
}

// 准备数据
xvalue data = xvoCreateTable();
xvoTableSet(data, "name", xvoCreateText("张三", 0, FALSE));
xvoTableSet(data, "age", xvoCreateInt(28));
xvoTableSet(data, "vip", xvoCreateBool(TRUE));
xvoTableSet(data, "vipLevel", xvoCreateInt(5));

// 生成文档
size_t size;
str result = xteMake(tpl, data, NULL, NULL, &size);

if (result) {
    printf("%s\n", result);
    xrtFree(result);
}

// 清理
xvoUnref(data);
xteParseFree(tpl);
```

**输出：**
```
用户信息：
姓名：张三
年龄：28
VIP等级：5
```

---

## 高级特性

### 1. 循环数组

```c
str template_text = 
    "{#foreach:users}\n"
    "  {$ name} - {$ email}\n"
    "{#end}";

XTE_LiteObject tpl = xteParse(template_text, 0, NULL);

// 准备数据
xvalue users = xvoCreateArray();

xvalue user1 = xvoCreateTable();
xvoTableSetText(user1, "name", 0, "Alice", 0, FALSE);
xvoTableSetText(user1, "email", 0, "alice@example.com", 0, FALSE);
xvoArrayAppendValue(users, user1, TRUE);

xvalue user2 = xvoCreateTable();
xvoTableSetText(user2, "name", 0, "Bob", 0, FALSE);
xvoTableSetText(user2, "email", 0, "bob@example.com", 0, FALSE);
xvoArrayAppendValue(users, user2, TRUE);

xvalue data = xvoCreateTable();
xvoTableSetValue(data, "users", 0, users, TRUE);

// 生成
size_t size;
str result = xteMake(tpl, data, NULL, NULL, &size);
printf("%s", result);

xrtFree(result);
xvoUnref(data);
xteParseFree(tpl);
```

**输出：**
```
  Alice - alice@example.com
  Bob - bob@example.com
```

---

### 2. 计次循环

```c
str template_text = 
    "{#for:1:5:1}\n"
    "  第 {% __index__} 项\n"
    "{#end}";

XTE_LiteObject tpl = xteParse(template_text, 0, NULL);
xvalue data = xvoCreateTable();

size_t size;
str result = xteMake(tpl, data, NULL, NULL, &size);
printf("%s", result);

xrtFree(result);
xvoUnref(data);
xteParseFree(tpl);
```

**输出：**
```
  第 1 项
  第 2 项
  第 3 项
  第 4 项
  第 5 项
```

---

### 3. 嵌套控制语句

```c
str template_text = 
    "{#foreach:users}\n"
    "  {#if:active}\n"
    "    {$ name} (活跃)\n"
    "  {#else}\n"
    "    {$ name} (非活跃)\n"
    "  {#end}\n"
    "{#end}";

XTE_LiteObject tpl = xteParse(template_text, 0, NULL);

// 准备数据
xvalue users = xvoCreateArray();

xvalue u1 = xvoCreateTable();
xvoTableSetText(u1, "name", 0, "Alice", 0, FALSE);
xvoTableSetBool(u1, "active", 0, TRUE);
xvoArrayAppendValue(users, u1, TRUE);

xvalue u2 = xvoCreateTable();
xvoTableSetText(u2, "name", 0, "Bob", 0, FALSE);
xvoTableSetBool(u2, "active", 0, FALSE);
xvoArrayAppendValue(users, u2, TRUE);

xvalue data = xvoCreateTable();
xvoTableSetValue(data, "users", 0, users, TRUE);

size_t size;
str result = xteMake(tpl, data, NULL, NULL, &size);
printf("%s", result);

xrtFree(result);
xvoUnref(data);
xteParseFree(tpl);
```

**输出：**
```
    Alice (活跃)
    Bob (非活跃)
```

---

### 4. 子模板

```c
str template_text = 
    "{#define:userCard}\n"
    "  姓名：{$ name}\n"
    "  年龄：{% age}\n"
    "{#end}\n"
    "\n"
    "{#foreach:users}\n"
    "  {= userCard}\n"
    "{#end}";

XTE_LiteObject tpl = xteParse(template_text, 0, NULL);

// 数据准备...
// 使用 {= userCard} 调用子模板
```

**子模板特殊变量：**
- 在子模板内部可以使用 `{$ __self__}` 访问代入值
- 子模板不可嵌套定义

---

### 5. 包含外部模板

```c
// 主模板
str main_template = 
    "{#include:header}\n"
    "主要内容\n"
    "{#include:footer}";

// 准备包含的模板
xdict includes = xdictCreate();

str header_tpl_text = "<header>网站头部</header>";
XTE_LiteObject header_tpl = xteParse(header_tpl_text, 0, NULL);
xdictSet(includes, "header", header_tpl);

str footer_tpl_text = "<footer>网站底部</footer>";
XTE_LiteObject footer_tpl = xteParse(footer_tpl_text, 0, NULL);
xdictSet(includes, "footer", footer_tpl);

// 主模板
XTE_LiteObject main_tpl = xteParse(main_template, 0, NULL);

// 生成（传入 includes）
size_t size;
str result = xteMake(main_tpl, NULL, NULL, includes, &size);

printf("%s\n", result);

// 清理
xrtFree(result);
xteParseFree(main_tpl);
xteParseFree(header_tpl);
xteParseFree(footer_tpl);
xdictFree(includes);
```

---

## 使用场景

### 1. HTML生成

```c
str html_template = 
    "<!DOCTYPE html>\n"
    "<html>\n"
    "<head><title>{$ title}</title></head>\n"
    "<body>\n"
    "  <h1>{$ title}</h1>\n"
    "  <ul>\n"
    "  {#foreach:items}\n"
    "    <li>{$ name}: {$ value}</li>\n"
    "  {#end}\n"
    "  </ul>\n"
    "</body>\n"
    "</html>";

// 解析和生成...
```

---

### 2. 邮件模板

```c
str email_template = 
    "尊敬的 {$ name}：\n"
    "\n"
    "您的订单 {$ orderId} 已{$ status}。\n"
    "\n"
    "{#if:hasTracking}\n"
    "物流单号：{$ trackingNumber}\n"
    "{#end}\n"
    "\n"
    "感谢您的购买！";

// 使用...
```

---

### 3. 代码生成

```c
str code_template = 
    "class {$ className} {\n"
    "{#foreach:fields}\n"
    "    private {% type} {$ name};\n"
    "{#end}\n"
    "\n"
    "{#foreach:fields}\n"
    "    public {% type} get{$ Name}() {\n"
    "        return this.{$ name};\n"
    "    }\n"
    "{#end}\n"
    "}";

// 生成类代码...
```

---

### 4. 报表生成

```c
str report_template = 
    "销售报表 - {& date : YYYY-MM-DD}\n"
    "=====================================\n"
    "{#foreach:items}\n"
    "{$ product}: {% quantity} 件，金额 {% amount} 元\n"
    "{#end}\n"
    "-------------------------------------\n"
    "总计：{% total} 元";

// 生成报表...
```

---

## 最佳实践

### 1. 错误处理

```c
XTE_LiteObject tpl = xteParse(template_text, 0, NULL);
if (!tpl) {
    fprintf(stderr, "Template allocation failed\n");
    return;
}

if (!tpl->Success) {
    fprintf(stderr, "Template parse error:\n");
    fprintf(stderr, "  Error: %s\n", tpl->ErrorDesc);
    fprintf(stderr, "  Line: %u, Pos: %u\n", 
        tpl->ErrorLine, tpl->ErrorPos);
    xteParseFree(tpl);
    return;
}

// 使用模板...
xteParseFree(tpl);
```

---

### 2. 模板缓存

```c
typedef struct {
    xdict templates;
} TemplateCache;

TemplateCache* CreateCache() {
    TemplateCache* cache = xrtMalloc(sizeof(TemplateCache));
    cache->templates = xdictCreate();
    return cache;
}

XTE_LiteObject GetTemplate(TemplateCache* cache, str name, str text) {
    XTE_LiteObject tpl = xdictGet(cache->templates, name);
    if (!tpl) {
        tpl = xteParse(text, 0, NULL);
        if (tpl && tpl->Success) {
            xdictSet(cache->templates, name, tpl);
        }
    }
    return tpl;
}
```

---

### 3. 数据准备

```c
xvalue PrepareUserData(User* user) {
    xvalue data = xvoCreateTable();
    xvoTableSet(data, "id", xvoCreateInt(user->id));
    xvoTableSet(data, "name", xvoCreateText(user->name, 0, FALSE));
    xvoTableSet(data, "email", xvoCreateText(user->email, 0, FALSE));
    xvoTableSet(data, "vip", xvoCreateBool(user->is_vip));
    return data;
}

str RenderUserCard(User* user) {
    xvalue data = PrepareUserData(user);
    
    XTE_LiteObject tpl = GetCachedTemplate("user_card");
    size_t size;
    str result = xteMake(tpl, data, NULL, NULL, &size);
    
    xvoUnref(data);
    return result;
}
```

---

## 性能提示

### 1. 重用模板对象

```c
// ❌ 低效：每次都解析
for (int i = 0; i < 1000; i++) {
    XTE_LiteObject tpl = xteParse(template_text, 0, NULL);
    str result = xteMake(tpl, data[i], NULL, NULL, &size);
    xteParseFree(tpl);
    xrtFree(result);
}

// ✅ 高效：解析一次，重用多次
XTE_LiteObject tpl = xteParse(template_text, 0, NULL);
for (int i = 0; i < 1000; i++) {
    str result = xteMake(tpl, data[i], NULL, NULL, &size);
    ProcessResult(result);
    xrtFree(result);
}
xteParseFree(tpl);
```

---

### 2. 预编译模板

```c
// 启动时预编译
void InitTemplates() {
    g_user_template = xteParse(LoadFile("user.tpl"), 0, NULL);
    g_order_template = xteParse(LoadFile("order.tpl"), 0, NULL);
    g_report_template = xteParse(LoadFile("report.tpl"), 0, NULL);
}

// 使用时直接生成
str RenderUser(xvalue data) {
    size_t size;
    return xteMake(g_user_template, data, NULL, NULL, &size);
}
```

---

## 常见错误

### 1. 忘记释放

```c
// ❌ 内存泄漏
XTE_LiteObject tpl = xteParse(text, 0, NULL);
str result = xteMake(tpl, data, NULL, NULL, &size);
// 忘记 xteParseFree(tpl) 和 xrtFree(result)

// ✅ 正确
XTE_LiteObject tpl = xteParse(text, 0, NULL);
str result = xteMake(tpl, data, NULL, NULL, &size);
UseResult(result);
xrtFree(result);
xteParseFree(tpl);
```

---

### 2. 语法错误

```c
// ❌ 忘记 {#end}
str bad_template = 
    "{#if:condition}\n"
    "  内容\n";
    // 缺少 {#end}

// ✅ 正确
str good_template = 
    "{#if:condition}\n"
    "  内容\n"
    "{#end}";
```

---

## 错误代码

| 错误代码 | 描述 | 说明 |
|---------|------|------|
| 0 | success | 成功 |
| 1 | malloc failed | 内存申请失败 |
| 2 | token list append failed | Token 列表添加失败 |
| 3 | unrecognized symbols | 无法识别的符号 |
| 4 | empty symbols are not allowed | 不允许使用空符号 |
| 5 | too many parameters | 参数数量过多（最大 6 个） |
| 6 | statement not ended | 语句未结束（缺少 `{#end}`） |
| 7 | Undefined identifier | 未定义标识符 |
| 8 | Missing parameters | 缺少参数 |
| 9 | Nesting of define statements is not allowed | define 语句不允许嵌套 |
| 10 | syntax error | 语法错误 |

---

## 转义符号

| 符号 | 说明 | 转义方式 | 示例 |
|------|------|----------|------|
| `{` | 模板起始符号 | `{{` | `{{` → `{` |
| `}` | 模板结束符号 | `\}` | `\}` → `}` |
| `:` | 参数分隔符 | `\:` | `\:` → `:` |
| `\` | 语句内转义符号 | `\\` | `\\` → `\` |

**注意：** 转义符号仅在模板语句内部生效，普通文本中的这些字符无需转义。

---

## 相关文档

- [Value 动态类型系统](api-value.md) - 模板数据模型
- [String 字符串处理](api-string.md) - 字符串操作
- [Dict 字典](api-dict.md) - 包含模板管理
- [File 文件操作](api-file.md) - 读取模板文件
- [主索引](README.md)

---

<div align="center">

[⬆️ 返回顶部](#template-模板引擎库)

</div>
