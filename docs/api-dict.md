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

### xrtDictCreate

创建字典

**函数原型：**
```c
XXAPI xdict xrtDictCreate(uint32 iItemLength);
```

**参数：**
- `iItemLength` - 值的大小（0表示存储指针）

**释放：** ✅ 需要 `xrtDictDestroy` 释放

**示例：**
```c
xdict dict = xrtDictCreate(sizeof(int));  // 存储int值
bool isNew;
int* val = (int*)xrtDictSet(dict, "key", 3, &isNew);
*val = 100;
xrtDictDestroy(dict);
```

---

### xrtDictDestroy

释放字典

**函数原型：**
```c
XXAPI void xrtDictDestroy(xdict objHT);
```

---

## 键值操作

### xrtDictSet / xrtDictSetPtr

设置键值对

**函数原型：**
```c
XXAPI ptr xrtDictSet(xdict objHT, ptr sKey, uint32 iKeyLen, bool* bNewRet);
XXAPI bool xrtDictSetPtr(xdict objHT, ptr sKey, uint32 iKeyLen, ptr pVal, ptr* ppOldVal);
```

**参数：**
- `sKey` - 键（任意二进制数据）
- `iKeyLen` - 键长度
- `bNewRet` - 输出：是否为新键

**说明：**
- xrtDictSet: 返回值指针，需要手动赋值
- xrtDictSetPtr: 直接设置指针值

**示例：**
```c
xdict dict = xrtDictCreate(sizeof(int));

bool isNew;
str key = "age";
int* val = (int*)xrtDictSet(dict, key, strlen(key), &isNew);
if (isNew) {
    *val = 25;
}
```

---

### xrtDictGet / xrtDictGetPtr

获取值

**函数原型：**
```c
XXAPI ptr xrtDictGet(xdict objHT, ptr sKey, uint32 iKeyLen);
XXAPI ptr xrtDictGetPtr(xdict objHT, ptr sKey, uint32 iKeyLen);
```

**返回值：**
- xrtDictGet: 返回值指针
- xrtDictGetPtr: 返回存储的指针值

**示例：**
```c
str key = "age";
int* val = (int*)xrtDictGet(dict, key, strlen(key));
if (val) {
    printf("Age: %d\n", *val);
}
```

---

### xrtDictRemove / xrtDictRemovePtr

移除键值对

**函数原型：**
```c
XXAPI bool xrtDictRemove(xdict objHT, ptr sKey, uint32 iKeyLen);
XXAPI ptr xrtDictRemovePtr(xdict objHT, ptr sKey, uint32 iKeyLen);
```

**返回值：**
- `TRUE` - 成功移除
- `FALSE` - 键不存在

---

### xrtDictExists

检查键是否存在

**函数原型：**
```c
XXAPI bool xrtDictExists(xdict objHT, ptr sKey, uint32 iKeyLen);
```

**示例：**
```c
str key = "name";
if (xrtDictExists(dict, key, strlen(key))) {
    printf("Key exists\n");
}
```

---

## 遍历

### xrtDictCount

获取键值对数量

**函数原型：**
```c
XXAPI uint32 xrtDictCount(xdict objHT);
```

---

### xrtDictWalk

遍历字典

**函数原型：**
```c
typedef bool (*Dict_EachProc)(Dict_Key* pKey, ptr pVal, ptr pArg);
XXAPI void xrtDictWalk(xdict objHT, Dict_EachProc procEach, ptr pArg);
```

**示例：**
```c
bool PrintItem(Dict_Key* pKey, ptr pVal, ptr pArg) {
    printf("%.*s = %d\n", pKey->KeyLen, (str)pKey->Key, *(int*)pVal);
    return true;  // 继续遍历
}

xrtDictWalk(dict, PrintItem, NULL);
```

---

## 使用场景

### 1. 配置管理

```c
xdict config = xrtDictCreate(0);  // 存储指针

void LoadConfig(str filename) {
    str content = xrtFileReadAll(filename, XRT_CP_UTF8);
    str* lines = xrtSplit(content, 0, "\n", 0, FALSE);
    
    for (int i = 0; lines[i]; i++) {
        str* parts = xrtSplit(lines[i], 0, "=", 0, FALSE);
        if (parts[0] && parts[1]) {
            str key = parts[0];
            xrtDictSetPtr(config, key, strlen(key), parts[1], NULL);
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
