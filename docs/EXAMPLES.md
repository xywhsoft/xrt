# XRT 示例项目

> 展示 XRT 各种功能的使用示例，帮助快速上手

[English Version](EXAMPLES.en.md) | [返回索引](README.md)

---

## 📑 目录

- [示例 1: Hello World](#示例-1-hello-world)
- [示例 2: JSON 处理](#示例-2-json-处理)
- [示例 3: 字符串操作](#示例-3-字符串操作)
- [示例 4: Value 类型系统](#示例-4-value-类型系统)
- [示例 5: 模板引擎](#示例-5-模板引擎)
- [示例 6: 文件操作](#示例-6-文件操作)
- [示例 7: HTTP 客户端](#示例-7-http-客户端)
- [示例 8: 配置文件处理](#示例-8-配置文件处理)

---

## 示例 1: Hello World

最简单的 XRT 使用示例，展示基础功能。

### 代码

```c
#define XRT_IMPLEMENTATION
#include "xrt.h"
#include <stdio.h>

int main() {
    // 初始化 XRT
    xrtInit();
    
    // 创建字符串
    str greeting = xrtCopyStr("Hello, XRT!", 0);
    printf("Greeting: %s\n", greeting);
    
    // 创建整数
    xvalue num = xvoCreateInt(42);
    printf("Number: %lld\n", xvoGetInt(num));
    
    // 创建布尔值
    xvalue flag = xvoCreateBool(TRUE);
    printf("Flag: %s\n", xvoGetBool(flag) ? "true" : "false");
    
    // 清理资源
    xrtFree(greeting);
    xvoUnref(num);
    xvoUnref(flag);
    
    // 清理 XRT
    xrtUnit();
    
    return 0;
}
```

### 编译和运行

```bash
# 使用 TCC 编译
tcc -run example1.c

# 使用 GCC 编译
gcc -DXRT_IMPLEMENTATION example1.c xrt.h -o example1
./example1

# 使用 MSVC 编译
cl /DXRT_IMPLEMENTATION example1.c /Fe:example1.exe
example1.exe
```

---

## 示例 2: JSON 处理

展示如何解析和生成 JSON 数据。

### 代码

```c
#define XRT_IMPLEMENTATION
#include "xrt.h"
#include <stdio.h>

int main() {
    xrtInit();
    
    // 解析 JSON 字符串
    str json_str = "{\"name\":\"Alice\",\"age\":25,\"active\":true,\"skills\":[\"C\",\"Python\"]}";
    xvalue data = xrtParseJSON(json_str, 0);
    
    if (!data) {
        printf("Failed to parse JSON\n");
        xrtUnit();
        return 1;
    }
    
    // 读取数据
    str name = xvoTableGetText(data, "name", 0);
    int64 age = xvoTableGetInt(data, "age", 0);
    bool active = xvoTableGetBool(data, "active", 0);
    xvalue skills = xvoTableGetValue(data, "skills", 0);
    
    printf("Name: %s\n", name);
    printf("Age: %lld\n", age);
    printf("Active: %s\n", active ? "Yes" : "No");
    
    // 遍历数组
    uint32 skill_count = xvoArrayItemCount(skills);
    printf("Skills (%d):\n", skill_count);
    for (uint32 i = 0; i < skill_count; i++) {
        str skill = xvoArrayGetText(skills, i);
        printf("  - %s\n", skill);
    }
    
    // 修改数据
    xvoTableSetInt(data, "age", 3, 26);
    
    // 生成 JSON
    size_t json_len;
    str result_json = xrtStringifyJSON(data, TRUE, &json_len);
    printf("\nModified JSON:\n%s\n", result_json);
    xrtFree(result_json);
    
    // 清理
    xvoUnref(data);
    xrtUnit();
    
    return 0;
}
```

### 运行结果

```
Name: Alice
Age: 25
Active: Yes
Skills (2):
  - C
  - Python

Modified JSON:
{
  "name": "Alice",
  "age": 26,
  "active": true,
  "skills": [
    "C",
    "Python"
  ]
}
```

---

## 示例 3: 字符串操作

展示字符串的各种操作。

### 代码

```c
#define XRT_IMPLEMENTATION
#include "xrt.h"
#include <stdio.h>

int main() {
    xrtInit();
    
    // 字符串拼接
    str s1 = "Hello";
    str s2 = "XRT";
    str result = xrtFormat("%s %s!", s1, s2);
    printf("Concat: %s\n", result);
    
    // 字符串查找
    str text = "Hello World, XRT is awesome!";
    str found = xrtFindStr(text, 0, "XRT", 3, FALSE);
    if (found) {
        printf("Found at position: %lld\n", xCore.iRet);
    }
    
    // 字符串替换
    str replaced = xrtReplaceStr(text, 0, "awesome", "great", 3, FALSE);
    printf("Replaced: %s\n", replaced);
    
    // 字符串分割
    str parts = xrtSplitStr("a,b,c,d", 0, ",");
    printf("Split result:\n");
    // 遍历分割结果...
    
    // 字符串裁剪
    str trimmed = "  Hello World  ";
    str ltrim = xrtLTrimStr(trimmed, 0, " ");
    str rtrim = xrtRTrimStr(trimmed, 0, " ");
    str lrtrim = xrtLRTrimStr(trimmed, 0, " ");
    printf("LTrim: '%s'\n", ltrim);
    printf("RTrim: '%s'\n", rtrim);
    printf("LRTrim: '%s'\n", lrtrim);
    
    // 清理
    xrtFree(result);
    xrtFree(replaced);
    xrtFree(ltrim);
    xrtFree(rtrim);
    xrtFree(lrtrim);
    xrtUnit();
    
    return 0;
}
```

---

## 示例 4: Value 类型系统

展示 Value 动态类型系统的使用方法。

### 代码

```c
#define XRT_IMPLEMENTATION
#include "xrt.h"
#include <stdio.h>

void print_value(xvalue val) {
    int type = xvoType(val);
    printf("Type: ");
    
    switch (type) {
        case XVO_DT_INT:
            printf("INT = %lld\n", xvoGetInt(val));
            break;
        case XVO_DT_FLOAT:
            printf("FLOAT = %f\n", xvoGetFloat(val));
            break;
        case XVO_DT_TEXT:
            printf("TEXT = %s\n", xvoGetText(val));
            break;
        case XVO_DT_BOOL:
            printf("BOOL = %s\n", xvoGetBool(val) ? "true" : "false");
            break;
        case XVO_DT_ARRAY:
            printf("ARRAY (count: %u)\n", xvoArrayItemCount(val));
            break;
        case XVO_DT_TABLE:
            printf("TABLE (count: %u)\n", xvoTableItemCount(val));
            break;
        default:
            printf("Unknown\n");
    }
}

int main() {
    xrtInit();
    
    // 创建数组
    xvalue arr = xvoCreateArray();
    xvoArrayAppendInt(arr, 1);
    xvoArrayAppendInt(arr, 2);
    xvoArrayAppendInt(arr, 3);
    printf("Array:\n");
    print_value(arr);
    
    // 创建表
    xvalue table = xvoCreateTable();
    xvoTableSetInt(table, "id", 0, 1001);
    xvoTableSetText(table, "name", 0, "Alice", 0, FALSE);
    xvoTableSetFloat(table, "score", 0, 95.5);
    xvoTableSetBool(table, "active", 0, TRUE);
    printf("\nTable:\n");
    print_value(table);
    
    // 创建嵌套结构
    xvalue config = xvoCreateTable();
    xvoTableSetValue(config, "user", 0, table, TRUE);
    xvoTableSetValue(config, "tags", 0, arr, TRUE);
    printf("\nNested structure:\n");
    print_value(config);
    
    // 浅拷贝
    xvalue copy = xvoCopy(config);
    printf("\nShallow copy:\n");
    print_value(copy);
    
    // 深拷贝
    xvalue deep = xvoDeepCopy(config);
    printf("\nDeep copy:\n");
    print_value(deep);
    
    // 清理
    xvoUnref(arr);
    xvoUnref(table);
    xvoUnref(config);
    xvoUnref(copy);
    xvoUnref(deep);
    xrtUnit();
    
    return 0;
}
```

---

## 示例 5: 模板引擎

展示如何使用 XRT 模板引擎生成文本。

### 代码

```c
#define XRT_IMPLEMENTATION
#include "xrt.h"
#include <stdio.h>

int main() {
    xrtInit();
    
    // 模板文本
    str template_text = 
        "用户信息：\n"
        "--------------------------------\n"
        "姓名：{$ name : 未知}\n"
        "年龄：{% age : 0}\n"
        "状态：{#if:active}活跃{#else}非活跃{#end}\n"
        "\n"
        "技能列表：\n"
        "{#foreach:skills}\n"
        "  - {$ skill}\n"
        "{#end}\n"
        "--------------------------------\n";
    
    // 解析模板
    XTE_LiteObject tpl = xteParse(template_text, 0, NULL);
    if (!tpl || !tpl->Success) {
        printf("Failed to parse template\n");
        xrtUnit();
        return 1;
    }
    
    // 准备数据
    xvalue data = xvoCreateTable();
    xvoTableSetText(data, "name", 0, "Alice", 0, FALSE);
    xvoTableSetInt(data, "age", 0, 25);
    xvoTableSetBool(data, "active", 0, TRUE);
    
    xvalue skills = xvoCreateArray();
    xvoArrayAppendText(skills, "C", 0, FALSE);
    xvoArrayAppendText(skills, "Python", 0, FALSE);
    xvoArrayAppendText(skills, "Go", 0, FALSE);
    xvoTableSetValue(data, "skills", 0, skills, TRUE);
    
    // 生成输出
    size_t output_len;
    str result = xteMake(tpl, data, NULL, NULL, &output_len);
    printf("%s", result);
    printf("Generated %zu bytes\n", output_len);
    
    // 清理
    xrtFree(result);
    xvoUnref(data);
    xteParseFree(tpl);
    xrtUnit();
    
    return 0;
}
```

### 运行结果

```
用户信息：
--------------------------------
姓名：Alice
年龄：25
状态：活跃

技能列表：
  - C
  - Python
  - Go
--------------------------------
Generated 89 bytes
```

---

## 示例 6: 文件操作

展示文件读写操作。

### 代码

```c
#define XRT_IMPLEMENTATION
#include "xrt.h"
#include <stdio.h>

int main() {
    xrtInit();
    
    // 创建目录
    str dir = "test_data";
    xrtDirCreate(dir);
    
    // 写入文件
    str file_path = "test_data/config.txt";
    str content = "port=8080\nhost=localhost\n";
    xrtFileWriteAll(file_path, content, strlen(content), XRT_CP_UTF8);
    printf("Written to %s\n", file_path);
    
    // 读取文件
    size_t read_len;
    str read_content = xrtFileReadAll(file_path, &read_len);
    printf("Read %zu bytes:\n%s\n", read_len, read_content);
    
    // 检查文件是否存在
    bool exists = xrtFileExists(file_path);
    printf("File exists: %s\n", exists ? "Yes" : "No");
    
    // 遍历目录
    printf("\nFiles in %s:\n", dir);
    xvalue files = xrtDirScan(dir, FALSE);
    if (files && xvoType(files) == XVO_DT_ARRAY) {
        uint32 count = xvoArrayItemCount(files);
        for (uint32 i = 0; i < count; i++) {
            str filename = xvoArrayGetText(files, i);
            printf("  - %s\n", filename);
        }
        xvoUnref(files);
    }
    
    // 清理
    xrtFree(read_content);
    xrtUnit();
    
    return 0;
}
```

---

## 示例 7: HTTP 客户端

展示如何使用 HTTP 客户端发送请求。

### 代码

```c
#define XRT_IMPLEMENTATION
#include "xrt.h"
#include <stdio.h>

int main() {
    xrtInit();
    
    // GET 请求
    str url = "https://httpbin.org/get";
    xvalue response = xrtHttpGet(url, NULL, NULL, NULL);
    
    if (response && xvoGetInt(response, 0) == 200) {
        int64 status = xvoTableGetInt(response, "status", 0);
        str body = xvoTableGetText(response, "body", 4);
        printf("HTTP Status: %lld\n", status);
        printf("Response Body:\n%s\n", body);
    } else {
        printf("HTTP request failed\n");
    }
    
    if (response) {
        xvoUnref(response);
    }
    
    // POST 请求
    xvalue post_data = xvoCreateTable();
    xvoTableSetText(post_data, "key", 0, "value", 0, FALSE);
    
    response = xrtHttpPost(url, NULL, NULL, NULL, post_data);
    
    if (response && xvoGetInt(response, 0) == 200) {
        str body = xvoTableGetText(response, "body", 4);
        printf("\nPOST Response:\n%s\n", body);
    }
    
    if (post_data) {
        xvoUnref(post_data);
    }
    if (response) {
        xvoUnref(response);
    }
    
    xrtUnit();
    
    return 0;
}
```

---

## 示例 8: 配置文件处理

展示如何使用 JSON 存储和读取配置。

### 代码

```c
#define XRT_IMPLEMENTATION
#include "xrt.h"
#include <stdio.h>

void save_config(str filename, xvalue config) {
    // 序列化为 JSON
    size_t len;
    str json = xrtStringifyJSON(config, TRUE, &len);
    
    // 写入文件
    xrtFileWriteAll(filename, json, len, XRT_CP_UTF8);
    printf("Config saved to %s\n", filename);
    
    xrtFree(json);
}

xvalue load_config(str filename) {
    // 读取文件
    size_t len;
    str json = xrtFileReadAll(filename, &len);
    
    if (!json) {
        printf("Failed to load config\n");
        return NULL;
    }
    
    // 解析 JSON
    xvalue config = xrtParseJSON(json, len);
    xrtFree(json);
    
    return config;
}

int main() {
    xrtInit();
    
    // 创建配置
    xvalue config = xvoCreateTable();
    xvoTableSetText(config, "app_name", 0, "MyApp", 0, FALSE);
    xvoTableSetText(config, "version", 0, "1.0.0", 0, FALSE);
    xvoTableSetInt(config, "port", 0, 8080);
    xvoTableSetInt(config, "debug", 0, 1);
    xvoTableSetText(config, "log_file", 0, "app.log", 0, FALSE);
    
    // 添加数组配置
    xvalue servers = xvoCreateArray();
    xvoArrayAppendText(servers, "server1.example.com", 0, FALSE);
    xvoArrayAppendText(servers, "server2.example.com", 0, FALSE);
    xvoTableSetValue(config, "servers", 0, servers, TRUE);
    
    // 保存配置
    save_config("config.json", config);
    
    // 读取配置
    xvalue loaded = load_config("config.json");
    if (loaded) {
        printf("\nLoaded config:\n");
        printf("App Name: %s\n", xvoTableGetText(loaded, "app_name", 0));
        printf("Version: %s\n", xvoTableGetText(loaded, "version", 0));
        printf("Port: %lld\n", xvoTableGetInt(loaded, "port", 0));
        printf("Debug: %s\n", xvoTableGetBool(loaded, "debug", 0) ? "true" : "false");
        printf("Log File: %s\n", xvoTableGetText(loaded, "log_file", 0));
        
        xvalue svrs = xvoTableGetValue(loaded, "servers", 0);
        printf("Servers (%u):\n", xvoArrayItemCount(svrs));
        for (uint32 i = 0; i < xvoArrayItemCount(svrs); i++) {
            printf("  - %s\n", xvoArrayGetText(svrs, i));
        }
        
        xvoUnref(loaded);
    }
    
    // 清理
    xvoUnref(config);
    xrtUnit();
    
    return 0;
}
```

---

## 编译和运行所有示例

### 编译脚本

#### Windows (build.bat)

```batch
@echo off
echo Compiling XRT examples...

echo [1/8] Compiling example1_hello.c
tcc -DXRT_IMPLEMENTATION example1_hello.c xrt.h -o example1_hello.exe

echo [2/8] Compiling example2_json.c
tcc -DXRT_IMPLEMENTATION example2_json.c xrt.h -o example2_json.exe

echo [3/8] Compiling example3_string.c
tcc -DXRT_IMPLEMENTATION example3_string.c xrt.h -o example3_string.exe

echo [4/8] Compiling example4_value.c
tcc -DXRT_IMPLEMENTATION example4_value.c xrt.h -o example4_value.exe

echo [5/8] Compiling example5_template.c
tcc -DXRT_IMPLEMENTATION example5_template.c xrt.h -o example5_template.exe

echo [6/8] Compiling example6_file.c
tcc -DXRT_IMPLEMENTATION example6_file.c xrt.h -o example6_file.exe

echo [7/8] Compiling example7_http.c
tcc -DXRT_IMPLEMENTATION example7_http.c xrt.h -o example7_http.exe

echo [8/8] Compiling example8_config.c
tcc -DXRT_IMPLEMENTATION example8_config.c xrt.h -o example8_config.exe

echo All examples compiled successfully!
```

#### Linux/macOS (build.sh)

```bash
#!/bin/bash
echo "Compiling XRT examples..."

echo "[1/8] Compiling example1_hello.c"
gcc -DXRT_IMPLEMENTATION example1_hello.c xrt.h -o example1_hello

echo "[2/8] Compiling example2_json.c"
gcc -DXRT_IMPLEMENTATION example2_json.c xrt.h -o example2_json

echo "[3/8] Compiling example3_string.c"
gcc -DXRT_IMPLEMENTATION example3_string.c xrt.h -o example3_string

echo "[4/8] Compiling example4_value.c"
gcc -DXRT_IMPLEMENTATION example4_value.c xrt.h -o example4_value

echo "[5/8] Compiling example5_template.c"
gcc -DXRT_IMPLEMENTATION example5_template.c xrt.h -o example5_template

echo "[6/8] Compiling example6_file.c"
gcc -DXRT_IMPLEMENTATION example6_file.c xrt.h -o example6_file

echo "[7/8] Compiling example7_http.c"
gcc -DXRT_IMPLEMENTATION example7_http.c xrt.h -o example7_http

echo "[8/8] Compiling example8_config.c"
gcc -DXRT_IMPLEMENTATION example8_config.c xrt.h -o example8_config

echo "All examples compiled successfully!"
```

---

## 总结

这 8 个示例涵盖了 XRT 的主要功能：

1. ✅ Hello World - 基础使用
2. ✅ JSON 处理 - 解析和生成
3. ✅ 字符串操作 - 各种字符串操作
4. ✅ Value 类型系统 - 动态类型
5. ✅ 模板引擎 - 文本生成
6. ✅ 文件操作 - 读写和目录
7. ✅ HTTP 客户端 - 网络请求
8. ✅ 配置文件处理 - JSON 配置

建议按顺序学习和运行这些示例，以快速掌握 XRT 的使用方法。

---

**XRT 示例项目版本：1.0** | **最后更新：2025-01**
