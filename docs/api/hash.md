# Hash API

## 设计契约

XRT 把“快速确定性哈希”和“哈希表抗碰撞攻击”拆成两个契约，不再把校验和、内容寻址、唯一 ID 或密码摘要混进 `hash`：

- `XRT_FEATURE_HASH32`：nmhash32x v2.0，适合 32 位分桶和紧凑索引。
- `XRT_FEATURE_HASH64`：rapidhash v3.0 compact，适合通用内存索引和低碰撞率分桶。
- `XRT_FEATURE_HASH_KEYED`：SipHash-2-4，一次性与流式带密钥哈希，适合不可信哈希表键。

三个功能组只依赖 core，可以分别裁剪。相同字节、长度和 seed/key 在支持的平台、字节序、指针宽度和编译器之间得到相同结果。32/64 位确定性函数的输出已经按旧版向量冻结；升级 XRT 不会静默更换算法。

哈希冲突始终可能发生。哈希表必须在哈希相等后继续比较原始键，不能把哈希值当作唯一标识。

## 用途边界

| 需求 | 应使用 |
|---|---|
| 进程内可信键分桶 | `xrtHash64`，32 位表可选 `xrtHash32` |
| 不可信请求字段作为哈希表键 | 每个表或进程使用随机 `xsipkey` 的 `xrtSipHash` |
| 固定跨节点分片 | 固定 seed 的 `xrtHash32Seed` 或 `xrtHash64Seed` |
| 文件传输误码检测 | checksum 模块的 CRC 等明确校验算法 |
| 内容寻址、防篡改、签名、密码 | crypto 模块的 SHA/HMAC 等密码摘要 |
| 唯一标识 | XID/UUID 类标识生成器 |

普通 seed 只改变确定性输出，不是密钥，也不让 nmhash32x 或 rapidhash 获得密码学安全性。旧文档中“随机 seed 即可防 Hash-DoS”“快速哈希可做文件完整性或内容寻址”的说法已经废止。

## 确定性哈希

### `xrtHash32` 与 `xrtHash64`

```c
uint32 xrtHash32(const void* pData, size_t iSize);
uint64 xrtHash64(const void* pData, size_t iSize);
```

最短名称保留给默认 seed 为零的常见路径。函数读取恰好 `iSize` 字节，不要求对齐，也不把文本零字节视为结尾。`pData == NULL && iSize == 0` 是合法空输入；非零长度配空指针返回零并设置 `XERR_ARGUMENT`。

`xrtHash32` 不是简单截断 `xrtHash64`。它保留专门的 32 位算法，在 32 位容器中避免不必要的 64 位运算和存储。长输入根据编译目标使用 SIMD 或无未定义行为的标量路径，两条路径输出相同。

### `xrtHash32Seed` 与 `xrtHash64Seed`

```c
uint32 xrtHash32Seed(const void* pData, size_t iSize, uint32 iSeed);
uint64 xrtHash64Seed(const void* pData, size_t iSize, uint64 iSeed);
```

显式 seed 适合固定分片、布隆过滤器派生和调用方定义的独立哈希域。需要跨进程稳定时，输入序列化方式和 seed 都必须固定：文本编码、换行、数值字节序、字段顺序和结构体填充都会改变输入字节。

不要直接哈希含有未初始化填充的 C 结构体。稳定协议应逐字段编码为明确字节序后再哈希。

## 带密钥哈希

### `xsipkey` 与 `xrtSipKey`

```c
typedef struct xsipkey {
	uint64 Low;
	uint64 High;
} xsipkey;

xsipkey xrtSipKey(uint64 iLow, uint64 iHigh);
```

SipHash 使用完整 128 位密钥。面向不可信输入的哈希表应从操作系统密码随机源生成密钥，并按表或进程保存；不能使用时间、地址、递增计数或普通伪随机默认 seed 代替。`xrtSipKey` 只组装调用方已有的两个字，不负责产生随机性，因此 hash 模块不会被迫依赖 random/crypto。

后续容器模块会在自身生命周期内持有密钥；底层用户仍可显式提供密钥，避免隐藏全局状态。

### `xrtSipHash`

```c
uint64 xrtSipHash(const void* pData, size_t iSize, xsipkey Key);
```

一次性便捷函数执行 SipHash-2-4。它与流式 API 共享同一实现和结果，可直接处理空输入与嵌入零字节。SipHash 用于短输入认证式散列和抗选择碰撞，不替代通用消息认证码；协议认证仍使用 crypto 模块的 HMAC 或专用 AEAD。

## 流式 SipHash

### `xsiphash`

状态对象由调用方放在栈、对象或连接上下文中，不执行堆分配。字段公开是为了让 C 用户明确控制存储，不代表允许直接修改。状态可以按值复制，以便做分支计算或快照。

### `xrtSipHashInit`

```c
void xrtSipHashInit(xsiphash* pState, xsipkey Key);
```

初始化或重置状态。每次使用前都必须调用。库会写入 guard，并在后续操作中校验尾部长度与累计长度关系；漏初始化或状态损坏返回 `XERR_STATE`，不会访问越界尾部。

### `xrtSipHashUpdate`

```c
bool xrtSipHashUpdate(xsiphash* pState, const void* pData, size_t iSize);
```

追加任意分块。实现最多在状态中保留 7 个尾字节，不随总输入增长分配缓冲区。输入字节不能与状态对象重叠，避免更新状态时破坏尚未读取的数据。空块是合法 no-op。参数、状态或累计长度检查失败时，本次调用不修改状态；累计长度超过 `uint64` 时设置 `XERR_RANGE`。

### `xrtSipHashFinal`

```c
uint64 xrtSipHashFinal(const xsiphash* pState);
```

在状态副本上终结，因此可以重复调用，也可以在观察中间结果后继续 `Update`。这避免了“Final 后状态是否失效”的隐式规则。

```c
xsipkey Key = xrtSipKey(secret0, secret1);
xsiphash State;

xrtSipHashInit(&State, Key);
xrtSipHashUpdate(&State, header, headerSize);
xrtSipHashUpdate(&State, body, bodySize);
uint64 iHash = xrtSipHashFinal(&State);
```

## 线程与所有权

一次性函数没有可变全局状态，可以并发调用。同一个 `xsiphash` 不能由多个线程同时修改；不同状态完全独立。所有函数只借用输入，不保存指针、不分配结果，也不要求调用初始化整个 XRT 运行时。

## 旧 API 决策

- `_WithSeed` 改为短且统一的 `Seed` 后缀，不保留双版本名称。
- `xrtHash64_Micro` 与 `xrtHash64_Nano` 不再公开。它们的输出差异和长度选择不应泄漏到业务代码；标准 `xrtHash64` 固定使用经过验证的 compact profile。
- `HASH32_SEED`、`HASH64_SEED` 宏删除。默认值是 API 契约，不允许应用通过预处理器悄悄改变全库行为。
- seed 版本不再宣传为安全哈希；需要抗攻击时使用 128 位 keyed API。

## 完整示例

- `examples/hash/hash32/main.c`：默认和显式 seed 的 32 位哈希。
- `examples/hash/hash64/main.c`：通用确定性 64 位哈希。
- `examples/hash/keyed/main.c`：不可信键的分块 SipHash。

第三方算法来源和许可证见 `docs/THIRD_PARTY.md`。
