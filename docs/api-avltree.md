# AVLTree 平衡二叉树库

> AVL平衡二叉搜索树

[返回索引](README.md)

---

## 📑 目录

- [树操作](#树操作)
- [节点操作](#节点操作)
- [遍历](#遍历)

---

## 树操作

### xatCreate

创建AVL树

**函数原型：**
```c
XXAPI xavltree xatCreate();
```

**释放：** ✅ 需要 `xatFree` 释放

**示例：**
```c
xavltree tree = xatCreate();
xatAdd(tree, "key", data);
xatFree(tree);
```

---

### xatFree

释放AVL树

**函数原型：**
```c
XXAPI void xatFree(xavltree objTree);
```

---

## 节点操作

### xatAdd

添加节点

**函数原型：**
```c
XXAPI bool xatAdd(xavltree objTree, str sKey, ptr pData);
```

**参数：**
- `sKey` - 键（字符串）
- `pData` - 数据

**返回值：**
- `TRUE` - 成功
- `FALSE` - 键已存在

**示例：**
```c
xavltree tree = xatCreate();
xatAdd(tree, "name", "Tom");
xatAdd(tree, "age", (ptr)25);
```

---

### xatGet

获取数据

**函数原型：**
```c
XXAPI ptr xatGet(xavltree objTree, str sKey);
```

**返回值：**
- 找到：返回数据指针
- 未找到：返回 `NULL`

**示例：**
```c
ptr data = xatGet(tree, "name");
if (data) {
    printf("Found: %s\n", (str)data);
}
```

---

### xatRemove

移除节点

**函数原型：**
```c
XXAPI bool xatRemove(xavltree objTree, str sKey);
```

**返回值：**
- `TRUE` - 成功移除
- `FALSE` - 键不存在

---

### xatExists

检查键是否存在

**函数原型：**
```c
XXAPI bool xatExists(xavltree objTree, str sKey);
```

---

## 遍历

### xatCount

获取节点数量

**函数原型：**
```c
XXAPI uint xatCount(xavltree objTree);
```

---

### xatGetFirst

获取第一个节点

**函数原型：**
```c
XXAPI ptr xatGetFirst(xavltree objTree, str* psKey);
```

**参数：**
- `psKey` - 输出参数，接收键

**返回值：**
- 节点数据

---

### xatGetNext

获取下一个节点

**函数原型：**
```c
XXAPI ptr xatGetNext(xavltree objTree, str* psKey);
```

**示例：**
```c
str key;
ptr data = xatGetFirst(tree, &key);
while (data) {
    printf("%s = %s\n", key, (str)data);
    data = xatGetNext(tree, &key);
}
```

---

## 使用场景

### 1. 有序字典

```c
xavltree config = xatCreate();
xatAdd(config, "host", "localhost");
xatAdd(config, "port", "8080");

str host = xatGet(config, "host");
printf("Host: %s\n", host);
```

---

### 2. 符号表

```c
typedef struct {
    str name;
    int type;
    ptr value;
} Symbol;

xavltree symbol_table = xatCreate();

void AddSymbol(str name, Symbol* sym) {
    xatAdd(symbol_table, name, sym);
}

Symbol* LookupSymbol(str name) {
    return (Symbol*)xatGet(symbol_table, name);
}
```

---

## 相关文档

- [Dict 字典](api-dict.md)
- [主索引](README.md)

---

<div align="center">

[⬆️ 返回顶部](#avltree-平衡二叉树库)

</div>
