# Network 网络功能库

> 网络信息获取功能

[返回索引](README.md)

---

## 📑 目录

- [本机信息](#本机信息)

---

## 本机信息

### xrtGetLocalRawIP

获取本机IP地址

**函数原型：**
```c
XXAPI uint32 xrtGetLocalRawIP();
```

**返回值：**
- 本机IP地址（uint32）

**说明：**
- 自动在 `xrtInit()` 中调用
- 结果存储在 `xCore.LocalAddr`

**示例：**
```c
uint32 ip = xrtGetLocalRawIP();
printf("IP: %u.%u.%u.%u\n",
    (ip >> 24) & 0xFF,
    (ip >> 16) & 0xFF,
    (ip >> 8) & 0xFF,
    ip & 0xFF
);
```

---

## 使用场景

### 1. 分布式ID生成

```c
// XID 使用本机IP作为机器标识
xid id = xrtMakeXID();
```

---

## 相关文档

- [XID 分布式ID](api-xid.md)
- [主索引](README.md)

---

<div align="center">

[⬆️ 返回顶部](#network-网络功能库)

</div>
