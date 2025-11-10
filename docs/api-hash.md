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
XXAPI uint32 xrtHash32(ptr key, size_t len);
XXAPI uint32 xrtHash32_WithSeed(ptr key, size_t len, uint32 seed);
```

**参数：**
- `key` - 数据
- `len` - 长度
- `seed` - 种子（默认0）

**返回值：**
- 32位哈希值

**示例：**
```c
str text = "Hello";
uint32 hash = xrtHash32(text, strlen(text));
printf("Hash: %u\n", hash);

// 使用自定义种子
uint32 hash2 = xrtHash32_WithSeed(text, strlen(text), 12345);
```

---

## 64位哈希

### xrtHash64

计算64位哈希值

**函数原型：**
```c
XXAPI uint64 xrtHash64(ptr key, size_t len);
XXAPI uint64 xrtHash64_WithSeed(ptr key, size_t len, uint64 seed);
XXAPI uint64 xrtHash64_Micro(ptr key, size_t len);
XXAPI uint64 xrtHash64_Micro_WithSeed(ptr key, size_t len, uint64 seed);
XXAPI uint64 xrtHash64_Nano(ptr key, size_t len);
XXAPI uint64 xrtHash64_Nano_WithSeed(ptr key, size_t len, uint64 seed);
```

**参数：**
- `key` - 数据
- `len` - 长度
- `seed` - 种子

**说明：**
- xrtHash64: 标准版本（高质量）
- xrtHash64_Micro: 微型版本
- xrtHash64_Nano: 超小版本

**返回值：**
- 64位哈希值

**示例：**
```c
uint64 hash = xrtHash64(data, size);

// 高性能版本
uint64 hash_fast = xrtHash64_Nano(data, size);
```

---

## 使用场景

### 1. 快速查找

```c
uint32 GetHash(str key) {
    return xrtHash32(key, strlen(key));
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
