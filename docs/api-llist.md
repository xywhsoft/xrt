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

### xllCreate

创建链表

**函数原型：**
```c
XXAPI xllist xllCreate();
```

**释放：** ✅ 需要 `xllFree` 释放

**示例：**
```c
xllist list = xllCreate();
xllAddTail(list, data1);
xllAddTail(list, data2);
xllFree(list);
```

---

### xllFree

释放链表

**函数原型：**
```c
XXAPI void xllFree(xllist objList);
```

---

## 节点操作

### xllAddHead

添加到头部

**函数原型：**
```c
XXAPI void xllAddHead(xllist objList, ptr pData);
```

---

### xllAddTail

添加到尾部

**函数原型：**
```c
XXAPI void xllAddTail(xllist objList, ptr pData);
```

**示例：**
```c
xllist list = xllCreate();
xllAddTail(list, "item1");
xllAddTail(list, "item2");
xllAddTail(list, "item3");
```

---

### xllRemoveHead

移除头节点

**函数原型：**
```c
XXAPI ptr xllRemoveHead(xllist objList);
```

**返回值：**
- 头节点数据
- `NULL` - 链表空

---

### xllRemoveTail

移除尾节点

**函数原型：**
```c
XXAPI ptr xllRemoveTail(xllist objList);
```

---

## 遍历

### xllGetFirst

获取第一个节点

**函数原型：**
```c
XXAPI ptr xllGetFirst(xllist objList);
```

---

### xllGetNext

获取下一个节点

**函数原型：**
```c
XXAPI ptr xllGetNext(xllist objList, ptr pNode);
```

---

### xllCount

获取节点数量

**函数原型：**
```c
XXAPI uint xllCount(xllist objList);
```

**示例：**
```c
uint count = xllCount(list);
printf("Count: %u\n", count);
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
    q->queue = xllCreate();
    return q;
}

void Enqueue(Queue* q, ptr item) {
    xllAddTail(q->queue, item);
}

ptr Dequeue(Queue* q) {
    return xllRemoveHead(q->queue);
}
```

---

### 2. LRU缓存

```c
xllist lru_list = xllCreate();

void AccessItem(ptr item) {
    // 移到链表头部
    xllRemove(lru_list, item);
    xllAddHead(lru_list, item);
}

ptr EvictOldest() {
    return xllRemoveTail(lru_list);
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
