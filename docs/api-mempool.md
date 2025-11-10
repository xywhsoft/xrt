# BSMM/MemUnit/FSMemPool 内存池库

> 块内存管理和固定大小内存池

[返回索引](README.md)

---

## 📑 目录

- [BSMM 块内存管理](#bsmm-块内存管理)
- [MemUnit 内存单元](#memunit-内存单元)
- [FSMemPool 固定内存池](#fsmempool-固定内存池)

---

## BSMM 块内存管理

### xrtBSMMCreate

创建块内存管理器

**函数原型：**
```c
XXAPI xbsmm xrtBSMMCreate(uint32 iCount);
```

**参数：**
- `iCount` - 块数量

**释放：** ✅ 需要 `xrtBSMMDestroy` 释放

---

### xrtBSMMAlloc / xrtBSMMAllocClr

分配块

**函数原型：**
```c
XXAPI ptr xrtBSMMAlloc(xbsmm objBSMM);
XXAPI ptr xrtBSMMAllocClr(xbsmm objBSMM);  // 带清零
```

---

### xrtBSMMFree

释放块

**函数原型：**
```c
XXAPI bool xrtBSMMFree(xbsmm objBSMM, ptr pObj);
```

---

## MemUnit 内存单元

### xrtMemUnitCreate

创建内存单元（256字节页）

**函数原型：**
```c
XXAPI xmemunit xrtMemUnitCreate(uint32 iItemLength, uint32 iFlag);
```

**参数：**
- `iItemLength` - 项大小
- `iFlag` - 标志

**释放：** ✅ 需要 `xrtMemUnitDestroy` 释放

**示例：**
```c
xmemunit mu = xrtMemUnitCreate(32, 0);
ptr data = xrtMemUnitAlloc(mu);
xrtMemUnitDestroy(mu);
```

---

### xrtMemUnitAlloc

分配内存

**函数原型：**
```c
XXAPI ptr xrtMemUnitAlloc(xmemunit objUnit);
```

---

## FSMemPool 固定内存池

### xrtFSMemPoolCreate

创建固定大小内存池

**函数原型：**
```c
XXAPI xfsmempool xrtFSMemPoolCreate(uint32 iItemLength);
```

**参数：**
- `iItemLength` - 固定项大小

**释放：** ✅ 需要 `xrtFSMemPoolDestroy` 释放

**示例：**
```c
xfsmempool pool = xrtFSMemPoolCreate(sizeof(MyStruct));
MyStruct* item = xrtFSMemPoolAlloc(pool);
// 使用item
xrtFSMemPoolFree(pool, item);
xrtFSMemPoolDestroy(pool);
```

---

### xrtFSMemPoolAlloc

从池中分配

**函数原型：**
```c
XXAPI ptr xrtFSMemPoolAlloc(xfsmempool objMM);
```

---

### xrtFSMemPoolFree

归还到池

**函数原型：**
```c
XXAPI void xrtFSMemPoolFree(xfsmempool objMM, ptr p);
```

---

## 使用场景

### 1. 对象池

```c
typedef struct {
    int id;
    str name;
} Entity;

xfsmempool entity_pool = xrtFSMemPoolCreate(sizeof(Entity));

// 分配
Entity* e1 = xrtFSMemPoolAlloc(entity_pool);
Entity* e2 = xrtFSMemPoolAlloc(entity_pool);

// 使用后归还
xrtFSMemPoolFree(entity_pool, e1);
xrtFSMemPoolFree(entity_pool, e2);

xrtFSMemPoolDestroy(entity_pool);
```

---

## 相关文档

- [MemPool 通用内存池](api-mempool.md)
- [Base 基础函数库](api-base.md)
- [主索引](README.md)

---

<div align="center">

[⬆️ 返回顶部](#bsmmemunitfsmempool-内存池库)

</div>
