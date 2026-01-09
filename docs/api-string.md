# String 字符串处理库

> 字符串操作、搜索、替换、编码解码功能

[English](api-string.en.md) | [中文](api-string.md) | [返回索引](README.md)

---

## 📑 目录

- [常量定义](#常量定义)
- [字符串复制](#字符串复制)
- [字符串比较](#字符串比较)
- [大小写转换](#大小写转换)
- [字符串搜索](#字符串搜索)
- [通配符匹配](#通配符匹配)
- [字符串裁剪](#字符串裁剪)
- [字符串过滤](#字符串过滤)
- [字符串操作](#字符串操作)
- [数值格式化](#数值格式化)
- [字符串相似度](#字符串相似度)
- [字符串约等于](#字符串约等于)
- [编码解码](#编码解码)
- [UTF-8 工具函数](#utf-8-工具函数)
- [使用场景](#使用场景)
- [最佳实践](#最佳实践)

---

## 常量定义

### RandStringDefaultTemplate

随机字符串默认模板（内部使用）。

**定义：**
```c
static const str RandStringDefaultTemplate = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz-_";
```

**说明：**
- 当 `xrtRandStr` 的 `sTemplate` 参数为 `NULL` 时使用此模板
- 包含 64 个字符：数字 + 大写字母 + 小写字母 + 连字符 + 下划线

### Base64EncodeTable

标准 Base64 编码表（内部使用）。

**定义：**
```c
static const str Base64EncodeTable = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
```

**说明：**
- 当 `xrtBase64Encode` / `xrtBase64Decode` 的 `sTable` 参数为 `NULL` 时使用标准表

---

## 字符串复制

### xrtCopyStr

复制UTF-8字符串。

**函数原型：**
```c
XXAPI str xrtCopyStr(str sText, size_t iSize);
```

**参数：**
- `sText` - 源字符串
- `iSize` - 字节长度（0表示自动计算）

**返回值：**
- 新分配的字符串副本
- 复制后的字节长度存储在 `xCore.iRet`
- 失败返回 `xCore.sNull`

**内存释放：** ✅ 需要 `xrtFree` 释放

**示例：**
```c
#include "xrt.h"
#include <stdio.h>

int main() {
    xrtInit();
    
    str original = (str)"Hello World";
    str copy = xrtCopyStr(original, 0);
    
    printf("原始: %s\n", original);
    printf("副本: %s\n", copy);
    printf("长度: %" PRId64 "\n", xCore.iRet);
    
    xrtFree(copy);
    xrtUnit();
    return 0;
}
```

**补充说明：**
- 如果 `sText` 为 `NULL` 或长度为 0，返回 `xCore.sNull`
- 返回的字符串已自动添加 `\0` 结尾符

---

### xrtCopyStrU16

复制UTF-16字符串。

**函数原型：**
```c
XXAPI u16str xrtCopyStrU16(u16str sText, size_t iSize);
```

**参数：**
- `sText` - 源UTF-16字符串
- `iSize` - 字符数（非Bytes，0表示自动计算）

**返回值：**
- 新分配的UTF-16字符串副本
- 复制后的字符数存储在 `xCore.iRet`

**内存释放：** ✅ 需要 `xrtFree` 释放

**示例：**
```c
#include "xrt.h"
#include <stdio.h>

int main() {
    xrtInit();
    
    u16str original = (u16str)L"Hello";
    u16str copy = xrtCopyStrU16(original, 0);
    printf("字符数: %" PRId64 "\n", xCore.iRet);
    
    xrtFree(copy);
    xrtUnit();
    return 0;
}
```

---

### xrtCopyStrU32

复制UTF-32字符串。

**函数原型：**
```c
XXAPI u32str xrtCopyStrU32(u32str sText, size_t iSize);
```

**参数：**
- `sText` - 源UTF-32字符串
- `iSize` - 字符数（非Bytes，0表示自动计算）

**返回值：**
- 新分配的UTF-32字符串副本
- 复制后的字符数存储在 `xCore.iRet`

**内存释放：** ✅ 需要 `xrtFree` 释放

---

### xrtCopyMem

复制内存块。

**函数原型：**
```c
XXAPI ptr xrtCopyMem(ptr pMem, size_t iSize);
```

**参数：**
- `pMem` - 源内存
- `iSize` - 字节数

**返回值：**
- 新分配的内存副本
- 复制后的字节数存储在 `xCore.iRet`

**内存释放：** ✅ 需要 `xrtFree` 释放

**示例：**
```c
#include "xrt.h"
#include <stdio.h>

int main() {
    xrtInit();
    
    uint8 data[] = {0x01, 0x02, 0x03, 0x04, 0x05};
    ptr copy = xrtCopyMem(data, sizeof(data));
    
    printf("复制字节数: %" PRId64 "\n", xCore.iRet);
    
    // 验证复制内容
    uint8* p = (uint8*)copy;
    for (int i = 0; i < 5; i++) {
        printf("%02X ", p[i]);
    }
    printf("\n");
    
    xrtFree(copy);
    xrtUnit();
    return 0;
}
```

---

## 字符串比较

### xrtStrComp

字符串比较。

**函数原型：**
```c
XXAPI int xrtStrComp(str s1, str s2, size_t iSize, bool bCase);
```

**参数：**
- `s1` - 字符串1
- `s2` - 字符串2
- `iSize` - 比较长度（0表示全部）
- `bCase` - 是否忽略大小写
  - `TRUE` - **忽略**大小写（case-insensitive）
  - `FALSE` - **区分**大小写（case-sensitive）

**返回值：**
- `0` - 相等
- `<0` - s1 < s2
- `>0` - s1 > s2

**示例：**
```c
#include "xrt.h"
#include <stdio.h>

int main() {
    xrtInit();
    
    // 区分大小写 (bCase=FALSE)
    int r1 = xrtStrComp((str)"Hello", (str)"hello", 0, FALSE);
    printf("区分大小写: %d (不相等)\n", r1);
    
    // 忽略大小写 (bCase=TRUE)
    int r2 = xrtStrComp((str)"Hello", (str)"hello", 0, TRUE);
    printf("忽略大小写: %d (相等)\n", r2);
    
    // 比较前3个字符
    int r3 = xrtStrComp((str)"Hello", (str)"Help", 3, FALSE);
    printf("前3字符: %d (相等)\n", r3);
    
    // 比较前4个字符
    int r4 = xrtStrComp((str)"Hello", (str)"Help", 4, FALSE);
    printf("前4字符: %d (不相等)\n", r4);
    
    xrtUnit();
    return 0;
}
```

**补充说明：**
- Windows 上使用 `stricmp`/`strnicmp`
- Linux/macOS 上使用 `strcasecmp`/`strncasecmp`

---

## 大小写转换

### xrtLCase

转换为小写。

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
#include "xrt.h"
#include <stdio.h>

int main() {
    xrtInit();
    
    // 创建新字符串
    str lower = xrtLCase((str)"HELLO WORLD", 0, FALSE);
    printf("新字符串: %s\n", lower);  // "hello world"
    xrtFree(lower);
    
    // 修改源字符串
    str text = xrtCopyStr((str)"WORLD", 0);
    xrtLCase(text, 0, TRUE);
    printf("修改后: %s\n", text);   // "world"
    xrtFree(text);
    
    xrtUnit();
    return 0;
}
```

**补充说明：**
- 自动跳过 OEM 编码字符（高位为 1 的字节）
- 仅处理 ASCII 范围内的字母

---

### xrtUCase

转换为大写。

**函数原型：**
```c
XXAPI str xrtUCase(str sText, size_t iSize, bool bSrcRevise);
```

**参数：**同 `xrtLCase`

**内存释放：**同 `xrtLCase`

**示例：**
```c
#include "xrt.h"
#include <stdio.h>

int main() {
    xrtInit();
    
    str upper = xrtUCase((str)"hello world", 0, FALSE);
    printf("%s\n", upper);  // "HELLO WORLD"
    xrtFree(upper);
    
    xrtUnit();
    return 0;
}
```

---

## 字符串搜索

### xrtFindStr

查找子字符串。

**函数原型：**
```c
XXAPI str xrtFindStr(str sText, size_t iSize, str sSubText, size_t iSubSize, bool bCase);
```

**参数：**
- `sText` - 被搜索的字符串
- `iSize` - 长度（0自动）
- `sSubText` - 要查找的子串
- `iSubSize` - 子串长度（0自动）
- `bCase` - 是否忽略大小写（`TRUE`=忽略）

**返回值：**
- 找到：返回子串位置指针
- 未找到：返回 `NULL`
- 找到的位置索引（从1开始）存储在 `xCore.iRet`

**示例：**
```c
#include "xrt.h"
#include <stdio.h>

int main() {
    xrtInit();
    
    str text = (str)"Hello World, Hello XRT";
    
    // 区分大小写查找
    str found = xrtFindStr(text, 0, (str)"World", 0, FALSE);
    if (found) {
        printf("找到: %s\n", found);  // "World, Hello XRT"
        printf("位置: %" PRId64 "\n", xCore.iRet);  // 7
    }
    
    // 忽略大小写查找
    found = xrtFindStr(text, 0, (str)"world", 0, TRUE);
    if (found) {
        printf("忽略大小写找到: %s\n", found);
    }
    
    // 未找到
    found = xrtFindStr(text, 0, (str)"xyz", 0, FALSE);
    if (!found) {
        printf("未找到 xyz\n");
    }
    
    xrtUnit();
    return 0;
}
```

---

### xrtInStr

查找子串位置（返回索引）。

**函数原型：**
```c
XXAPI uint xrtInStr(str sText, size_t iSize, str sSubText, size_t iSubSize, bool bCase);
```

**返回值：**
- 找到：返回位置索引（从1开始）
- 未找到：返回 0

**示例：**
```c
#include "xrt.h"
#include <stdio.h>

int main() {
    xrtInit();
    
    str text = (str)"Hello World";
    uint pos = xrtInStr(text, 0, (str)"World", 0, FALSE);
    printf("位置: %u\n", pos);  // 7
    
    pos = xrtInStr(text, 0, (str)"xyz", 0, FALSE);
    printf("未找到: %u\n", pos);  // 0
    
    xrtUnit();
    return 0;
}
```

**补充说明：**
- 内部调用 `xrtFindStr`，然后返回 `xCore.iRet`

---

### xrtCheckStr

检查字符串是否包含指定字符。

**函数原型：**
```c
XXAPI str xrtCheckStr(str sText, size_t iSize, str sSubText, size_t iSubSize);
```

**参数：**
- `sText` - 被检查的字符串
- `iSize` - 长度（0自动）
- `sSubText` - 字符集合
- `iSubSize` - 字符集长度（0自动）

**返回值：**
- 找到：返回第一个匹配字符的位置
- 未找到：返回 `NULL`

**说明：**
- 检查 `sText` 中是否包含 `sSubText` 中的**任意字符**
- 支持 UTF-8 多字节字符（最高 6 字节）

**示例：**
```c
#include "xrt.h"
#include <stdio.h>

int main() {
    xrtInit();
    
    str text = (str)"Hello123World";
    
    // 检查是否包含数字
    str found = xrtCheckStr(text, 0, (str)"0123456789", 0);
    if (found) {
        printf("找到数字: %s\n", found);  // "123World"
    }
    
    // 检查是否包含特殊字符
    found = xrtCheckStr(text, 0, (str)"!@#$%", 0);
    if (!found) {
        printf("不包含特殊字符\n");
    }
    
    xrtUnit();
    return 0;
}
```

---

## 通配符匹配

### xrtStrLike

通配符模式匹配。

**函数原型：**
```c
XXAPI bool xrtStrLike(str sText, size_t iTextSize, str sPattern, size_t iPatSize, bool bCase);
```

**参数：**
- `sText` - 待匹配的字符串
- `iTextSize` - 字符串长度（0表示自动计算）
- `sPattern` - 通配符模式
- `iPatSize` - 模式长度（0表示自动计算）
- `bCase` - 是否忽略大小写
  - `TRUE` - **忽略**大小写（case-insensitive）
  - `FALSE` - **区分**大小写（case-sensitive）

**通配符说明：**
- `*` - 匹配任意字符序列（包括空序列）
- `?` - 匹配单个 UTF-8 字符（支持多字节字符）

**返回值：**
- `TRUE` - 匹配成功
- `FALSE` - 匹配失败

**算法特性：**
- 使用贪婪匹配算法
- 时间复杂度：O(n*m) 最坏情况
- 空间复杂度：O(1)

**示例：**
```c
#include "xrt.h"
#include <stdio.h>

int main() {
    xrtInit();
    
    // 基础匹配
    printf("%d\n", xrtStrLike((str)"hello", 0, (str)"hello", 0, FALSE));  // 1
    printf("%d\n", xrtStrLike((str)"hello", 0, (str)"world", 0, FALSE));  // 0
    
    // * 通配符：匹配任意字符序列
    printf("%d\n", xrtStrLike((str)"hello", 0, (str)"*", 0, FALSE));      // 1
    printf("%d\n", xrtStrLike((str)"hello", 0, (str)"h*", 0, FALSE));     // 1
    printf("%d\n", xrtStrLike((str)"hello", 0, (str)"*o", 0, FALSE));     // 1
    printf("%d\n", xrtStrLike((str)"hello", 0, (str)"h*o", 0, FALSE));    // 1
    printf("%d\n", xrtStrLike((str)"hello", 0, (str)"*ll*", 0, FALSE));   // 1
    
    // ? 通配符：匹配单个字符
    printf("%d\n", xrtStrLike((str)"hello", 0, (str)"h?llo", 0, FALSE));  // 1
    printf("%d\n", xrtStrLike((str)"hello", 0, (str)"?????", 0, FALSE));  // 1
    printf("%d\n", xrtStrLike((str)"hello", 0, (str)"??????", 0, FALSE)); // 0
    
    // 混合使用
    printf("%d\n", xrtStrLike((str)"hello world", 0, (str)"h*o ?orld", 0, FALSE));  // 1
    
    // 大小写不敏感
    printf("%d\n", xrtStrLike((str)"HELLO", 0, (str)"hello", 0, FALSE));  // 0
    printf("%d\n", xrtStrLike((str)"HELLO", 0, (str)"hello", 0, TRUE));   // 1
    printf("%d\n", xrtStrLike((str)"HeLLo", 0, (str)"h*O", 0, TRUE));     // 1
    
    // UTF-8 支持（? 匹配完整的 UTF-8 字符）
    printf("%d\n", xrtStrLike((str)"中文测试", 0, (str)"*测试", 0, FALSE));    // 1
    printf("%d\n", xrtStrLike((str)"中文测试", 0, (str)"中?测试", 0, FALSE));  // 1
    printf("%d\n", xrtStrLike((str)"中文测试", 0, (str)"????", 0, FALSE));     // 1
    printf("%d\n", xrtStrLike((str)"中文测试", 0, (str)"?????", 0, FALSE));    // 0
    
    xrtUnit();
    return 0;
}
```

**应用场景：**
```c
#include "xrt.h"
#include <stdio.h>

// 文件名匹配
bool MatchFileName(str filename, str pattern) {
    return xrtStrLike(filename, 0, pattern, 0, TRUE);  // 文件名通常不区分大小写
}

int main() {
    xrtInit();
    
    // 匹配所有 .txt 文件
    printf("%d\n", MatchFileName((str)"readme.txt", (str)"*.txt"));      // 1
    printf("%d\n", MatchFileName((str)"README.TXT", (str)"*.txt"));      // 1
    printf("%d\n", MatchFileName((str)"document.pdf", (str)"*.txt"));    // 0
    
    // 匹配 test 开头的文件
    printf("%d\n", MatchFileName((str)"test_main.c", (str)"test*"));     // 1
    printf("%d\n", MatchFileName((str)"main_test.c", (str)"test*"));     // 0
    
    // 匹配单字符变化
    printf("%d\n", MatchFileName((str)"file1.log", (str)"file?.log"));   // 1
    printf("%d\n", MatchFileName((str)"file10.log", (str)"file?.log"));  // 0
    
    xrtUnit();
    return 0;
}
```

**补充说明：**
- `?` 匹配一个完整的 UTF-8 字符，而非单个字节
- 空模式只能匹配空字符串
- 空字符串只能被全是 `*` 的模式匹配
- 大小写转换仅对 ASCII 字母有效

---

## 字符串裁剪

### xrtLTrim

左侧裁剪。

**函数原型：**
```c
XXAPI str xrtLTrim(str sText, size_t iSize, str sSubText, size_t iSubSize, bool bSrcRevise);
```

**参数：**
- `sText` - 源字符串
- `iSize` - 长度（0自动）
- `sSubText` - 要裁剪的字符集（`NULL` 默认为 ` \t\r\n`）
- `iSubSize` - 字符集长度（0自动）
- `bSrcRevise` - 是否修改源

**返回值：**
- 裁剪后的字符串
- 裁剪后的长度存储在 `xCore.iRet`

**内存释放：**
- `bSrcRevise=FALSE` - ✅ 需要释放

**示例：**
```c
#include "xrt.h"
#include <stdio.h>

int main() {
    xrtInit();
    
    str text = (str)"   Hello World   ";
    
    // 裁剪左侧空格
    str trimmed = xrtLTrim(text, 0, (str)" ", 0, FALSE);
    printf("[%s]\n", trimmed);  // "[Hello World   ]"
    printf("裁剪后长度: %" PRId64 "\n", xCore.iRet);
    xrtFree(trimmed);
    
    // 使用默认字符集 (\u7a7a格\t\r\n)
    str text2 = (str)"\t\n  Hello";
    str trimmed2 = xrtLTrim(text2, 0, NULL, 0, FALSE);
    printf("[%s]\n", trimmed2);  // "[Hello]"
    xrtFree(trimmed2);
    
    xrtUnit();
    return 0;
}
```

---

### xrtRTrim

右侧裁剪。

**函数原型：**
```c
XXAPI str xrtRTrim(str sText, size_t iSize, str sSubText, size_t iSubSize, bool bSrcRevise);
```

**参数：**同 `xrtLTrim`

**示例：**
```c
#include "xrt.h"
#include <stdio.h>

int main() {
    xrtInit();
    
    str text = (str)"   Hello   ";
    str trimmed = xrtRTrim(text, 0, (str)" ", 0, FALSE);
    printf("[%s]\n", trimmed);  // "[   Hello]"
    xrtFree(trimmed);
    
    xrtUnit();
    return 0;
}
```

---

### xrtTrim

两侧裁剪。

**函数原型：**
```c
XXAPI str xrtTrim(str sText, size_t iSize, str sSubText, size_t iSubSize, bool bSrcRevise);
```

**参数：**同 `xrtLTrim`

**示例：**
```c
#include "xrt.h"
#include <stdio.h>

int main() {
    xrtInit();
    
    // 裁剪空白字符
    str text = (str)"   Hello World   ";
    str trimmed = xrtTrim(text, 0, (str)" \t\r\n", 0, FALSE);
    printf("[%s]\n", trimmed);  // "[Hello World]"
    xrtFree(trimmed);
    
    // 使用默认字符集
    str text2 = (str)"\n\t  Content  \t\n";
    str trimmed2 = xrtTrim(text2, 0, NULL, 0, FALSE);
    printf("[%s]\n", trimmed2);  // "[Content]"
    xrtFree(trimmed2);
    
    // 裁剪指定字符
    str text3 = (str)"###Title###";
    str trimmed3 = xrtTrim(text3, 0, (str)"#", 0, FALSE);
    printf("[%s]\n", trimmed3);  // "[Title]"
    xrtFree(trimmed3);
    
    xrtUnit();
    return 0;
}
```

**补充说明：**
- 支持 UTF-8 多字节字符
- `sSubText` 为 `NULL` 时默认裁剪 ` \t\r\n`

---

## 字符串过滤

### xrtFilterStr

过滤字符。

**函数原型：**
```c
XXAPI str xrtFilterStr(str sText, size_t iSize, str sSubText, size_t iSubSize, bool bSrcRevise);
```

**参数：**
- `sText` - 源字符串
- `iSize` - 长度（0自动）
- `sSubText` - 要过滤掉的字符集
- `iSubSize` - 字符集长度（0自动）
- `bSrcRevise` - 是否修改源

**返回值：**
- 过滤后的字符串
- 被过滤掉的字符数存储在 `xCore.iRet`

**内存释放：**
- `bSrcRevise=FALSE` - ✅ 需要释放

**示例：**
```c
#include "xrt.h"
#include <stdio.h>

int main() {
    xrtInit();
    
    str text = (str)"Hello123World456";
    
    // 过滤数字
    str filtered = xrtFilterStr(text, 0, (str)"0123456789", 0, FALSE);
    printf("%s\n", filtered);  // "HelloWorld"
    printf("过滤掉 %" PRId64 " 个字符\n", xCore.iRet);  // 6
    xrtFree(filtered);
    
    // 过滤空格和换行
    str text2 = (str)"Hello World\nNew Line";
    str filtered2 = xrtFilterStr(text2, 0, (str)" \n", 0, FALSE);
    printf("%s\n", filtered2);  // "HelloWorldNewLine"
    xrtFree(filtered2);
    
    xrtUnit();
    return 0;
}
```

**补充说明：**
- 支持 UTF-8 多字节字符
- `sSubText` 为 `NULL` 时直接返回原字符串副本

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
#include "xrt.h"
#include <stdio.h>

int main() {
    xrtInit();
    
    // 基本格式化
    str text = xrtFormat((str)"Hello %s, you are %d years old", "Tom", 25);
    printf("%s\n", text);  // "Hello Tom, you are 25 years old"
    xrtFree(text);
    
    // 构建路径
    str path = xrtFormat((str)"%s\\%s", "C:\\folder", "file.txt");
    printf("%s\n", path);  // "C:\folder\file.txt"
    xrtFree(path);
    
    // 格式化数字
    str num = xrtFormat((str)"Value: %.2f, Count: %06d", 3.14159, 42);
    printf("%s\n", num);  // "Value: 3.14, Count: 000042"
    xrtFree(num);
    
    xrtUnit();
    return 0;
}
```

**补充说明：**
- 内部使用 `vsnprintf` 实现，支持所有标准格式说明符
- 返回结果长度存储在 `xCore.iRet`

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
#include "xrt.h"
#include <stdio.h>

int main() {
    xrtInit();
    
    // 基本替换
    str replaced = xrtReplace((str)"Hello World", 0, (str)"World", 0, (str)"XRT", 0);
    printf("%s\n", replaced);  // "Hello XRT"
    xrtFree(replaced);
    
    // 全部替换（所有匹配项）
    str result = xrtReplace((str)"a b a b a", 0, (str)"a", 0, (str)"c", 0);
    printf("%s\n", result);  // "c b c b c"
    xrtFree(result);
    
    // 删除子串（替换为空）
    str deleted = xrtReplace((str)"Hello World", 0, (str)"World", 0, (str)"", 0);
    printf("[%s]\n", deleted);  // "[Hello ]"
    xrtFree(deleted);
    
    // 替换长度不等的子串
    str longer = xrtReplace((str)"AB", 0, (str)"B", 0, (str)"XYZ", 0);
    printf("%s (长度: %" PRId64 ")\n", longer, xCore.iRet);  // "AXYZ (长度: 4)"
    xrtFree(longer);
    
    xrtUnit();
    return 0;
}
```

**补充说明：**
- 返回的字符串长度存储在 `xCore.iRet`
- 会替换**所有**匹配的子串，不只是第一个

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
#include "xrt.h"
#include <stdio.h>

int main() {
    xrtInit();
    
    // 分割并修改源字符串 (bSrcRevise=TRUE)
    str text1 = xrtCopyStr((str)"apple,banana,orange", 0);
    str* parts1 = xrtSplit(text1, 0, (str)",", 0, TRUE);
    printf("分割数: %" PRId64 "\n", xCore.iRet);  // 3
    for (int i = 0; parts1[i] != NULL; i++) {
        printf("%d: %s\n", i, parts1[i]);
    }
    xrtFree(parts1);  // 释放数组
    xrtFree(text1);   // 释放源字符串
    
    printf("---\n");
    
    // 分割不修改源字符串 (bSrcRevise=FALSE)
    str text2 = (str)"one|two|three";
    str* parts2 = xrtSplit(text2, 0, (str)"|", 0, FALSE);
    for (int i = 0; parts2[i] != NULL; i++) {
        printf("%d: %s\n", i, parts2[i]);
    }
    xrtFree(parts2);  // 只需释放数组，字符串在数组内存中
    
    printf("---\n");
    
    // 多字符分隔符
    str csv = xrtCopyStr((str)"a::b::c", 0);
    str* parts3 = xrtSplit(csv, 0, (str)"::", 0, TRUE);
    for (int i = 0; parts3[i] != NULL; i++) {
        printf("%d: [%s]\n", i, parts3[i]);
    }
    xrtFree(parts3);
    xrtFree(csv);
    
    xrtUnit();
    return 0;
}
```

**补充说明：**
- 分割数存储在 `xCore.iRet`
- 返回的数组以 `NULL` 结尾
- `bSrcRevise=TRUE` 时会在源字符串中插入 `\0`，破坏原始数据
- `bSrcRevise=FALSE` 时字符串数据和数组在同一块内存中，只需释放一次

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
#include "xrt.h"
#include <stdio.h>

int main() {
    xrtInit();
    
    // 生成8位数字密码
    str pin = xrtRandStr((str)"0123456789", 0, 8);
    printf("PIN: %s\n", pin);  // 例如: "57382910"
    xrtFree(pin);
    
    // 生成16位混合密码
    str password = xrtRandStr(
        (str)"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789",
        0, 16
    );
    printf("Password: %s\n", password);
    xrtFree(password);
    
    // 使用默认模板（NULL）
    str token = xrtRandStr(NULL, 0, 32);
    printf("Token: %s\n", token);  // 包含数字+字母+-_
    xrtFree(token);
    
    // 生成十六进制字符串
    str hex = xrtRandStr((str)"0123456789ABCDEF", 0, 8);
    printf("Hex: %s\n", hex);  // 例如: "3F7A9C2E"
    xrtFree(hex);
    
    xrtUnit();
    return 0;
}
```

**补充说明：**
- `sTemplate` 为 `NULL` 时使用默认模板（64字符：数字+大小写字母+-_）
- 内部使用 `xrtRandRange` 生成随机索引

---

## 数值格式化

### xrtIntFormat

整数格式化。

**函数原型：**
```c
XXAPI str xrtIntFormat(int64 value, str format);
```

**参数：**
- `value` - 要格式化的 64 位整数
- `format` - 格式字符串（`NULL` 表示默认格式）

**格式符：**
| 格式符 | 说明 | 示例 |
|--------|------|------|
| `,` | 千分位分隔 | `1,234,567` |
| `0N` | 前导零填充至 N 位 | `00000042` |
| `+` | 始终显示正号 | `+123` |
| `x` | 十六进制（小写） | `ff` |
| `X` | 十六进制（大写） | `FF` |
| `o` | 八进制 | `100` |
| `b` | 二进制 | `1010` |

**返回值：**
- 格式化后的字符串
- 失败返回 `xCore.sNull`

**内存释放：** ✅ 需要 `xrtFree` 释放

**示例：**
```c
#include "xrt.h"
#include <stdio.h>

int main() {
    xrtInit();
    
    // 基础转换
    str s1 = xrtIntFormat(1234567, NULL);
    printf("%s\n", s1);  // "1234567"
    xrtFree(s1);
    
    // 千分位
    str s2 = xrtIntFormat(1234567, (str)",");
    printf("%s\n", s2);  // "1,234,567"
    xrtFree(s2);
    
    // 前导零
    str s3 = xrtIntFormat(42, (str)"08");
    printf("%s\n", s3);  // "00000042"
    xrtFree(s3);
    
    // 正号
    str s4 = xrtIntFormat(123, (str)"+");
    printf("%s\n", s4);  // "+123"
    xrtFree(s4);
    
    // 十六进制
    str s5 = xrtIntFormat(255, (str)"X");
    printf("%s\n", s5);  // "FF"
    xrtFree(s5);
    
    // 十六进制 + 前导零
    str s6 = xrtIntFormat(255, (str)"08X");
    printf("%s\n", s6);  // "000000FF"
    xrtFree(s6);
    
    // 八进制
    str s7 = xrtIntFormat(64, (str)"o");
    printf("%s\n", s7);  // "100"
    xrtFree(s7);
    
    // 二进制
    str s8 = xrtIntFormat(10, (str)"b");
    printf("%s\n", s8);  // "1010"
    xrtFree(s8);
    
    // 负数 + 千分位
    str s9 = xrtIntFormat(-1234567, (str)",");
    printf("%s\n", s9);  // "-1,234,567"
    xrtFree(s9);
    
    xrtUnit();
    return 0;
}
```

**补充说明：**
- 非十进制不支持千分位和符号显示
- 格式符可以组合使用，如 `+08` 表示正号 + 8位前导零

---

### xrtNumFormat

浮点数格式化。

**函数原型：**
```c
XXAPI str xrtNumFormat(double value, str format);
```

**参数：**
- `value` - 要格式化的双精度浮点数
- `format` - 格式字符串（`NULL` 表示默认 6 位小数）

**格式符：**
| 格式符 | 说明 | 示例 |
|--------|------|------|
| `,` | 千分位分隔（整数部分） | `1,234,567.89` |
| `.N` | N 位小数 | `3.14` |
| `+` | 始终显示正号 | `+3.14` |
| `%` | 百分比（自动×100） | `12.34%` |

**返回值：**
- 格式化后的字符串
- 失败返回 `xCore.sNull`

**内存释放：** ✅ 需要 `xrtFree` 释放

**示例：**
```c
#include "xrt.h"
#include <stdio.h>

int main() {
    xrtInit();
    
    // 默认格式（6位小数）
    str s1 = xrtNumFormat(3.14159, NULL);
    printf("%s\n", s1);  // "3.141590"
    xrtFree(s1);
    
    // 指定小数位数
    str s2 = xrtNumFormat(3.14159, (str)".2");
    printf("%s\n", s2);  // "3.14"
    xrtFree(s2);
    
    // 补零
    str s3 = xrtNumFormat(3.1, (str)".3");
    printf("%s\n", s3);  // "3.100"
    xrtFree(s3);
    
    // 千分位 + 小数
    str s4 = xrtNumFormat(1234567.89, (str)",.2");
    printf("%s\n", s4);  // "1,234,567.89"
    xrtFree(s4);
    
    // 百分比
    str s5 = xrtNumFormat(0.1234, (str)".2%");
    printf("%s\n", s5);  // "12.34%"
    xrtFree(s5);
    
    // 百分比（不带小数）
    str s6 = xrtNumFormat(0.75, (str)".0%");
    printf("%s\n", s6);  // "75%"
    xrtFree(s6);
    
    // 正号
    str s7 = xrtNumFormat(3.14, (str)"+.2");
    printf("%s\n", s7);  // "+3.14"
    xrtFree(s7);
    
    // 负数
    str s8 = xrtNumFormat(-1234.56, (str)",.2");
    printf("%s\n", s8);  // "-1,234.56"
    xrtFree(s8);
    
    xrtUnit();
    return 0;
}
```

**补充说明：**
- 小数位数最大支持 15 位
- 特殊值处理：`NaN`、`Inf`、`-Inf`
- 格式符可以组合使用，如 `,.2%` 表示千分位 + 2位小数 + 百分比

---

## 字符串相似度

### xrtStrSim

计算两个字符串的相似度（基于 Levenshtein 编辑距离）。

**函数原型：**
```c
XXAPI double xrtStrSim(str s1, size_t len1, str s2, size_t len2);
```

**参数：**
- `s1` - 第一个字符串
- `len1` - 第一个字符串长度（0表示自动计算）
- `s2` - 第二个字符串
- `len2` - 第二个字符串长度（0表示自动计算）

**返回值：**
- `0.0` - `1.0` 之间的相似度
  - `1.0` - 完全相同
  - `0.0` - 完全不同

**算法特性：**
- 基于 Levenshtein 编辑距离算法
- 时间复杂度：O(m*n)
- 空间复杂度：O(min(m,n))（优化的一维 DP 数组）
- 相似度计算：`1 - (editDistance / maxLength)`

**示例：**
```c
#include "xrt.h"
#include <stdio.h>

int main() {
    xrtInit();
    
    // 完全相同
    printf("%.4f\n", xrtStrSim("hello", 0, "hello", 0));  // 1.0000
    
    // 差一个字符
    printf("%.4f\n", xrtStrSim("hello", 0, "helo", 0));   // 0.8000
    
    // 完全不同
    printf("%.4f\n", xrtStrSim("abc", 0, "xyz", 0));      // 0.0000
    
    // 经典测试用例：kitten -> sitting
    printf("%.4f\n", xrtStrSim("kitten", 0, "sitting", 0)); // 0.5714
    
    // 空字符串
    printf("%.4f\n", xrtStrSim("", 0, "", 0));            // 1.0000
    printf("%.4f\n", xrtStrSim("abc", 0, "", 0));         // 0.0000
    
    xrtUnit();
    return 0;
}
```

**应用场景：**
```c
#include "xrt.h"
#include <stdio.h>

// 模糊搜索：查找相似的单词
bool FuzzyMatch(str input, str target, double threshold) {
    return xrtStrSim(input, 0, target, 0) >= threshold;
}

int main() {
    xrtInit();
    
    str words[] = {"apple", "application", "apply", "banana", "orange"};
    str search = "appla";  // 用户输入（可能有拼写错误）
    
    printf("搜索: %s\n", search);
    for (int i = 0; i < 5; i++) {
        double sim = xrtStrSim(search, 0, words[i], 0);
        if (sim >= 0.6) {
            printf("  匹配: %s (相似度: %.2f)\n", words[i], sim);
        }
    }
    
    xrtUnit();
    return 0;
}
```

**补充说明：**
- `NULL` 指针视为空字符串
- 不适合超长字符串（>10KB），建议使用其他算法如 Dice/SimHash

---

## 字符串约等于

### 配置常量

字符串约等于模式常量。

```c
#define XRT_STR_APPROX_LIKE     0   // 通配符模式（s2为模式串）
#define XRT_STR_APPROX_SIM      1   // 相似度阈值模式
```

### xCore 配置项

字符串约等于通过 `xCore` 全局变量配置。

| 配置项 | 类型 | 说明 | 默认值 |
|--------|------|------|--------|
| `xCore.iApproxStrMode` | int | 模式选择 | `XRT_STR_APPROX_SIM` (1) |
| `xCore.fApproxStrTol` | double | 相似度阈值 | `0.95` (95%) |
| `xCore.bApproxStrCase` | bool | 通配符大小写开关 | `FALSE` (区分) |

### xrtStrApprox

字符串约等于判断（使用 xCore 配置）。

**函数原型：**
```c
XXAPI bool xrtStrApprox(str s1, size_t len1, str s2, size_t len2);
```

**参数：**
- `s1` - 第一个字符串
- `len1` - 第一个字符串长度（0表示自动计算）
- `s2` - 第二个字符串（通配符模式下为模式串）
- `len2` - 第二个字符串长度（0表示自动计算）

**返回值：**
- `TRUE` - 约等于
- `FALSE` - 不等

**模式说明：**
- **通配符模式** (`XRT_STR_APPROX_LIKE`): 使用 `xrtStrLike` 匹配，`s2` 为模式串
- **相似度模式** (`XRT_STR_APPROX_SIM`): 使用 `xrtStrSim` 计算，结果 >= 阈值则约等

**示例：**
```c
#include "xrt.h"
#include <stdio.h>

int main() {
    xrtInit();
    
    // === 相似度模式（默认） ===
    // 默认 95% 阈值
    printf("%d\n", xrtStrApprox("hello", 0, "hello", 0));  // 1 (完全相同)
    printf("%d\n", xrtStrApprox("hello", 0, "helo", 0));   // 0 (80% < 95%)
    
    // 调整阈值到 80%
    xCore.fApproxStrTol = 0.80;
    printf("%d\n", xrtStrApprox("hello", 0, "helo", 0));   // 1 (80% >= 80%)
    
    // === 通配符模式 ===
    xCore.iApproxStrMode = XRT_STR_APPROX_LIKE;
    printf("%d\n", xrtStrApprox("hello", 0, "h*o", 0));    // 1
    printf("%d\n", xrtStrApprox("hello", 0, "h???o", 0));  // 1
    printf("%d\n", xrtStrApprox("hello", 0, "world", 0));  // 0
    
    // 忽略大小写
    xCore.bApproxStrCase = TRUE;
    printf("%d\n", xrtStrApprox("HELLO", 0, "h*o", 0));    // 1
    
    // 恢复默认配置
    xCore.iApproxStrMode = XRT_STR_APPROX_SIM;
    xCore.fApproxStrTol = 0.95;
    xCore.bApproxStrCase = FALSE;
    
    xrtUnit();
    return 0;
}
```

**应用场景：**
```c
#include "xrt.h"
#include <stdio.h>

// 设置为文件名模糊匹配模式
void SetupFileMatch() {
    xCore.iApproxStrMode = XRT_STR_APPROX_LIKE;
    xCore.bApproxStrCase = TRUE;  // 文件名通常不区分大小写
}

// 设置为文本相似度检测模式
void SetupTextSimilarity(double threshold) {
    xCore.iApproxStrMode = XRT_STR_APPROX_SIM;
    xCore.fApproxStrTol = threshold;
}

int main() {
    xrtInit();
    
    // 文件名匹配
    SetupFileMatch();
    printf("匹配 *.txt: %d\n", xrtStrApprox("readme.txt", 0, "*.txt", 0));
    printf("匹配 *.txt: %d\n", xrtStrApprox("README.TXT", 0, "*.txt", 0));
    
    // 拼写检查（90% 相似度）
    SetupTextSimilarity(0.90);
    printf("拼写检查: %d\n", xrtStrApprox("receive", 0, "recieve", 0));
    
    xrtUnit();
    return 0;
}
```

**补充说明：**
- 与 `xrtIntApprox`、`xrtNumApprox`、`xrtTimeApprox` 风格一致
- 配置在 `xrtInit()` 时初始化为默认值
- 修改配置后影响后续所有调用

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
#include "xrt.h"
#include <stdio.h>

int main() {
    xrtInit();
    
    uint8 data[] = {0x12, 0x34, 0x56, 0xAB, 0xCD};
    str hex = xrtHexEncode(data, sizeof(data));
    
    printf("%s\n", hex);  // "123456ABCD"
    printf("编码后长度: %" PRId64 "\n", xCore.iRet);  // 10
    
    xrtFree(hex);
    xrtUnit();
    return 0;
}
```

**补充说明：**
- 输出为大写十六进制字符
- 编码后长度存储在 `xCore.iRet`，等于输入长度 × 2

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
#include "xrt.h"
#include <stdio.h>

int main() {
    xrtInit();
    
    // 解码 "Hello" 的 HEX
    str hex = (str)"48656C6C6F";
    ptr data = xrtHexDecode(hex, 0);
    size_t len = xCore.iRet;
    
    printf("解码后: %.*s\n", (int)len, (char*)data);  // "Hello"
    printf("解码长度: %zu\n", len);  // 5
    
    xrtFree(data);
    
    // 小写十六进制也支持
    str hex2 = (str)"576f726c64";  // "World"
    ptr data2 = xrtHexDecode(hex2, 0);
    printf("解码: %.*s\n", (int)xCore.iRet, (char*)data2);
    xrtFree(data2);
    
    xrtUnit();
    return 0;
}
```

**补充说明：**
- 支持大写和小写十六进制字符
- 解码后长度存储在 `xCore.iRet`，等于输入长度 / 2

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
#include "xrt.h"
#include <stdio.h>
#include <string.h>

int main() {
    xrtInit();
    
    // 基本编码
    str text = (str)"Hello World";
    str b64 = xrtBase64Encode(text, strlen((char*)text), NULL);
    printf("编码: %s\n", b64);  // "SGVsbG8gV29ybGQ="
    printf("编码后长度: %" PRId64 "\n", xCore.iRet);  // 16
    xrtFree(b64);
    
    // 编码二进制数据
    uint8 binary[] = {0x00, 0x01, 0x02, 0xFF};
    str b64_bin = xrtBase64Encode(binary, sizeof(binary), NULL);
    printf("二进制编码: %s\n", b64_bin);  // "AAEC/w=="
    xrtFree(b64_bin);
    
    xrtUnit();
    return 0;
}
```

**补充说明：**
- 编码后长度存储在 `xCore.iRet`
- 自动添加填充字符 `=`

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
#include "xrt.h"
#include <stdio.h>

int main() {
    xrtInit();
    
    // 基本解码
    str b64 = (str)"SGVsbG8gV29ybGQ=";
    ptr data = xrtBase64Decode(b64, 0, NULL);
    size_t len = xCore.iRet;
    
    printf("解码: %.*s\n", (int)len, (char*)data);  // "Hello World"
    printf("解码长度: %zu\n", len);  // 11
    xrtFree(data);
    
    // 解码二进制数据
    str b64_bin = (str)"AAEC/w==";
    ptr binary = xrtBase64Decode(b64_bin, 0, NULL);
    uint8* bytes = (uint8*)binary;
    printf("二进制: ");
    for (size_t i = 0; i < (size_t)xCore.iRet; i++) {
        printf("%02X ", bytes[i]);
    }
    printf("\n");  // "00 01 02 FF"
    xrtFree(binary);
    
    xrtUnit();
    return 0;
}
```

**补充说明：**
- 输入长度必须是 4 的倍数，否则返回错误
- 包含非法字符时返回错误并设置 `xCore.LastError`
- 解码后长度存储在 `xCore.iRet`

---

## UTF-8 工具函数

### xrtCharLenU8

获取 UTF-8 字符的字节数。

**函数原型：**
```c
static inline int xrtCharLenU8(unsigned char c);
```

**参数：**
- `c` - UTF-8 字符的首字节

**返回值：**
- 该 UTF-8 字符占用的字节数（1-6）

**UTF-8 编码规则：**
| 首字节模式 | 字节数 | Unicode 范围 |
|--------------|--------|---------------|
| `0xxxxxxx` | 1 | U+0000 - U+007F (ASCII) |
| `110xxxxx` | 2 | U+0080 - U+07FF |
| `1110xxxx` | 3 | U+0800 - U+FFFF |
| `11110xxx` | 4 | U+10000 - U+1FFFFF |
| `111110xx` | 5 | U+200000 - U+3FFFFFF |
| `1111110x` | 6 | U+4000000 - U+7FFFFFFF |

**特性：**
- 内联函数，零开销
- 异常字节返回 1

**示例：**
```c
#include "xrt.h"
#include <stdio.h>

int main() {
    xrtInit();
    
    // ASCII 字符（1 字节）
    printf("%d\n", xrtCharLenU8('A'));         // 1
    printf("%d\n", xrtCharLenU8('0'));         // 1
    printf("%d\n", xrtCharLenU8(' '));         // 1
    
    // 中文字符（3 字节）
    str chinese = (str)"中文";
    printf("%d\n", xrtCharLenU8(chinese[0]));  // 3
    
    // 表情符号（4 字节）
    str emoji = (str)"😀";
    printf("%d\n", xrtCharLenU8(emoji[0]));    // 4
    
    // 遍历 UTF-8 字符串
    str text = (str)"Hello中文";
    size_t i = 0;
    int charCount = 0;
    while (text[i]) {
        int len = xrtCharLenU8(text[i]);
        printf("字符 %d: 字节数=%d\n", charCount, len);
        i += len;
        charCount++;
    }
    // 输出: 字符 0-6 分别为 1,1,1,1,1,3,3 字节
    
    xrtUnit();
    return 0;
}
```

**应用场景：**
```c
#include "xrt.h"
#include <stdio.h>

// 计算 UTF-8 字符串的字符数（非字节数）
size_t Utf8CharCount(str text) {
    if (!text) return 0;
    size_t count = 0;
    size_t i = 0;
    while (text[i]) {
        i += xrtCharLenU8(text[i]);
        count++;
    }
    return count;
}

// 截取 UTF-8 字符串的前 N 个字符
str Utf8Substring(str text, size_t n) {
    if (!text || n == 0) return xCore.sNull;
    size_t i = 0;
    size_t count = 0;
    while (text[i] && count < n) {
        i += xrtCharLenU8(text[i]);
        count++;
    }
    return xrtCopyStr(text, i);
}

int main() {
    xrtInit();
    
    str text = (str)"Hello中文World";
    printf("字符数: %zu\n", Utf8CharCount(text));  // 12
    printf("字节数: %zu\n", strlen((char*)text));   // 16
    
    str sub = Utf8Substring(text, 7);
    printf("前7个字符: %s\n", sub);  // "Hello中文"
    xrtFree(sub);
    
    xrtUnit();
    return 0;
}
```

**补充说明：**
- 这是一个 `static inline` 函数，可在多处调用而无函数调用开销
- 用于正确遍历 UTF-8 字符串
- `xrtStrLike` 内部使用此函数处理 `?` 通配符

---

## 使用场景

### 1. 文本处理

```c
#include "xrt.h"
#include <stdio.h>

// 清理用户输入
str CleanInput(str input) {
    // 裁剪空白
    str trimmed = xrtTrim(input, 0, (str)" \t\r\n", 0, FALSE);
    // 转小写
    str lower = xrtLCase(trimmed, 0, TRUE);
    // trimmed 已被原地修改，直接返回
    return lower;
}

int main() {
    xrtInit();
    
    str input = (str)"   Hello WORLD   ";
    str clean = CleanInput(input);
    printf("清理后: [%s]\n", clean);  // "[hello world]"
    xrtFree(clean);
    
    xrtUnit();
    return 0;
}
```

---

### 2. 配置解析

```c
#include "xrt.h"
#include <stdio.h>

// 解析 key=value
void ParseConfig(str line) {
    str* parts = xrtSplit(line, 0, (str)"=", 0, FALSE);
    if (parts[0] && parts[1]) {
        str key = xrtTrim(parts[0], 0, (str)" ", 0, FALSE);
        str value = xrtTrim(parts[1], 0, (str)" ", 0, FALSE);
        printf("%s = %s\n", key, value);
        xrtFree(key);
        xrtFree(value);
    }
    xrtFree(parts);
}

int main() {
    xrtInit();
    
    ParseConfig((str)"  name = XRT Library  ");
    ParseConfig((str)"version=1.0.0");
    ParseConfig((str)"  path = C:\\Program Files  ");
    
    xrtUnit();
    return 0;
}
```

---

### 3. URL 编码

```c
#include "xrt.h"
#include <stdio.h>

str UrlEncode(str text) {
    // 简化版：替换常见特殊字符
    str step1 = xrtReplace(text, 0, (str)" ", 0, (str)"%20", 0);
    str step2 = xrtReplace(step1, 0, (str)"&", 0, (str)"%26", 0);
    str step3 = xrtReplace(step2, 0, (str)"=", 0, (str)"%3D", 0);
    xrtFree(step1);
    xrtFree(step2);
    return step3;
}

int main() {
    xrtInit();
    
    str url = (str)"name=Hello World&value=1+2=3";
    str encoded = UrlEncode(url);
    printf("编码后: %s\n", encoded);
    xrtFree(encoded);
    
    xrtUnit();
    return 0;
}
```

---

## 最佳实践

### 1. 内存管理

```c
#include "xrt.h"
#include <stdio.h>

void UseString(str s) {
    printf("使用: %s\n", s);
}

int main() {
    xrtInit();
    
    // ✓ 正确：及时释放
    str lower = xrtLCase((str)"TEXT", 0, FALSE);
    UseString(lower);
    xrtFree(lower);
    
    // ✗ 内存泄漏示例（不要这样做）
    // str leaked = xrtLCase((str)"TEXT", 0, FALSE);
    // 忘记释放...
    
    xrtUnit();
    return 0;
}
```

---

### 2. 原地修改

```c
#include "xrt.h"
#include <stdio.h>
#include <string.h>

int main() {
    xrtInit();
    
    // 可以修改源数据时，使用原地修改更高效
    str text = xrtMalloc(100);
    strcpy((char*)text, "HELLO WORLD");
    
    printf("修改前: %s\n", text);
    xrtLCase(text, 0, TRUE);  // 原地修改
    printf("修改后: %s\n", text);  // "hello world"
    
    xrtFree(text);
    xrtUnit();
    return 0;
}
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
