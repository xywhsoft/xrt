# Buffer 动态缓冲区库

> 可变大小内存缓冲区

[返回索引](README.md)

---

## 📑 目录

- [缓冲区操作](#缓冲区操作)
- [数据读写](#数据读写)

---

## 缓冲区操作

### xbufCreate

创建缓冲区

**函数原型：**
```c
XXAPI xbuffer xbufCreate(size_t iSize);
```

**参数：**
- `iSize` - 初始大小

**返回值：**
- 缓冲区对象

**释放：** ✅ 需要 `xbufFree` 释放

**示例：**
```c
xbuffer buf = xbufCreate(1024);
// 使用缓冲区
xbufFree(buf);
```

---

### xbufFree

释放缓冲区

**函数原型：**
```c
XXAPI void xbufFree(xbuffer objBuffer);
```

---

### xbufClear

清空缓冲区

**函数原型：**
```c
XXAPI void xbufClear(xbuffer objBuffer);
```

---

## 数据读写

### xbufWrite

写入数据

**函数原型：**
```c
XXAPI size_t xbufWrite(xbuffer objBuffer, ptr pData, size_t iSize);
```

**说明：**
- 自动扩展缓冲区

**示例：**
```c
xbuffer buf = xbufCreate(10);
xbufWrite(buf, "Hello", 5);
xbufWrite(buf, "World", 5);
```

---

### xbufRead

读取数据

**函数原型：**
```c
XXAPI size_t xbufRead(xbuffer objBuffer, ptr pData, size_t iSize);
```

---

### xbufGetData

获取数据指针

**函数原型：**
```c
XXAPI ptr xbufGetData(xbuffer objBuffer);
```

**返回值：**
- 内部数据指针（不要释放）

---

### xbufGetSize

获取数据大小

**函数原型：**
```c
XXAPI size_t xbufGetSize(xbuffer objBuffer);
```

---

## 使用场景

### 1. 动态数据构建

```c
xbuffer BuildPacket() {
    xbuffer buf = xbufCreate(128);
    
    uint32 header = 0x12345678;
    xbufWrite(buf, &header, 4);
    
    str data = "payload";
    xbufWrite(buf, data, strlen(data));
    
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
