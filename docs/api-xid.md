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
typedef struct {
    xtime Time;        // 当前时间戳
    int32 Addr;        // 本机IP地址
    int32 Tick;        // CPU时钟（低32位）
    int64 Rand;        // 随机数
} xid_struct, *xid;
```

**特点：**
- 192位（24字节）
- 全局唯一
- 时间有序
- 包含机器标识

---

## ID操作

### xrtMakeXID

创建新ID

**函数原型：**
```c
XXAPI xid xrtMakeXID();
```

**返回值：**
- 新的XID对象

**内存释放：** ✅ 需要 `xrtFree` 释放

**示例：**
```c
xid id = xrtMakeXID();
// 使用ID
xrtFree(id);
```

---

### xrtCompXID

比较两个ID

**函数原型：**
```c
XXAPI bool xrtCompXID(xid pXID1, xid pXID2);
```

**参数：**
- `pXID1` - 第ID1
- `pXID2` - 第ID2

**返回值：**
- `TRUE` - 相同
- `FALSE` - 不同

---

## ID转换

### xrtEncodeXID

ID转字符串

**函数原型：**
```c
XXAPI str xrtEncodeXID(xid pXID);
```

**返回值：**
- HEX格式字符串（48字符）

**内存释放：** ✅ 需要 `xrtFree` 释放

**示例：**
```c
xid id = xrtMakeXID();
str s = xrtEncodeXID(id);
printf("ID: %s\n", s);
xrtFree(s);
xrtFree(id);
```

---

### xrtDecodeXID

字符串转ID

**函数原型：**
```c
XXAPI xid xrtDecodeXID(str sXID);
```

**参数：**
- `sXID` - XID字符串

**内存释放：** ✅ 需要 `xrtFree` 释放

---

## 使用场景

### 1. 唯一订单号

```c
str GenerateOrderID() {
    xid id = xrtMakeXID();
    str order_id = xrtEncodeXID(id);
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
