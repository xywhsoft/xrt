# Math 与 Hash 入门：随机数、约等于和稳定指纹不是一回事

> 目标：把 `xrtRand*`、`xrtIntApprox()` / `xrtNumApprox()`、`xrtHash32()` / `xrtHash64*()` 讲成一组“基础计算工具”。读完这页后，你应该明确知道：什么时候你要的是随机选择，什么时候你要的是容差判断，什么时候你要的是稳定指纹；以及多线程、跨进程、跨平台时，这三组能力分别该怎么用。

[返回教学文档](README.md)

---

## 1. 为什么要把 `math / hash` 单独讲一页

很多小程序、工具程序和服务端代码，都会很快碰到这几类需求：

- 做一个随机退避时间
- 给压测、抽样、试验流量生成随机值
- 判断“这次结果是不是已经足够接近目标值”
- 给请求参数、缓存键、分桶路由生成稳定指纹

这些需求看起来都像“算一算”，但它们解决的其实是三件完全不同的事：

- `随机数`
	- 解决“从一个范围里选一个值”
- `约等于`
	- 解决“这两个数是否已经够接近”
- `哈希`
	- 解决“把同一份输入稳定映射成一个短指纹”

如果这三件事混着用，代码很快就会出问题：

- 用随机数去做稳定键
- 用哈希去做安全校验
- 用 `==` 去比较本来就该允许误差的数值

所以这一页的重点，不是公式，而是边界。


## 2. 先把 6 个边界分清楚

### 2.1 随机数不是哈希

随机数的目标是：

- 每次调用都可能不同
- 在一个范围里挑值
- 用在退避、抽样、洗牌、模拟

哈希的目标是：

- 同一份输入始终得到同一结果
- 输入稍有变化，结果也会明显变化
- 用在分桶、缓存键、快速指纹、查找结构

更直接地说：

- 你要“每次都可能不一样”，想随机数
- 你要“每次都必须一样”，想哈希


### 2.2 `xrtRand32()` / `xrtRand64()` / `xrtRandRange()` 是方便版，不是并发版

XRT 里的随机数有两条线：

- 方便版
	- `xrtRand32()`
	- `xrtRand64()`
	- `xrtRandRange()`
- 调用者管理状态的 Ex 版
	- `xrtRandSeed()`
	- `xrtRand32Ex()`
	- `xrtRand64Ex()`
	- `xrtRandRangeEx()`

最稳的理解是：

- 单线程小程序、脚本、简单工具
	- 方便版就够用
- 多线程 worker、长期服务、想要可重现序列
	- 直接上 Ex 版

因为 Ex 版的核心价值是：

- 你自己持有 `xrand`
- 你自己决定 seed 和 sequence
- 你不会和别的线程抢同一份全局随机状态


### 2.3 `约等于` 解决的是“业务容差”，不是“二进制完全相等”

`xrtIntApprox()` 和 `xrtNumApprox()` 适合的场景是：

- 成功数离目标值差一点点还能接受
- 延迟、价格、统计值允许一定误差
- 浮点比较不应该死盯 `==`

它们不适合的场景是：

- 结构字段完全相等校验
- 哈希值比较
- 业务主键比较

另外它们还有一个必须记住的边界：

- 这两个函数使用的是运行时全局容差配置

也就是说，你在使用前应该明确设置：

- `pCore->iApproxIntMode`
- `pCore->fApproxIntTol`
- `pCore->iApproxNumMode`
- `pCore->fApproxNumTol`

不要默认指望“库里刚好是我想要的容差”。


### 2.4 快速哈希不是加密摘要

`xrtHash32()`、`xrtHash64()`、`xrtHash64_Micro()`、`xrtHash64_Nano()` 适合：

- 哈希表键
- 分桶路由
- 缓存键
- 快速内容指纹

它们不适合直接承担：

- 防篡改签名
- 认证摘要
- 密码学安全校验
- 安全令牌

如果你要的是：

- 加密安全随机字节
- SHA / HMAC / HKDF

那条线应该去看 `crypto`，不是继续把快速哈希往安全场景里硬推。


### 2.5 32 位哈希、64 位哈希、Micro、Nano 各自服务不同场景

可以先这样记：

- `xrtHash32()`
	- 适合分桶、哈希表、短键查找
- `xrtHash64()`
	- 适合更稳的大范围快速指纹
- `xrtHash64_Micro()`
	- 适合中小块数据
- `xrtHash64_Nano()`
	- 适合很短的字符串或小键名

所以不要上来就死记哪个“更高级”，先看你的输入大小和用途。


### 2.6 跨平台稳定性靠的是“同样的字节流”，不是“看起来一样的文本”

哈希和 XID 都有一个很容易忽略的问题：

- 看起来一样的文本
	- 不一定是同样的字节

例如：

- UTF-8 和 UTF-16
- `\n` 和 `\r\n`
- 多一个空格
- URL 参数顺序不同

这些都会改变哈希结果。

所以如果你要跨平台、跨进程稳定一致，先保证的是：

- 同样的编码
- 同样的换行
- 同样的拼接顺序
- 同样的 seed


## 3. 最小 DEMO：用固定 seed 做一份可重现的随机退避序列

第一步先不要碰哈希。  
先把最容易写错的一条线走稳：

- 你要的是随机退避
- 但你又希望在测试里能复现同一串结果

这时最稳的主线就是：

- `xrand`
- `XRAND_INITIALIZER`
- `xrtRandSeed()`
- `xrtRandRangeEx()`

```c
#include "xrt.h"
#include <stdio.h>

static void procPrintRetryPlan(xrand* pRng, const char* sTitle)
{
	int i = 0;

	printf("%s\n", sTitle);
	for ( i = 0; i < 6; i++ ) {
		int iDelayMs = xrtRandRangeEx(pRng, 80, 140);
		printf("  retry[%d] delay = %d ms\n", i, iDelayMs);
	}
}


int main(void)
{
	xrtGlobalData* pCore = NULL;
	xrand rngRetry = XRAND_INITIALIZER;

	pCore = xrtInit();
	if ( pCore == NULL ) {
		return 1;
	}

	xrtRandSeed(&rngRetry, 20260331ull, 7ull);
	procPrintRetryPlan(&rngRetry, "first run:");

	xrtRandSeed(&rngRetry, 20260331ull, 7ull);
	procPrintRetryPlan(&rngRetry, "second run with same seed:");

	xrtUnit();
	return 0;
}
```

这个最小例子最想让你先记住的是：

1. 你想复现随机序列时，不要直接依赖全局随机状态
2. Ex 版随机数的价值，不只是“线程安全”，还是“我能重放同一串结果”
3. 退避、抽样、试验参数这类场景，固定 seed 在测试里非常有用


## 4. 升级 DEMO：把随机试验和容差判断接成一条质量检查主线

很多时候你不是只想“随便取一个数”，而是想做一条完整判断：

- 先用随机数模拟一批采样
- 再看这批结果是否已经足够接近目标

这时就可以把：

- `xrtRandRangeEx()`
- `xrtIntApprox()`
- `xrtNumApprox()`

接成一条完整工具链。

```c
#include "xrt.h"
#include <stdio.h>

int main(void)
{
	xrtGlobalData* pCore = NULL;
	xrand rngSample = XRAND_INITIALIZER;
	int i = 0;
	int iOkCount = 0;
	double fLatencySum = 0.0;
	double fAvgLatency = 0.0;
	int iTargetOkCount = 95;
	double fTargetLatency = 200.0;

	pCore = xrtInit();
	if ( pCore == NULL ) {
		return 1;
	}

	xrtRandSeed(&rngSample, 12345ull, 3ull);

	for ( i = 0; i < 100; i++ ) {
		int iLatencyMs = xrtRandRangeEx(&rngSample, 185, 215);
		fLatencySum += (double)iLatencyMs;
		if ( iLatencyMs <= 205 ) {
			iOkCount++;
		}
	}
	fAvgLatency = fLatencySum / 100.0;

	pCore->iApproxIntMode = XRT_APPROX_DIFF;
	pCore->fApproxIntTol = 3.0;

	pCore->iApproxNumMode = XRT_APPROX_PERCENT;
	pCore->fApproxNumTol = 0.03;

	printf("ok count      : %d / 100\n", iOkCount);
	printf("avg latency   : %.2f ms\n", fAvgLatency);
	printf(
		"ok near goal  : %s\n",
		xrtIntApprox(iOkCount, iTargetOkCount) ? "yes" : "no"
	);
	printf(
		"lat near goal : %s\n",
		xrtNumApprox(fAvgLatency, fTargetLatency) ? "yes" : "no"
	);

	xrtUnit();
	return 0;
}
```

这个升级例子真正想教会你的事有 5 个：

1. `约等于` 应该服务业务容差，而不是替代所有比较
2. 整数容差和浮点容差应该分开配
3. `差值模式` 和 `百分比模式` 是两套不同语义
4. 容差配置最好在一个清晰的位置集中设置
5. 单元测试、压测 smoke check、数据波动判断都很适合这条线


## 5. 再升级：用稳定哈希做分桶和缓存指纹

当你已经把“随机”和“容差”分清后，下一步最常见的需求就是：

- 给请求做分桶
- 给缓存内容做稳定指纹
- 给短键和长 payload 选不同哈希变体

这时主线就变成：

- `xrtHash32()`
- `xrtHash64_Nano()`
- `xrtHash64_Micro()`
- `xrtHash64()`

```c
#include "xrt.h"
#include <stdio.h>
#include <string.h>

static uint64 procPickHash64(const char* sData, size_t iLen)
{
	if ( iLen <= 48u ) {
		return xrtHash64_Nano((ptr)sData, iLen);
	}
	if ( iLen <= 512u ) {
		return xrtHash64_Micro((ptr)sData, iLen);
	}
	return xrtHash64((ptr)sData, iLen);
}


int main(void)
{
	xrtGlobalData* pCore = NULL;
	const char* sRoute = "/api/dashboard";
	const char* sQuery = "user=42&tab=overview&lang=zh-CN";
	const char* sPayload =
		"{\"widgets\":[\"sales\",\"ops\",\"alerts\"],"
		"\"range\":\"7d\",\"refresh\":true}";
	str sOutDir = NULL;
	str sOutPath = NULL;
	char sCacheKey[512] = {0};
	char sReport[2048] = {0};
	size_t iCacheKeyLen = 0;
	size_t iPayloadLen = 0;
	uint32 u32BucketHash = 0;
	uint32 u32Bucket = 0;
	uint64 u64KeyHash = 0;
	uint64 u64PayloadHash = 0;
	int iRet = 1;

	pCore = xrtInit();
	if ( pCore == NULL ) {
		return 1;
	}

	if ( snprintf(sCacheKey, sizeof(sCacheKey), "%s?%s", sRoute, sQuery) < 0 ) {
		fprintf(stderr, "build cache key failed\n");
		goto cleanup;
	}

	iCacheKeyLen = strlen(sCacheKey);
	iPayloadLen = strlen(sPayload);

	u32BucketHash = xrtHash32((ptr)sCacheKey, iCacheKeyLen);
	u32Bucket = u32BucketHash % 16u;
	u64KeyHash = procPickHash64(sCacheKey, iCacheKeyLen);
	u64PayloadHash = procPickHash64(sPayload, iPayloadLen);

	sOutDir = xrtPathJoin(3, pCore->AppPath, "build", "math-hash-demo");
	sOutPath = xrtPathJoin(4, pCore->AppPath, "build", "math-hash-demo", "hash-report.txt");
	if ( (sOutDir == NULL) || (sOutPath == NULL) ) {
		fprintf(stderr, "build output path failed\n");
		goto cleanup;
	}

	if ( xrtDirCreateAll(sOutDir) != TRUE ) {
		fprintf(stderr, "create output dir failed\n");
		goto cleanup;
	}

	if ( snprintf(
		sReport,
		sizeof(sReport),
		"cache key      : %s\r\n"
		"bucket hash32  : 0x%08X\r\n"
		"bucket index   : %u\r\n"
		"key hash64     : 0x%016llX\r\n"
		"payload hash64 : 0x%016llX\r\n",
		sCacheKey,
		u32BucketHash,
		u32Bucket,
		(unsigned long long)u64KeyHash,
		(unsigned long long)u64PayloadHash
	) < 0 ) {
		fprintf(stderr, "format report failed\n");
		goto cleanup;
	}

	if ( xrtFileWriteAll(sOutPath, sReport, 0u, XRT_CP_UTF8) <= 0 ) {
		fprintf(stderr, "write report failed\n");
		goto cleanup;
	}

	printf("%s", sReport);
	printf("report         : %s\n", sOutPath);
	iRet = 0;

cleanup:
	if ( sOutPath != NULL ) {
		xrtFree(sOutPath);
	}
	if ( sOutDir != NULL ) {
		xrtFree(sOutDir);
	}
	xrtUnit();
	return iRet;
}
```

这个例子里最关键的教学点是：

1. 分桶适合优先用 `xrtHash32()`
2. 短键和中等长度 payload 可以优先考虑 `Nano / Micro`
3. 真正大的输入再上标准 `xrtHash64()`
4. 同一份输入、同样的字节流，会得到稳定相同的哈希
5. 哈希结果适合做分桶和指纹，不适合直接当业务展示文本


## 6. 如果只有一条主线程，会卡在哪里

`math / hash` 这条线通常不会像 I/O 那样“卡住主线程很久”，但它也有自己的代价：

- 反复重新哈希大 payload，会烧 CPU
- 在多线程里共用全局随机状态，会让结果不可控
- 容差配置到处乱改，会让判断语义漂移

所以这里真正要优化的不是“阻塞等待”，而是：

- 让随机状态归属清晰
- 让容差配置集中
- 让大输入的哈希不要无意义重复计算


## 7. Windows / Linux 跨平台时最该记住的事

- 固定 seed 的 Ex 随机序列更适合跨平台复现测试
- 哈希稳定性取决于输入字节流，编码和换行必须统一
- 不要把本地默认编码下的文本直接拿来期望跨平台同哈希
- `double` 比较要通过容差语义理解，而不是想象成“平台一定全等”


## 8. 常见错误

### 错误一：用 `xrtRand32() % n` 替代 `xrtRandRange()`

这会把“范围选择”和“模运算偏差”混在一起。  
如果你要范围随机值，优先 `xrtRandRange()` 或 `xrtRandRangeEx()`。

### 错误二：多线程里继续共用全局随机 API

一旦随机状态要跨线程，直接切到 Ex 版，别让全局状态成为隐式耦合。

### 错误三：把快速哈希当成安全摘要

快速哈希适合分桶和快速指纹，不适合承担密码学安全场景。

### 错误四：缓存键或分桶键用了不稳定 seed

持久化缓存、跨进程分桶、跨节点路由这类场景，seed 必须稳定。  
只有在进程内抗碰撞防护这类场景，才适合引入随机 seed。

### 错误五：用 `==` 死磕本来就该允许误差的数据

如果业务语义本来就是“差一点也算通过”，那就应该明确配置容差，而不是继续把所有数据都按绝对相等处理。

### 错误六：忽略编码和换行差异

同样一句文本，在不同编码和换行下并不是同样的输入字节。


## 9. 建议继续阅读

- [Buffer 入门：什么时候你需要一块会长大的字节区，而不是数组和字符串](buffer-intro.md)
- [Math API](../api/api-math.md)
- [Hash API](../api/api-hash.md)
- [XID 入门：什么时候该直接生成唯一 ID，什么时候还不够](xid-intro.md)
- [时间、路径与文件系统入门：先把程序落到真实世界](time-path-file-intro.md)
- [用 `file + path + value + json` 构造配置系统](../case/json-config-system.md)

---

**一句话总结：** `math / hash` 这组能力最重要的不是“会不会调用函数”，而是先分清楚随机、容差和稳定指纹是三种完全不同的语义，再把正确的那一组工具放进正确的位置。 
