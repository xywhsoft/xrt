# XRT 常见问题解答 (FAQ)

> XRT 使用过程中的常见问题和解决方案

[English Version](FAQ.en.md) | [返回文档索引](docs/README.md)

---

## 📑 目录

- [编译问题](#编译问题)
- [内存管理问题](#内存管理问题)
- [API使用问题](#api使用问题)
- [性能优化问题](#性能优化问题)
- [跨平台问题](#跨平台问题)
- [JSON 处理问题](#json处理问题)
- [Value 类型问题](#value类型问题)
- [网络通信问题](#网络通信问题)

---

## 编译问题

### 问题1：提示 "undefined symbol"

**错误信息**：
```
tcc: error: undefined symbol 'xrtMalloc'
```

**原因**：忘记定义 `XRT_IMPLEMENTATION`

**解决方案**：
```c
// ❌ 错误用法
#include "xrt.h"

// ✅ 正确用法
#define XRT_IMPLEMENTATION
#include "xrt.h"
```

### 问题2：TCC 编译错误

**错误信息**：
```
tcc: error: could not write 'test.exe': Permission denied
```

**原因**：程序正在运行或被占用

**解决方案**：
```batch
REM 方法1：关闭进程
taskkill /F /IM test.exe

REM 方法2：使用 /force 标志
tcc -force xrt_log.c test.c -o test.exe
```

### 问题3：GCC 链接错误

**错误信息**：
```
gcc: error: undefined reference to 'xrtMalloc'
```

**原因**：忘记链接 xrt.c

**解决方案**：
```bash
# 正确方式
gcc xrt_log.c test.c -o test.exe

# 错误方式
gcc test.c -o test.exe  # xrt_log.c 未链接
```

### 问题4：MSVC 编译错误

**错误信息**：
```
error LNK2019: unresolved external symbol xrtMalloc
```

**原因**：未添加 xrt.c 到项目

**解决方案**：
```
# 在 Visual Studio 项目中添加
xrt_log.c
test.c
```

---

## 内存管理问题

### 问题5：内存泄漏

**原因**：使用了 `xrtMalloc` 分配但未使用 `xrtFree` 释放

**解决方案**：
```c
// ❌ 错误用法
str data = xrtMalloc(1024);
// 使用 data...
// 忘记释放！

// ✅ 正确用法
str data = xrtMalloc(1024);
// 使用 data...
xrtFree(data);  // 必须释放
```

### 问题6：使用临时内存后程序崩溃

**原因**：使用了超过32个临时内存槽位

**解决方案**：
```c
// ❌ 错误用法
for (int i = 0; i < 100; i++) {
    str temp = xrtTempMemory(1024);
    // 如果 i >= 32，会释放槽位 0，导致之前分配的内存失效
}

// ✅ 正确用法
for (int i = 0; i < 32; i++) {
    str temp = xrtTempMemory(1024);
    // 使用临时内存...
    // 函数返回时自动释放
}
```

### 问题7：频繁 malloc 导致内存碎片

**原因**：频繁小块分配/释放

**解决方案**：
```c
// ❌ 不好的做法
for (int i = 0; i < 1000; i++) {
    void* p = xrtMalloc(128);  // 1000次分配128字节
    // 使用 p ...
    xrtFree(p);
}

// ✅ 好做法：使用内存池
xvalue pool = xvoMemPoolCreate(128);
for (int i = 0; i < 1000; i++) {
    void* p = xrtMemPoolAlloc(pool, 128);
    // 使用 p ...
    xrtMemPoolFree(pool, p);  // 无碎片
}
```

---

## API使用问题

### 问题8：如何正确释放 Value 类型

**错误示例**：
```c
// ❌ 错误：直接释放
xvalue val = xvoCreateInt(123);
xrtFree(val);  // 错误！Value 必须使用 xvoUnref
```

**正确示例**：
```c
// ✅ 正确：使用引用计数
xvalue val = xvoCreateInt(123);
// ... 使用 val ...
xvoUnref(val);  // 正确：减少引用计数
```

### 问题9：Value 类型转换失败

**错误示例**：
```c
// ❌ 错误：直接转换
int64 ival = (int64)xvoInt(value);  // 错误：不是整型
```

**正确示例**：
```c
// ✅ 正确：使用类型检查
if (xvoIsInt(value)) {
    int64 ival = xvoGetInt(value);
}
```

### 问题10：如何判断 Value 类型

**错误示例**：
```c
// ❌ 错误：直接访问内部字段
if (value->Type == 2) {  // 错误：直接访问内部字段
    printf("Int\n");
}
```

**正确示例**：
```c
// ✅ 正确：使用类型判断函数
if (xvoIsInt(value)) {
    int64 ival = xvoGetInt(value);
    printf("Int: %lld\n", ival);
} else if (xvoIsText(value)) {
    str s = xvoGetText(value);
    printf("Text: %s\n", s);
}
```

### 问题11：数组操作越界

**错误示例**：
```c
xvalue arr = xvoCreateArray();
xvoArrayAlloc(arr, 10);      // 分配10个元素
xvoArrayAppendInt(arr, 1);
xvoArrayAppendInt(arr, 2);
xvoArrayGetValue(arr, 10);    // 错误：越界！
```

**正确示例**：
```c
xvalue arr = xvoCreateArray();
xvoArrayAlloc(arr, 10);      // 分配10个元素，索引0-9
for (int i = 0; i < 10; i++) {
    xvoArrayAppendInt(arr, i + 1);
    xvoArraySetValue(arr, i, xvoCreateInt(i + 1));
}
for (int i = 0; i < 10; i++) {
    int64 val = xvoGetInt(xvoArrayGetValue(arr, i));
    printf("arr[%d] = %lld\n", i, val);
}
```

### 问题12：Dict/Table 的键不存在

**错误示例**：
```c
xvalue dict = xvoCreateTable();
xvoTableSetText(dict, "name", 0, "XRT", 0, FALSE);
str name = xvoTableGetText(dict, "version", 0);  // 键误：键不存在！
printf("Version: %s\n", name);  // 未定义行为
```

**正确示例**：
```c
xvalue dict = xvoCreateTable();
xvoTableSetText(dict, "name", 0, "XRT", 0, FALSE);
str name = xvoTableGetText(dict, "name", 0);  // 正确：键存在
str version = xvoTableGetText(dict, "version", 0);  // 添加这个字段
if (version != xCore->sNull) {
    printf("Version: %s\n", version);
}
```

---

## 性能优化问题

### 问题13：字符串拼接性能差

**问题场景**：
```c
// ❌ 不好：频繁分配内存
str result = xrtConcat("Hello ", "World ", "!");  // 3次分配
xrtFree(result);
```

**解决方案**：
```c
// ✅ 好方式：使用格式化
str result = xrtFormat("Hello %s!", "World");
xrtFree(result);  // 只分配1次
```

**性能提升**：70-80%

### 问题14：数组操作慢

**问题场景**：
```c
// ❌ 不好：逐个赋值
xvalue arr = xvoCreateArray();
for (int i = 0; i < 1000; i++) {
    xvoArraySetValue(arr, i, xvoCreateInt(i));
}
```

**解决方案**：
```c
// ✅ 好方式：预分配+设置
xvalue arr = xvoCreateArray();
xvoArrayAlloc(arr, 1000);
for (int i = 0; i < 1000; i++) {
    xvoArraySetValue(arr, i, xvoCreateInt(i));
}
```

**性能提升**：80-90%

### 问题15：JSON 解析内存占用高

**问题场景**：
```c
str json = "{\"data\":[";
for (int i = 0; i < 10000; i++) {
    json = xrtFormat("%s{\"id\":%d},", json, i);  // 拼接字符串会很大
}
json = xrtFormat("%s]}", json);
xvalue data = xrtParseJSON(json, 0);  // DOM 方式，内存占用高
xvoUnref(data);
```

**解决方案**：
```c
// ✅ 好方式：使用 SAX 解析
json_sax_cb_t cb = {
    .on_number = on_json_number,
    .on_string = on_json_string,
    .on_start_object = on_json_start,
    .on_end_object = on_json_end,
    // ... 其他回调
};

xrtJsonParseSAX(json_text, strlen(json_text), &cb);
// SAX 方式：边解析边释放，内存占用低
```

**内存节省**：50-70%

---

## 跨平台问题

### 问题16：Windows 下路径错误

**问题场景**：
```c
// ❌ 错误：路径分隔符错误
char* path = "data\\files\\config.json";
FILE* fp = fopen(path, "r");  // 在 Linux 下会失败
```

**解决方案**：
```c
// ✅ 好方式：使用 XRT 路径 API
char* path = xrtPathJoin("data", "files", "config.json");
FILE* fp = fopen(path, "r");  // 跨平台兼容
```

### 问题17：换行符不兼容

**问题场景**：
```c
// ❌ 错误：硬编码换行符
char* log = "line1\nline2\n";
FILE* fp = fopen("log.txt", "w");
fprintf(fp, "%s\n", log);  // Windows 下换行符不正确
```

**解决方案**：
```c
// ✅ 好方式：使用 XRT 换行符常量
char* log = "line1" LINE_END "line2" LINE_END;
FILE* fp = fopen("log.txt", "w");
fprintf(fp, "%s", log);  // 自动使用平台换行符
fclose(fp);
```

**LINE_END 定义**：
```c
#ifdef _WIN32 || defined(_WIN64)
#define LINE_END "\r\n"
#else
#define LINE_END "\n"
#endif
```

### 问题18：中文路径乱码

**问题场景**：
```c
// ❌ 错误：未指定字符集
FILE* fp = fopen("数据\\配置.json", "r");
char buf[1024];
fread(buf, 1, 1024, fp);
// 输出乱码
```

**解决方案**：
```c
// ✅ 好方式：指定字符集
#include "xrt.h"

FILE* fp = fopen("数据\\配置.json", "r", XRT_CP_UTF8);
char buf[1024];
fread(buf, 1, 1024, fp);
fclose(fp);
```

**字符集常量**：
```c
XRT_CP_UTF8     // UTF-8
XRT_CP_UTF16    // UTF-16
XRT_CP_UTF32    // UTF-32
XRT_CP_AUTO      // 自动检测
```

---

## JSON 处理问题

### 问题19：JSON 解析失败

**问题场景**：
```c
str json = "{\"name\":\"xrt\",\"features\":[\"fast\",\"simple\"}";
xvalue data = xrtParseJSON(json, 0);
if (!data || xvoIsNull(data)) {
    printf("JSON parse failed!\n");
    return;
}
// 无错误信息，无法知道为什么失败
```

**解决方案：添加错误处理和详细报告**
```c
// ✅ 好方式：使用错误回调
void on_error(str sError) {
    printf("JSON Error: %s\n", sError);
}

xrtGlobalData* xCore = xrtInit();
xCore->OnError = on_error;  // 设置错误回调

str json = "{\"name\":\"xrt\",\"features\":[\"fast\",\"simple\"]}";
xvalue data = xrtParseJSON(json, 0);
if (!data || xvoIsNull(data)) {
    printf("JSON parse failed! Last Error: %s\n", xCore->LastError);
    return;
}
```

### 问题20：JSON 格式化输出不美观

**问题场景**：
```c
xvalue data = xvoCreateTable();
xvoTableSetText(data, "title", 0, "XRT", 0, FALSE);
xvoTableSetInt(data, "count", 0, 42);
xvoTableSetText(data, "features", 0, "fast, simple", 0, TRUE);

size_t len = 0;
str json = xrtStringifyJSON(data, FALSE, &len);  // FALSE = 压缩输出
printf("%s\n", json);
xrtFree(json);
```

**输出结果（不美观）**：
```json
{"title":"XRT","count":42,"features":"fast,simple"}
```

**解决方案**：
```c
// ✅ 好方式：使用格式化输出
size_t len = 0;
str json = xrtStringifyJSON(data, TRUE, &len);  // TRUE = 格式化
printf("%s\n", json);
xrtFree(json);
```

**输出结果（美观）**：
```json
{
    "title": "XRT",
    "count": 42,
    "features": "fast, simple",
    "features": [
        "fast",
        "simple"
    ]
}
```

---

## Value 类型问题

### 问题21：Value 比较失败

**问题场景**：
```c
xvalue v1 = xvoCreateInt(100);
xvalue v2 = xvoCreateInt(100);

// ❌ 错误：直接比较
if (v1 == v2) {
    printf("Equal!\n");
}
```

**错误原因**：直接比较对象指针，只比较指针地址

**解决方案**：使用类型安全的比较函数
```c
// ✅ 好方式：使用比较函数
if (xvoEqual(v1, v2)) {
    printf("Equal!\n");
}

// ✅ 也可以：使用类型检查
if (xvoIsInt(v1) && xvoIsInt(v2)) {
    if (xvoGetInt(v1) == xvoGetInt(v2)) {
        printf("Values are equal!\n");
    }
}
```

### 问题22：遍历 Array/List 很慢

**问题场景**：
```c
xvalue arr = xvoCreateArray();
xvoArrayAlloc(arr, 10000);  // 10000个元素

// ❌ 不好：每次调用 xvoArrayGetValue 都会触发引用检查
for (int i = 0; i < 10000; i++) {
    int64 val = xvoGetInt(xvoArrayGetValue(arr, i));
    printf("arr[%d] = %lld\n", i, val);
}
```

**解决方案：使用缓存访问**
```c
// ✅ 好方式：使用指针数组（array_point.h）
xparray ptr_arr = xrtPtrArrayCreate(10, 10);
for (int i = 0; i < 10000; i++) {
    xvoArraySetValue(arr, i, xvoCreateInt(i));
}

// 获取指针数组
xparray ptr_array = xvoGetArray(arr);
for (int i = 0; i < 10000; i++) {
    xvalue val = xvoIntArrGetValue(ptr_arr, i);
    int64 ival = xvoGetInt(val);
    printf("arr[%d] = %lld\n", i, ival);
}
```

**性能提升**：200-300%

### 问题23：深拷贝很慢

**问题场景**：
```c
xvalue data = xvoCreateTable();
xvoTableSetText(data, "name", 0, "XRT", 0, FALSE);
xvoTableSetInt(data, "version", 0, 1);
xvoTableSetInt(data, "features", 0, "xrt", 0, TRUE);
xvoTableSetInt(data, "items", 0, NULL, 0);  // 数组

xvalue copy = xvoDeepCopy(data);  // 深拷贝整个树结构
xvoUnref(data);
xvoUnref(copy);
```

**原因**：
- Dict/List 使用 AVL 树，深拷贝会遍历整个树
- 每个节点都需要分配新内存
- 引用计数操作频繁

**解决方案**：
```c
// ✅ 好方式：浅拷贝 + 设置父级
xvalue parent_table = xvoCreateTable();
xvoTableSetParent(data, parent_table);

for (int i = 0; i < 10; i++) {
    str name = xvoTableGetText(data, "items", 0);
    xvalue item = xvoCreateInt(i + 1);
    xvoTableSetInt(parent_table, name, 0, item);
}
xvoUnref(parent_table);
xvoUnref(data);
```

**性能提升**：80-90%

---

## 网络通信问题

### 问题24：HTTP 请求超时

**问题场景**：
```c
xvalue response = xrtHttpGet("https://api.example.com/data", NULL, NULL);
// 没有超时，会无限等待
```

**解决方案**：设置超时时间**

```c
xvalue options = xvoCreateTable();
xrtTableSetText(options, "timeout", 0, "30", 0, FALSE);
xrtTableSetInt(options, "connect", 0, 30);  // 30秒超时
xrtTableSetInt(options, "read", 0, 30);  // 30秒读取超时

xvalue response = xrtHttpGet("https://api.example.com/data", NULL, options);
```

### 问题25：TLS 握手失败

**问题场景**：
```c
xvalue tls_options = xvoCreateTable();
xrtTableSetText(tls_options, "host", 0, "example.com", 0, FALSE);
xrtTableSetInt(tls_options, "port", 0, 443, 0, 1);  // HTTPS 端口
xrtTableSetInt(tls_options, "verify", 0, TRUE);  // 验证证书

xvalue response = xrtHttpsGet("https://example.com/api", tls_options, NULL, NULL);
// TLS 握手失败，没有详细的错误信息
```

**解决方案：获取详细错误信息**
```c
void tls_error_callback(str msg) {
    printf("TLS Error: %s\n", msg);
}

xrtCore->OnError = tls_error_callback;
```

---

## 其他问题

### 问题26：程序崩溃

**现象**：运行到某一行就崩溃，没有错误信息

**可能原因**：
1. 空指针解引用
2. 栈溢出
3. 内存越界
4. 类型不匹配
5. 引用计数错误

**解决方案：使用 Valgrind 检测**

```bash
# 编译时加入调试符号
gcc -g -O0 -g xrt_log.c test.c -o test.exe

# 使用 Valgrind 检测内存泄漏
valgrind --leak-check=full ./test.exe
```

### 问题27：性能不达标

**现象**：理论性能应该很快，但实际很慢

**优化检查清单**：
```c
// 1. 是否使用了正确的容器类型
// 2. 避免了不必要的拷贝
// 3. 是否使用了预分配
// 4. 是否使用了高效算法
// 5. 是否启用了优化编译
```

### 问题28：编译后程序变大

**现象**：加了调试符号后，exe 文件很大

**解决方案**：发布版本去掉调试符号

```batch
# 调试版本（带调试符号）
gcc -g -O3 xrt_log.c test.c -o test_d.exe

# 发布版本（优化+stripped）
gcc -O3 -s -DNDEBUG xrt_log.c test.c -o test.exe

# 去除调试符号
strip test.exe
```

---

## 调试技巧

### 使用断点调试

**GDB 使用**：
```bash
gcc -g -o test.exe xrt_log.c test.c
gdb ./test.exe
(gdb) b main
(gdb) run
(gdb) cont
```

**VS Code 使用**：
```c
// 在代码行号左侧点击，设置断点
F5 - 启动调试
```

### 打印调试

```c
// 添加打印语句跟踪执行流程
printf("Step 1\n");
printf("Step 2\n");
// ...
```

### 内存使用分析

```bash
# 查看进程内存使用
tasklist | grep <程序名>

# 使用 Valgrind 检测内存泄漏
valgrind --leak-check=full ./program
```

---

## 相关链接

- [README.md](README.md) - 项目说明
- [api-value.md](docs/api-value.md) - Value 类型详细文档
- [api-json.md](docs/api-json.md) - JSON 处理文档
- [docs/api-string.md](docs/api-string.md) - 字符串操作文档
- [docs/api-file.md](docs/api-file.md) - 文件操作文档
- [docs/api-base.md](docs/api-base.md) - 基础函数文档

---

**XRT FAQ 文档版本：1.0** | **最后更新：2025-01**
