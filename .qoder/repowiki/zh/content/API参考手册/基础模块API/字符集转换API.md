# 字符集转换API

<cite>
**本文档引用的文件**
- [lib/charset.h](file://lib/charset.h)
- [docs/api-charset.md](file://docs/api-charset.md)
- [test/test_charset.h](file://test/test_charset.h)
- [lib/base.h](file://lib/base.h)
- [lib/string.h](file://lib/string.h)
</cite>

## 目录
1. [简介](#简介)
2. [项目结构](#项目结构)
3. [核心组件](#核心组件)
4. [架构概览](#架构概览)
5. [详细组件分析](#详细组件分析)
6. [依赖关系分析](#依赖关系分析)
7. [性能考虑](#性能考虑)
8. [故障排除指南](#故障排除指南)
9. [结论](#结论)
10. [附录](#附录)

## 简介

字符集转换API是xrt库中专门处理UTF-8、UTF-16、UTF-32之间相互转换的核心模块。该API提供了完整的字符编码转换功能，包括单向转换、双向转换、编码检测、字节序转换以及通用编码转换接口。该模块支持跨平台使用，在Windows平台上还集成了Win32 API进行更广泛的字符集支持。

## 项目结构

字符集转换功能主要分布在以下文件中：

```mermaid
graph TB
subgraph "核心实现"
CH[lib/charset.h<br/>字符集转换核心实现]
end
subgraph "文档说明"
DOC[docs/api-charset.md<br/>API文档]
end
subgraph "测试验证"
TEST[test/test_charset.h<br/>功能测试]
end
subgraph "基础支持"
BASE[lib/base.h<br/>内存管理基础]
STR[lib/string.h<br/>字符串操作]
end
CH --> BASE
CH --> STR
DOC --> CH
TEST --> CH
```

**图表来源**
- [lib/charset.h](file://lib/charset.h#L1-L908)
- [docs/api-charset.md](file://docs/api-charset.md#L1-L1122)
- [test/test_charset.h](file://test/test_charset.h#L1-L101)

**章节来源**
- [lib/charset.h](file://lib/charset.h#L1-L908)
- [docs/api-charset.md](file://docs/api-charset.md#L1-L1122)

## 核心组件

字符集转换API包含以下核心组件：

### 1. 基础数据类型定义

```c
// 字符集代码页常量
#define XRT_CP_AUTO         -2      // 自动识别字符集
#define XRT_CP_BINARY       -1      // 二进制文件
#define XRT_CP_OEM          0       // 本机OEM字符集
#define XRT_CP_UTF8         65001   // UTF-8
#define XRT_CP_UTF16        1200    // UTF-16 (小端序)
#define XRT_CP_UTF16_BE     1201    // UTF-16 (大端序)
#define XRT_CP_UTF32        65005   // UTF-32 (小端序)
#define XRT_CP_UTF32_BE     65006   // UTF-32 (大端序)

// BOM标记
#define XRT_CP_BOM          0x40000000  // 带BOM标记
#define XRT_MASK_BOM        0xBFFFFFFF  // BOM掩码
```

### 2. 转换函数族

API提供六组基础转换函数：
- UTF-8 ↔ UTF-16
- UTF-8 ↔ UTF-32  
- UTF-16 ↔ UTF-32
- 字节序转换（UTF-16/UTF-32）
- 通用编码转换（支持所有内置编码）

**章节来源**
- [lib/charset.h](file://lib/charset.h#L18-L444)
- [docs/api-charset.md](file://docs/api-charset.md#L20-L50)

## 架构概览

字符集转换API采用分层架构设计：

```mermaid
graph TB
subgraph "应用层"
APP[应用程序]
end
subgraph "API接口层"
API[转换API]
DETECT[编码检测]
UTIL[工具函数]
end
subgraph "核心实现层"
UTF8[UTF-8转换器]
UTF16[UTF-16转换器]
UTF32[UTF-32转换器]
BOM[BOM处理]
end
subgraph "平台适配层"
WIN[Windows API]
POSIX[POSIX实现]
end
subgraph "内存管理层"
MEM[内存分配]
ERR[错误处理]
end
APP --> API
API --> DETECT
API --> UTIL
DETECT --> UTF8
DETECT --> UTF16
DETECT --> UTF32
UTIL --> UTF8
UTIL --> UTF16
UTIL --> UTF32
UTF8 --> WIN
UTF16 --> WIN
UTF32 --> WIN
UTF8 --> POSIX
UTF16 --> POSIX
UTF32 --> POSIX
API --> MEM
DETECT --> ERR
```

**图表来源**
- [lib/charset.h](file://lib/charset.h#L488-L710)
- [lib/base.h](file://lib/base.h#L5-L132)

## 详细组件分析

### UTF-8转换器

UTF-8转换器负责UTF-8与其他编码之间的转换，支持完整的Unicode范围处理。

#### UTF-8到UTF-16转换

```mermaid
flowchart TD
START([开始转换]) --> CHECK_NULL{检查输入是否为空}
CHECK_NULL --> |是| RETURN_NULL[返回NULL]
CHECK_NULL --> |否| CALC_SIZE[计算目标长度]
CALC_SIZE --> ALLOC_MEM[分配内存]
ALLOC_MEM --> |失败| RETURN_NULL
ALLOC_MEM --> |成功| CONVERT_LOOP[遍历UTF-8字符]
CONVERT_LOOP --> CHECK_BYTE{检查字节类型}
CHECK_BYTE --> |ASCII| ASCII_CONV[ASCII字符转换]
CHECK_BYTE --> |2字节| UTF16_CONV[UTF-16字符转换]
CHECK_BYTE --> |3字节| UTF16_CONV
CHECK_BYTE --> |4字节| SURROGATE_CHECK{检查代理区}
SURROGATE_CHECK --> |超出范围| REPLACE_CHAR[替换为FFFD]
SURROGATE_CHECK --> |在范围内| UTF16_PAIR[生成代理对]
CHECK_BYTE --> |5-6字节| REPLACE_CHAR
ASCII_CONV --> NEXT_CHAR[下一个字符]
UTF16_CONV --> NEXT_CHAR
UTF16_PAIR --> NEXT_CHAR
REPLACE_CHAR --> NEXT_CHAR
NEXT_CHAR --> CONVERT_LOOP
CONVERT_LOOP --> |完成| TERMINATE[终止字符串]
TERMINATE --> RETURN_RESULT[返回结果]
```

**图表来源**
- [lib/charset.h](file://lib/charset.h#L18-L103)

#### UTF-8到UTF-32转换

UTF-8到UTF-32转换相对简单，因为UTF-32可以表示所有Unicode字符：

```mermaid
sequenceDiagram
participant Caller as 调用者
participant UTF8to32 as UTF8to32函数
participant Memory as 内存管理
participant Converter as 转换器
Caller->>UTF8to32 : 调用转换函数
UTF8to32->>UTF8to32 : 检查输入参数
UTF8to32->>Memory : 分配目标内存
Memory-->>UTF8to32 : 返回内存指针
UTF8to32->>Converter : 遍历UTF-8字符
loop 每个UTF-8字符
Converter->>Converter : 解析字符编码
Converter->>Converter : 转换为UTF-32码点
end
Converter->>UTF8to32 : 写入目标缓冲区
UTF8to32->>UTF8to32 : 终止字符串
UTF8to32-->>Caller : 返回UTF-32字符串
```

**图表来源**
- [lib/charset.h](file://lib/charset.h#L107-L156)

**章节来源**
- [lib/charset.h](file://lib/charset.h#L18-L156)

### UTF-16转换器

UTF-16转换器专门处理UTF-16编码的转换，包括代理对的正确处理。

#### UTF-16代理对处理

UTF-16使用代理对来表示超出基本平面的字符：

```mermaid
flowchart TD
START([开始处理]) --> CHECK_SURROGATE{检查是否为代理区}
CHECK_SURROGATE --> |是| CHECK_PAIR{检查代理对完整性}
CHECK_SURROGATE --> |否| DIRECT_MAP[直接映射到UTF-32]
CHECK_PAIR --> |完整| CREATE_CODEPOINT[计算码点值]
CHECK_PAIR --> |不完整| REPLACE_INVALID[替换为FFFD]
CREATE_CODEPOINT --> MAP_TO_UTF8[映射到UTF-8]
DIRECT_MAP --> MAP_TO_UTF8
REPLACE_INVALID --> MAP_TO_UTF8
MAP_TO_UTF8 --> END([结束])
```

**图表来源**
- [lib/charset.h](file://lib/charset.h#L248-L296)

**章节来源**
- [lib/charset.h](file://lib/charset.h#L160-L296)

### UTF-32转换器

UTF-32转换器提供最直接的Unicode码点处理：

#### UTF-32到UTF-16转换

UTF-32到UTF-16转换需要处理代理对生成：

```mermaid
flowchart TD
INPUT[输入UTF-32码点] --> CHECK_RANGE{检查码点范围}
CHECK_RANGE --> |<= 0xFFFF| DIRECT_MAP[直接映射]
CHECK_RANGE --> |<= 0x10FFFF| SURROGATE_GEN[生成代理对]
CHECK_RANGE --> |> 0x10FFFF| REPLACE_INVALID[替换为FFFD]
DIRECT_MAP --> OUTPUT[输出UTF-16]
SURROGATE_GEN --> OUTPUT
REPLACE_INVALID --> OUTPUT
```

**图表来源**
- [lib/charset.h](file://lib/charset.h#L392-L444)

**章节来源**
- [lib/charset.h](file://lib/charset.h#L300-L444)

### 编码检测系统

编码检测系统提供智能的编码识别能力：

```mermaid
flowchart TD
START([开始检测]) --> CHECK_BOM{检查BOM标记}
CHECK_BOM --> |有BOM| DETECT_BOM[基于BOM确定编码]
CHECK_BOM --> |无BOM| CHECK_UTF8[检查UTF-8有效性]
CHECK_UTF8 --> |有效| DETECT_UTF8[确定为UTF-8]
CHECK_UTF8 --> |无效| CHECK_ZEROES[分析零字节分布]
CHECK_ZEROES --> |UTF-16特征| DETECT_UTF16[确定为UTF-16]
CHECK_ZEROES --> |UTF-32特征| DETECT_UTF32[确定为UTF-32]
CHECK_ZEROES --> |其他| DETECT_OEM[确定为OEM/二进制]
DETECT_BOM --> END([检测完成])
DETECT_UTF8 --> END
DETECT_UTF16 --> END
DETECT_UTF32 --> END
DETECT_OEM --> END
```

**图表来源**
- [lib/charset.h](file://lib/charset.h#L742-L890)

**章节来源**
- [lib/charset.h](file://lib/charset.h#L714-L890)

### 通用编码转换器

通用编码转换器提供灵活的编码转换接口：

```mermaid
sequenceDiagram
participant Client as 客户端
participant ConvCharset as 通用转换器
participant UTF8to16 as UTF8to16
participant UTF16to8 as UTF16to8
participant Platform as 平台适配
Client->>ConvCharset : 请求编码转换
ConvCharset->>ConvCharset : 检查编码组合
alt 内置支持的转换
ConvCharset->>UTF8to16 : 调用相应转换函数
UTF8to16-->>ConvCharset : 返回转换结果
else Windows平台
ConvCharset->>Platform : 调用Win32 API
Platform-->>ConvCharset : 返回转换结果
else 其他平台
ConvCharset->>ConvCharset : 报告不支持
end
ConvCharset-->>Client : 返回转换结果
```

**图表来源**
- [lib/charset.h](file://lib/charset.h#L488-L710)

**章节来源**
- [lib/charset.h](file://lib/charset.h#L488-L710)

## 依赖关系分析

字符集转换API的依赖关系如下：

```mermaid
graph TB
subgraph "外部依赖"
STDIO[C标准库]
STRING[C字符串库]
MEMORY[C内存库]
end
subgraph "内部依赖"
BASE[基础库]
STRING_LIB[字符串库]
BUFFER[缓冲区管理]
end
subgraph "核心实现"
CHARSET[字符集转换]
DETECTION[编码检测]
CONVERSION[通用转换]
end
STDIO --> CHARSET
STRING --> CHARSET
MEMORY --> CHARSET
BASE --> CHARSET
STRING_LIB --> CHARSET
BUFFER --> CHARSET
BASE --> DETECTION
STRING_LIB --> DETECTION
CHARSET --> CONVERSION
DETECTION --> CONVERSION
```

**图表来源**
- [lib/charset.h](file://lib/charset.h#L1-L908)
- [lib/base.h](file://lib/base.h#L1-L132)

**章节来源**
- [lib/charset.h](file://lib/charset.h#L1-L908)
- [lib/base.h](file://lib/base.h#L1-L132)

## 性能考虑

### 内存管理优化

字符集转换API采用了多种内存管理策略：

1. **延迟分配**：只在确认转换必要时才分配内存
2. **批量处理**：支持一次性转换多个字符串
3. **原地转换**：某些转换支持原地操作以节省内存

### 算法复杂度

- **时间复杂度**：O(n)，其中n是输入字符数
- **空间复杂度**：O(n)，用于存储转换结果
- **内存峰值**：通常为输入大小的1-2倍

### 性能最佳实践

1. **避免重复转换**：缓存转换结果
2. **使用原地转换**：在可写内存上直接转换
3. **批量处理**：一次处理多个字符串
4. **合理使用缓冲区**：预估内存需求

**章节来源**
- [docs/api-charset.md](file://docs/api-charset.md#L946-L1031)

## 故障排除指南

### 常见问题及解决方案

#### 1. 内存泄漏问题

**症状**：程序运行一段时间后内存持续增长

**原因**：忘记调用`xrtFree`释放转换结果

**解决方案**：
```c
// ❌ 错误：忘记释放
u16str text = xrtUTF8to16(utf8_text, 0);
DisplayText(text); // 忘记释放

// ✅ 正确：及时释放
u16str text = xrtUTF8to16(utf8_text, 0);
DisplayText(text);
xrtFree(text);
```

#### 2. 字符数与字节数混淆

**症状**：转换结果不正确或崩溃

**原因**：UTF-8转换使用字节数，UTF-16/UTF-32转换使用字符数

**解决方案**：
```c
// UTF-8转换使用字节数
size_t byte_len = strlen(utf8_text);
u16str utf16_text = xrtUTF8to16(utf8_text, byte_len, NULL);

// UTF-16转换使用字符数
size_t char_len = u16len(utf16_text);
str utf8_text = xrtUTF16to8(utf16_text, char_len, NULL);
```

#### 3. 代理对错误处理

**症状**：特殊字符显示为替换字符

**原因**：UTF-16代理对不完整或超出支持范围

**解决方案**：检查输入数据的有效性

**章节来源**
- [docs/api-charset.md](file://docs/api-charset.md#L1035-L1105)

### 错误处理机制

字符集转换API提供了完善的错误处理机制：

```mermaid
flowchart TD
CALL[函数调用] --> VALIDATE{参数验证}
VALIDATE --> |失败| SET_ERROR[设置错误信息]
VALIDATE --> |成功| EXECUTE[执行转换]
EXECUTE --> SUCCESS{转换成功?}
SUCCESS --> |是| RETURN_SUCCESS[返回结果]
SUCCESS --> |否| SET_ERROR
SET_ERROR --> RETURN_NULL[返回NULL]
```

**图表来源**
- [lib/base.h](file://lib/base.h#L88-L132)

**章节来源**
- [lib/base.h](file://lib/base.h#L88-L132)

## 结论

字符集转换API提供了完整、高效的多编码转换解决方案。其特点包括：

1. **完整性**：支持UTF-8、UTF-16、UTF-32之间的所有转换
2. **准确性**：正确处理Unicode代理对和边界情况
3. **效率性**：优化的算法和内存管理策略
4. **可靠性**：完善的错误处理和编码检测机制
5. **易用性**：清晰的API设计和丰富的使用示例

该API适用于各种应用场景，包括文件处理、网络通信、国际化软件开发等，为跨平台字符集处理提供了可靠的基础设施。

## 附录

### 使用示例

#### 1. 基本转换示例

```c
#include "xrt.h"

int main() {
    xrtInit();
    
    // UTF-8到UTF-16转换
    str utf8_text = "Hello 世界";
    u16str utf16_text = xrtUTF8to16(utf8_text, 0, NULL);
    
    if (utf16_text) {
        printf("转换成功\n");
        xrtFree(utf16_text);
    }
    
    xrtUnit();
    return 0;
}
```

#### 2. 编码检测示例

```c
// 检测文件编码
size_t file_size = 0;
ptr file_data = xrtFileGetAll("input.txt", &file_size);

if (file_data) {
    int charset = xrtDetectCharset(file_data, file_size, TRUE);
    printf("检测到编码: %d\n", charset);
    
    xrtFree(file_data);
}
```

#### 3. 通用转换示例

```c
// 任意编码转换
str input_text = "文本内容";
ptr result = xrtConvCharset(
    input_text, 
    0, 
    XRT_CP_OEM, 
    XRT_CP_UTF8, 
    NULL
);
```

**章节来源**
- [docs/api-charset.md](file://docs/api-charset.md#L75-L814)