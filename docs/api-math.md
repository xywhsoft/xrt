# Math 数学运算库

> 随机数生成和数学运算功能

[返回索引](README.md)

---

## 📑 目录

- [随机数生成](#随机数生成)
- [使用场景](#使用场景)
- [最佳实践](#最佳实践)

---

## 随机数生成

### xrtRand32

生成32位随机数

**函数原型：**
```c
XXAPI uint32 xrtRand32();
```

**返回值：**
- 返回 0 ~ 2^32-1 范围的随机数

**说明：**
- 使用 PCG32 算法（高质量伪随机数生成器）
- 自动在 `xrtInit()` 中初始化种子
- 线程不安全（多线程需要同步）

**示例：**
```c
// 生成随机数
uint32 random = xrtRand32();
printf("Random: %u\n", random);

// 生成多个随机数
for (int i = 0; i < 10; i++) {
    printf("%u ", xrtRand32());
}
```

---

### xrtSetRandSeed32

设置32位随机数种子

**函数原型：**
```c
XXAPI void xrtSetRandSeed32(uint64 seed, uint64 seq);
```

**参数：**
- `seed` - 随机数种子
- `seq` - 序列号（用于生成不同序列）

**说明：**
- 相同的种子和序列产生相同的随机数序列
- `xrtInit()` 自动使用时间和CPU时钟初始化
- 可用于可重现的随机序列（测试、调试）

**示例：**
```c
// 使用固定种子（可重现）
xrtSetRandSeed32(12345, 0);
uint32 r1 = xrtRand32();  // 每次运行结果相同

// 使用时间作为种子（不可重现）
xrtSetRandSeed32(time(NULL), 0);
uint32 r2 = xrtRand32();  // 每次运行结果不同

// 生成不同的随机序列
xrtSetRandSeed32(12345, 0);  // 序列A
xrtSetRandSeed32(12345, 1);  // 序列B（与A不同）
```

---

### xrtRand64

生成64位随机数

**函数原型：**
```c
XXAPI uint64 xrtRand64();
```

**返回值：**
- 返回 0 ~ 2^64-1 范围的随机数

**说明：**
- 使用 PCG64 算法
- 自动在 `xrtInit()` 中初始化
- 适用于需要更大范围的场景

**示例：**
```c
// 生成64位随机数
uint64 random = xrtRand64();
printf("Random: %llu\n", random);

// 生成大范围随机ID
uint64 id = xrtRand64();
```

---

### xrtSetRandSeed64

设置64位随机数种子

**函数原型：**
```c
XXAPI void xrtSetRandSeed64(
    uint64 lowseed, 
    uint64 lowseq,
    uint64 highseed, 
    uint64 highseq
);
```

**参数：**
- `lowseed` - 低位种子
- `lowseq` - 低位序列号
- `highseed` - 高位种子
- `highseq` - 高位序列号

**说明：**
- 64位随机数需要4个64位参数初始化
- `xrtInit()` 自动使用复杂算法初始化

**示例：**
```c
// 自定义64位种子
xrtSetRandSeed64(
    0x123456789ABCDEF0ULL, 0,
    0xFEDCBA9876543210ULL, 0
);
uint64 r = xrtRand64();
```

---

### xrtRandRange

生成指定范围的随机整数

**函数原型：**
```c
XXAPI int xrtRandRange(int min, int max);
```

**参数：**
- `min` - 最小值（包含）
- `max` - 最大值（包含）

**返回值：**
- 返回 [min, max] 范围内的随机整数

**说明：**
- 基于 `xrtRand32()` 实现
- 均匀分布
- **包含**最大值和最小值

**示例：**
```c
// 生成 1-100 的随机数
int dice = xrtRandRange(1, 100);

// 生成 0-9 的随机数
int digit = xrtRandRange(0, 9);

// 生成负数范围
int value = xrtRandRange(-10, 10);

// 骰子模拟
int roll = xrtRandRange(1, 6);
printf("Dice: %d\n", roll);
```

---

## 使用场景

### 1. 随机数组生成

```c
// 生成随机整数数组
void GenerateRandomArray(int* arr, int count, int min, int max) {
    for (int i = 0; i < count; i++) {
        arr[i] = xrtRandRange(min, max);
    }
}

// 使用
int numbers[100];
GenerateRandomArray(numbers, 100, 1, 1000);
```

---

### 2. 随机字符串生成

```c
// 生成随机字符串
str GenerateRandomString(int length) {
    const char* chars = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789";
    int charset_size = strlen(chars);
    
    str result = xrtMalloc(length + 1);
    for (int i = 0; i < length; i++) {
        int idx = xrtRandRange(0, charset_size - 1);
        result[i] = chars[idx];
    }
    result[length] = '\0';
    
    return result;
}

// 生成8位随机密码
str password = GenerateRandomString(8);
printf("Password: %s\n", password);
xrtFree(password);
```

---

### 3. 数组随机打乱

```c
// Fisher-Yates 洗牌算法
void ShuffleArray(int* arr, int count) {
    for (int i = count - 1; i > 0; i--) {
        int j = xrtRandRange(0, i);
        // 交换 arr[i] 和 arr[j]
        int temp = arr[i];
        arr[i] = arr[j];
        arr[j] = temp;
    }
}

// 洗牌
int cards[52];
for (int i = 0; i < 52; i++) cards[i] = i;
ShuffleArray(cards, 52);
```

---

### 4. 概率事件

```c
// 按概率返回 true/false
bool RandomChance(double probability) {
    // probability: 0.0 ~ 1.0
    uint32 threshold = (uint32)(probability * 0xFFFFFFFF);
    return xrtRand32() < threshold;
}

// 30% 概率触发
if (RandomChance(0.3)) {
    printf("Event triggered!\n");
}

// 模拟抽奖
bool IsWinner() {
    return RandomChance(0.01);  // 1% 中奖率
}
```

---

### 5. 随机采样

```c
// 从数组中随机选择 N 个元素
void RandomSample(int* source, int src_count, int* dest, int sample_count) {
    // 复制源数组
    int* temp = xrtMalloc(src_count * sizeof(int));
    memcpy(temp, source, src_count * sizeof(int));
    
    // 洗牌
    ShuffleArray(temp, src_count);
    
    // 取前 N 个
    memcpy(dest, temp, sample_count * sizeof(int));
    xrtFree(temp);
}
```

---

### 6. 随机延迟

```c
// 随机延迟（防止同步）
void RandomDelay(int min_ms, int max_ms) {
    int delay = xrtRandRange(min_ms, max_ms);
    xrtSleep(delay);
}

// 使用
RandomDelay(100, 500);  // 100-500ms 随机延迟
```

---

### 7. UUID/GUID 生成

```c
// 简化版 UUID 生成（非标准）
str GenerateSimpleUUID() {
    return xrtFormat(
        "%08X-%04X-%04X-%04X-%012llX",
        xrtRand32(),
        xrtRand32() & 0xFFFF,
        xrtRand32() & 0xFFFF,
        xrtRand32() & 0xFFFF,
        xrtRand64() & 0xFFFFFFFFFFFFULL
    );
}

str uuid = GenerateSimpleUUID();
printf("UUID: %s\n", uuid);
xrtFree(uuid);
```

---

## 最佳实践

### 1. 初始化种子

```c
int main() {
    xrtInit();  // 自动初始化随机数种子
    
    // 如需自定义种子
    xrtSetRandSeed32(time(NULL), getpid());
    
    // 使用随机数
    // ...
}
```

---

### 2. 可重现的随机序列

```c
// 用于测试：固定种子
#ifdef DEBUG
    xrtSetRandSeed32(12345, 0);
#else
    // 生产环境：使用时间
    xrtSetRandSeed32(time(NULL), 0);
#endif
```

---

### 3. 避免模运算偏差

```c
// ❌ 有偏差的方法
int bad_random = xrtRand32() % 100;  // 0-99，有轻微偏差

// ✅ 无偏差的方法
int good_random = xrtRandRange(0, 99);  // 内部处理偏差
```

---

### 4. 浮点随机数

```c
// 生成 [0.0, 1.0) 的浮点随机数
double RandomDouble() {
    return (double)xrtRand32() / (double)0x100000000ULL;
}

// 生成 [min, max) 的浮点随机数
double RandomDoubleRange(double min, double max) {
    return min + (max - min) * RandomDouble();
}

// 使用
double price = RandomDoubleRange(10.0, 100.0);
```

---

### 5. 多线程安全

```c
// 使用互斥锁保护随机数生成
#include "thread.h"

static xmutex rand_mutex;

void InitRandom() {
    xrtInit();
    // 初始化互斥锁
    pthread_mutex_init(&rand_mutex, NULL);
}

uint32 ThreadSafeRand32() {
    pthread_mutex_lock(&rand_mutex);
    uint32 result = xrtRand32();
    pthread_mutex_unlock(&rand_mutex);
    return result;
}
```

---

### 6. 权重随机选择

```c
// 根据权重随机选择
int WeightedRandom(int* items, double* weights, int count) {
    double total = 0;
    for (int i = 0; i < count; i++) {
        total += weights[i];
    }
    
    double random = RandomDouble() * total;
    double cumulative = 0;
    
    for (int i = 0; i < count; i++) {
        cumulative += weights[i];
        if (random < cumulative) {
            return items[i];
        }
    }
    
    return items[count - 1];
}

// 使用
int items[] = {1, 2, 3, 4};
double weights[] = {0.1, 0.2, 0.3, 0.4};  // 权重
int selected = WeightedRandom(items, weights, 4);
```

---

## 性能提示

### 1. 批量生成

```c
// ❌ 低效：频繁函数调用
for (int i = 0; i < 1000000; i++) {
    int r = xrtRandRange(1, 100);
    Process(r);
}

// ✅ 高效：预生成
int* randoms = xrtMalloc(1000000 * sizeof(int));
for (int i = 0; i < 1000000; i++) {
    randoms[i] = xrtRandRange(1, 100);
}
for (int i = 0; i < 1000000; i++) {
    Process(randoms[i]);
}
xrtFree(randoms);
```

---

### 2. 避免重复初始化

```c
// ❌ 错误：每次都重置种子
void BadRandom() {
    xrtSetRandSeed32(time(NULL), 0);  // 同一秒内结果相同
    return xrtRand32();
}

// ✅ 正确：只在开始时初始化一次
void Setup() {
    xrtSetRandSeed32(time(NULL), 0);
}

void GoodRandom() {
    return xrtRand32();  // 每次调用结果不同
}
```

---

## 算法质量

### PCG 随机数生成器

XRT 使用 **PCG (Permuted Congruential Generator)** 算法：

**优点：**
- ✅ 高质量随机性
- ✅ 速度快（比标准 `rand()` 快）
- ✅ 周期长（PCG32: 2^64, PCG64: 2^128）
- ✅ 通过多项随机性测试（TestU01）

**对比：**
- 比 `rand()` 质量更高
- 比 `MT19937` 更快
- 适合游戏、模拟、测试等场景

---

## 常见错误

### 1. 忘记初始化

```c
// ❌ 错误：未初始化
int main() {
    int r = xrtRand32();  // 可能结果不理想
}

// ✅ 正确：先初始化
int main() {
    xrtInit();  // 初始化种子
    int r = xrtRand32();
}
```

---

### 2. 范围理解错误

```c
// xrtRandRange 包含两端
int r = xrtRandRange(1, 6);  // 可能是 1,2,3,4,5,6

// 如需 [1, 6) 范围
int r = xrtRandRange(1, 5);  // 1,2,3,4,5
```

---

## 相关文档

- [Base 基础函数库](api-base.md) - 库初始化
- [String 字符串处理](api-string.md) - 随机字符串生成
- [Time 时间处理](api-time.md) - 时间种子
- [主索引](README.md) - 返回文档首页

---

<div align="center">

[⬆️ 返回顶部](#math-数学运算库)

</div>
