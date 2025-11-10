# Charset 字符集转换库

> 字符编码转换、检测和处理功能

[返回索引](README.md)

---

## 📑 目录

- [字符集常量](#字符集常量)
- [UTF编码转换](#utf编码转换)
- [字节序转换](#字节序转换)
- [通用编码转换](#通用编码转换)
- [编码检测](#编码检测)
- [辅助函数](#辅助函数)

---

## 字符集常量

### 字符集代码页

```c
#define XRT_CP_AUTO         -2      // 自动识别字符集
#define XRT_CP_BINARY       -1      // 二进制文件
#define XRT_CP_OEM          0       // 本机OEM字符集
#define XRT_CP_UTF8         65001   // UTF-8
#define XRT_CP_UTF16        1200    // UTF-16 (小端序)
#define XRT_CP_UTF16_BE     1201    // UTF-16 (大端序)
#define XRT_CP_UTF32        65005   // UTF-32 (小端序)
#define XRT_CP_UTF32_BE     65006   // UTF-32 (大端序)
```

### BOM 标记

```c
#define XRT_CP_BOM          0x40000000  // 带BOM标记
#define XRT_MASK_BOM        0xBFFFFFFF  // BOM掩码
```

**使用方式：**
```c
// 使用带BOM的UTF-8
int charset = XRT_CP_UTF8 | XRT_CP_BOM;

// 去除BOM标记
int pure_charset = charset & XRT_MASK_BOM;
```

---

## UTF编码转换

### xrtUTF8to16

UTF-8 转 UTF-16

**函数原型：**
```c
XXAPI u16str xrtUTF8to16(u8str sText, size_t iSize);
```

**参数：**
- `sText` - UTF-8 字符串
- `iSize` - 字节长度（0表示自动计算到`\0`）

**返回值：**
- 成功：返回 UTF-16 字符串指针
- 失败：返回 `NULL`

**内存释放：** ✅ 需要使用 `xrtFree` 释放

**示例：**
```c
str utf8 = "你好，世界！";
u16str utf16 = xrtUTF8to16(utf8, 0);
if (utf16) {
    // 使用 UTF-16 字符串
    wprintf(L"%ls\n", utf16);
    xrtFree(utf16);
}
```

---

### xrtUTF8to32

UTF-8 转 UTF-32

**函数原型：**
```c
XXAPI u32str xrtUTF8to32(u8str sText, size_t iSize);
```

**参数：**
- `sText` - UTF-8 字符串
- `iSize` - 字节长度（0表示自动计算）

**返回值：**
- 成功：返回 UTF-32 字符串指针
- 失败：返回 `NULL`

**内存释放：** ✅ 需要使用 `xrtFree` 释放

**示例：**
```c
str utf8 = "Hello";
u32str utf32 = xrtUTF8to32(utf8, 0);
if (utf32) {
    // UTF-32 每个字符占4字节
    xrtFree(utf32);
}
```

---

### xrtUTF16to8

UTF-16 转 UTF-8

**函数原型：**
```c
XXAPI u8str xrtUTF16to8(u16str sText, size_t iSize);
```

**参数：**
- `sText` - UTF-16 字符串
- `iSize` - 字符数（0表示自动计算到`\0`）

**返回值：**
- 成功：返回 UTF-8 字符串指针
- 失败：返回 `NULL`

**内存释放：** ✅ 需要使用 `xrtFree` 释放

**示例：**
```c
u16str utf16 = L"你好";
u8str utf8 = xrtUTF16to8(utf16, 0);
if (utf8) {
    printf("%s\n", utf8);
    xrtFree(utf8);
}
```

---

### xrtUTF16to32

UTF-16 转 UTF-32

**函数原型：**
```c
XXAPI u32str xrtUTF16to32(u16str sText, size_t iSize);
```

**参数：**
- `sText` - UTF-16 字符串
- `iSize` - 字符数（0表示自动计算）

**返回值：**
- 成功：返回 UTF-32 字符串指针
- 失败：返回 `NULL`

**内存释放：** ✅ 需要使用 `xrtFree` 释放

---

### xrtUTF32to8

UTF-32 转 UTF-8

**函数原型：**
```c
XXAPI u8str xrtUTF32to8(u32str sText, size_t iSize);
```

**参数：**
- `sText` - UTF-32 字符串
- `iSize` - 字符数（0表示自动计算）

**返回值：**
- 成功：返回 UTF-8 字符串指针
- 失败：返回 `NULL`

**内存释放：** ✅ 需要使用 `xrtFree` 释放

---

### xrtUTF32to16

UTF-32 转 UTF-16

**函数原型：**
```c
XXAPI u16str xrtUTF32to16(u32str sText, size_t iSize);
```

**参数：**
- `sText` - UTF-32 字符串
- `iSize` - 字符数（0表示自动计算）

**返回值：**
- 成功：返回 UTF-16 字符串指针
- 失败：返回 `NULL`

**内存释放：** ✅ 需要使用 `xrtFree` 释放

---

## 字节序转换

### xrtUTF16LEtoBE

UTF-16 小端序与大端序转换

**函数原型：**
```c
XXAPI u16str xrtUTF16LEtoBE(u16str sText, size_t iSize, bool bSrcRevise);
```

**参数：**
- `sText` - UTF-16 字符串
- `iSize` - 字符数（0表示自动计算）
- `bSrcRevise` - 是否直接修改源数据
  - `TRUE` - 直接修改源字符串，返回源指针
  - `FALSE` - 创建新字符串，需要 `xrtFree` 释放

**返回值：**
- `bSrcRevise=TRUE`：返回源指针 `sText`
- `bSrcRevise=FALSE`：返回新分配的字符串

**内存释放：** 
- `bSrcRevise=TRUE` - ⭕ 无需释放
- `bSrcRevise=FALSE` - ✅ 需要 `xrtFree` 释放

**示例：**
```c
// 不修改源数据
u16str le_text = L"Hello";
u16str be_text = xrtUTF16LEtoBE(le_text, 0, FALSE);
// 使用 be_text
xrtFree(be_text);

// 直接修改源数据
u16str text = xrtMalloc(100 * sizeof(u16));
// ... 填充数据
xrtUTF16LEtoBE(text, 0, TRUE);  // 直接转换
xrtFree(text);
```

---

### xrtUTF32LEtoBE

UTF-32 小端序与大端序转换

**函数原型：**
```c
XXAPI u32str xrtUTF32LEtoBE(u32str sText, size_t iSize, bool bSrcRevise);
```

**参数：**
- `sText` - UTF-32 字符串
- `iSize` - 字符数（0表示自动计算）
- `bSrcRevise` - 是否直接修改源数据

**返回值：**
- `bSrcRevise=TRUE`：返回源指针
- `bSrcRevise=FALSE`：返回新字符串

**内存释放：**
- `bSrcRevise=TRUE` - ⭕ 无需释放
- `bSrcRevise=FALSE` - ✅ 需要 `xrtFree` 释放

---

## 通用编码转换

### xrtConvCharset

任意字符集转换

**函数原型：**
```c
XXAPI ptr xrtConvCharset(ptr sText, size_t iSize, int iInCP, int iOutCP);
```

**参数：**
- `sText` - 源字符串
- `iSize` - 源字符串字节长度（0表示自动计算）
- `iInCP` - 输入字符集代码页
- `iOutCP` - 输出字符集代码页

**返回值：**
- 成功：返回转换后的字符串
- 失败：返回 `NULL`

**内存释放：** ✅ 需要使用 `xrtFree` 释放

**支持的字符集：**
- `XRT_CP_AUTO` - 自动检测（仅作为输入）
- `XRT_CP_BINARY` - 二进制（不转换）
- `XRT_CP_OEM` - 本机字符集（Windows: ANSI, Linux: UTF-8）
- `XRT_CP_UTF8` - UTF-8
- `XRT_CP_UTF16` - UTF-16 (LE)
- `XRT_CP_UTF16_BE` - UTF-16 (BE)
- `XRT_CP_UTF32` - UTF-32 (LE)
- `XRT_CP_UTF32_BE` - UTF-32 (BE)

**示例：**
```c
// UTF-8 转 UTF-16
str utf8 = "你好";
u16str utf16 = xrtConvCharset(utf8, 0, XRT_CP_UTF8, XRT_CP_UTF16);
xrtFree(utf16);

// 自动检测输入编码，转换为 UTF-8
ptr data = ReadFile("unknown.txt");
str utf8 = xrtConvCharset(data, 0, XRT_CP_AUTO, XRT_CP_UTF8);
xrtFree(utf8);

// 本机编码转 UTF-8
str oem = "本机编码文本";
str utf8 = xrtConvCharset(oem, 0, XRT_CP_OEM, XRT_CP_UTF8);
xrtFree(utf8);
```

---

## 编码检测

### xrtIsUTF8

判断是否为合法的 UTF-8 字符串

**函数原型：**
```c
XXAPI bool xrtIsUTF8(str sText, size_t iSize);
```

**参数：**
- `sText` - 要检测的字符串
- `iSize` - 字节长度（0表示自动计算）

**返回值：**
- `TRUE` - 是合法的 UTF-8
- `FALSE` - 不是 UTF-8 或含有非法序列

**示例：**
```c
str text1 = "Hello 世界";
bool is_utf8 = xrtIsUTF8(text1, 0);  // TRUE

char text2[] = {0xFF, 0xFE, 0x00};
is_utf8 = xrtIsUTF8(text2, 3);       // FALSE
```

---

### xrtDetectCharset

自动检测字符集编码

**函数原型：**
```c
XXAPI int xrtDetectCharset(ptr sText, size_t iSize, bool bBOM);
```

**参数：**
- `sText` - 要检测的数据
- `iSize` - 数据长度
- `bBOM` - 检测到的BOM信息（通过返回值传递）

**返回值：**
- 检测到的字符集代码页（可能包含 `XRT_CP_BOM` 标记）
- 如果带BOM，返回值会包含 `XRT_CP_BOM` 位

**检测规则：**
1. 首先检测 BOM 标记
2. 然后检测是否为合法的 UTF-8
3. 根据 `\0` 的分布推测 UTF-16/UTF-32
4. 都不符合则返回 `XRT_CP_OEM` 或 `XRT_CP_BINARY`

**BOM 标记：**
- UTF-8 BOM: `EF BB BF`
- UTF-16 LE BOM: `FF FE`
- UTF-16 BE BOM: `FE FF`
- UTF-32 LE BOM: `FF FE 00 00`
- UTF-32 BE BOM: `00 00 FE FF`

**示例：**
```c
ptr data = ReadFile("unknown.txt");
size_t size = GetFileSize("unknown.txt");

int charset = xrtDetectCharset(data, size, TRUE);

// 检查是否有BOM
if (charset & XRT_CP_BOM) {
    printf("File has BOM\n");
    charset &= XRT_MASK_BOM;  // 去除BOM标记
}

// 识别字符集
switch (charset) {
    case XRT_CP_UTF8:
        printf("UTF-8 encoding\n");
        break;
    case XRT_CP_UTF16:
        printf("UTF-16 LE encoding\n");
        break;
    case XRT_CP_UTF16_BE:
        printf("UTF-16 BE encoding\n");
        break;
    case XRT_CP_UTF32:
        printf("UTF-32 LE encoding\n");
        break;
    case XRT_CP_UTF32_BE:
        printf("UTF-32 BE encoding\n");
        break;
    case XRT_CP_OEM:
        printf("OEM/ANSI encoding\n");
        break;
    case XRT_CP_BINARY:
        printf("Binary file\n");
        break;
}

xrtFree(data);
```

---

### xrtGetCharSize

获取字符集的字符大小

**函数原型：**
```c
XXAPI int xrtGetCharSize(int iCP);
```

**参数：**
- `iCP` - 字符集代码页

**返回值：**
- 该字符集每个字符占用的字节数
- `-1` 表示可变长度编码

**字符大小：**
- `XRT_CP_UTF8` → `-1` (可变: 1-6字节)
- `XRT_CP_UTF16` → `2` (固定2字节，不考虑代理对)
- `XRT_CP_UTF32` → `4` (固定4字节)
- `XRT_CP_OEM` → `-1` (可变)

**示例：**
```c
int size_utf8 = xrtGetCharSize(XRT_CP_UTF8);    // -1
int size_utf16 = xrtGetCharSize(XRT_CP_UTF16);  // 2
int size_utf32 = xrtGetCharSize(XRT_CP_UTF32);  // 4
```

---

## 辅助函数

### 字符串长度函数

这些函数在 `base.h` 中定义，补充了 UTF-16/32 支持：

```c
// UTF-16 字符串长度（字符数）
XXAPI size_t u16len(u16str sText);

// UTF-32 字符串长度（字符数）
XXAPI size_t u32len(u32str sText);
```

**示例：**
```c
u16str text16 = L"Hello";
size_t len16 = u16len(text16);  // 5

u32str text32 = U"World";
size_t len32 = u32len(text32);  // 5
```

---

## 使用场景

### 1. 跨平台文本处理

```c
// Windows: 使用 UTF-16（宽字符）
#ifdef _WIN32
    u16str wtext = xrtUTF8to16("文本", 0);
    MessageBoxW(NULL, wtext, L"Title", MB_OK);
    xrtFree(wtext);
#else
    // Linux/macOS: 直接使用 UTF-8
    printf("%s\n", "文本");
#endif
```

---

### 2. 文件编码转换

```c
// 读取文件并自动检测编码
ptr file_data = xrtFileGetAll("input.txt");
size_t file_size = xCore.iRet;

int detected = xrtDetectCharset(file_data, file_size, TRUE);
int charset = detected & XRT_MASK_BOM;

// 转换为 UTF-8
str utf8_text = xrtConvCharset(
    file_data, file_size,
    charset, XRT_CP_UTF8
);

// 保存为 UTF-8
xrtFileWriteAll("output.txt", utf8_text, 0, XRT_CP_UTF8);

xrtFree(file_data);
xrtFree(utf8_text);
```

---

### 3. 网络数据处理

```c
// 接收 UTF-16 数据并转换为 UTF-8
u16str received_data = ReceiveFromNetwork();
str utf8_data = xrtUTF16to8(received_data, 0);

// 处理 UTF-8 数据
ProcessText(utf8_data);

xrtFree(utf8_data);
```

---

### 4. 国际化字符串

```c
// 多语言文本处理
str chinese = "你好世界";
str english = "Hello World";

// 统一转换为 UTF-16 在 Windows 上显示
u16str wchinese = xrtUTF8to16(chinese, 0);
u16str wenglish = xrtUTF8to16(english, 0);

DisplayText(wchinese);
DisplayText(wenglish);

xrtFree(wchinese);
xrtFree(wenglish);
```

---

## 最佳实践

### 1. 统一使用 UTF-8

```c
// ✅ 推荐：内部统一使用 UTF-8
str internal_text = "内部文本";

// 只在需要时才转换
#ifdef _WIN32
    u16str display_text = xrtUTF8to16(internal_text, 0);
    // ... 使用 display_text
    xrtFree(display_text);
#endif
```

---

### 2. 自动编码检测

```c
// ✅ 处理未知编码的文件
ptr data = LoadFile("unknown.txt");
size_t size = GetFileSize("unknown.txt");

// 自动转换为 UTF-8
str utf8 = xrtConvCharset(data, size, XRT_CP_AUTO, XRT_CP_UTF8);
if (utf8) {
    ProcessUTF8(utf8);
    xrtFree(utf8);
}
xrtFree(data);
```

---

### 3. BOM 处理

```c
// 检测并处理 BOM
int charset = xrtDetectCharset(data, size, TRUE);

if (charset & XRT_CP_BOM) {
    // 有 BOM，跳过 BOM 字节
    ptr text_start = data;
    size_t bom_size = 0;
    
    switch (charset & XRT_MASK_BOM) {
        case XRT_CP_UTF8:    bom_size = 3; break;  // EF BB BF
        case XRT_CP_UTF16:   bom_size = 2; break;  // FF FE
        case XRT_CP_UTF16_BE:bom_size = 2; break;  // FE FF
        case XRT_CP_UTF32:   bom_size = 4; break;  // FF FE 00 00
        case XRT_CP_UTF32_BE:bom_size = 4; break;  // 00 00 FE FF
    }
    
    text_start += bom_size;
    size -= bom_size;
}
```

---

### 4. 字节序兼容性

```c
// 网络传输：统一使用大端序
u16str local_text = L"数据";

// 转换为大端序后发送
u16str be_text = xrtUTF16LEtoBE(local_text, 0, FALSE);
SendToNetwork(be_text);
xrtFree(be_text);

// 接收大端序数据并转换回本机序
u16str received = ReceiveFromNetwork();
u16str le_text = xrtUTF16LEtoBE(received, 0, FALSE);
ProcessText(le_text);
xrtFree(le_text);
```

---

## 性能提示

### 1. 避免重复转换

```c
// ❌ 低效：重复转换
for (int i = 0; i < 1000; i++) {
    u16str wtext = xrtUTF8to16(utf8_array[i], 0);
    Display(wtext);
    xrtFree(wtext);
}

// ✅ 高效：预转换
u16str* wtext_array = xrtMalloc(1000 * sizeof(u16str));
for (int i = 0; i < 1000; i++) {
    wtext_array[i] = xrtUTF8to16(utf8_array[i], 0);
}
// ... 使用
for (int i = 0; i < 1000; i++) {
    xrtFree(wtext_array[i]);
}
xrtFree(wtext_array);
```

---

### 2. 使用原地转换

```c
// 字节序转换时，如果可以修改源数据
u16str text = AllocateAndFillData();

// 原地转换，无需额外内存
xrtUTF16LEtoBE(text, 0, TRUE);
// ... 使用
xrtFree(text);
```

---

## 常见错误

### 1. 忘记释放内存

```c
// ❌ 内存泄漏
u16str text = xrtUTF8to16("text", 0);
// 忘记 xrtFree(text);

// ✅ 正确
u16str text = xrtUTF8to16("text", 0);
UseText(text);
xrtFree(text);
```

---

### 2. 字符数与字节数混淆

```c
// ❌ 错误：UTF-8 字节数不等于字符数
str utf8 = "你好";  // 6字节，2个汉字
u16str utf16 = xrtUTF8to16(utf8, 2);  // 错误！2是字节数

// ✅ 正确：UTF-8 转换时用字节数
u16str utf16 = xrtUTF8to16(utf8, 6);  // 或用 0 自动计算
```

---

## 相关文档

- [类型定义](types.md) - 字符串类型定义
- [String 字符串处理](api-string.md) - 字符串操作
- [File 文件操作](api-file.md) - 文件编码处理
- [主索引](README.md) - 返回文档首页

---

<div align="center">

[⬆️ 返回顶部](#charset-字符集转换库)

</div>
