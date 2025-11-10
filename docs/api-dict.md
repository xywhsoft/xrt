# Dict 字典库

> 哈希表字典（键值对存储）

[返回索引](README.md)

---

## 📑 目录

- [字典操作](#字典操作)
- [键值操作](#键值操作)
- [遍历](#遍历)

---

## 字典操作

### xdictCreate

创建字典

**函数原型：**
```c
XXAPI xdict xdictCreate();
```

**释放：** ✅ 需要 `xdictFree` 释放

**示例：**
```c
xdict dict = xdictCreate();
xdictSet(dict, "key", value);
ptr val = xdictGet(dict, "key");
xdictFree(dict);
```

---

### xdictFree

释放字典

**函数原型：**
```c
XXAPI void xdictFree(xdict objDict);
```

---

## 键值操作

### xdictSet

设置键值对

**函数原型：**
```c
XXAPI void xdictSet(xdict objDict, str sKey, ptr pValue);
```

**说明：**
- 键已存在则更新
- 键不存在则添加

**示例：**
```c
xdict dict = xdictCreate();
xdictSet(dict, "name", "Tom");
xdictSet(dict, "age", (ptr)25);
```

---

### xdictGet

获取值

**函数原型：**
```c
XXAPI ptr xdictGet(xdict objDict, str sKey);
```

**返回值：**
- 找到：返回值指针
- 未找到：返回 `NULL`

**示例：**
```c
str name = (str)xdictGet(dict, "name");
if (name) {
    printf("Name: %s\n", name);
}
```

---

### xdictRemove

移除键值对

**函数原型：**
```c
XXAPI bool xdictRemove(xdict objDict, str sKey);
```

**返回值：**
- `TRUE` - 成功移除
- `FALSE` - 键不存在

---

### xdictExists

检查键是否存在

**函数原型：**
```c
XXAPI bool xdictExists(xdict objDict, str sKey);
```

**示例：**
```c
if (xdictExists(dict, "key")) {
    printf("Key exists\n");
}
```

---

## 遍历

### xdictCount

获取键值对数量

**函数原型：**
```c
XXAPI uint xdictCount(xdict objDict);
```

---

### xdictGetFirst

获取第一个键值对

**函数原型：**
```c
XXAPI ptr xdictGetFirst(xdict objDict, str* psKey);
```

**参数：**
- `psKey` - 输出参数，接收键

**返回值：**
- 值指针

---

### xdictGetNext

获取下一个键值对

**函数原型：**
```c
XXAPI ptr xdictGetNext(xdict objDict, str* psKey);
```

**示例：**
```c
str key;
ptr value = xdictGetFirst(dict, &key);
while (value) {
    printf("%s = %s\n", key, (str)value);
    value = xdictGetNext(dict, &key);
}
```

---

## 使用场景

### 1. 配置管理

```c
xdict config = xdictCreate();

void LoadConfig(str filename) {
    str content = xrtFileReadAll(filename, XRT_CP_UTF8);
    str* lines = xrtSplit(content, 0, "\n", 0, FALSE);
    
    for (int i = 0; lines[i]; i++) {
        str* parts = xrtSplit(lines[i], 0, "=", 0, FALSE);
        if (parts[0] && parts[1]) {
            xdictSet(config, parts[0], parts[1]);
        }
        xrtFree(parts);
    }
    
    xrtFree(lines);
    xrtFree(content);
}

str GetConfig(str key) {
    return (str)xdictGet(config, key);
}
```

---

### 2. 缓存

```c
xdict cache = xdictCreate();

ptr GetCached(str key) {
    ptr data = xdictGet(cache, key);
    if (!data) {
        data = LoadFromDatabase(key);
        xdictSet(cache, key, data);
    }
    return data;
}
```

---

## 性能特点

- **时间复杂度：**
  - 插入：O(1) 平均
  - 查找：O(1) 平均
  - 删除：O(1) 平均

- **空间复杂度：** O(n)

---

## 相关文档

- [AVLTree 平衡二叉树](api-avltree.md)
- [Hash 哈希计算](api-hash.md)
- [主索引](README.md)

---

<div align="center">

[⬆️ 返回顶部](#dict-字典库)

</div>
