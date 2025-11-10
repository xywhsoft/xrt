# Buffer 动态缓冲区库

> 可变大小内存缓冲区

[返回索引](README.md)

---

## 📑 目录

- [缓冲区操作](#缓冲区操作)
- [数据读写](#数据读写)

---

## 缓冲区操作

### xrtBufferCreate

创建缓冲区

**函数原型：**
```c
XXAPI xbuffer xrtBufferCreate(uint32 iStep);
```

**参数：**
- `iStep` - 预分配步长（0使用默认）

**返回值：**
- 缓冲区对象

**释放：** ✅ 需要 `xrtBufferDestroy` 释放

**示例：**
```c
xbuffer buf = xrtBufferCreate(0);
// 使用缓冲区
xrtBufferDestroy(buf);
```

---

### xrtBufferDestroy

释放缓冲区

**函数原型：**
```c
XXAPI void xrtBufferDestroy(xbuffer pBuf);
```

---

### xrtBufferUnit

清空缓冲区（不释放对象）

**函数原型：**
```c
XXAPI void xrtBufferUnit(xbuffer pBuf);
```

---

## 数据读写

### xrtBufferAppend

写入数据

**函数原型：**
```c
XXAPI bool xrtBufferAppend(xbuffer pBuf, ptr pData, uint32 iSize, uint32 bStrMode);
```

**参数：**
- `pBuf` - 缓冲区对象
- `pData` - 数据
- `iSize` - 大小
- `bStrMode` - 字符串模式（如果为0则复制数据）

**说明：**
- 自动扩展缓冲区

**示例：**
```c
xbuffer buf = xrtBufferCreate(0);
xrtBufferAppend(buf, "Hello", 5, 0);
xrtBufferAppend(buf, "World", 5, 0);
xrtBufferDestroy(buf);
```

---

**注意：** xrt.h 中只提供了基本的缓冲区操作函数，没有 `xbufRead`, `xbufGetData`, `xbufGetSize` 等函数。
可以直接访问 `xbuffer_struct` 结构体的字段：

```c
xbuffer buf = xrtBufferCreate(0);
xrtBufferAppend(buf, "data", 4, 0);

// 访问数据
ptr data = buf->Buffer;
uint32 size = buf->Length;

xrtBufferDestroy(buf);
```

---

## 使用场景

### 1. 动态数据构建

```c
xbuffer BuildPacket() {
    xbuffer buf = xrtBufferCreate(0);
    
    uint32 header = 0x12345678;
    xrtBufferAppend(buf, &header, 4, 0);
    
    str data = "payload";
    xrtBufferAppend(buf, data, strlen(data), 0);
    
    return buf;
}
```

---

## 相关文档

- [Base 基础函数库](api-base.md)
- [主索引](README.md)

---

<div align="center">

[⬆️ 返回顶部](#buffer-动态缓冲区库)

</div>
