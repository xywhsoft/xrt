# Template Engine Library

> XTE (X Template Engine) Lightweight Template Engine

[English](api-template.en.md) | [中文](api-template.md) | [Back to Index](README.en.md)

---

## 📑 Table of Contents

- [Template Syntax](#template-syntax)
- [Expression Syntax](#expression-syntax)
- [Data Structures](#data-structures)
- [Lexical Analysis](#lexical-analysis)
- [Path Resolution](#path-resolution)
- [Expression Evaluation](#expression-evaluation)
- [Template Parsing](#template-parsing)
- [Template Generation](#template-generation)
- [Advanced Features](#advanced-features)

---

## Template Syntax

### Token Types

```
// Maximum supported parameter count
#define XTE_PARAM_MAXCOUNT  6

// Basic Tokens
#define XTE_TK_TEXT         0       // Text content
#define XTE_TK_COMMEN       1       // Comment: {! * }
#define XTE_TK_VAR          0x100   // Variable substitution: {$ * : *}
#define XTE_TK_NUM          0x101   // Numeric variable substitution: {% * : *}
#define XTE_TK_TIME         0x102   // Time variable substitution: {& * : *}
#define XTE_TK_BOOL         0x103   // Boolean conditional substitution: {? * : * : *}
#define XTE_TK_ARR          0x104   // Array substitution: {* * : *}
#define XTE_TK_PROC         0x105   // Function or procedure substitution: {@ * : * ...}
#define XTE_TK_SUBTEMPLATE  0x106   // Sub-template substitution: {= * : *}
#define XTE_TK_SYMBOL       0xFFFF  // Predefined symbol: {# * : *}
#define XTE_MODE_BLOCK      0xFFFE  // Data block collection mode (ends with {#end})

// Token Extension Numbers
#define XTE_TK_INCLUDE      0x10000 // Include external file: {#include *}
#define XTE_TK_DEFINE       0x10001 // Define sub-template: {#define *}
#define XTE_TK_SCRIPT       0x10002 // Script block
#define XTE_TK_IF           0x20000 // Conditional statement: {#if *}
#define XTE_TK_ELSEIF       0x20001 // {#elseif *}
#define XTE_TK_ELSE         0x20002 // {#else}
#define XTE_TK_FOR          0x30000 // Loop statement: {#for *}
#define XTE_TK_FOREACH      0x30001 // Iteration loop statement: {#foreach *}
#define XTE_TK_BREAK        0x30002 // Break from loop: {#break}
#define XTE_TK_CONTINUE     0x30003 // Continue to next iteration: {#continue}
#define XTE_TK_END          0xFFFFFF // Statement end: {#end}
#define XTE_TK_USER         0x1000000 // User-defined extension starting number

// Identifier Types
#define XTE_IDTPE_DEFAULT   0       // Single statement
#define XTE_IDTPE_BLOCK     1       // Independent statement block (ends with {#end})
```

---

### Basic Syntax

#### Variable Substitution

```
Name: {$ name : Unknown}
Age: {% age : 0}
Time: {& createTime : YYYY-MM-DD}
```

**Description:**
- `{$ name : Unknown}` - String variable, default value "Unknown"
- `{% age : 0}` - Numeric variable, default value 0
- `{& time : format}` - Time variable, with specified format

---

#### Conditional Statements

Conditional statements support full expression evaluation:

```
{#if:age > 18}
  User is an adult
{#elseif:age > 12}
  Teenager
{#else}
  Child
{#end}

{#if:score >= 90}
  Grade A
{#elseif:score >= 60}
  Passing
{#else}
  Failing
{#end}
```

**Supported operators:**
- Comparison: `=`, `!=`, `~=` (approximately equal), `>`, `<`, `>=`, `<=`
- Logical: `and`, `or`, `not`
- Parentheses: `(`, `)`

---

#### Counting Loop (for)

```
{#for:start:end:step}
  {$ __index__}
{#end}

{#for:1:5:1}
  Item {% __index__ }
{#end}

{#for:10:1:-1}
  Countdown: {% __index__ }
{#end}
```

---

#### Iteration Loop (foreach)

**Iterating Arrays:**
```
{#foreach:users}
  {$ name} - {$ email}
{#end}

{#foreach:items}
  Index: {% __index__ }
  Current element property: {$ name}
{#end}
```

**Iterating Tables (Key-Value Pairs):**
```
{#foreach:config}
  {$__key__} = {$__value__}
{#end}
```

When iterating tables, `__key__` and `__value__` represent the key name and corresponding value respectively.

---

#### Loop Control (break/continue)

Control statements can be used within `for` and `foreach` loops:

- `{#break}` - Immediately exit the current loop
- `{#continue}` - Skip the current iteration and proceed to the next

```
{! Using break in for loop }
{#for:1:10:1}
  {#if:__index__ > 3}
    {#break}
  {#end}
  {%__index__} 
{#end}
Output: 1 2 3

{! Using continue in for loop }
{#for:1:5:1}
  {#if:__index__ = 3}
    {#continue}
  {#end}
  {%__index__},
{#end}
Output: 1,2,4,5, (skipped 3)

{! Using break in foreach loop }
{#foreach:items}
  {#if:__index__ = 2}
    {#break}
  {#end}
  {$ name},
{#end}

{! Using continue in foreach loop }
{#foreach:items}
  {#if:skip = 1}
    {#continue}
  {#end}
  [{$ name}]
{#end}
```

**Note:** `{#break}` and `{#continue}` can only be used inside loops and only affect the innermost loop.

---

#### Built-in Variables

| Variable | Description | Available Scope |
|----------|-------------|-----------------|
| `__self__` | Current substitution value | Inside sub-template |
| `__index__` | Current loop index | Inside `for`, `foreach` loops |
| `__value__` | Current iteration value | Inside `foreach` loop |
| `__key__` | Current iteration key (when iterating table) | Inside `foreach` loop |

**Note:** Loop iteration has a maximum limit (default 100,000 iterations). Loops exceeding this limit will automatically stop to prevent infinite loops.

**Example:**
```
{! Iterating arrays }
{#foreach:items}
  Index: {% __index__}, Value: {$ name}
{#end}

{! Iterating tables - output all key-value pairs }
{#foreach:settings}
  {$__key__}={$__value__};
{#end}

{! Using __self__ in sub-template }
{#define item}
  Current value: {$ __self__}
{#end}
```

---

#### Comments

```
{! This is a comment, will not output }
```

---

#### Whitespace in Tags

The template parser supports whitespace inside tags and automatically trims:

```
{$  name  }           equivalent to {$name}
{%  price  :  .2  }   equivalent to {%price:.2}
{&  time  :  yyyy-mm-dd  }   equivalent to {&time:yyyy-mm-dd}
```

**Note:** The opening symbol and type character (like `{$`, `{%`, `{#`) must be written together.

---

## Expression Syntax

Conditional statements `{#if}`, `{#elseif}` support full expression evaluation.

### Operators

| Operator | Description | Example |
|----------|-------------|---------|
| `=` | Equal | `age = 18` |
| `!=` | Not equal | `status != "active"` |
| `~=` | Approximately equal (number/string/time) | `price ~= 100` |
| `>` | Greater than | `score > 60` |
| `<` | Less than | `count < 10` |
| `>=` | Greater than or equal | `level >= 5` |
| `<=` | Less than or equal | `age <= 65` |
| `and` | Logical AND | `age > 18 and vip` |
| `or` | Logical OR | `admin or manager` |
| `not` | Logical NOT | `not disabled` |
| `(` `)` | Parentheses grouping | `(a or b) and c` |

### Literals

```
Number:   100, 3.14, -50
String:   "hello", 'world'
Boolean:  true, false
```

### Variable Paths

Supports dot notation and array indexing:

```
user.name           Nested property access
user.profile.age    Multi-level nesting
items[0]            Array indexing
items[0].title      Array element property
```

### Expression Examples

```
{#if:age >= 18 and active}
  Adult active user
{#end}

{#if:score >= 90 or vip}
  Excellent user
{#end}

{#if:not disabled and (role = "admin" or role = "manager")}
  Administrator
{#end}

{#if:user.profile.level > 5}
  Premium user
{#end}
```

---

## Data Structures

### XTE_IdentInfo - Identifier Information

Used to define custom identifiers.

```c
typedef struct {
    char* Ident;              // Identifier
    uint32 TokenIndex;        // Corresponding Token number
    unsigned short Type;      // 0 = single statement, 1 = independent block (ends with {#end})
    unsigned short Size;      // Identifier length
    unsigned short MinParamCount; // Minimum parameter count
    unsigned short MaxParamCount; // Maximum parameter count
    uint32 Hash;              // Identifier hash value
} XTE_IdentInfo_Struct, *XTE_IdentInfo;
```

---

### XTE_TokenItem - Token Item

Represents a single parsed Token.

```c
typedef struct {
    uint32 Type;                          // Token definition number
    char* Text;                           // Associated text
    size_t Size;                          // Associated text length
    uint32 ParamCount;                    // Parameter count
    char* ParamText[XTE_PARAM_MAXCOUNT];  // Parameter text
    uint32 ParamSize[XTE_PARAM_MAXCOUNT]; // Parameter length
    XTE_IdentInfo IdentInfo;              // Identifier info struct pointer for identifier statements
    uint32 RefLine;                       // Statement line number in source file
    uint32 RefLinePos;                    // Statement position in source line
    uint32 RefPos;                        // Statement position in source file
    uint32 RefSize;                       // Statement length in source file
} XTE_TokenItem_Struct, *XTE_TokenItem;
```

---

### XTE_TokenList - Token List

Result structure returned by lexical analysis.

```c
typedef struct {
    int Success;              // Whether parsing succeeded
    int ErrorCode;            // Error code (0=success)
    const char* ErrorDesc;    // Error description
    uint32 ErrorLine;         // Error line number
    uint32 ErrorLinePos;      // Error line position
    uint32 ErrorPos;          // Error position
    uint32 ErrorRefLine;      // Error reference line
    uint32 ErrorRefLinePos;   // Error reference line position
    uint32 ErrorRefPos;       // Error reference position
    xarray_struct Tokens;     // Token list
} XTE_TokenList_Struct, *XTE_TokenList;
```

---

### XTE_LiteObject - Template Object

Compiled template object structure.

```c
typedef struct {
    int Success;              // Whether parsing succeeded
    int ErrorCode;            // Error code (0=success)
    const char* ErrorDesc;    // Error description
    uint32 ErrorLine;         // Error line number
    uint32 ErrorLinePos;      // Error line position
    uint32 ErrorPos;          // Error position
    uint32 ErrorRefLine;      // Error reference line
    uint32 ErrorRefLinePos;   // Error reference line position
    uint32 ErrorRefPos;       // Error reference position
    xarray Tokens;            // Token list
    xparray_struct Actions;   // Compiled action list
    xdict_struct SubTemplates;// Sub-template list (hash table)
} XTE_LiteStruct, *XTE_LiteObject;
```

---

## Lexical Analysis

### xteCreateIdentList

Create keyword list for custom extensions.

**Prototype:**
```c
XXAPI xarray xteCreateIdentList();
```

**Return Value:**
- Success: Keyword list
- Failure: `NULL`

---

### xteDestroyIdentList

Destroy keyword list.

**Prototype:**
```c
XXAPI void xteDestroyIdentList(xarray objList);
```

---

### xteAddIdentToList

Add a keyword to the list.

**Prototype:**
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

**Parameters:**
- `objList` - Keyword list
- `sID` - Identifier string
- `iSize` - Identifier length (0 for auto-calculate)
- `iIndex` - Corresponding Token number
- `iType` - Identifier type (`XTE_IDTPE_DEFAULT` or `XTE_IDTPE_BLOCK`)
- `iMinParamCount` - Minimum parameter count
- `iMaxParamCount` - Maximum parameter count

**Return Value:**
- Success: Index of new item
- Failure: `0`

**Example:**
```c
xarray identList = xteCreateIdentList();
xteAddIdentToList(identList, "myTag", 0, XTE_TK_USER, XTE_IDTPE_DEFAULT, 1, 3);
xteAddIdentToList(identList, "myBlock", 0, XTE_TK_USER + 1, XTE_IDTPE_BLOCK, 0, 1);
// Use identList for lexical analysis...
xteDestroyIdentList(identList);
```

---

### xteLexer

Parse template file into Token list.

**Prototype:**
```c
XXAPI XTE_TokenList xteLexer(
    char* sText,
    size_t iSize,
    xarray objIdentList,
    char* sBracket
);
```

**Parameters:**
- `sText` - Template text
- `iSize` - Length (0 for auto-calculate)
- `objIdentList` - Identifier list (supports `{#xxx}` syntax, can pass `NULL`)
- `sBracket` - Bracket characters (e.g., `"{}"` or `NULL` for default)

**Return Value:**
- `XTE_TokenList` struct pointer

**Release:** ✅ Requires `xteLexerFree` to release

---

### xteLexerFree

Release Token list.

**Prototype:**
```c
XXAPI void xteLexerFree(XTE_TokenList arrToken);
```

---

### xteParseFromTokenList

Convert Token list to template object.

**Prototype:**
```c
XXAPI XTE_LiteObject xteParseFromTokenList(XTE_TokenList objToks);
```

**Parameters:**
- `objToks` - Token list (will be released after call)

**Return Value:**
- `XTE_LiteObject` template object pointer

**Note:** The passed `objToks` will be released by the function, cannot be used after call.

---

## Path Resolution

### xteResolvePath

Resolve variable path, supporting dot notation and array indexing.

**Prototype:**
```c
XXAPI xvalue xteResolvePath(
    const char* path,
    size_t pathLen,
    xvalue tblVal,
    xvalue tblRoot,
    xvalue tblENV
);
```

**Parameters:**
- `path` - Path string (e.g., `"user.profile.name"` or `"items[0].title"`)
- `pathLen` - Path length (0 for auto-calculate)
- `tblVal` - Current scope
- `tblRoot` - Root scope (can pass `NULL`)
- `tblENV` - Environment variables (can pass `NULL`)

**Return Value:**
- Success: Resolved `xvalue`
- Failure: `&XVO_VALUE_NULL`

**Path lookup order:**
1. First search in `tblVal`
2. If not found and `tblRoot != NULL`, search in `tblRoot`
3. If not found and `tblENV != NULL`, search in `tblENV`

**Example:**
```c
// Create data structure
xvalue data = xvoCreateTable();
xvalue user = xvoCreateTable();
xvalue profile = xvoCreateTable();

xvoTableSetText(profile, "name", 0, "John", 0, FALSE);
xvoTableSetInt(profile, "age", 0, 25);
xvoTableSetValue(user, "profile", 0, profile, TRUE);
xvoTableSetValue(data, "user", 0, user, TRUE);

// Simple path
xvalue v1 = xteResolvePath("user", 0, data, NULL, NULL);
// v1 -> user table

// Dot notation
xvalue v2 = xteResolvePath("user.profile.name", 0, data, NULL, NULL);
str name = xvoGetText(v2);  // "John"

// Array indexing
xvalue items = xvoCreateArray();
xvalue item0 = xvoCreateTable();
xvoTableSetText(item0, "title", 0, "First Item", 0, FALSE);
xvoArrayAppendValue(items, item0, TRUE);
xvoTableSetValue(data, "items", 0, items, TRUE);

xvalue v3 = xteResolvePath("items[0].title", 0, data, NULL, NULL);
str title = xvoGetText(v3);  // "First Item"

xvoUnref(data);
```

---

## Expression Evaluation

### xteExprEvalBool

Parse and evaluate expression, returning boolean result.

**Prototype:**
```c
XXAPI int xteExprEvalBool(
    const char* expr,
    size_t len,
    xvalue tblVal,
    xvalue tblRoot,
    xvalue tblENV
);
```

**Parameters:**
- `expr` - Expression string
- `len` - Expression length (0 for auto-calculate)
- `tblVal` - Current scope (for variable lookup)
- `tblRoot` - Root scope (can pass `NULL`)
- `tblENV` - Environment variables (can pass `NULL`)

**Return Value:**
- True: Non-zero value
- False: 0

**Example:**
```c
xvalue data = xvoCreateTable();
xvoTableSetInt(data, "age", 0, 25);
xvoTableSetText(data, "name", 0, "Alice", 0, FALSE);
xvoTableSetBool(data, "active", 0, TRUE);
xvoTableSetFloat(data, "score", 0, 85.5);

// Comparison operations
int r1 = xteExprEvalBool("age = 25", 0, data, NULL, NULL);      // 1
int r2 = xteExprEvalBool("age > 18", 0, data, NULL, NULL);      // 1
int r3 = xteExprEvalBool("score >= 90", 0, data, NULL, NULL);   // 0

// String comparison
int r4 = xteExprEvalBool("name = \"Alice\"", 0, data, NULL, NULL); // 1
int r5 = xteExprEvalBool("name != \"Bob\"", 0, data, NULL, NULL);  // 1

// Logical operations
int r6 = xteExprEvalBool("age > 18 and active", 0, data, NULL, NULL);   // 1
int r7 = xteExprEvalBool("age < 18 or active", 0, data, NULL, NULL);    // 1
int r8 = xteExprEvalBool("not active", 0, data, NULL, NULL);            // 0

// Parentheses
int r9 = xteExprEvalBool("(age > 20) and (score > 80)", 0, data, NULL, NULL); // 1

// Literals
int r10 = xteExprEvalBool("true", 0, NULL, NULL, NULL);   // 1
int r11 = xteExprEvalBool("100 > 50", 0, NULL, NULL, NULL); // 1

xvoUnref(data);
```

---

## Template Parsing

### xteParse

Parse template text

**Prototype:**
```c
XXAPI XTE_LiteObject xteParse(char* sText, size_t iSize, char* sBracket);
```

**Parameters:**
- `sText` - Template text
- `iSize` - Length (0 for auto)
- `sBracket` - Bracket characters (usually `"{}"` or `NULL` for default)

**Return Value:**
- Success: Template object pointer
- Failure: `NULL`

**Release:** ✅ Requires `xteParseFree` to release

**Example:**
```c
str template_text = 
    "Hello {$ name}!\n"
    "{#if:vip}\n"
    "  You are a VIP user\n"
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

Release template object

**Prototype:**
```c
XXAPI void xteParseFree(XTE_LiteObject objLite);
```

---

## Template Generation

### xteMakeActions

Generate document from action list (low-level function).

**Prototype:**
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

**Parameters:**
- `arrAction` - Action list
- `objTemplate` - Template object
- `tblVal` - Current data table
- `tblRoot` - Root data table (for sub-template access to parent data)
- `tblENV` - Environment variable table (optional, pass `NULL`)
- `tblInclude` - Included template dictionary (optional, pass `NULL`)
- `pRetSize` - Output: generated text length

**Return Value:**
- Success: Generated text
- Failure: `NULL`

**Memory Release:** ✅ Requires `xrtFree` to release

---

### xteMake

Generate document from template

**Prototype:**
```c
XXAPI char* xteMake(
    XTE_LiteObject objTemplate,
    xvalue tblVal,
    xvalue tblENV,
    xdict tblInclude,
    size_t* pRetSize
);
```

**Parameters:**
- `objTemplate` - Template object
- `tblVal` - Data table (xvalue Table or Array type)
- `tblENV` - Environment variable table (optional, pass `NULL`)
- `tblInclude` - Included template dictionary (optional, pass `NULL`)
- `pRetSize` - Output: generated text length

**Return Value:**
- Success: Generated text
- Failure: `NULL`

**Memory Release:** ✅ Requires `xrtFree` to release

**Example:**
```c
// Prepare template
str template_text = 
    "User Information:\n"
    "Name: {$ name : Unknown}\n"
    "Age: {% age : 0}\n"
    "{#if:vip}\n"
    "VIP Level: {% vipLevel}\n"
    "{#end}";

XTE_LiteObject tpl = xteParse(template_text, 0, NULL);
if (!tpl || !tpl->Success) {
    printf("Template parse error\n");
    return;
}

// Prepare data
xvalue data = xvoCreateTable();
xvoTableSet(data, "name", xvoCreateText("John", 0, FALSE));
xvoTableSet(data, "age", xvoCreateInt(28));
xvoTableSet(data, "vip", xvoCreateBool(TRUE));
xvoTableSet(data, "vipLevel", xvoCreateInt(5));

// Generate document
size_t size;
str result = xteMake(tpl, data, NULL, NULL, &size);

if (result) {
    printf("%s\n", result);
    xrtFree(result);
}

// Cleanup
xvoUnref(data);
xteParseFree(tpl);
```

**Output:**
```
User Information:
Name: John
Age: 28
VIP Level: 5
```

---

## Advanced Features

### 1. Looping Arrays

```c
str template_text = 
    "{#foreach:users}\n"
    "  {$ name} - {$ email}\n"
    "{#end}";

XTE_LiteObject tpl = xteParse(template_text, 0, NULL);

// Prepare data
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

// Generate
size_t size;
str result = xteMake(tpl, data, NULL, NULL, &size);
printf("%s", result);

xrtFree(result);
xvoUnref(data);
xteParseFree(tpl);
```

**Output:**
```
  Alice - alice@example.com
  Bob - bob@example.com
```

---

### 2. Counting Loop

```
str template_text = 
    "{#for:1:5:1}\n"
    "  Item {% __index__}\n"
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

**Output:**
```
  Item 1
  Item 2
  Item 3
  Item 4
  Item 5
```

---

### 3. Iterating Tables (Key-Value Pairs)

When using `foreach` to iterate table type data, you can access key names and values through `__key__` and `__value__`.

```c
str template_text = 
    "Configuration:\n"
    "{#foreach:config}"
    "  {$__key__} = {$__value__}\n"
    "{#end}";

XTE_LiteObject tpl = xteParse(template_text, 0, NULL);

// Prepare table data
xvalue data = xvoCreateTable();
xvalue config = xvoCreateTable();
xvoTableSetText(config, "host", 0, "localhost", 0, FALSE);
xvoTableSetText(config, "port", 0, "8080", 0, FALSE);
xvoTableSetText(config, "debug", 0, "true", 0, FALSE);
xvoTableSetValue(data, "config", 0, config, TRUE);

// Generate
size_t size;
str result = xteMake(tpl, data, NULL, NULL, &size);
printf("%s", result);

xrtFree(result);
xvoUnref(data);
xteParseFree(tpl);
```

**Output example (key order is determined by dictionary implementation):**
```
Configuration:
  debug = true
  host = localhost
  port = 8080
```

**Note:** When iterating tables, the output order of keys is determined by the internal dictionary implementation and is not guaranteed to match the insertion order.

---

### 4. Nested Control Statements

```
str template_text = 
    "{#foreach:users}\n"
    "  {#if:active}\n"
    "    {$ name} (active)\n"
    "  {#else}\n"
    "    {$ name} (inactive)\n"
    "  {#end}\n"
    "{#end}";

XTE_LiteObject tpl = xteParse(template_text, 0, NULL);

// Prepare data
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

**Output:**
```
    Alice (active)
    Bob (inactive)
```

---

### 5. Sub-templates

```
str template_text = 
    "{#define:userCard}\n"
    "  Name: {$ name}\n"
    "  Age: {% age}\n"
    "{#end}\n"
    "\n"
    "{#foreach:users}\n"
    "  {= userCard}\n"
    "{#end}";

XTE_LiteObject tpl = xteParse(template_text, 0, NULL);

// Prepare data...
// Use {= userCard} to call sub-template
```

**Sub-template Special Variables:**
- Inside sub-template, use `{$ __self__}` to access substitution value
- Sub-templates cannot be nested

---

### 6. Including External Templates

```
// Main template
str main_template = 
    "{#include:header}\n"
    "Main content\n"
    "{#include:footer}";

// Prepare included templates
xdict includes = xdictCreate();

str header_tpl_text = "<header>Website Header</header>";
XTE_LiteObject header_tpl = xteParse(header_tpl_text, 0, NULL);
xdictSet(includes, "header", header_tpl);

str footer_tpl_text = "<footer>Website Footer</footer>";
XTE_LiteObject footer_tpl = xteParse(footer_tpl_text, 0, NULL);
xdictSet(includes, "footer", footer_tpl);

// Main template
XTE_LiteObject main_tpl = xteParse(main_template, 0, NULL);

// Generate (pass includes)
size_t size;
str result = xteMake(main_tpl, NULL, NULL, includes, &size);

printf("%s\n", result);

// Cleanup
xrtFree(result);
xteParseFree(main_tpl);
xteParseFree(header_tpl);
xteParseFree(footer_tpl);
xdictFree(includes);
```

---

## Use Cases

### 1. HTML Generation

```
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

// Parse and generate...
```

---

### 2. Email Template

```
str email_template = 
    "Dear {$ name},\n"
    "\n"
    "Your order {$ orderId} has been {$ status}.\n"
    "\n"
    "{#if:hasTracking}\n"
    "Tracking number: {$ trackingNumber}\n"
    "{#end}\n"
    "\n"
    "Thank you for your purchase!";

// Use...
```

---

### 3. Code Generation

```
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

// Generate class code...
```

---

### 4. Report Generation

```
str report_template = 
    "Sales Report - {& date : YYYY-MM-DD}\n"
    "=====================================\n"
    "{#foreach:items}\n"
    "{$ product}: {% quantity} units, Amount {% amount}\n"
    "{#end}\n"
    "-------------------------------------\n"
    "Total: {% total}";

// Generate report...
```

---

## Best Practices

### 1. Error Handling

```
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

// Use template...
xteParseFree(tpl);
```

---

### 2. Template Caching

```
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

### 3. Data Preparation

```
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

## Performance Tips

### 1. Reuse Template Objects

```c
// ❌ Inefficient: Parse every time
for (int i = 0; i < 1000; i++) {
    XTE_LiteObject tpl = xteParse(template_text, 0, NULL);
    str result = xteMake(tpl, data[i], NULL, NULL, &size);
    xteParseFree(tpl);
    xrtFree(result);
}

// ✅ Efficient: Parse once, reuse multiple times
XTE_LiteObject tpl = xteParse(template_text, 0, NULL);
for (int i = 0; i < 1000; i++) {
    str result = xteMake(tpl, data[i], NULL, NULL, &size);
    ProcessResult(result);
    xrtFree(result);
}
xteParseFree(tpl);
```

---

### 2. Expression AST Caching

The template engine internally caches the Abstract Syntax Tree (AST) of expressions. Identical expressions only need to be parsed once across multiple executions.

```c
// When evaluating the same expression multiple times in a loop, the engine automatically caches the AST
for (int i = 0; i < 1000; i++) {
    // Expression "age > 18 and active" is only parsed on the first call
    int result = xteExprEvalBool("age > 18 and active", 0, data[i], NULL, NULL);
}

// Conditional expressions in templates are also cached
// {#if:score >= 90}, {#elseif:score >= 60}, etc.
```

**Note:** The cache is valid throughout the application lifecycle and requires no manual management.

---

### 3. Pre-compile Templates

```
// Pre-compile at startup
void InitTemplates() {
    g_user_template = xteParse(LoadFile("user.tpl"), 0, NULL);
    g_order_template = xteParse(LoadFile("order.tpl"), 0, NULL);
    g_report_template = xteParse(LoadFile("report.tpl"), 0, NULL);
}

// Generate directly when using
str RenderUser(xvalue data) {
    size_t size;
    return xteMake(g_user_template, data, NULL, NULL, &size);
}
```

---

### 4. Loop Iteration Limit

To prevent infinite loops or unexpectedly large loops, `for` and `foreach` loops have a maximum iteration limit (default 100,000 iterations).

```c
// Loops exceeding the limit will be automatically truncated
str template_text = "{#for:1:999999:1}x{#end}";
XTE_LiteObject tpl = xteParse(template_text, 0, NULL);
xvalue data = xvoCreateTable();
size_t size;
str result = xteMake(tpl, data, NULL, NULL, &size);
// Actual output will not exceed 100,000 'x' characters
printf("Actual output length: %zu\n", size);  // Output: 100000
```

**Note:** This limit is a safety mechanism and does not affect normal usage.

---

## Common Errors

### 1. Forgetting to Release

```
// ❌ Memory leak
XTE_LiteObject tpl = xteParse(text, 0, NULL);
str result = xteMake(tpl, data, NULL, NULL, &size);
// Forgot xteParseFree(tpl) and xrtFree(result)

// ✅ Correct
XTE_LiteObject tpl = xteParse(text, 0, NULL);
str result = xteMake(tpl, data, NULL, NULL, &size);
UseResult(result);
xrtFree(result);
xteParseFree(tpl);
```

---

### 2. Syntax Errors

```
// ❌ Forgot {#end}
str bad_template = 
    "{#if:condition}\n"
    "  Content\n";
    // Missing {#end}

// ✅ Correct
str good_template = 
    "{#if:condition}\n"
    "  Content\n"
    "{#end}";
```

---

## Error Codes

| Error Code | Description | Explanation |
|------------|-------------|-------------|
| 0 | success | Success |
| 1 | malloc failed | Memory allocation failed |
| 2 | token list append failed | Token list append failed |
| 3 | unrecognized symbols | Unrecognized symbols |
| 4 | empty symbols are not allowed | Empty symbols are not allowed |
| 5 | too many parameters | Too many parameters (max 6) |
| 6 | statement not ended | Statement not ended (missing `{#end}`) |
| 7 | Undefined identifier | Undefined identifier |
| 8 | Missing parameters | Missing parameters |
| 9 | Nesting of define statements is not allowed | Define statements cannot be nested |
| 10 | syntax error | Syntax error |

---

## Escape Characters

| Symbol | Description | Escape Method | Example |
|--------|-------------|---------------|---------|
| `{` | Template start symbol | `{{` | `{{` → `{` |
| `}` | Template end symbol | `\}` | `\}` → `}` |
| `:` | Parameter separator | `\:` | `\:` → `:` |
| `\` | Escape character in statement | `\\` | `\\` → `\` |

**Note:** Escape characters only take effect inside template statements; these characters in plain text do not need escaping.

---

## Related Documents

- [Value Dynamic Type System](api-value.en.md) - Template data model
- [String Processing](api-string.en.md) - String operations
- [Dict Dictionary](api-dict.en.md) - Include template management
- [File Operations](api-file.en.md) - Reading template files
- [Main Index](README.en.md)

---

<div align="center">

[⬆️ Back to Top](#template-engine-library)

</div>
