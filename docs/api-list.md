# List 列表库

> 整数索引列表（类似数组但支持字符串值）

[返回索引](README.md)

---

## 📑 目录

- [列表操作](#列表操作)
- [元素操作](#元素操作)
- [遍历](#遍历)

---

## 列表操作

### xrtListCreate

创建列表

**函数原型：**
```c
XXAPI xlist xrtListCreate(uint32 iItemLength);
```

**参数：**
- `iItemLength` - 值的大小（0表示存储指针）

**释放：** ✅ 需要 `xrtListDestroy` 释放

**示例：**
```c
xlist list = xrtListCreate(sizeof(int));
bool isNew;
int* val = (int*)xrtListSet(list, 0, &isNew);
*val = 100;
xrtListDestroy(list);
```

---

### xrtListDestroy

释放列表

**函数原型：**
```c
XXAPI void xrtListDestroy(xlist objList);
```

---

## 元素操作

### xrtListSet / xrtListSetPtr

设置元素

**函数原型：**
```c
XXAPI ptr xrtListSet(xlist objList, int64 iKey, bool* bNewRet);
XXAPI bool xrtListSetPtr(xlist objList, int64 iKey, ptr pVal, ptr* ppOldVal);
```

**参数：**
- `iKey` - 索引（int64类型）
- `bNewRet` - 输出：是否为新键

**说明：**
- 索引可以为负数
- xrtListSet: 返回值指针
- xrtListSetPtr: 直接设置指针值

**示例：**
```c
xlist list = xrtListCreate(sizeof(int));

bool isNew;
int* val = (int*)xrtListSet(list, 0, &isNew);
*val = 100;

int* val2 = (int*)xrtListSet(list, 10, &isNew);  // 跳跃索引
*val2 = 200;
```

---

### xrtListGet / xrtListGetPtr

获取元素

**函数原型：**
```c
XXAPI ptr xrtListGet(xlist objList, int64 iKey);
XXAPI ptr xrtListGetPtr(xlist objList, int64 iKey);
```

**返回值：**
- xrtListGet: 返回值指针
- xrtListGetPtr: 返回存储的指针值

**示例：**
```c
int* val = (int*)xrtListGet(list, 0);
if (val) {
    printf("Value: %d\n", *val);
}
```

---

### xrtListRemove / xrtListRemovePtr

移除元素

**函数原型：**
```c
XXAPI bool xrtListRemove(xlist objList, int64 iKey);
XXAPI ptr xrtListRemovePtr(xlist objList, int64 iKey);
```

---

## 遍历

### xrtListCount

获取元素数量

**函数原型：**
```c
XXAPI uint32 xrtListCount(xlist objList);
```

**示例：**
```c
uint32 count = xrtListCount(list);
printf("Count: %u\n", count);
```

---

## 使用场景

### 1. 动态数组

```c
xlist items = xrtListCreate(0);  // 存储指针

void AddItem(str item) {
    int64 idx = xrtListCount(items);
    xrtListSetPtr(items, idx, xrtCopyStr(item, 0), NULL);
}

str GetItem(int64 index) {
    return (str)xrtListGetPtr(items, index);
}

uint32 GetCount() {
    return xrtListCount(items);
}
```

---

### 2. 命令行参数

```c
xlist args = xrtListCreate(0);

void ParseArgs(int argc, char** argv) {
    for (int i = 1; i < argc; i++) {
        xrtListSetPtr(args, i-1, argv[i], NULL);
    }
}

str GetArg(int64 index) {
    return (str)xrtListGetPtr(args, index);
}
```

---

## 相关文档

- [Array 结构体数组](api-array.md)
- [PtrArray 指针数组](api-ptrarray.md)
- [主索引](README.md)

---

<div align="center">

[⬆️ 返回顶部](#list-列表库)

</div>
