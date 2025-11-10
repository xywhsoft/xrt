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

### xbsmmCreate

创建块内存管理器

**函数原型：**
```c
XXAPI xbsmm xbsmmCreate(size_t iBlockSize, uint iBlockCount);
```

**参数：**
- `iBlockSize` - 块大小
- `iBlockCount` - 块数量

**释放：** ✅ 需要 `xbsmmFree` 释放

---

### xbsmmAlloc

分配块

**函数原型：**
```c
XXAPI ptr xbsmmAlloc(xbsmm objBSMM);
```

---

### xbsmmDealloc

释放块

**函数原型：**
```c
XXAPI void xbsmmDealloc(xbsmm objBSMM, ptr pBlock);
```

---

## MemUnit 内存单元

### xmuCreate

创建内存单元（256字节页）

**函数原型：**
```c
XXAPI xmemunit xmuCreate();
```

**释放：** ✅ 需要 `xmuFree` 释放

**示例：**
```c
xmemunit mu = xmuCreate();
ptr data = xmuAlloc(mu, 100);
xmuFree(mu);
```

---

### xmuAlloc

分配内存

**函数原型：**
```c
XXAPI ptr xmuAlloc(xmemunit objMemUnit, size_t iSize);
```

**说明：**
- 从256字节页中分配
- 适合小内存频繁分配

---

## FSMemPool 固定内存池

### xfmpCreate

创建固定大小内存池

**函数原型：**
```c
XXAPI xfsmempool xfmpCreate(size_t iItemSize);
```

**参数：**
- `iItemSize` - 固定项大小

**释放：** ✅ 需要 `xfmpFree` 释放

**示例：**
```c
xfsmempool pool = xfmpCreate(sizeof(MyStruct));
MyStruct* item = xfmpAlloc(pool);
// 使用item
xfmpDealloc(pool, item);
xfmpFree(pool);
```

---

### xfmpAlloc

从池中分配

**函数原型：**
```c
XXAPI ptr xfmpAlloc(xfsmempool objPool);
```

---

### xfmpDealloc

归还到池

**函数原型：**
```c
XXAPI void xfmpDealloc(xfsmempool objPool, ptr pItem);
```

---

## 使用场景

### 1. 对象池

```c
typedef struct {
    int id;
    str name;
} Entity;

xfsmempool entity_pool = xfmpCreate(sizeof(Entity));

// 分配
Entity* e1 = xfmpAlloc(entity_pool);
Entity* e2 = xfmpAlloc(entity_pool);

// 使用后归还
xfmpDealloc(entity_pool, e1);
xfmpDealloc(entity_pool, e2);

xfmpFree(entity_pool);
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
