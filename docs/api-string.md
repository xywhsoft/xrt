# String 字符串处理库

> 字符串操作、搜索、替换、编码解码功能

[返回索引](README.md)

---

## 📑 目录

- [字符串复制](#字符串复制)
- [字符串比较](#字符串比较)
- [大小写转换](#大小写转换)
- [字符串搜索](#字符串搜索)
- [字符串裁剪](#字符串裁剪)
- [字符串过滤](#字符串过滤)
- [字符串操作](#字符串操作)
- [编码解码](#编码解码)

---

## 字符串复制

### xrtCopyStr

复制UTF-8字符串

**函数原型：**
```c
XXAPI str xrtCopyStr(str sText, size_t iSize);
```

**参数：**
- `sText` - 源字符串
- `iSize` - 字节长度（0表示自动计算）

**返回值：**
- 新分配的字符串副本

**内存释放：** ✅ 需要 `xrtFree` 释放

**示例：**
```c
str original = "Hello";
str copy = xrtCopyStr(original, 0);
printf("%s\n", copy);
xrtFree(copy);
```

---

### xrtCopyStrU16

复制UTF-16字符串

**函数原型：**
```c
XXAPI u16str xrtCopyStrU16(u16str sText, size_t iSize);
```

**内存释放：** ✅ 需要 `xrtFree` 释放

---

### xrtCopyStrU32

复制UTF-32字符串

**函数原型：**
```c
XXAPI u32str xrtCopyStrU32(u32str sText, size_t iSize);
```

**内存释放：** ✅ 需要 `xrtFree` 释放

---

### xrtCopyMem

复制内存块

**函数原型：**
```c
XXAPI ptr xrtCopyMem(ptr pMem, size_t iSize);
```

**参数：**
- `pMem` - 源内存
- `iSize` - 字节数

**返回值：**
- 新分配的内存副本

**内存释放：** ✅ 需要 `xrtFree` 释放

---

## 字符串比较

### xrtStrComp

字符串比较

**函数原型：**
```c
XXAPI int xrtStrComp(str s1, str s2, size_t iSize, bool bCase);
```

**参数：**
- `s1` - 字符串1
- `s2` - 字符串2
- `iSize` - 比较长度（0表示全部）
- `bCase` - 是否区分大小写
  - `TRUE` - 区分大小写
  - `FALSE` - 不区分大小写

**返回值：**
- `0` - 相等
- `<0` - s1 < s2
- `>0` - s1 > s2

**示例：**
```c
// 区分大小写
int r1 = xrtStrComp("Hello", "hello", 0, TRUE);   // != 0

// 不区分大小写
int r2 = xrtStrComp("Hello", "hello", 0, FALSE);  // == 0

// 比较前3个字符
int r3 = xrtStrComp("Hello", "Help", 3, TRUE);    // == 0
```

---

## 大小写转换

### xrtLCase

转换为小写

**函数原型：**
```c
XXAPI str xrtLCase(str sText, size_t iSize, bool bSrcRevise);
```

**参数：**
- `sText` - 源字符串
- `iSize` - 长度（0表示自动）
- `bSrcRevise` - 是否修改源字符串
  - `TRUE` - 直接修改源，返回源指针
  - `FALSE` - 创建新字符串

**返回值：**
- 小写字符串指针

**内存释放：**
- `bSrcRevise=TRUE` - ⭕ 无需释放
- `bSrcRevise=FALSE` - ✅ 需要 `xrtFree` 释放

**示例：**
```c
// 创建新字符串
str lower = xrtLCase("HELLO", 0, FALSE);
printf("%s\n", lower);  // "hello"
xrtFree(lower);

// 修改源字符串
str text = xrtCopyStr("WORLD", 0);
xrtLCase(text, 0, TRUE);
printf("%s\n", text);   // "world"
xrtFree(text);
```

---

### xrtUCase

转换为大写

**函数原型：**
```c
XXAPI str xrtUCase(str sText, size_t iSize, bool bSrcRevise);
```

**参数：**同 `xrtLCase`

**内存释放：**同 `xrtLCase`

**示例：**
```c
str upper = xrtUCase("hello", 0, FALSE);
printf("%s\n", upper);  // "HELLO"
xrtFree(upper);
```

---

## 字符串搜索

### xrtFindStr

查找子字符串

**函数原型：**
```c
XXAPI str xrtFindStr(str sText, size_t iSize, str sSubText, size_t iSubSize, bool bCase);
```

**参数：**
- `sText` - 被搜索的字符串
- `iSize` - 长度（0自动）
- `sSubText` - 要查找的子串
- `iSubSize` - 子串长度（0自动）
- `bCase` - 是否区分大小写

**返回值：**
- 找到：返回子串位置指针
- 未找到：返回 `NULL`

**示例：**
```c
str text = "Hello World";
str found = xrtFindStr(text, 0, "World", 0, TRUE);
if (found) {
    printf("Found at: %s\n", found);  // "World"
}

// 不区分大小写
found = xrtFindStr(text, 0, "world", 0, FALSE);
if (found) {
    printf("Found\n");
}
```

---

### xrtInStr

查找子串位置（返回索引）

**函数原型：**
```c
XXAPI uint xrtInStr(str sText, size_t iSize, str sSubText, size_t iSubSize, bool bCase);
```

**返回值：**
- 找到：返回位置索引（从1开始）
- 未找到：返回 0

**示例：**
```c
str text = "Hello World";
uint pos = xrtInStr(text, 0, "World", 0, TRUE);
printf("Position: %u\n", pos);  // 7
```

---

### xrtCheckStr

检查字符串是否包含指定字符

**函数原型：**
```c
XXAPI str xrtCheckStr(str sText, size_t iSize, str sSubText, size_t iSubSize);
```

**参数：**
- `sText` - 被检查的字符串
- `sSubText` - 字符集合

**返回值：**
- 找到：返回第一个匹配字符的位置
- 未找到：返回 `NULL`

**说明：**
- 检查 `sText` 中是否包含 `sSubText` 中的任意字符
- 支持 UTF-8 多字节字符

**示例：**
```c
str text = "Hello123";
str found = xrtCheckStr(text, 0, "0123456789", 0);
if (found) {
    printf("Found digit at: %s\n", found);  // "123"
}
```

---

## 字符串裁剪

### xrtLTrim

左侧裁剪

**函数原型：**
```c
XXAPI str xrtLTrim(str sText, size_t iSize, str sSubText, size_t iSubSize, bool bSrcRevise);
```

**参数：**
- `sText` - 源字符串
- `sSubText` - 要裁剪的字符集
- `bSrcRevise` - 是否修改源

**返回值：**
- 裁剪后的字符串

**内存释放：**
- `bSrcRevise=FALSE` - ✅ 需要释放

**示例：**
```c
str text = "   Hello   ";
str trimmed = xrtLTrim(text, 0, " ", 0, FALSE);
printf("[%s]\n", trimmed);  // "[Hello   ]"
xrtFree(trimmed);
```

---

### xrtRTrim

右侧裁剪

**函数原型：**
```c
XXAPI str xrtRTrim(str sText, size_t iSize, str sSubText, size_t iSubSize, bool bSrcRevise);
```

**示例：**
```c
str text = "   Hello   ";
str trimmed = xrtRTrim(text, 0, " ", 0, FALSE);
printf("[%s]\n", trimmed);  // "[   Hello]"
xrtFree(trimmed);
```

---

### xrtTrim

两侧裁剪

**函数原型：**
```c
XXAPI str xrtTrim(str sText, size_t iSize, str sSubText, size_t iSubSize, bool bSrcRevise);
```

**示例：**
```c
str text = "   Hello   ";
str trimmed = xrtTrim(text, 0, " \t\r\n", 0, FALSE);
printf("[%s]\n", trimmed);  // "[Hello]"
xrtFree(trimmed);
```

---

## 字符串过滤

### xrtFilterStr

过滤字符

**函数原型：**
```c
XXAPI str xrtFilterStr(str sText, size_t iSize, str sFilter, size_t iSubSize, bool bSrcRevise);
```

**参数：**
- `sText` - 源字符串
- `sFilter` - 要过滤掉的字符集
- `bSrcRevise` - 是否修改源

**返回值：**
- 过滤后的字符串

**内存释放：**
- `bSrcRevise=FALSE` - ✅ 需要释放

**示例：**
```c
str text = "Hello123World456";
str filtered = xrtFilterStr(text, 0, "0123456789", 0, FALSE);
printf("%s\n", filtered);  // "HelloWorld"
xrtFree(filtered);
```

---

## 字符串操作

### xrtFormat

格式化字符串

**函数原型：**
```c
XXAPI str xrtFormat(str sFormat, ...);
```

**参数：**
- `sFormat` - 格式化字符串（类似 `printf`）
- `...` - 可变参数

**返回值：**
- 格式化后的字符串

**内存释放：** ✅ 需要 `xrtFree` 释放

**示例：**
```c
str text = xrtFormat("Hello %s, you are %d years old", "Tom", 25);
printf("%s\n", text);
xrtFree(text);

str path = xrtFormat("%s\\%s", "C:\\folder", "file.txt");
xrtFree(path);
```

---

### xrtReplace

字符串替换

**函数原型：**
```c
XXAPI str xrtReplace(str sText, size_t iSize, str sSubText, size_t iSubSize, str sRepText, size_t iRepSize);
```

**参数：**
- `sText` - 源字符串
- `sSubText` - 要替换的子串
- `sRepText` - 替换成的字符串

**返回值：**
- 替换后的新字符串

**内存释放：** ✅ 需要 `xrtFree` 释放

**示例：**
```c
str text = "Hello World";
str replaced = xrtReplace(text, 0, "World", 0, "XRT", 0);
printf("%s\n", replaced);  // "Hello XRT"
xrtFree(replaced);

// 全部替换
str multi = "a b a b a";
str result = xrtReplace(multi, 0, "a", 0, "c", 0);
printf("%s\n", result);  // "c b c b c"
xrtFree(result);
```

---

### xrtSplit

字符串分割

**函数原型：**
```c
XXAPI str* xrtSplit(str sText, size_t iSize, str sSepText, size_t iSepSize, bool bSrcRevise);
```

**参数：**
- `sText` - 源字符串
- `sSepText` - 分隔符
- `bSrcRevise` - 是否修改源
  - `TRUE` - 直接修改源（在原字符串中插入 `\0`）
  - `FALSE` - 创建新字符串

**返回值：**
- 字符串指针数组（以 `NULL` 结尾）

**内存释放：** ✅ **总是需要** `xrtFree` 释放数组

**示例：**
```c
str text = xrtCopyStr("apple,banana,orange", 0);
str* parts = xrtSplit(text, 0, ",", 0, TRUE);

// 遍历结果
for (int i = 0; parts[i] != NULL; i++) {
    printf("%d: %s\n", i, parts[i]);
}
// 输出:
// 0: apple
// 1: banana
// 2: orange

xrtFree(parts);  // 释放数组
xrtFree(text);   // 释放源字符串
```

---

### xrtRandStr

生成随机字符串

**函数原型：**
```c
XXAPI str xrtRandStr(str sTemplate, size_t iSize, size_t iLen);
```

**参数：**
- `sTemplate` - 字符模板（从中随机选择）
- `iSize` - 模板长度（0自动）
- `iLen` - 生成长度

**返回值：**
- 随机字符串

**内存释放：** ✅ 需要 `xrtFree` 释放

**示例：**
```c
// 生成8位数字密码
str pwd = xrtRandStr("0123456789", 0, 8);
printf("PIN: %s\n", pwd);  // 例如: "57382910"
xrtFree(pwd);

// 生成混合密码
str password = xrtRandStr(
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789",
    0, 16
);
printf("Password: %s\n", password);
xrtFree(password);
```

---

## 编码解码

### xrtHexEncode

HEX 编码

**函数原型：**
```c
XXAPI str xrtHexEncode(ptr pMem, size_t iSize);
```

**参数：**
- `pMem` - 二进制数据
- `iSize` - 数据长度

**返回值：**
- HEX 字符串（大写）

**内存释放：** ✅ 需要 `xrtFree` 释放

**示例：**
```c
char data[] = {0x12, 0x34, 0x56, 0xAB, 0xCD};
str hex = xrtHexEncode(data, sizeof(data));
printf("%s\n", hex);  // "123456ABCD"
xrtFree(hex);
```

---

### xrtHexDecode

HEX 解码

**函数原型：**
```c
XXAPI ptr xrtHexDecode(str pText, size_t iSize);
```

**参数：**
- `pText` - HEX 字符串
- `iSize` - 字符串长度（0自动）

**返回值：**
- 二进制数据
- 解码后的长度存储在 `xCore.iRet`

**内存释放：** ✅ 需要 `xrtFree` 释放

**示例：**
```c
str hex = "48656C6C6F";  // "Hello" 的HEX
ptr data = xrtHexDecode(hex, 0);
size_t len = xCore.iRet;
printf("%.*s\n", (int)len, (char*)data);  // "Hello"
xrtFree(data);
```

---

### xrtBase64Encode

Base64 编码

**函数原型：**
```c
XXAPI str xrtBase64Encode(ptr pMem, size_t iSize, str sTable);
```

**参数：**
- `pMem` - 二进制数据
- `iSize` - 数据长度
- `sTable` - 编码表（`NULL` 使用标准表）

**返回值：**
- Base64 字符串

**内存释放：** ✅ 需要 `xrtFree` 释放

**示例：**
```c
str text = "Hello World";
str b64 = xrtBase64Encode(text, strlen(text), NULL);
printf("%s\n", b64);  // "SGVsbG8gV29ybGQ="
xrtFree(b64);

// 使用自定义编码表
str custom = xrtBase64Encode(data, size, "custom_table...");
xrtFree(custom);
```

---

### xrtBase64Decode

Base64 解码

**函数原型：**
```c
XXAPI ptr xrtBase64Decode(str sText, size_t iSize, str sTable);
```

**参数：**
- `sText` - Base64 字符串
- `iSize` - 长度（0自动）
- `sTable` - 解码表（`NULL` 使用标准表）

**返回值：**
- 二进制数据
- 长度存储在 `xCore.iRet`

**内存释放：** ✅ 需要 `xrtFree` 释放

**示例：**
```c
str b64 = "SGVsbG8gV29ybGQ=";
ptr data = xrtBase64Decode(b64, 0, NULL);
size_t len = xCore.iRet;
printf("%.*s\n", (int)len, (char*)data);  // "Hello World"
xrtFree(data);
```

---

## 使用场景

### 1. 文本处理

```c
// 清理用户输入
str CleanInput(str input) {
    // 裁剪空白
    str trimmed = xrtTrim(input, 0, " \t\r\n", 0, FALSE);
    // 转小写
    str lower = xrtLCase(trimmed, 0, TRUE);
    xrtFree(trimmed);
    return lower;
}
```

---

### 2. 配置解析

```c
// 解析 key=value
void ParseConfig(str line) {
    str* parts = xrtSplit(line, 0, "=", 0, FALSE);
    if (parts[0] && parts[1]) {
        str key = xrtTrim(parts[0], 0, " ", 0, FALSE);
        str value = xrtTrim(parts[1], 0, " ", 0, FALSE);
        printf("%s = %s\n", key, value);
        xrtFree(key);
        xrtFree(value);
    }
    xrtFree(parts);
}
```

---

### 3. URL 编码

```c
str UrlEncode(str text) {
    // 简化版
    str encoded = xrtReplace(text, 0, " ", 0, "%20", 0);
    // ... 处理其他字符
    return encoded;
}
```

---

## 最佳实践

### 1. 内存管理

```c
// ✅ 正确
str lower = xrtLCase("TEXT", 0, FALSE);
UseString(lower);
xrtFree(lower);

// ❌ 内存泄漏
str lower = xrtLCase("TEXT", 0, FALSE);
// 忘记释放
```

---

### 2. 原地修改

```c
// 可以修改源数据时
str text = xrtMalloc(100);
strcpy(text, "HELLO");
xrtLCase(text, 0, TRUE);  // 原地修改
xrtFree(text);
```

---

## 相关文档

- [Charset 字符集转换](api-charset.md)
- [Path 路径处理](api-path.md)
- [主索引](README.md)

---

<div align="center">

[⬆️ 返回顶部](#string-字符串处理库)

</div>
