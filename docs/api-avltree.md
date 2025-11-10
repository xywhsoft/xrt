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

### xrtAVLTreeCreate

创建AVL树

**函数原型：**
```c
XXAPI xavltree xrtAVLTreeCreate(uint32 iItemLength, AVLTree_CompProc procComp);
```

**参数：**
- `iItemLength` - 节点数据大小
- `procComp` - 比较函数

**释放：** ✅ 需要 `xrtAVLTreeDestroy` 释放

**示例：**
```c
int CompareInt(ptr pNode, ptr pKey) {
    int* a = (int*)pNode;
    int* b = (int*)pKey;
    return *a - *b;
}

xavltree tree = xrtAVLTreeCreate(sizeof(int), CompareInt);
bool isNew;
int key = 42;
int* data = (int*)xrtAVLTreeInsert(tree, &key, &isNew);
*data = key;
xrtAVLTreeDestroy(tree);
```

---

### xrtAVLTreeDestroy

释放AVL树

**函数原型：**
```c
XXAPI void xrtAVLTreeDestroy(xavltree objAVLT);
```

---

## 节点操作

### xrtAVLTreeInsert

插入节点

**函数原型：**
```c
XXAPI ptr xrtAVLTreeInsert(xavltree objAVLT, ptr pKey, bool* bNew);
```

**参数：**
- `pKey` - 键指针
- `bNew` - 输出参数，指示是否为新节点

**返回值：**
- 返回节点数据指针

**示例：**
```c
bool isNew;
int key = 100;
int* data = (int*)xrtAVLTreeInsert(tree, &key, &isNew);
if (isNew) {
    *data = key;
}
```

---

### xrtAVLTreeSearch

查找数据

**函数原型：**
```c
XXAPI ptr xrtAVLTreeSearch(xavltree objAVLT, ptr pKey);
```

**返回值：**
- 找到：返回数据指针
- 未找到：返回 `NULL`

**示例：**
```c
int search_key = 100;
int* found = (int*)xrtAVLTreeSearch(tree, &search_key);
if (found) {
    printf("Found: %d\n", *found);
}
```

---

### xrtAVLTreeRemove

移除节点

**函数原型：**
```c
XXAPI bool xrtAVLTreeRemove(xavltree objAVLT, ptr pKey);
```

**返回值：**
- `TRUE` - 成功移除
- `FALSE` - 键不存在

---

## 遍历

### 遍历示例

**直接访问结构体字段：**
```c
uint32 count = tree->Count;  // 节点数量
xavltnode root = tree->RootNode;  // 根节点

// 遍历需要使用 xrtAVLTreeWalk 或自定义递归函数
```

---

## 使用场景

### 1. 整数索引

```c
int CompareInt(ptr pNode, ptr pKey) {
    return *(int*)pNode - *(int*)pKey;
}

xavltree tree = xrtAVLTreeCreate(sizeof(int), CompareInt);

int key = 100;
bool isNew;
int* data = (int*)xrtAVLTreeInsert(tree, &key, &isNew);
*data = key;

int search = 100;
int* found = (int*)xrtAVLTreeSearch(tree, &search);
if (found) {
    printf("Found: %d\n", *found);
}

xrtAVLTreeDestroy(tree);
```

---

### 2. 自定义结构

```c
typedef struct {
    int id;
    str name;
} User;

int CompareUser(ptr pNode, ptr pKey) {
    return ((User*)pNode)->id - *(int*)pKey;
}

xavltree users = xrtAVLTreeCreate(sizeof(User), CompareUser);

int id = 1001;
bool isNew;
User* user = (User*)xrtAVLTreeInsert(users, &id, &isNew);
if (isNew) {
    user->id = id;
    user->name = "Alice";
}

int search_id = 1001;
User* found = (User*)xrtAVLTreeSearch(users, &search_id);
if (found) {
    printf("User: %s\n", found->name);
}

xrtAVLTreeDestroy(users);
```

---

## 相关文档

- [Dict 字典](api-dict.md)
- [主索引](README.md)

---

<div align="center">

[⬆️ 返回顶部](#avltree-平衡二叉树库)

</div>
