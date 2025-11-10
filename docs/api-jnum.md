# JNUM 数值转换库

> JSON数字与字符串高性能转换

[返回索引](README.md)

---

## 📑 目录

- [数字转字符串](#数字转字符串)
- [字符串转数字](#字符串转数字)
- [数字解析](#数字解析)

---

## 数字转字符串

### xrtI32ToStr / xrtI64ToStr

整数转字符串

**函数原型：**
```c
XXAPI int xrtI32ToStr(int32_t num, char* buffer);
XXAPI int xrtI64ToStr(int64_t num, char* buffer);
```

**参数：**
- `num` - 整数值
- `buffer` - 输出缓冲区（需预分配足够空间）

**返回值：**
- 写入的字符数

**示例：**
```c
char buf[32];
int len = xrtI32ToStr(12345, buf);
printf("%s (length: %d)\n", buf, len);  // "12345" (length: 5)

len = xrtI64ToStr(-999, buf);
printf("%s\n", buf);  // "-999"
```

---

### xrtU32ToStr / xrtU64ToStr

无符号整数转字符串

**函数原型：**
```c
XXAPI int xrtU32ToStr(uint32_t num, char* buffer);
XXAPI int xrtU64ToStr(uint64_t num, char* buffer);
```

---

### xrtNumToStr

浮点数转字符串

**函数原型：**
```c
XXAPI int xrtNumToStr(double num, char* buffer);
```

**参数：**
- `num` - 浮点数值
- `buffer` - 输出缓冲区

**返回值：**
- 写入的字符数

**示例：**
```c
char buf[64];
int len = xrtNumToStr(3.14159, buf);
printf("%s\n", buf);  // "3.14159"
```

---

## 字符串转数字

### xrtStrToI32 / xrtStrToI64

字符串转整数

**函数原型：**
```c
XXAPI int32_t xrtStrToI32(const char* pStr);
XXAPI int64_t xrtStrToI64(const char* pStr);
```

**参数：**
- `pStr` - 数字字符串

**返回值：**
- 整数值

**示例：**
```c
int32_t n1 = xrtStrToI32("12345");    // 12345
int64_t n2 = xrtStrToI64("-999");     // -999
```

---

### xrtStrToU32 / xrtStrToU64

字符串转无符号整数

**函数原型：**
```c
XXAPI uint32_t xrtStrToU32(const char* pStr);
XXAPI uint64_t xrtStrToU64(const char* pStr);
```

---

### xrtStrToNum

字符串转浮点数

**函数原型：**
```c
XXAPI double xrtStrToNum(const char* pStr);
```

**参数：**
- `pStr` - 数字字符串

**返回值：**
- 浮点数值

**支持格式：**
- 普通格式：`3.14`, `-2.5`
- 科学计数法：`1.23e10`, `4.56E-5`
- 整数：`100` → `100.0`

**示例：**
```c
double n1 = xrtStrToNum("3.14159");      // 3.14159
double n2 = xrtStrToNum("-2.5");         // -2.5
double n3 = xrtStrToNum("1.23e10");      // 12300000000.0
```

---

## 数字解析

### xrtParseNum

解析数字字符串（带类型识别）

**函数原型：**
```c
XXAPI int xrtParseNum(const char *str, jnum_type_t *type, jnum_value_t *value);
```

**参数：**
- `str` - 数字字符串
- `type` - 输出：数字类型
- `value` - 输出：数字值

**类型枚举：**
```c
typedef enum {
    JNUM_NULL,     // null
    JNUM_BOOL,     // true/false
    JNUM_INT,      // int32
    JNUM_HEX,      // uint32十六进制
    JNUM_LINT,     // int64
    JNUM_LHEX,     // uint64十六进制
    JNUM_DOUBLE,   // double
} jnum_type_t;
```

**返回值：**
- 解析的字符数

**示例：**
```c
jnum_type_t type;
jnum_value_t value;

int len = xrtParseNum("12345", &type, &value);
if (type == JNUM_INT) {
    printf("Int: %d\n", value.vint);
}

len = xrtParseNum("0xFF", &type, &value);
if (type == JNUM_HEX) {
    printf("Hex: 0x%X\n", value.vhex);
}

len = xrtParseNum("3.14", &type, &value);
if (type == JNUM_DOUBLE) {
    printf("Double: %f\n", value.vdbl);
}
```

---

## 使用场景

### 1. 数字格式化

```c
void FormatNumber(int64_t num) {
    char buf[32];
    int len = xrtI64ToStr(num, buf);
    printf("Number: %s\n", buf);
}
```

---

### 2. 数字解析

```c
void ParseNumber(const char* str) {
    jnum_type_t type;
    jnum_value_t value;
    
    xrtParseNum(str, &type, &value);
    
    switch (type) {
        case JNUM_INT:
            printf("Integer: %d\n", value.vint);
            break;
        case JNUM_DOUBLE:
            printf("Double: %f\n", value.vdbl);
            break;
        default:
            break;
    }
}
```

---

### 3. 数据转换

```c
// 整数转字符串
void IntToString(int32_t value, char* output) {
    xrtI32ToStr(value, output);
}

// 字符串转整数
int32_t StringToInt(const char* input) {
    return xrtStrToI32(input);
}
```

---

## 性能特点

### 高性能转换

JNUM 使用优化算法：
- **比 sprintf/sscanf 快数倍**
- **比 atoi/atof 更准确**
- **支持完整的 JSON 数字格式**

### 基准测试参考

```c
// sprintf: ~500ns per call
// xjnumFromInt: ~100ns per call (5x faster)

// sscanf: ~800ns per call
// xjnumToInt: ~150ns per call (5x faster)
```

---

## 精度说明

### 整数精度

- 支持 64位有符号整数
- 范围：-9,223,372,036,854,775,808 ~ 9,223,372,036,854,775,807

### 浮点数精度

- 使用 IEEE 754 双精度（64位）
- 有效数字：约15-17位
- 范围：±2.23e-308 ~ ±1.80e+308

---

## 边界情况处理

### 整数溢出

```c
// 超出范围会截断
int64 n = xjnumToInt("99999999999999999999", 0);
// 返回 LLONG_MAX 或 LLONG_MIN
```

### 浮点数特殊值

```c
double inf = xjnumToFloat("Infinity", 0);    // +∞
double ninf = xjnumToFloat("-Infinity", 0);  // -∞
double nan = xjnumToFloat("NaN", 0);         // NaN

str s1 = xjnumFromFloat(INFINITY);    // "Infinity"
str s2 = xjnumFromFloat(-INFINITY);   // "-Infinity"
str s3 = xjnumFromFloat(NAN);         // "NaN"
```

---

## 最佳实践

### 1. 验证输入

```c
bool IsValidNumber(str text) {
    // 简单验证
    for (size_t i = 0; text[i]; i++) {
        char c = text[i];
        if (!isdigit(c) && c != '.' && c != '-' && 
            c != '+' && c != 'e' && c != 'E') {
            return false;
        }
    }
    return true;
}

double SafeToFloat(str text) {
    if (IsValidNumber(text)) {
        return xjnumToFloat(text, 0);
    }
    return 0.0;
}
```

---

### 2. 内存管理

```c
// ✅ 正确
str s = xjnumFromInt(123);
UseString(s);
xrtFree(s);

// ❌ 内存泄漏
str s = xjnumFromInt(123);
// 忘记释放
```

---

### 3. 批量转换优化

```c
// 预分配缓冲区
char buffer[32];

// 对于大量转换，可以考虑自己管理缓冲区
for (int i = 0; i < 1000000; i++) {
    str s = xjnumFromInt(i);
    ProcessString(s);
    xrtFree(s);
}
```

---

## 常见错误

### 1. 忘记释放

```c
// ❌ 错误
for (int i = 0; i < 1000; i++) {
    str s = xjnumFromInt(i);
    // 忘记 xrtFree(s)
}

// ✅ 正确
for (int i = 0; i < 1000; i++) {
    str s = xjnumFromInt(i);
    UseString(s);
    xrtFree(s);
}
```

---

### 2. 无效输入

```c
// ❌ 可能导致未定义行为
double n = xjnumToFloat("abc", 0);

// ✅ 验证后转换
if (IsValidNumber("abc")) {
    double n = xjnumToFloat("abc", 0);
} else {
    // 处理错误
}
```

---

## 与标准库对比

| 函数 | 标准库 | JNUM | 优势 |
|------|--------|------|------|
| 整数→字符串 | `sprintf` | `xjnumFromInt` | 5x faster |
| 字符串→整数 | `atoi/strtol` | `xjnumToInt` | 更安全 |
| 浮点→字符串 | `sprintf` | `xjnumFromFloat` | 智能精度 |
| 字符串→浮点 | `atof/strtod` | `xjnumToFloat` | 更快 |

---

## 相关文档

- [JSON 处理](api-json.md) - JSON解析和生成
- [Value 动态类型](api-value.md) - 动态类型转换
- [String 字符串处理](api-string.md) - 字符串操作
- [主索引](README.md)

---

<div align="center">

[⬆️ 返回顶部](#jnum-数值转换库)

</div>
