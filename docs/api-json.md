# JSON 处理库

> JSON 解析、生成和操作

[返回索引](README.md)

---

## 📑 目录

- [JSON解析](#json解析)
- [JSON生成](#json生成)
- [JSON操作](#json操作)

---

## JSON解析

### xjsonParse

解析JSON字符串

**函数原型：**
```c
XXAPI xvalue xjsonParse(str sJson, size_t iSize);
```

**参数：**
- `sJson` - JSON字符串
- `iSize` - 长度（0自动）

**返回值：**
- 成功：xvalue对象
- 失败：`NULL`（错误信息在 `xCore.LastError`）

**释放：** 🔄 使用 `xvoUnref` 释放

**支持的JSON类型：**
- `null` → `XVT_NULL`
- `true/false` → `XVT_BOOL`
- 数字 → `XVT_INT` 或 `XVT_FLOAT`
- 字符串 → `XVT_TEXT`
- 数组 → `XVT_ARRAY`
- 对象 → `XVT_TABLE`

**示例：**
```c
str json = "{\"name\":\"Tom\",\"age\":25,\"active\":true}";
xvalue obj = xjsonParse(json, 0);

if (obj) {
    xvalue name = xvoTableGet(obj, "name");
    xvalue age = xvoTableGet(obj, "age");
    xvalue active = xvoTableGet(obj, "active");
    
    printf("Name: %s\n", xvoGetText(name));
    printf("Age: %lld\n", xvoGetInt(age));
    printf("Active: %s\n", xvoGetBool(active) ? "true" : "false");
    
    xvoUnref(obj);
} else {
    printf("Parse error: %s\n", xCore.LastError);
}
```

---

### 复杂JSON示例

```c
str json = 
"{"
"  \"users\": ["
"    {\"id\": 1, \"name\": \"Alice\"},"
"    {\"id\": 2, \"name\": \"Bob\"}"
"  ],"
"  \"count\": 2"
"}";

xvalue root = xjsonParse(json, 0);
if (root) {
    xvalue users = xvoTableGet(root, "users");
    uint count = xvoArrayCount(users);
    
    for (uint i = 0; i < count; i++) {
        xvalue user = xvoArrayGet(users, i);
        xvalue id = xvoTableGet(user, "id");
        xvalue name = xvoTableGet(user, "name");
        
        printf("User %lld: %s\n", 
            xvoGetInt(id), 
            xvoGetText(name)
        );
    }
    
    xvoUnref(root);
}
```

---

## JSON生成

### xjsonStringify

生成JSON字符串

**函数原型：**
```c
XXAPI str xjsonStringify(xvalue pVal, bool bFormat);
```

**参数：**
- `pVal` - xvalue对象
- `bFormat` - 是否格式化
  - `TRUE` - 带缩进和换行
  - `FALSE` - 紧凑格式

**返回值：**
- JSON字符串

**内存释放：** ✅ 需要 `xrtFree` 释放

**示例：**
```c
// 创建对象
xvalue obj = xvoCreateTable();
xvoTableSet(obj, "name", xvoCreateText("Tom", 0, FALSE));
xvoTableSet(obj, "age", xvoCreateInt(25));
xvoTableSet(obj, "active", xvoCreateBool(TRUE));

// 紧凑格式
str compact = xjsonStringify(obj, FALSE);
printf("%s\n", compact);
// {"name":"Tom","age":25,"active":true}
xrtFree(compact);

// 格式化
str formatted = xjsonStringify(obj, TRUE);
printf("%s\n", formatted);
// {
//   "name": "Tom",
//   "age": 25,
//   "active": true
// }
xrtFree(formatted);

xvoUnref(obj);
```

---

### 复杂结构生成

```c
// 创建用户数组
xvalue users = xvoCreateArray();

xvalue user1 = xvoCreateTable();
xvoTableSet(user1, "id", xvoCreateInt(1));
xvoTableSet(user1, "name", xvoCreateText("Alice", 0, FALSE));
xvoArrayAdd(users, user1);

xvalue user2 = xvoCreateTable();
xvoTableSet(user2, "id", xvoCreateInt(2));
xvoTableSet(user2, "name", xvoCreateText("Bob", 0, FALSE));
xvoArrayAdd(users, user2);

// 创建根对象
xvalue root = xvoCreateTable();
xvoTableSet(root, "users", users);
xvoTableSet(root, "count", xvoCreateInt(2));

// 生成JSON
str json = xjsonStringify(root, TRUE);
printf("%s\n", json);
xrtFree(json);

xvoUnref(root);
```

**输出：**
```json
{
  "users": [
    {
      "id": 1,
      "name": "Alice"
    },
    {
      "id": 2,
      "name": "Bob"
    }
  ],
  "count": 2
}
```

---

## JSON操作

### xjsonGet

获取JSON路径值

**函数原型：**
```c
XXAPI xvalue xjsonGet(xvalue pVal, str sPath);
```

**参数：**
- `pVal` - JSON对象
- `sPath` - 路径（用 `.` 分隔）

**返回值：**
- 找到：返回xvalue
- 未找到：返回 `NULL`

**路径语法：**
- `key` - 获取对象的key
- `[index]` - 获取数组的index
- `key.subkey` - 嵌套对象
- `key[0]` - 对象中的数组
- `[0].key` - 数组中的对象

**示例：**
```c
str json = 
"{\"user\":{\"name\":\"Tom\",\"tags\":[\"admin\",\"user\"]}}";

xvalue root = xjsonParse(json, 0);

// 获取嵌套值
xvalue name = xjsonGet(root, "user.name");
printf("Name: %s\n", xvoGetText(name));  // "Tom"

// 获取数组元素
xvalue tag0 = xjsonGet(root, "user.tags[0]");
printf("Tag: %s\n", xvoGetText(tag0));  // "admin"

xvoUnref(root);
```

---

### xjsonSet

设置JSON路径值

**函数原型：**
```c
XXAPI bool xjsonSet(xvalue pVal, str sPath, xvalue pItem);
```

**参数：**
- `pVal` - JSON对象
- `sPath` - 路径
- `pItem` - 新值

**返回值：**
- `TRUE` - 成功
- `FALSE` - 失败

**示例：**
```c
xvalue root = xvoCreateTable();

// 设置简单值
xjsonSet(root, "name", xvoCreateText("Alice", 0, FALSE));
xjsonSet(root, "age", xvoCreateInt(30));

// 设置嵌套值（自动创建中间对象）
xjsonSet(root, "address.city", xvoCreateText("Beijing", 0, FALSE));
xjsonSet(root, "address.zip", xvoCreateText("100000", 0, FALSE));

str json = xjsonStringify(root, TRUE);
printf("%s\n", json);
xrtFree(json);
xvoUnref(root);
```

**输出：**
```json
{
  "name": "Alice",
  "age": 30,
  "address": {
    "city": "Beijing",
    "zip": "100000"
  }
}
```

---

## 使用场景

### 1. 配置文件

```c
// 读取配置
xvalue LoadConfig(str filename) {
    str content = xrtFileReadAll(filename, XRT_CP_UTF8);
    if (!content) return NULL;
    
    xvalue config = xjsonParse(content, 0);
    xrtFree(content);
    return config;
}

// 保存配置
bool SaveConfig(str filename, xvalue config) {
    str json = xjsonStringify(config, TRUE);
    bool ok = xrtFileWriteAll(filename, json, 0, XRT_CP_UTF8);
    xrtFree(json);
    return ok;
}

// 使用
xvalue config = LoadConfig("config.json");
if (config) {
    str host = xvoGetText(xjsonGet(config, "server.host"));
    int64 port = xvoGetInt(xjsonGet(config, "server.port"));
    
    printf("Server: %s:%lld\n", host, port);
    
    xvoUnref(config);
}
```

---

### 2. API响应处理

```c
void HandleAPIResponse(str json_response) {
    xvalue resp = xjsonParse(json_response, 0);
    if (!resp) {
        printf("Invalid JSON\n");
        return;
    }
    
    // 检查状态码
    xvalue code = xjsonGet(resp, "code");
    if (xvoGetInt(code) != 200) {
        str msg = xvoGetText(xjsonGet(resp, "message"));
        printf("Error: %s\n", msg);
        xvoUnref(resp);
        return;
    }
    
    // 处理数据
    xvalue data = xjsonGet(resp, "data");
    if (xvoIsArray(data)) {
        uint count = xvoArrayCount(data);
        for (uint i = 0; i < count; i++) {
            xvalue item = xvoArrayGet(data, i);
            ProcessItem(item);
        }
    }
    
    xvoUnref(resp);
}
```

---

### 3. 数据序列化

```c
typedef struct {
    int id;
    str name;
    bool active;
} User;

// 对象转JSON
xvalue UserToJSON(User* user) {
    xvalue obj = xvoCreateTable();
    xvoTableSet(obj, "id", xvoCreateInt(user->id));
    xvoTableSet(obj, "name", xvoCreateText(user->name, 0, FALSE));
    xvoTableSet(obj, "active", xvoCreateBool(user->active));
    return obj;
}

// JSON转对象
User* JSONToUser(xvalue json) {
    User* user = xrtMalloc(sizeof(User));
    user->id = xvoGetInt(xvoTableGet(json, "id"));
    user->name = xrtCopyStr(xvoGetText(xvoTableGet(json, "name")), 0);
    user->active = xvoGetBool(xvoTableGet(json, "active"));
    return user;
}

// 使用
User user = {1, "Tom", TRUE};
xvalue json = UserToJSON(&user);
str text = xjsonStringify(json, FALSE);

printf("JSON: %s\n", text);

xrtFree(text);
xvoUnref(json);
```

---

### 4. 数据验证

```c
bool ValidateJSON(xvalue json, str schema) {
    // 简化版JSON Schema验证
    
    // 检查必需字段
    if (!xjsonGet(json, "name")) {
        xrtSetError("Missing 'name' field", FALSE);
        return false;
    }
    
    // 检查类型
    xvalue age = xjsonGet(json, "age");
    if (age && !xvoIsInt(age)) {
        xrtSetError("'age' must be integer", FALSE);
        return false;
    }
    
    // 检查范围
    if (age && xvoGetInt(age) < 0) {
        xrtSetError("'age' must be positive", FALSE);
        return false;
    }
    
    return true;
}
```

---

## 性能优化

### 1. 解析大文件

```c
// 对于大JSON文件，考虑流式解析
xvalue ParseLargeJSON(str filename) {
    str content = xrtFileReadAll(filename, XRT_CP_UTF8);
    if (!content) return NULL;
    
    xvalue root = xjsonParse(content, 0);
    xrtFree(content);  // 立即释放文件内容
    
    return root;
}
```

---

### 2. 缓存常用路径

```c
typedef struct {
    xvalue root;
    xvalue user_name;
    xvalue user_age;
} CachedJSON;

CachedJSON* CreateCache(xvalue root) {
    CachedJSON* cache = xrtMalloc(sizeof(CachedJSON));
    cache->root = xvoRef(root);
    cache->user_name = xjsonGet(root, "user.name");
    cache->user_age = xjsonGet(root, "user.age");
    return cache;
}

void FreeCache(CachedJSON* cache) {
    xvoUnref(cache->root);
    xrtFree(cache);
}
```

---

## 最佳实践

### 1. 错误处理

```c
xvalue SafeParse(str json) {
    xvalue root = xjsonParse(json, 0);
    if (!root) {
        fprintf(stderr, "JSON Parse Error: %s\n", xCore.LastError);
        return NULL;
    }
    return root;
}
```

---

### 2. 资源管理

```c
// ✅ 正确
xvalue json = xjsonParse(text, 0);
if (json) {
    ProcessJSON(json);
    xvoUnref(json);
}

// ❌ 内存泄漏
xvalue json = xjsonParse(text, 0);
ProcessJSON(json);
// 忘记 xvoUnref
```

---

### 3. 类型安全

```c
// ✅ 安全：检查类型
xvalue val = xjsonGet(root, "count");
if (val && xvoIsInt(val)) {
    int64 count = xvoGetInt(val);
    printf("Count: %lld\n", count);
}

// ❌ 危险：不检查类型
int64 count = xvoGetInt(xjsonGet(root, "count"));
```

---

## 常见错误

### 1. JSON格式错误

```c
// ❌ 错误的JSON
str bad_json = "{name:Tom}";  // 键没有引号
xvalue v = xjsonParse(bad_json, 0);  // 返回NULL

// ✅ 正确的JSON
str good_json = "{\"name\":\"Tom\"}";
xvalue v = xjsonParse(good_json, 0);
```

---

### 2. 路径错误

```c
xvalue root = xjsonParse("{\"user\":{\"name\":\"Tom\"}}", 0);

// ❌ 错误路径
xvalue wrong = xjsonGet(root, "users.name");  // NULL

// ✅ 正确路径
xvalue right = xjsonGet(root, "user.name");   // "Tom"

xvoUnref(root);
```

---

## JSON规范支持

### 支持的特性

- ✅ UTF-8 编码
- ✅ 转义字符 (`\"`, `\\`, `\/`, `\b`, `\f`, `\n`, `\r`, `\t`)
- ✅ Unicode转义 (`\uXXXX`)
- ✅ 数字（整数、浮点、科学计数法）
- ✅ 嵌套对象和数组
- ✅ null, true, false

### 不支持的特性

- ❌ 注释（JSON标准不支持注释）
- ❌ 尾随逗号
- ❌ 单引号字符串

---

## 性能特点

### 解析速度

- **中等性能**：适合大多数应用
- **内存效率**：使用 xvalue 自动管理
- **错误恢复**：提供详细错误信息

### 基准参考

```c
// 解析 1KB JSON: ~100μs
// 解析 10KB JSON: ~800μs  
// 解析 100KB JSON: ~8ms
```

---

## 相关文档

- [Value 动态类型系统](api-value.md) - JSON数据模型
- [JNUM 数值转换](api-jnum.md) - 数字解析
- [String 字符串处理](api-string.md) - 字符串操作
- [File 文件操作](api-file.md) - 读写JSON文件
- [主索引](README.md)

---

<div align="center">

[⬆️ 返回顶部](#json-处理库)

</div>
