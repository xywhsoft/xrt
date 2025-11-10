# XID 分布式ID库

> 192位分布式唯一ID生成器

[返回索引](README.md)

---

## 📑 目录

- [XID结构](#xid结构)
- [ID操作](#id操作)
- [ID转换](#id转换)

---

## XID结构

### 结构定义

```c
typedef struct xid_struct {
    uint32 Addr;        // 机器IP地址
    uint32 PID;         // 进程ID
    uint64 Time;        // 时间戳（微秒）
    uint64 Count;       // 计数器
} xid_struct, *xid;
```

**特点：**
- 192位（24字节）
- 全局唯一
- 时间有序
- 包含机器标识

---

## ID操作

### xidCreate

创建新ID

**函数原型：**
```c
XXAPI xid xidCreate();
```

**返回值：**
- 新的XID对象

**内存释放：** ✅ 需要 `xrtFree` 释放

**示例：**
```c
xid id = xidCreate();
// 使用ID
xrtFree(id);
```

---

### xidCopy

复制ID

**函数原型：**
```c
XXAPI xid xidCopy(xid pID);
```

---

## ID转换

### xidToStr

ID转字符串

**函数原型：**
```c
XXAPI str xidToStr(xid pID);
```

**返回值：**
- HEX格式字符串（48字符）

**内存释放：** ✅ 需要 `xrtFree` 释放

**示例：**
```c
xid id = xidCreate();
str s = xidToStr(id);
printf("ID: %s\n", s);
xrtFree(s);
xrtFree(id);
```

---

### xidFromStr

字符串转ID

**函数原型：**
```c
XXAPI xid xidFromStr(str sID, size_t iSize);
```

**内存释放：** ✅ 需要 `xrtFree` 释放

---

## 使用场景

### 1. 唯一订单号

```c
str GenerateOrderID() {
    xid id = xidCreate();
    str order_id = xidToStr(id);
    xrtFree(id);
    return order_id;
}
```

---

### 2. 分布式追踪

```c
typedef struct {
    xid trace_id;
    // 其他字段
} TraceContext;
```

---

## 相关文档

- [Network 网络功能](api-network.md)
- [String 字符串处理](api-string.md)
- [主索引](README.md)

---

<div align="center">

[⬆️ 返回顶部](#xid-分布式id库)

</div>
