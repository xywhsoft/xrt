# JSON 处理库

> JSON SAX 解析与生成 + Value 系统

[返回索引](README.md)

---

## 📑 目录

- [重要说明](#重要说明)
- [JSON类型系统](#json类型系统)
- [SAX解析](#sax解析)
- [JSON生成](#json生成)
- [推荐方式：使用Value系统](#推荐方式使用value系统)

---

## 重要说明

**xrt.h 中的 JSON 库提供两种使用方式：**

1. **SAX API（底层）：** 流式解析，适合处理大文件或性能关键场景
2. **Value 系统（推荐）：** 提供类似 JSON 的数据结构和简单 API，**强烈推荐日常使用**

---

## JSON类型系统

### JSON类型枚举

```c
typedef enum {
    JSON_NULL = 0,      // null
    JSON_BOOL,          // true, false
    JSON_INT,           // int32
    JSON_HEX,           // uint32 十六进制
    JSON_LINT,          // int64
    JSON_LHEX,          // uint64 十六进制
    JSON_DOUBLE,        // double
    JSON_STRING,        // 字符串
    JSON_ARRAY,         // 数组
    JSON_OBJECT         // 对象
} json_type_t;
```

---

## SAX解析

### xrtJsonParseSAX

SAX风格解析JSON（流式处理）

**函数原型：**
```c
XXAPI int xrtJsonParseSAX(str text, size_t str_len, json_sax_cb_t cb);
```

**参数：**
- `text` - JSON字符串
- `str_len` - 长度
- `cb` - SAX回调函数

**回调函数类型：**
```c
typedef int (*json_sax_cb_t)(json_sax_parser_t *parser);
```

**示例：**
```c
int MyJSONCallback(json_sax_parser_t *parser) {
    json_type_t type = parser->array[parser->index].info.type;
    
    switch (type) {
        case JSON_STRING:
            printf("String: %s\n", parser->value.vstr.str);
            break;
        case JSON_INT:
            printf("Int: %d\n", parser->value.vnum.vint);
            break;
        // ... 处理其他类型
    }
    
    return JSON_SAX_PARSE_CONTINUE;
}

str json = "{\"name\":\"Tom\",\"age\":25}";
xrtJsonParseSAX(json, strlen(json), MyJSONCallback);
```

---

## JSON生成

### xrtJsonPrintStart / xrtJsonPrintValue / xrtJsonPrintFinish

SAX风格JSON生成

**函数原型：**
```c
XXAPI json_sax_print_hd xrtJsonPrintStart(json_print_choice_t *choice);
XXAPI int xrtJsonPrintValue(json_sax_print_hd handle, json_type_t type, json_string_t *jkey, const void *value);
XXAPI char* xrtJsonPrintFinish(json_sax_print_hd handle, size_t *length, json_print_ptr_t *ptr);
```

**示例：**
```c
json_print_choice_t choice = {0};
json_sax_print_hd handle = xrtJsonPrintStart(&choice);

// 生成 {"name":"Tom","age":25}
json_string_t key_name = {"name", {0, 0, 0, 0, 4}};
json_string_t key_age = {"age", {0, 0, 0, 0, 3}};

xrtJsonPrintValue(handle, JSON_OBJECT, NULL, (void*)JSON_SAX_START);
xrtJsonPrintValue(handle, JSON_STRING, &key_name, "Tom");
int age = 25;
xrtJsonPrintValue(handle, JSON_INT, &key_age, &age);
xrtJsonPrintValue(handle, JSON_OBJECT, NULL, (void*)JSON_SAX_FINISH);

size_t len;
char* result = xrtJsonPrintFinish(handle, &len, NULL);
printf("%s\n", result);
xrtFree(result);
```

---

## 推荐方式：使用Value系统

**强烈推荐使用 [Value 动态类型系统](api-value.md) 来处理 JSON 数据！**

Value 系统提供了类似 JSON 的数据结构和简单易用的 API：

### 创建 JSON 结构

```c
// 创建对象
xvalue user = xvoCreateTable();
xvoTableSetText(user, "name", 4, "Tom", 0, FALSE);
xvoTableSetInt(user, "age", 3, 25);
xvoTableSetBool(user, "active", 6, TRUE);

// 创建数组
xvalue tags = xvoCreateArray();
xvoArrayAddText(tags, "developer", 0, FALSE);
xvoArrayAddText(tags, "golang", 0, FALSE);
xvoTableSetValue(user, "tags", 4, tags, TRUE);

// 访问数据
xvalue name = xvoTableGetValue(user, "name", 4);
printf("Name: %s\n", xvoGetText(name));

xvalue age = xvoTableGetValue(user, "age", 3);
printf("Age: %lld\n", xvoGetInt(age));

xvoUnref(user);
```

### 嵌套结构

```c
// 创建嵌套对象
xvalue config = xvoCreateTable();

xvalue server = xvoCreateTable();
xvoTableSetText(server, "host", 4, "localhost", 0, FALSE);
xvoTableSetInt(server, "port", 4, 8080);
xvoTableSetValue(config, "server", 6, server, TRUE);

xvalue db = xvoCreateTable();
xvoTableSetText(db, "name", 4, "mydb", 0, FALSE);
xvoTableSetValue(config, "database", 8, db, TRUE);

// 访问嵌套数据
xvalue server_obj = xvoTableGetValue(config, "server", 6);
xvalue host = xvoTableGetValue(server_obj, "host", 4);
printf("Host: %s\n", xvoGetText(host));

xvoUnref(config);
```

### 数组操作

```c
xvalue users = xvoCreateArray();

// 添加对象到数组
for (int i = 0; i < 3; i++) {
    xvalue user = xvoCreateTable();
    xvoTableSetInt(user, "id", 2, i);
    xvoArrayAddValue(users, user);
}

// 遍历数组
uint32 count = xvoArrayItemCount(users);
for (uint32 i = 0; i < count; i++) {
    xvalue user = xvoArrayGetValue(users, i);
    xvalue id = xvoTableGetValue(user, "id", 2);
    printf("User ID: %lld\n", xvoGetInt(id));
}

xvoUnref(users);
```

---

## 相关文档

- **[Value 动态类型系统](api-value.md)** - 推荐使用，提供完整的 JSON 数据处理能力
- [JNUM 数值转换](api-jnum.md) - 数字解析
- [主索引](README.md)

---

<div align="center">

[⬆️ 返回顶部](#json-处理库)

</div>
