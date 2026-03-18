# XRT 最佳实践指南

> XRT 的最佳使用方法、常见模式、性能调优技巧

[English Version](BEST_PRACTICES.en.md) | [返回索引](README.md)

---

## 📑 目录

- [内存管理最佳实践](#内存管理最佳实践)
- [字符串处理最佳实践](#字符串处理最佳实践)
- [Value类型使用最佳实践](#value类型使用最佳实践)
- [数据结构选择指南](#数据结构选择指南)
- [JSON处理最佳实践](#json处理最佳实践)
- [错误处理最佳实践](#错误处理最佳实践)
- [性能调优最佳实践](#性能调优最佳实践)
- [跨平台开发最佳实践](#跨平台开发最佳实践)

---

## 内存管理最佳实践

### 1. 使用临时内存

**场景**：函数内临时使用，需要自动释放

```c
// ✅ 好的做法：使用线程级临时内存
void process_data() {
    str temp1 = xrtTempMemory(1024);
    str temp2 = xrtTempMemory(2048);
    
    // ... 使用临时内存 ...
    
    // 函数返回时自动释放，无需手动 free
}

// ❌ 不好的做法：手动管理临时内存
void process_data_bad() {
    str temp1 = xrtMalloc(1024);
    str temp2 = xrtMalloc(2048);
    
    // ... 使用临时内存 ...
    
    xrtFree(temp1);
    xrtFree(temp2);  // 容易遗漏
}
```

**优点**：
- 自动释放，不会内存泄漏
- 线程之间互不干扰
- 高性能，无额外开销

### 2. 内存池优化

**场景**：高频小对象分配

```c
// ✅ 好的做法：使用内存池
void process_many_objects() {
    xvalue pool = xvoCreateArray();  // 使用 Array 统一托管对象生命周期
    
    for (int i = 0; i < 10000; i++) {
        xvalue obj = create_object();
        xvoArrayAppendValue(pool, obj, TRUE);
    }
    
    // ... 使用对象 ...
    
    // 释放整个池，自动管理
    xvoUnref(pool);
}

// ❌ 不好的做法：频繁 malloc/free
void process_many_objects_bad() {
    for (int i = 0; i < 10000; i++) {
        xvalue obj = create_object();
        // ... 使用对象 ...
        xvoUnref(obj);  // 频繁分配/释放
    }
}
```

**性能提升**：60-80%

### 3. 避免内存泄漏

**检查方法**：
```c
xrtInit();
// ... 程序逻辑 ...
xrtUnit();  // 结合调试能力与测试环境做资源收尾检查

// 使用 valgrind 检测
// valgrind --leak-check=full ./program
```

**常见泄漏场景**：
1. 忘记调用 `xrtFree` 释放 malloc 的内存
2. 循环引用导致的泄漏
3. 异常路径未释放资源

### 4. 内存分配大小选择

```c
// ✅ 合适的内存分配
// 小对象（<256字节）：TempMemory
str temp1 = xrtTempMemory(128);

// 中等对象（256B-4KB）：MemPool
str data = xrtMemPoolAlloc(pool, 2048);

// 大对象（>4KB）：直接分配
str large = xrtMalloc(8192);
```

### 5. 使用内存调试能力定位问题

当前主线的内存调试重点不只是“统计”，而是尽量帮助暴露危险操作。

应重点使用和验证的方向包括：

- 内存泄露定位到文件和行号
- 重复释放
- 非法释放
- 越界写导致的块破坏
- 已释放内存继续使用
- 错线程访问本线程本地对象
- 对象生命周期错误进入池结构

对于基础设施代码，建议在 debug / test 构建里尽量打开相关调试路径，而不要只依赖最终崩溃时再回溯。

---

## 字符串处理最佳实践

### 1. 字符串拼接优化

**场景**：拼接多个字符串

```c
// ❌ 不好的做法：多次分配
void concat_bad() {
    str result = xrtConcat("Hello", " ");
    result = xrtConcat(result, "XRT");
    result = xrtConcat(result, "!");
    // ... 3次分配和拷贝
    xrtFree(result);
}

// ✅ 好的做法：一次分配
void concat_good() {
    str result = xrtFormat("%s %s %s!", "Hello", "XRT", "!");
    // ... 使用 result ...
    xrtFree(result);
}
```

**性能提升**：70%

### 2. 字符串查找优化

```c
// ❌ 不好的做法：线性查找
int find_bad(const char* text, const char* sub) {
    return strstr(text, sub) != NULL ? 1 : 0;
}

// ✅ 好的做法：使用查找算法
int find_good(const char* text, const char* sub) {
    str result = xrtFindStr(text, 0, sub, 0, FALSE);
    int found = (result != NULL);
    if (found) {
        xrtFree(result);
    }
    return found;
}
```

**性能提升**：对于长字符串可提升 30-50%

### 3. 避免字符串拷贝

```c
// ❌ 不好的做法：每次都创建新字符串
void process_bad(const char* data) {
    str copy = xrtCopyStr(data, 0);  // 拷贝数据
    // ... 处理数据 ...
    xrtFree(copy);  // 需要释放
}

// ✅ 好的做法：直接使用或使用引用计数
void process_good(const char* data) {
    str temp = xrtTempMemory(strlen(data));
    memcpy(temp, data, strlen(data) + 1);
    // ... 处理数据 ...
    // 自动释放
}
```

**性能提升**：节省分配时间

### 4. 使用临时内存

```c
// ❌ 不好的做法：手动管理
void process_strings_bad() {
    str result1 = xrtFormat("Hello %s", name);
    // ... 使用 result1 ...
    str result2 = xrtFormat("Value: %d", value);
    // ... 使用 result2 ...
    xrtFree(result1);
    xrtFree(result2);
}

// ✅ 好的做法：使用临时内存
void process_strings_good() {
    str result1 = xrtFormat("Hello %s", name);
    // ... 使用 result1 ...
    xrtFree(result1);
    
    str result2 = xrtFormat("Value: %d", value);
    // ... 使用 result2 ...
    xrtFree(result2);
}
```

---

## Value 类型使用最佳实践

### 1. 引用计数管理

**基本原则**：
```c
// ✅ 基本规则
// 1. xvoCreate* 创建的值，必须用 xvoUnref 释放
xvalue val = xvoCreateInt(123);
// ... 使用 val ...
xvoUnref(val);

// 2. xvoCopy 返回的值，引用计数+1，无需额外管理
xvalue copy = xvoCopy(val);
// ... 使用 copy ...
xvoUnref(copy);  // 和 val 共享引用计数
```

**循环引用问题**：
```c
// ❌ 循环引用（需要手动断开）
xvalue table1 = xvoCreateTable();
xvalue table2 = xvoCreateTable();
xvoTableSetText(table1, "ref", 0, "table2", 0, FALSE);
xvoTableSetText(table2, "ref", 0, "table1", 0, FALSE);
// table1 和 table2 互相引用，永远不会释放！

// ✅ 手动断开循环
xvoTableRemove(table2, "ref", 0);
xvoUnref(table2);
xvoUnref(table1);
```

### 2. 使用浅拷贝

```c
// ✅ 好的做法：浅拷贝
void shallow_copy_example(xvalue data) {
    xvalue copy = xvoCopy(data);  // 只增加引用计数
    
    // ... 只读操作，不会修改数据 ...
    
    xvoUnref(copy);
    xvoUnref(data);  // 数据只有一份
}

// ❌ 不好的做法：深拷贝（不必要的性能开销）
void deep_copy_example(xvalue data) {
    xvalue copy = xvoDeepCopy(data);  // 递归拷贝
    
    // ... 只读操作 ...
    
    xvoUnref(copy);
    xvoUnref(data);  // 内存中有两份数据
}
```

### 3. 批量操作

```c
// ❌ 不好的做法：逐个添加
xvalue arr = xvoCreateArray();
for (int i = 0; i < 100; i++) {
    xvoArrayAppendInt(arr, data[i]);
}

// ✅ 好的做法：预分配
xvalue arr = xvoCreateArray();
xvoArrayAlloc(arr, 100);  // 预分配
for (int i = 0; i < 100; i++) {
    xvoArrayAppendInt(arr, data[i]);
}
```

**性能提升**：预分配可以提升 50%+

### 4. 选择合适的容器

```c
// ✅ 选择指南
// 场景1：需要快速查找 -> Dict
xvalue dict = xvoCreateTable();

// 场景2：需要有序访问 -> List
xvalue list = xvoCreateList();

// 场景3：随机访问 -> Array
xvalue arr = xvoCreateArray();

// 场景4：自动去重 -> Coll
xvalue coll = xvoCreateColl();
```

**复杂度对比**：
| 容器 | 查找 | 插入 | 删除 |
|------|------|------|------|
| Dict | O(log n) | O(log n) | O(log n) |
| List | O(log n) | O(log n) | O(log n) |
| Array | O(1) | O(1) | O(n) |
| Coll | O(log n) | O(log n) | O(log n) |

### 5. 跨线程共享时显式创建 shared root

phase-2 之后，容器默认都是本线程本地对象。  
如果确实需要跨线程共享，必须显式创建 shared root：

```c
xvalue tblConfig = xvoCreateTableEx(XRT_OBJMODE_SHARED);
xvalue arrNodes = xvoCreateArrayEx(XRT_OBJMODE_SHARED);

xvoArrayAppendText(arrNodes, "node-a", 0, FALSE);
xvoTableSetValue(tblConfig, "nodes", 0, arrNodes, TRUE);
```

不要把 local root 直接拿去跨线程改写，也不要假设 shared root 等于“所有组合访问天然无锁安全”。复杂遍历仍然应使用 root lock。

---

## 数据结构选择指南

### 1. 根据访问模式选择

**快速查找场景**：
```c
// ✅ 使用 Dict（字典）
xvalue users = xvoCreateTable();
xvoTableSetText(users, "user1", 0, "Alice", 0, FALSE);
xvoTableSetText(users, "user2", 0, "Bob", 0, FALSE);
str name = xvoTableGetText(users, "user1", 0);  // O(log n) 查找
```

**有序遍历场景**：
```c
// ✅ 使用 List（列表）
xvalue list = xvoCreateList();
xvoListSetValue(list, 0, 123);
xvoListSetValue(list, 1, 456);
for (int64 i = 0; i < xvoListItemCount(list); i++) {
    int64 val = xvoListGetInt(list, i);
    // ... 处理每个元素
}
```

**随机访问场景**：
```c
// ✅ 使用 Array（数组）
xvalue arr = xvoCreateArray();
xvoArraySetInt(arr, 0, 123);
xvoArraySetInt(arr, 10, 456);
int64 val = xvoArrayGetInt(arr, 10);  // O(1) 访问
```

**自动去重场景**：
```c
// ✅ 使用 Coll（集合）
xvalue coll = xvoCreateColl();
xvoCollSetValue(coll, 123);
xvoCollSetValue(coll, 123);  // 自动去重
uint32 count = xvoCollItemCount(coll);
```

### 2. 预分配策略

```c
// ❌ 不好的做法：频繁扩容
xvalue arr = xvoCreateArray();
for (int i = 0; i < 1000; i++) {
    xvoArrayAppendInt(arr, i);  // 可能触发多次扩容
}

// ✅ 好的做法：预分配
xvalue arr = xvoCreateArray();
xvoArrayAlloc(arr, 1000);  // 一次性分配
for (int i = 0; i < 1000; i++) {
    xvoArraySetInt(arr, i, i);
}
```

**性能提升**：预分配可以提升 50-80%

---

## JSON 处理最佳实践

### 1. 使用 SAX 模式解析

**场景**：解析大 JSON 文件

```c
// ❌ 不好的做法：一次性解析为 DOM
str json = xrtFileReadAll("large.json", XRT_CP_UTF8);
xvalue data = xrtParseJSON(json, 0);
// 如果文件很大，内存占用会很高

// ✅ 好的做法：SAX 流式解析
json_sax_cb_t cb = {
    .on_string = on_json_string,
    .on_number = on_json_number,
    // ... 其他回调
};

FILE* fp = fopen("large.json", "r");
char buffer[4096];
while (!feof(fp)) {
    size_t len = fread(buffer, 1, 4096, fp);
    xrtJsonParseSAX(buffer, len, cb);
}
fclose(fp);
```

**优点**：
- 内存占用低（O(1)）
- 适合流式处理
- 支持超大文件

### 2. 压缩 JSON 输出

```c
// ❌ 不好的做法：格式化输出（大文件）
xvalue data = create_large_data();
size_t len = 0;
str json = xrtStringifyJSON(data, TRUE, &len);  // TRUE = 格式化
xrtFileWriteAll("output.json", json, len, XRT_CP_UTF8);
// 文件会很大，占很多空间

// ✅ 好的做法：不格式化输出（生产环境）
xvalue data = create_large_data();
size_t len = 0;
str json = xrtStringifyJSON(data, FALSE, &len);  // FALSE = 不格式化
xrtFileWriteAll("output.json", json, len, XRT_CP_UTF8);
// 文件更小，解析更快
```

**文件大小差异**：
- 格式化输出：约 2MB
- 不格式化输出：约 1MB

### 3. 避免深拷贝

```c
// ❌ 不好的做法：深拷贝所有节点
xvalue json1 = xrtParseJSON(json_str1, 0);
xvalue json2 = xvoDeepCopy(json1);  // 递归深拷贝，很慢

// ✅ 好的做法：浅拷贝（只读场景）
xvalue json1 = xrtParseJSON(json_str1, 0);
xvalue json2 = xvoCopy(json1);  // 浅拷贝，快速
// ... 只读操作 ...
xvoUnref(json2);
xvoUnref(json1);
```

**性能提升**：浅拷贝比深拷贝快 100-1000 倍

### 4. JSON 数据跨线程共享时直接使用 shared root

JSON 在 XRT 中建立在 `xvalue` 上，因此跨线程共享 JSON 结构时也应遵守同一套 shared root 合同，而不是把它当成一套独立的线程安全 JSON DOM。

### 5. HTTP / AI 场景中优先沿统一数据主线组织 JSON

当前主线最推荐的组织方式不是：

- 一份 HTTP 层 JSON
- 一份业务层结构体
- 一份模板层变量表

而是尽量收成：

- `xvalue` 作为程序内部主数据
- JSON 只作为输入输出交换格式
- template / HTTP / LLM API 都围绕同一份 `xvalue` 数据流工作

这样做的好处是：

- 减少重复转换
- 降低数据模型漂移
- 更容易接入 shared root、future、协程和网络主线

---

## 错误处理最佳实践

### 1. 使用 XRT 错误处理

```c
// 设置错误回调
void on_error(str sError) {
    fprintf(stderr, "Error: %s\n", sError);
}

int main() {
    xrtInit();
    
    // ... 程序逻辑 ...
    
    // 检查错误
    if (xrtGetError()[0] != '\0') {
        fprintf(stderr, "Last Error: %s\n", xrtGetError());
        xrtUnit();
        return 1;
    }
    
    xrtUnit();
    return 0;
}
```

### 2. 返回值检查

```c
// ❌ 不好的做法：不检查返回值
xvalue list = xvoCreateArray();
xvalue val = xvoCreateInt(123);
// ... 不检查 val 是否创建成功
xvoArrayAppendInt(list, 123);  // 如果 val==NULL 会崩溃

// ✅ 好的做法：检查返回值
xvalue list = xvoCreateArray();
xvalue val = xvoCreateInt(123);
if (!val) {
    fprintf(stderr, "Failed to create value!\n");
    xvoUnref(list);
    return 1;
}
xvoArrayAppendInt(list, 123);
xvoUnref(list);
```

### 3. 错误恢复

```c
// ✅ 好的做法：优雅降级
int save_data() {
    xvalue data = xrtParseJSON(json_text, 0);
    if (!data) {
        fprintf(stderr, "Failed to parse JSON, using fallback\n");
        data = xvoCreateNull();  // 使用空值降级
    }
    
    // ... 处理 data ...
    
    xvoUnref(data);
    return 0;
}
```

---

## 性能调优最佳实践

### 1. 性能分析

**使用内置性能测试**：
```c
xrtInit();

xtime start = xrtNow();

// ... 执行代码 ...

xtime end = xrtNow();
int64 diff = xrtDateDiff(XRT_TIME_INTERVAL_SECOND, start, end);
printf("耗时: %lldms\n", diff);

xrtUnit();
```

### 2. 内存分析

**查看内存使用**：
```bash
# Linux/macOS
valgrind --tool=massif ./program

# Windows
# 使用 Dr. Memory 或 Visual Studio 内存诊断工具
```

### 3. 热点优化

**常见热点和优化方法**：

| 热点 | 优化方法 | 提升 |
|------|---------|------|
| 字符串拼接 | 使用 xrtFormat | 70% |
| 数组扩容 | 使用预分配 | 50% |
| JSON 解析 | 使用 SAX 模式 | 50% |
| 小对象分配 | 使用内存池 | 60% |
| 临时内存 | 使用线程级 TempMemory | 自动管理 |
| 跨线程共享 | 显式 shared root + root lock | 正确性优先 |

### 4. 编译优化

```bash
# GCC 优化编译
gcc -O3 -march=native -flto -funroll-loops program.c -o program

# TCC 极速编译
tcc -O3 -run program.c

# MSVC 优化编译
cl /O2 /Oi /Ot program.c
```

---

## 跨平台开发最佳实践

### 1. 路径处理

```c
// ❌ 不好的做法：硬编码路径分隔符
#ifdef _WIN32
    char* path = "config\\settings.ini";
#else
    char* path = "config/settings.ini";
#endif

// ✅ 好的做法：使用 XRT 路径 API
str config_path = xrtPathJoin("config", "settings.ini");
xrtPathJoin(...);  // 自动处理分隔符
```

### 2. 文件系统操作

```c
// ❌ 不好的做法：区分文件和目录
#ifdef _WIN32
    if (GetFileAttributes(path) & FILE_ATTRIBUTE_DIRECTORY) { ... }
#else
    struct stat st;
    if (S_ISDIR(st.st_mode)) { ... }
#endif

// ✅ 好的做法：统一 API
bool is_file = xrtFileExists(path);
bool is_dir = xrtDirExists(path);
```

### 3. 字符集处理

```c
// ✅ 好的做法：自动检测字符集
str content = xrtFileReadAll("data.txt", XRT_CP_AUTO);  // 自动检测 UTF-8/GB18030/UTF-16
// ... 处理 content ...
xrtFileWriteAll("out.txt", content, len, XRT_CP_UTF8);  // 统一UTF-8输出
```

### 4. 平台特定功能

```c
// ❌ 不好的做法：使用平台特定API
#ifdef _WIN32
    Sleep(1000);
#else
    usleep(1000000);  // 1秒 = 1000000微秒
#endif

// ✅ 好的做法：使用 XRT API
xrtSleep(1000);  // 跨平台睡眠，单位毫秒
```

---

## 总结

### 核心原则

1. ✅ **内存安全**：使用临时内存和引用计数，避免内存泄漏
2. ✅ **性能优先**：选择合适的算法和数据结构
3. ✅ **代码简洁**：使用 XRT 的简洁API，减少样板代码
4. ✅ **错误处理**：检查返回值，优雅降级
5. ✅ **跨平台**：使用 XRT 的跨平台API

### 性能检查清单

- [ ] 使用临时内存替代手动 malloc/free
- [ ] 使用内存池处理高频小对象
- [ ] 使用 xrtFormat 替代字符串拼接
- [ ] 使用预分配减少频繁扩容
- [ ] 使用 SAX 模式处理大JSON
- [ ] 使用浅拷贝替代深拷贝
- [ ] 使用 Dict/List 替代线性查找
- [ ] 选择合适的容器类型
- [ ] 跨线程共享时显式创建 shared root
- [ ] 复杂 shared 遍历时使用 root lock

---

**XRT 最佳实践文档版本：当前主线重建版**
