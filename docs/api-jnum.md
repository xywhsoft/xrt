# JNUM 数值转换库

> JSON数字与字符串高性能转换

[返回索引](README.md)

---

## 📑 目录

- [整数转换](#整数转换)
- [浮点数转换](#浮点数转换)

---

## 整数转换

### xjnumFromInt

整数转字符串

**函数原型：**
```c
XXAPI str xjnumFromInt(int64 iVal);
```

**参数：**
- `iVal` - 整数值

**返回值：**
- 数字字符串

**内存释放：** ✅ 需要 `xrtFree` 释放

**示例：**
```c
str s = xjnumFromInt(12345);
printf("%s\n", s);  // "12345"
xrtFree(s);

str neg = xjnumFromInt(-999);
printf("%s\n", neg);  // "-999"
xrtFree(neg);
```

---

### xjnumToInt

字符串转整数

**函数原型：**
```c
XXAPI int64 xjnumToInt(str sVal, size_t iSize);
```

**参数：**
- `sVal` - 数字字符串
- `iSize` - 长度（0自动）

**返回值：**
- 整数值

**示例：**
```c
int64 n1 = xjnumToInt("12345", 0);    // 12345
int64 n2 = xjnumToInt("-999", 0);     // -999
int64 n3 = xjnumToInt("0xFF", 0);     // 255 (支持16进制)
```

---

## 浮点数转换

### xjnumFromFloat

浮点数转字符串

**函数原型：**
```c
XXAPI str xjnumFromFloat(double nVal);
```

**参数：**
- `nVal` - 浮点数值

**返回值：**
- 数字字符串（智能精度）

**内存释放：** ✅ 需要 `xrtFree` 释放

**说明：**
- 自动选择最佳精度
- 去除无意义的尾随零
- 支持科学计数法

**示例：**
```c
str s1 = xjnumFromFloat(3.14159);
printf("%s\n", s1);  // "3.14159"
xrtFree(s1);

str s2 = xjnumFromFloat(1000000.0);
printf("%s\n", s2);  // "1000000" 或 "1e6"
xrtFree(s2);

str s3 = xjnumFromFloat(0.00001);
printf("%s\n", s3);  // "0.00001" 或 "1e-5"
xrtFree(s3);
```

---

### xjnumToFloat

字符串转浮点数

**函数原型：**
```c
XXAPI double xjnumToFloat(str sVal, size_t iSize);
```

**参数：**
- `sVal` - 数字字符串
- `iSize` - 长度（0自动）

**返回值：**
- 浮点数值

**支持格式：**
- 普通格式：`3.14`, `-2.5`
- 科学计数法：`1.23e10`, `4.56E-5`
- 整数：`100` → `100.0`

**示例：**
```c
double n1 = xjnumToFloat("3.14159", 0);      // 3.14159
double n2 = xjnumToFloat("-2.5", 0);         // -2.5
double n3 = xjnumToFloat("1.23e10", 0);      // 12300000000.0
double n4 = xjnumToFloat("4.56E-5", 0);      // 0.0000456
```

---

## 使用场景

### 1. JSON序列化

```c
str SerializeNumber(xvalue val) {
    if (xvoIsInt(val)) {
        return xjnumFromInt(xvoGetInt(val));
    } else if (xvoIsFloat(val)) {
        return xjnumFromFloat(xvoGetFloat(val));
    }
    return NULL;
}
```

---

### 2. 配置文件解析

```c
void ParseConfigValue(str key, str value) {
    // 尝试解析为整数
    if (IsInteger(value)) {
        int64 n = xjnumToInt(value, 0);
        SetConfigInt(key, n);
    }
    // 尝试解析为浮点数
    else if (IsFloat(value)) {
        double n = xjnumToFloat(value, 0);
        SetConfigFloat(key, n);
    }
}
```

---

### 3. 数据交换

```c
// 发送数据
void SendValue(double value) {
    str text = xjnumFromFloat(value);
    SendToNetwork(text);
    xrtFree(text);
}

// 接收数据
double ReceiveValue() {
    str text = ReceiveFromNetwork();
    double value = xjnumToFloat(text, 0);
    xrtFree(text);
    return value;
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
