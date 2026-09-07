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

### `xrtHash32`

nmhash32x v2.0 一次性 32 位哈希（默认 seed 为零）；输出已按旧版向量冻结。

```c
uint32 xrtHash32(const void* pData, size_t iSize);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pData` | 输入 | 借用；空输入允许空指针 | 任意字节，不要求对齐 |
| `iSize` | 输入 | — | 字节数；显式长度，零字节不是结尾 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| `> 0` | 哈希值 | — |
| `0` | 非零长度配空指针 | `XERR_ARGUMENT` |

#### 错误

- `XERR_ARGUMENT` — 非零 `iSize` 配空 `pData`（零输入返回零值域哈希）

#### 范例

[hash/hash32 · 默认 seed](../../examples/hash/hash32/main.c) · 观察

```c
	printf("default: %08X\n", (unsigned int)xrtHash32(sKey, sizeof(sKey) - 1u));
```


### `xrtHash64`

rapidhash v3.0 compact 一次性 64 位哈希（默认 seed 为零）；适合通用内存索引与低碰撞分桶。

```c
uint64 xrtHash64(const void* pData, size_t iSize);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pData` | 输入 | 借用；空输入允许空指针 | 任意字节 |
| `iSize` | 输入 | — | 字节数 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| `> 0` | 哈希值 | — |
| `0` | 非零长度配空指针 | `XERR_ARGUMENT` |

#### 错误

- `XERR_ARGUMENT` — 非零 `iSize` 配空 `pData`

#### 范例

[hash/hash64 · 通用索引](../../examples/hash/hash64/main.c) · 观察

```c
	uint64 iHash = xrtHash64(sKey, sizeof(sKey) - 1u);
```


### `xrtHash32Seed`

带显式 seed 的 32 位哈希；适合固定分片、布隆过滤与独立哈希域。

```c
uint32 xrtHash32Seed(const void* pData, size_t iSize, uint32 iSeed);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pData` | 输入 | 借用 | 任意字节 |
| `iSize` | 输入 | — | 字节数 |
| `iSeed` | 输入 | — | 分片/域 seed（不是密钥） |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| `> 0` | 哈希值 | — |
| `0` | 非零长度配空指针 | `XERR_ARGUMENT` |

#### 错误

- `XERR_ARGUMENT` — 非零 `iSize` 配空 `pData`

#### 范例

[hash/hash32 · 固定 seed](../../examples/hash/hash32/main.c) · 观察

```c
	printf("seeded : %08X\n", (unsigned int)xrtHash32Seed(sKey,
		sizeof(sKey) - 1u, UINT32_C(0x12345678)));
```


### `xrtHash64Seed`

带显式 seed 的 64 位哈希；跨节点分片时输入序列化与 seed 都必须固定。

```c
uint64 xrtHash64Seed(const void* pData, size_t iSize, uint64 iSeed);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pData` | 输入 | 借用 | 任意字节 |
| `iSize` | 输入 | — | 字节数 |
| `iSeed` | 输入 | — | 分片 seed |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| `> 0` | 哈希值 | — |
| `0` | 非零长度配空指针 | `XERR_ARGUMENT` |

#### 错误

- `XERR_ARGUMENT` — 非零 `iSize` 配空 `pData`

#### 范例

[hash/variants · 分片](../../examples/hash/variants/main.c) · 观察

```c
		(unsigned long long)xrtHash64Seed("user:42", 7u, UINT64_C(0x1234)));
```



## 带密钥哈希

### `xrtSipHash`

SipHash-2-4 一次性带密钥哈希；面向不可信哈希表键的抗选择碰撞散列。

```c
uint64 xrtSipHash(const void* pData, size_t iSize, xsipkey Key);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pData` | 输入 | 借用；空输入允许空指针 | 任意字节（可含嵌入零） |
| `iSize` | 输入 | — | 字节数 |
| `Key` | 输入 | — | 128 位密钥 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| `> 0` | 哈希值 | — |
| `0` | 非零长度配空指针 | `XERR_ARGUMENT` |

#### 错误

- `XERR_ARGUMENT` — 非零 `iSize` 配空 `pData`
- 注：不替代通用 MAC——协议认证仍使用 crypto 模块 HMAC/AEAD

#### 范例

[hash/variants · 一次性](../../examples/hash/variants/main.c) · 观察

```c
		(unsigned long long)xrtSipHash("request:user-input", 18u, Key));
```


### `xrtSipKey`

组装 128 位 SipHash 密钥；只拼装调用方已有的两个字，不产生随机性。

```c
xsipkey xrtSipKey(uint64 iLow, uint64 iHigh);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `iLow` | 输入 | — | 密钥低 64 位 |
| `iHigh` | 输入 | — | 密钥高 64 位 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| `xsipkey` | 按值返回的密钥结构 | — |

#### 错误

- 无 — 纯组装；密钥应来自系统密码随机源

#### 范例

[hash/keyed · 密钥](../../examples/hash/keyed/main.c) · 观察

```c
	xsipkey Key = xrtSipKey(UINT64_C(0x0123456789ABCDEF),
```



## 流式 SipHash

### `xsiphash`

状态对象由调用方放在栈、对象或连接上下文中，不执行堆分配。字段公开是为了让 C 用户明确控制存储，不代表允许直接修改。状态可以按值复制，以便做分支计算或快照。

### `xrtSipHashInit`

初始化或重置流式 SipHash 状态；每次使用前必须调用。

```c
void xrtSipHashInit(xsiphash* pState, xsipkey Key);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pState` | 输出 | 非空 | 调用方持有（栈/对象/连接上下文） |
| `Key` | 输入 | — | 128 位密钥 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| 无 | 纯初始化，不失败 | — |

#### 错误

- 无 — 初始化不失败；库写入 guard 供后续校验

#### 范例

[hash/keyed · 流式](../../examples/hash/keyed/main.c) · 观察

```c
	xrtSipHashInit(&State, Key);
```


### `xrtSipHashUpdate`

向流式状态追加任意分块；最多保留 7 个尾字节，不随输入增长分配。失败时状态不变。

```c
bool xrtSipHashUpdate(xsiphash* pState, const void* pData, size_t iSize);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pState` | 输入/输出 | 已 Init | 流状态 |
| `pData` | 输入 | 借用、不得与状态重叠 | 输入字节；空块是合法 no-op |
| `iSize` | 输入 | — | 字节数 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| `true` | 已并入状态 | — |
| `false` | 参数/状态错误或累计溢出 | 状态不变；错误经 `xrtGetError()` 报告 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或输入与状态重叠
- `XERR_STATE` — 状态未初始化或 guard 损坏
- `XERR_RANGE` — 累计长度超过 `uint64`

#### 范例

[hash/keyed · 流式](../../examples/hash/keyed/main.c) · 观察

```c
	if ( !xrtSipHashUpdate(&State, "request:", 8) ||
		 !xrtSipHashUpdate(&State, "user-input", 10) ) {
```


### `xrtSipHashFinal`

在状态副本上终结并返回 64 位结果；可重复调用，也可观察中间结果后继续 Update。

```c
uint64 xrtSipHashFinal(const xsiphash* pState);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pState` | 输入 | 已 Init | 流状态（副本上终结） |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| `> 0` | 哈希值 | — |
| `0` | 状态非法 | `XERR_STATE` |

#### 错误

- `XERR_STATE` — 状态未初始化或尾部长度与累计不符

#### 范例

[hash/keyed · 流式](../../examples/hash/keyed/main.c) · 观察

```c
	iHash = xrtSipHashFinal(&State);
	printf("%016llX\n", (unsigned long long)iHash);
```



## 模块契约：错误

哈希 API 的失败经 `xrtGetError()` 报告：

| 错误 | 触发场景 |
|---|---|
| `XERR_ARGUMENT` | 指针为空、长度为零或种子非法 |
| `XERR_RANGE` | 单次更新超过单块上限（需分段） |
| `XERR_STATE` | 上下文已终结后继续使用 |

## 线程与所有权

流式三段式的完整形态（占位符示意，可运行版本见 keyed 范例）：

```c
xsipkey Key = xrtSipKey(secret0, secret1);
xsiphash State;

xrtSipHashInit(&State, Key);
xrtSipHashUpdate(&State, header, headerSize);
xrtSipHashUpdate(&State, body, bodySize);
uint64 iHash = xrtSipHashFinal(&State);
```


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
