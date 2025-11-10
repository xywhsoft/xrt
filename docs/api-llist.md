# LList 双向链表库

> 双向链表数据结构

[返回索引](README.md)

---

## 📑 目录

- [链表操作](#链表操作)
- [节点操作](#节点操作)
- [遍历](#遍历)

---

## 链表操作

### xrtLListCreate

创建链表

**函数原型：**
```c
XXAPI xllist xrtLListCreate(uint32 iItemLength);
```

**参数：**
- `iItemLength` - 节点数据大小

**释放：** ✅ 需要 `xrtLListDestroy` 释放

**示例：**
```c
xllist list = xrtLListCreate(sizeof(MyData));
xllistnode node = xrtLListInsertNext(list, NULL);
MyData* data = (MyData*)&node[1];
xrtLListDestroy(list);
```

---

### xrtLListDestroy

释放链表

**函数原型：**
```c
XXAPI void xrtLListDestroy(xllist objLL);
```

---

## 节点操作

### xrtLListInsertNext / xrtLListInsertPrev

插入节点

**函数原型：**
```c
XXAPI xllistnode xrtLListInsertNext(xllist objLL, xllistnode objNode);
XXAPI xllistnode xrtLListInsertPrev(xllist objLL, xllistnode objNode);
```

**参数：**
- `objNode` - 参考节点（NULL表示头/尾）

**返回值：**
- 新节点指针，数据位于 `&node[1]`

**示例：**
```c
xllist list = xrtLListCreate(sizeof(int));

// 插入到末尾
xllistnode n1 = xrtLListInsertNext(list, NULL);
int* data1 = (int*)&n1[1];
*data1 = 42;
```

---

### xrtLListRemove

移除节点

**函数原型：**
```c
XXAPI void xrtLListRemove(xllist objLL, xllistnode objNode);
```

---

## 遍历

### 遍历示例

**直接访问结构体字段：**
```c
uint32 count = list->Count;
xllistnode node = list->FirstNode;
while (node) {
    MyData* data = (MyData*)&node[1];
    // 处理数据
    node = node->Next;
}
```

---

## 使用场景

### 1. 队列实现

```c
typedef struct {
    xllist queue;
} Queue;

Queue* CreateQueue() {
    Queue* q = xrtMalloc(sizeof(Queue));
    q->queue = xrtLListCreate(sizeof(ptr));
    return q;
}

void Enqueue(Queue* q, ptr item) {
    xllistnode node = xrtLListInsertNext(q->queue, NULL);
    ptr* data = (ptr*)&node[1];
    *data = item;
}

ptr Dequeue(Queue* q) {
    if (!q->queue->FirstNode) return NULL;
    xllistnode node = q->queue->FirstNode;
    ptr* data = (ptr*)&node[1];
    ptr item = *data;
    xrtLListRemove(q->queue, node);
    return item;
}
```

---

### 2. LRU缓存

```c
xllist lru_list = xrtLListCreate(sizeof(ptr));

void AccessItem(ptr item) {
    // 移动到链表头部
    // 需要手动实现：先删除旧节点，再在头部插入新节点
    xllistnode node = xrtLListInsertPrev(lru_list, NULL);
    ptr* data = (ptr*)&node[1];
    *data = item;
}

ptr EvictOldest() {
    if (!lru_list->LastNode) return NULL;
    xllistnode node = lru_list->LastNode;
    ptr* data = (ptr*)&node[1];
    ptr item = *data;
    xrtLListRemove(lru_list, node);
    return item;
}
```

---

## 相关文档

- [Stack 栈](api-stack.md)
- [List 列表](api-list.md)
- [主索引](README.md)

---

<div align="center">

[⬆️ 返回顶部](#llist-双向链表库)

</div>
