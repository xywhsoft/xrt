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

### xlistCreate

创建列表

**函数原型：**
```c
XXAPI xlist xlistCreate();
```

**释放：** ✅ 需要 `xlistFree` 释放

**示例：**
```c
xlist list = xlistCreate();
xlistSet(list, 0, "item0");
xlistSet(list, 1, "item1");
xlistFree(list);
```

---

### xlistFree

释放列表

**函数原型：**
```c
XXAPI void xlistFree(xlist objList);
```

---

## 元素操作

### xlistSet

设置元素

**函数原型：**
```c
XXAPI void xlistSet(xlist objList, uint iIndex, ptr pValue);
```

**说明：**
- 自动扩展列表
- 索引从0开始

**示例：**
```c
xlist list = xlistCreate();
xlistSet(list, 0, "first");
xlistSet(list, 10, "eleventh");  // 自动扩展
```

---

### xlistGet

获取元素

**函数原型：**
```c
XXAPI ptr xlistGet(xlist objList, uint iIndex);
```

**返回值：**
- 找到：返回值指针
- 未找到：返回 `NULL`

**示例：**
```c
str item = (str)xlistGet(list, 0);
if (item) {
    printf("Item: %s\n", item);
}
```

---

### xlistAdd

追加元素

**函数原型：**
```c
XXAPI void xlistAdd(xlist objList, ptr pValue);
```

**示例：**
```c
xlist list = xlistCreate();
xlistAdd(list, "item1");
xlistAdd(list, "item2");
xlistAdd(list, "item3");
```

---

### xlistRemove

移除元素

**函数原型：**
```c
XXAPI void xlistRemove(xlist objList, uint iIndex);
```

---

## 遍历

### xlistCount

获取元素数量

**函数原型：**
```c
XXAPI uint xlistCount(xlist objList);
```

**示例：**
```c
uint count = xlistCount(list);
for (uint i = 0; i < count; i++) {
    str item = (str)xlistGet(list, i);
    printf("%u: %s\n", i, item);
}
```

---

## 使用场景

### 1. 动态数组

```c
xlist items = xlistCreate();

void AddItem(str item) {
    xlistAdd(items, xrtCopyStr(item, 0));
}

str GetItem(uint index) {
    return (str)xlistGet(items, index);
}

uint GetCount() {
    return xlistCount(items);
}
```

---

### 2. 命令行参数

```c
xlist args = xlistCreate();

void ParseArgs(int argc, char** argv) {
    for (int i = 1; i < argc; i++) {
        xlistAdd(args, argv[i]);
    }
}

str GetArg(uint index) {
    return (str)xlistGet(args, index);
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
