# Hash 哈希计算库

> 哈希函数和校验功能

[返回索引](README.md)

---

## 📑 目录

- [32位哈希](#32位哈希)
- [64位哈希](#64位哈希)

---

## 32位哈希

### xrtHash32

计算32位哈希值

**函数原型：**
```c
XXAPI uint32 xrtHash32(ptr pData, size_t iSize);
```

**参数：**
- `pData` - 数据
- `iSize` - 长度

**返回值：**
- 32位哈希值

**示例：**
```c
str text = "Hello";
uint32 hash = xrtHash32(text, strlen(text));
printf("Hash: %u\n", hash);
```

---

### xrtStrHash32

字符串哈希（不区分大小写）

**函数原型：**
```c
XXAPI uint32 xrtStrHash32(str sText, size_t iSize);
```

**说明：**
- 自动转为小写后计算哈希
- 适用于字典键比较

---

## 64位哈希

### xrtHash64

计算64位哈希值

**函数原型：**
```c
XXAPI uint64 xrtHash64(ptr pData, size_t iSize);
```

**返回值：**
- 64位哈希值

**示例：**
```c
uint64 hash = xrtHash64(data, size);
```

---

### xrtStrHash64

字符串64位哈希

**函数原型：**
```c
XXAPI uint64 xrtStrHash64(str sText, size_t iSize);
```

---

## 使用场景

### 1. 快速查找

```c
uint32 GetHash(str key) {
    return xrtStrHash32(key, 0);
}
```

---

### 2. 数据校验

```c
uint64 ComputeChecksum(ptr data, size_t size) {
    return xrtHash64(data, size);
}
```

---

## 相关文档

- [Dict 字典](api-dict.md)
- [主索引](README.md)

---

<div align="center">

[⬆️ 返回顶部](#hash-哈希计算库)

</div>
