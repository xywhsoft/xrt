# Value 动态类型系统库

> 15种动态数据类型，引用计数自动管理

[返回索引](README.md)

---

## 📑 目录

- [类型系统](#类型系统)
- [创建Value](#创建value)
- [类型判断](#类型判断)
- [类型转换](#类型转换)
- [引用管理](#引用管理)
- [容器操作](#容器操作)

---

## 类型系统

### Value类型

```c
#define XVT_NULL        0   // 空值
#define XVT_BOOL        1   // 布尔
#define XVT_INT         2   // 整数
#define XVT_FLOAT       3   // 浮点数
#define XVT_TEXT        4   // 字符串
#define XVT_TIME        5   // 时间
#define XVT_POINT       6   // 指针
#define XVT_ARRAY       7   // 数组
#define XVT_LIST        8   // 列表
#define XVT_COLL        9   // 集合(AVLTree)
#define XVT_TABLE       10  // 表(Dict)
#define XVT_STRUCT      11  // 结构体
#define XVT_OBJECT      12  // 对象
#define XVT_CUSTOM      13  // 自定义
```

### Value结构

```c
typedef struct xvalue_struct {
    uint32 Type:4;          // 类型
    uint32 Reserve:1;       // 保留
    uint32 IsStatic:1;      // 是否静态
    uint32 RefCount:26;     // 引用计数
    uint32 Size;            // 大小
    union {
        bool vBool;
        int64 vInt;
        double vFloat;
        str vText;
        xtime vTime;
        ptr vPoint;
        xparray vArray;
        xlist vList;
        xavltree vColl;
        xdict vTable;
        ptr vStruct;
        ptr vObject;
        ptr vCustom;
    };
} xvalue_struct, *xvalue;
```

---

## 创建Value

### xvoCreateNull

创建 null 值

**函数原型：**
```c
XXAPI xvalue xvoCreateNull();
```

**返回值：**
- null类型的 Value

**释放：** 🔄 使用 `xvoUnref` 释放

**示例：**
```c
xvalue v = xvoCreateNull();
xvoUnref(v);
```

---

### xvoCreateBool

创建布尔值

**函数原型：**
```c
XXAPI xvalue xvoCreateBool(bool bVal);
```

**示例：**
```c
xvalue v = xvoCreateBool(TRUE);
bool val = xvoGetBool(v);
xvoUnref(v);
```

---

### xvoCreateInt

创建整数

**函数原型：**
```c
XXAPI xvalue xvoCreateInt(int64 iVal);
```

**示例：**
```c
xvalue v = xvoCreateInt(12345);
int64 val = xvoGetInt(v);
xvoUnref(v);
```

---

### xvoCreateFloat

创建浮点数

**函数原型：**
```c
XXAPI xvalue xvoCreateFloat(double nVal);
```

---

### xvoCreateText

创建字符串

**函数原型：**
```c
XXAPI xvalue xvoCreateText(ptr sVal, uint32 iSize, bool bStatic);
```

**参数：**
- `sVal` - 字符串
- `iSize` - 长度（0自动）
- `bStatic` - 是否静态字符串
  - `TRUE` - 不复制，不释放
  - `FALSE` - 复制字符串

**示例：**
```c
// 动态字符串
xvalue v1 = xvoCreateText("Hello", 0, FALSE);
str s1 = xvoGetText(v1);
xvoUnref(v1);

// 静态字符串
xvalue v2 = xvoCreateText("Static", 0, TRUE);
xvoUnref(v2);
```

---

### xvoCreateTime

创建时间

**函数原型：**
```c
XXAPI xvalue xvoCreateTime(xtime iTime);
```

---

### xvoCreatePoint

创建指针

**函数原型：**
```c
XXAPI xvalue xvoCreatePoint(ptr pVal);
```

---

### xvoCreateArray

创建数组

**函数原型：**
```c
XXAPI xvalue xvoCreateArray();
```

**返回值：**
- 空数组 Value

**示例：**
```c
xvalue arr = xvoCreateArray();
xvalue item1 = xvoCreateInt(1);
xvalue item2 = xvoCreateText("two", 0, FALSE);
xvoArrayAdd(arr, item1);
xvoArrayAdd(arr, item2);
xvoUnref(arr);  // 自动释放所有子元素
```

---

### xvoCreateList

创建列表

**函数原型：**
```c
XXAPI xvalue xvoCreateList();
```

---

### xvoCreateColl

创建集合（AVLTree）

**函数原型：**
```c
XXAPI xvalue xvoCreateColl();
```

---

### xvoCreateTable

创建表（Dict）

**函数原型：**
```c
XXAPI xvalue xvoCreateTable();
```

**示例：**
```c
xvalue table = xvoCreateTable();
xvalue name = xvoCreateText("Tom", 0, FALSE);
xvalue age = xvoCreateInt(25);
xvoTableSet(table, "name", name);
xvoTableSet(table, "age", age);
xvoUnref(table);
```

---

## 类型判断

### xvoIsNull / xvoIsBool / xvoIsInt ...

检查类型

**函数原型：**
```c
XXAPI bool xvoIsNull(xvalue pVal);
XXAPI bool xvoIsBool(xvalue pVal);
XXAPI bool xvoIsInt(xvalue pVal);
XXAPI bool xvoIsFloat(xvalue pVal);
XXAPI bool xvoIsText(xvalue pVal);
XXAPI bool xvoIsTime(xvalue pVal);
XXAPI bool xvoIsPoint(xvalue pVal);
XXAPI bool xvoIsArray(xvalue pVal);
XXAPI bool xvoIsList(xvalue pVal);
XXAPI bool xvoIsColl(xvalue pVal);
XXAPI bool xvoIsTable(xvalue pVal);
```

**示例：**
```c
xvalue v = xvoCreateInt(123);
if (xvoIsInt(v)) {
    printf("It's an integer\n");
}
xvoUnref(v);
```

---

## 类型转换

### xvoGetBool / xvoGetInt / xvoGetFloat ...

获取值

**函数原型：**
```c
XXAPI bool xvoGetBool(xvalue pVal);
XXAPI int64 xvoGetInt(xvalue pVal);
XXAPI double xvoGetFloat(xvalue pVal);
XXAPI str xvoGetText(xvalue pVal);
XXAPI xtime xvoGetTime(xvalue pVal);
XXAPI ptr xvoGetPoint(xvalue pVal);
```

**示例：**
```c
xvalue v = xvoCreateInt(42);
int64 num = xvoGetInt(v);
printf("Number: %lld\n", num);
xvoUnref(v);
```

---

### xvoToInt / xvoToFloat / xvoToText

类型转换

**函数原型：**
```c
XXAPI int64 xvoToInt(xvalue pVal);
XXAPI double xvoToFloat(xvalue pVal);
XXAPI str xvoToText(xvalue pVal);
```

**说明：**
- 自动转换类型
- 例如：字符串 "123" → 整数 123

**示例：**
```c
xvalue v1 = xvoCreateText("123", 0, FALSE);
int64 num = xvoToInt(v1);  // 123
xvoUnref(v1);

xvalue v2 = xvoCreateInt(456);
str text = xvoToText(v2);  // "456"
xrtFree(text);
xvoUnref(v2);
```

---

## 引用管理

### xvoRef

增加引用计数

**函数原型：**
```c
XXAPI xvalue xvoRef(xvalue pVal);
```

**返回值：**
- 返回 pVal

**示例：**
```c
xvalue v = xvoCreateInt(100);
xvalue v2 = xvoRef(v);  // 引用计数+1
xvoUnref(v);
xvoUnref(v2);  // 两次 unref 才真正释放
```

---

### xvoUnref

减少引用计数

**函数原型：**
```c
XXAPI void xvoUnref(xvalue pVal);
```

**说明：**
- 引用计数-1
- 计数为0时自动释放
- 容器类型会递归释放子元素

---

## 容器操作

### 数组操作

```c
// 添加元素
XXAPI void xvoArrayAdd(xvalue pArray, xvalue pItem);

// 获取元素
XXAPI xvalue xvoArrayGet(xvalue pArray, uint iIndex);

// 设置元素
XXAPI void xvoArraySet(xvalue pArray, uint iIndex, xvalue pItem);

// 获取数量
XXAPI uint xvoArrayCount(xvalue pArray);
```

**示例：**
```c
xvalue arr = xvoCreateArray();
xvoArrayAdd(arr, xvoCreateInt(1));
xvoArrayAdd(arr, xvoCreateInt(2));
xvoArrayAdd(arr, xvoCreateInt(3));

uint count = xvoArrayCount(arr);  // 3
xvalue item = xvoArrayGet(arr, 0);
int64 val = xvoGetInt(item);      // 1

xvoUnref(arr);
```

---

### 列表操作

```c
XXAPI void xvoListAdd(xvalue pList, xvalue pItem);
XXAPI xvalue xvoListGet(xvalue pList, uint iIndex);
XXAPI void xvoListSet(xvalue pList, uint iIndex, xvalue pItem);
XXAPI uint xvoListCount(xvalue pList);
```

---

### 表操作

```c
// 设置键值对
XXAPI void xvoTableSet(xvalue pTable, str sKey, xvalue pItem);

// 获取值
XXAPI xvalue xvoTableGet(xvalue pTable, str sKey);

// 移除键
XXAPI void xvoTableRemove(xvalue pTable, str sKey);

// 检查键是否存在
XXAPI bool xvoTableExists(xvalue pTable, str sKey);

// 获取数量
XXAPI uint xvoTableCount(xvalue pTable);
```

**示例：**
```c
xvalue obj = xvoCreateTable();
xvoTableSet(obj, "name", xvoCreateText("Alice", 0, FALSE));
xvoTableSet(obj, "age", xvoCreateInt(30));
xvoTableSet(obj, "active", xvoCreateBool(TRUE));

xvalue name = xvoTableGet(obj, "name");
str s = xvoGetText(name);  // "Alice"

xvoUnref(obj);
```

---

## 使用场景

### 1. JSON风格数据

```c
// 创建对象
xvalue user = xvoCreateTable();
xvoTableSet(user, "id", xvoCreateInt(1001));
xvoTableSet(user, "name", xvoCreateText("John", 0, FALSE));
xvoTableSet(user, "email", xvoCreateText("john@example.com", 0, FALSE));

// 创建数组
xvalue users = xvoCreateArray();
xvoArrayAdd(users, user);

// 访问
xvalue first_user = xvoArrayGet(users, 0);
xvalue name = xvoTableGet(first_user, "name");
printf("Name: %s\n", xvoGetText(name));

xvoUnref(users);
```

---

### 2. 配置系统

```c
xvalue config = xvoCreateTable();
xvoTableSet(config, "port", xvoCreateInt(8080));
xvoTableSet(config, "host", xvoCreateText("0.0.0.0", 0, FALSE));
xvoTableSet(config, "debug", xvoCreateBool(TRUE));

// 读取配置
int64 port = xvoToInt(xvoTableGet(config, "port"));
str host = xvoGetText(xvoTableGet(config, "host"));
bool debug = xvoGetBool(xvoTableGet(config, "debug"));

xvoUnref(config);
```

---

### 3. 动态数据结构

```c
typedef struct {
    xvalue data;  // 可以存储任意类型
} DynamicContainer;

void SetValue(DynamicContainer* c, xvalue val) {
    if (c->data) {
        xvoUnref(c->data);
    }
    c->data = xvoRef(val);
}

xvalue GetValue(DynamicContainer* c) {
    return c->data;
}
```

---

## 最佳实践

### 1. 引用计数管理

```c
// ✅ 正确：创建后需要释放
xvalue v = xvoCreateInt(100);
UseValue(v);
xvoUnref(v);

// ✅ 正确：增加引用后需要额外释放
xvalue v2 = xvoRef(v);
xvoUnref(v);
xvoUnref(v2);
```

---

### 2. 容器自动管理子元素

```c
xvalue arr = xvoCreateArray();
xvoArrayAdd(arr, xvoCreateInt(1));
xvoArrayAdd(arr, xvoCreateInt(2));
// 只需释放容器，子元素自动释放
xvoUnref(arr);
```

---

### 3. 避免循环引用

```c
// ❌ 危险：循环引用导致内存泄漏
xvalue a = xvoCreateTable();
xvalue b = xvoCreateTable();
xvoTableSet(a, "b", b);  // a 引用 b
xvoTableSet(b, "a", a);  // b 引用 a
// 两者都无法释放
```

---

## 性能提示

### 1. 使用静态字符串

```c
// ✅ 高效：常量字符串使用静态模式
xvalue v = xvoCreateText("constant", 0, TRUE);

// ❌ 低效：常量字符串也复制
xvalue v = xvoCreateText("constant", 0, FALSE);
```

---

### 2. 批量操作

```c
// 预先创建容器
xvalue arr = xvoCreateArray();
for (int i = 0; i < 1000; i++) {
    xvoArrayAdd(arr, xvoCreateInt(i));
}
xvoUnref(arr);
```

---

## 相关文档

- [JSON 处理](api-json.md) - JSON与Value互转
- [Dict 字典](api-dict.md) - 底层字典实现
- [List 列表](api-list.md) - 底层列表实现
- [主索引](README.md)

---

<div align="center">

[⬆️ 返回顶部](#value-动态类型系统库)

</div>
