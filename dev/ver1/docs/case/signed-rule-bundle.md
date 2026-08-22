# 用 Charset + Regex + Crypto 导入一个签名规则包

> 这个案例聚焦“未知编码文本文件 -> 统一成 UTF-8 -> 逐行提取字段 -> HMAC 验签 -> 编译规则集合”这条主线，把 `charset + regex + crypto + file` 串成一个实际能落地的导入链。

[返回案例索引](README.md)

---

## 1. 场景

假设你要接一个“外部团队交付的文本规则包”，它有 6 个约束：

1. 文件编码不稳定，可能是 UTF-8，也可能是 UTF-16 BOM
2. 文件格式必须保持可人工编辑，而不是强迫对方改成二进制
3. 你需要从文本里提取多条 `route=` 规则
4. 导入前必须确认文件没有被篡改
5. 导入完成后，运行时希望直接拿着已编译规则去匹配路径
6. Windows / Linux 上都要沿同一套运行时语义工作

如果没有一条清晰主线，代码很容易变成：

- 先凭感觉猜文件编码
- 一边 `strchr()` 一边手搓 `key=value`
- 验签逻辑散在几处字符串拼接里
- 导入完以后还要再把规则重新编译一遍

这个案例要解决的，正是“签名文本规则包应该怎样沿着 XRT 当前正式主线导入”。


## 2. 为什么这次不用别的方案

### 2.1 为什么不是“先把文件当本机编码读进来再说”

因为这次的问题不是“我这台机器现在能不能看见中文”。

真正的问题是：

- 文件来源不稳定
- 业务主线要统一成 UTF-8
- 同一份包要在 Windows / Linux 上按同样语义导入

所以这页故意从：

- `xrtFileReadAll(..., XRT_CP_AUTO, ...)`

开始，而不是先让业务代码去赌本机代码页。


### 2.2 为什么不是直接把整份文件写成 JSON / XSON

当然可以，但这次的目标是：

- 保持文件像 `.env` / `.ini` 一样可人工编辑
- 用最小格式承载多条规则
- 把教学重点放在 `charset + regex + crypto`

这页故意选择“行文本 + `key=value`”格式，是为了把边界讲清楚，不是为了证明 JSON 不好。


### 2.3 为什么这里选 HMAC，而不是直接“加密文件”

因为这次的核心诉求是：

- 确认规则包没有被篡改
- 确认它来自持有共享密钥的一方

这正是：

- `xrtHMAC_SHA256()`

解决的问题。

如果你把文件“加密了但不校验来源”，导入时仍然分不清：

- 文件是不是被改过
- 密文是不是来自真正的发布方

所以这页故意不走“先加密再解密”的路线，而是先把“认证边界”讲透。


### 2.4 为什么 regex 这里负责“逐行提取”，而不是接管整份语法

因为这正是 `regex` 当前最稳的职责：

- 验证一行像不像 `key=value`
- 提取 `key` 和 `value`
- 编译多条 `route` 规则

它不应该接管：

- 完整语法树
- 多层嵌套对象
- 带状态机的协议解析

也就是说，这里是：

- `regex` 负责局部字段和规则
- 文件格式本身仍然保持简单


## 3. 这条链里每一层负责什么

| 层 | 这次承担的角色 | 你真正得到的能力 |
|----|----------------|------------------|
| `file + charset` | 读取未知编码文本并统一成 UTF-8 | 导入主线不被 BOM / UTF-16 绕乱 |
| 行级 `xregex` | 提取 `key=value` | 每一行的字段边界清楚 |
| `xrtHMAC_SHA256()` | 验证签名行前面的正文 | 来源认证和篡改检测 |
| `xrtHexDecode()` | 把签名行里的十六进制文本还原成二进制 MAC | 文本格式和加密原语接上 |
| `xregexset` | 把多条 `route=` 编译成可直接匹配的规则集合 | 导入完就能进入运行时匹配 |

可以先记一句话：

> `charset` 负责把输入读对，`regex` 负责把文本拆对，`HMAC` 负责把来源验对，`RegexSet` 负责把规则编译成可运行对象。


## 4. 文件格式约定

这页故意把规则包格式压到最小：

```text
bundle=partner-acl
route=^/admin/.+$
route=^/ops/(restart|metrics)$
hmac=4d2f...（64 个 hex 字符）
```

这里有 3 条明确规则：

1. `hmac=` 必须是最后一条有效配置行
2. HMAC 覆盖的是 `hmac=` 之前的 UTF-8 文本字节
3. `route=` 可以重复出现多次，最终统一编译成 `xregexset`

也就是说，这页故意不做“花哨格式”，而是把：

- 文本规范
- 签名范围
- 运行时编译目标

三件事都讲清楚。


## 5. 完整骨架

下面这段代码会做 6 件事：

1. 用 `XRT_CP_AUTO` 读取未知编码规则包
2. 用单行 regex 逐行提取 `key=value`
3. 保存 `bundle=` 和多条 `route=`
4. 记录最后一行 `hmac=`，并拒绝它后面再出现有效内容
5. 用 `xrtHexDecode()` + `xrtHMAC_SHA256()` 完成验签
6. 把所有 `route=` 编译成 `xregexset`

```c
#include "xrt.h"
#include <stdio.h>
#include <string.h>

#define DEMO_MAX_ROUTE	8

typedef struct
{
	char sBundleId[64];
	char* arrRoute[DEMO_MAX_ROUTE];
	uint32 iRouteCount;
	xregexset* pRouteSet;
} DemoSignedRuleBundle;


static bool procAsciiEqualsIgnoreCase(const char* sA, const char* sB)
{
	char cA;
	char cB;

	if ( (sA == NULL) || (sB == NULL) ) {
		return FALSE;
	}

	for ( ;; ) {
		cA = *sA++;
		cB = *sB++;

		if ( (cA >= 'A') && (cA <= 'Z') ) {
			cA = (char)(cA - 'A' + 'a');
		}
		if ( (cB >= 'A') && (cB <= 'Z') ) {
			cB = (char)(cB - 'A' + 'a');
		}

		if ( cA != cB ) {
			return FALSE;
		}
		if ( cA == '\0' ) {
			return TRUE;
		}
	}
}

static char* procCopySpan(const char* sText, xregexspan tSpan)
{
	size_t iLen;
	char* sOut;

	if ( (sText == NULL) || (tSpan.iEnd < tSpan.iBegin) ) {
		return NULL;
	}

	iLen = tSpan.iEnd - tSpan.iBegin;
	sOut = (char*)xrtMalloc(iLen + 1u);
	if ( sOut == NULL ) {
		return NULL;
	}

	memcpy(sOut, sText + tSpan.iBegin, iLen);
	sOut[iLen] = '\0';
	return sOut;
}

static void procFreeBundle(DemoSignedRuleBundle* pBundle)
{
	uint32 i;

	if ( pBundle == NULL ) {
		return;
	}

	if ( pBundle->pRouteSet != NULL ) {
		xrtRegexSetDestroy(pBundle->pRouteSet);
		pBundle->pRouteSet = NULL;
	}

	for ( i = 0; i < pBundle->iRouteCount; i++ ) {
		if ( pBundle->arrRoute[i] != NULL ) {
			xrtFree(pBundle->arrRoute[i]);
			pBundle->arrRoute[i] = NULL;
		}
	}

	pBundle->iRouteCount = 0;
	memset(pBundle->sBundleId, 0, sizeof(pBundle->sBundleId));
}

static bool procVerifyBundleHMAC(const char* sText, size_t iSignedSize, const char* sHexMAC, const char* sSecret)
{
	uint8 aucActual[32];
	uint8* pExpect = NULL;
	size_t iHexLen;
	bool bOk = FALSE;

	if ( (sText == NULL) || (sHexMAC == NULL) || (sSecret == NULL) ) {
		return FALSE;
	}

	iHexLen = strlen(sHexMAC);
	if ( iHexLen != 64u ) {
		return FALSE;
	}

	pExpect = (uint8*)xrtHexDecode((str)sHexMAC, iHexLen);
	if ( pExpect == NULL ) {
		goto cleanup;
	}

	xrtHMAC_SHA256(
		(const uint8*)sSecret,
		strlen(sSecret),
		(const uint8*)sText,
		iSignedSize,
		aucActual
	);

	bOk = memcmp(pExpect, aucActual, sizeof(aucActual)) == 0;

cleanup:
	if ( pExpect != NULL ) {
		xrtFree(pExpect);
	}
	return bOk;
}

static bool procCompileRouteSet(DemoSignedRuleBundle* pBundle)
{
	if ( (pBundle == NULL) || (pBundle->iRouteCount == 0u) ) {
		return FALSE;
	}

	pBundle->pRouteSet = xrtRegexSetCreate((const char* const*)pBundle->arrRoute, pBundle->iRouteCount);
	return pBundle->pRouteSet != NULL;
}

static bool procLoadSignedRuleBundle(const char* sPath, const char* sSecret, DemoSignedRuleBundle* pOut)
{
	static const char sLinePattern[] = "^[ \\t]*(?P<key>[A-Za-z_]+)[ \\t]*=[ \\t]*(?P<value>.*?)[ \\t]*$";
	xregexbuilder* pBuilder = NULL;
	xregex* pLineRegex = NULL;
	str sText = NULL;
	str sHexMAC = NULL;
	size_t iTextSize = 0;
	size_t iLineStart = 0;
	size_t iSignedSize = 0;
	xregexspan arrCaps[3] = {0};
	bool bSeenHMAC = FALSE;
	bool bOk = FALSE;

	memset(pOut, 0, sizeof(*pOut));

	sText = xrtFileReadAll((str)sPath, XRT_CP_AUTO, &iTextSize);
	if ( sText == NULL ) {
		goto cleanup;
	}
	if ( !xrtIsUTF8(sText, iTextSize) ) {
		goto cleanup;
	}

	if ( xrtRegexBuilderCreate(&pBuilder, sLinePattern, strlen(sLinePattern), NULL) != 0 ) {
		goto cleanup;
	}
	xrtRegexBuilderSetFlags(pBuilder, XRT_REGEX_FLAG_INSENSITIVE);
	if ( xrtRegexCreateFromBuilder(&pLineRegex, pBuilder, NULL) != 0 || (pLineRegex == NULL) ) {
		goto cleanup;
	}

	while ( iLineStart < iTextSize ) {
		size_t iLineEnd = iLineStart;
		size_t iParseEnd;
		char* sLine = NULL;
		char* sKey = NULL;
		char* sValue = NULL;

		while ( (iLineEnd < iTextSize) && (sText[iLineEnd] != '\n') ) {
			iLineEnd++;
		}

		iParseEnd = iLineEnd;
		if ( (iParseEnd > iLineStart) && (sText[iParseEnd - 1u] == '\r') ) {
			iParseEnd--;
		}

		if ( iParseEnd == iLineStart ) {
			iLineStart = (iLineEnd < iTextSize) ? (iLineEnd + 1u) : iLineEnd;
			continue;
		}

		sLine = procCopySpan(sText, (xregexspan){iLineStart, iParseEnd});
		if ( sLine == NULL ) {
			goto cleanup;
		}

		if ( sLine[0] == '#' ) {
			xrtFree(sLine);
			iLineStart = (iLineEnd < iTextSize) ? (iLineEnd + 1u) : iLineEnd;
			continue;
		}
		if ( bSeenHMAC ) {
			xrtFree(sLine);
			goto cleanup;
		}

		if ( xrtRegexCaptures(pLineRegex, sLine, strlen(sLine), arrCaps, 3u) <= 0 ) {
			xrtFree(sLine);
			goto cleanup;
		}

		sKey = procCopySpan(sLine, arrCaps[1]);
		sValue = procCopySpan(sLine, arrCaps[2]);
		xrtFree(sLine);
		sLine = NULL;

		if ( (sKey == NULL) || (sValue == NULL) ) {
			if ( sKey != NULL ) {
				xrtFree(sKey);
			}
			if ( sValue != NULL ) {
				xrtFree(sValue);
			}
			goto cleanup;
		}

		if ( procAsciiEqualsIgnoreCase(sKey, "bundle") ) {
			snprintf(pOut->sBundleId, sizeof(pOut->sBundleId), "%s", sValue);
		} else if ( procAsciiEqualsIgnoreCase(sKey, "route") ) {
			if ( pOut->iRouteCount >= DEMO_MAX_ROUTE ) {
				xrtFree(sKey);
				xrtFree(sValue);
				goto cleanup;
			}
			pOut->arrRoute[pOut->iRouteCount++] = sValue;
			sValue = NULL;
		} else if ( procAsciiEqualsIgnoreCase(sKey, "hmac") ) {
			sHexMAC = sValue;
			sValue = NULL;
			iSignedSize = iLineStart;
			bSeenHMAC = TRUE;
		}

		xrtFree(sKey);
		if ( sValue != NULL ) {
			xrtFree(sValue);
		}

		iLineStart = (iLineEnd < iTextSize) ? (iLineEnd + 1u) : iLineEnd;
	}

	if ( !bSeenHMAC || (sHexMAC == NULL) || (pOut->iRouteCount == 0u) ) {
		goto cleanup;
	}
	if ( !procVerifyBundleHMAC(sText, iSignedSize, sHexMAC, sSecret) ) {
		goto cleanup;
	}
	if ( !procCompileRouteSet(pOut) ) {
		goto cleanup;
	}

	bOk = TRUE;

cleanup:
	if ( !bOk ) {
		procFreeBundle(pOut);
	}
	if ( sHexMAC != NULL ) {
		xrtFree(sHexMAC);
	}
	if ( pLineRegex != NULL ) {
		xrtRegexDestroy(pLineRegex);
	}
	if ( pBuilder != NULL ) {
		xrtRegexBuilderDestroy(pBuilder);
	}
	if ( sText != NULL ) {
		xrtFree(sText);
	}
	return bOk;
}

int main(void)
{
	DemoSignedRuleBundle tBundle;
	uint32 arrIndexes[DEMO_MAX_ROUTE] = {0};
	uint32 iHitCount = 0;
	const char* sBundlePath = "build/demo-rules/partner-acl.rules";
	const char* sSecret = "demo-shared-secret";

	xrtInit();

	if ( !procLoadSignedRuleBundle(sBundlePath, sSecret, &tBundle) ) {
		fprintf(stderr, "load signed rule bundle failed\n");
		xrtUnit();
		return 1;
	}

	printf("bundle = %s\n", tBundle.sBundleId);
	printf("route_count = %u\n", (unsigned)tBundle.iRouteCount);

	if ( xrtRegexSetMatches(
		tBundle.pRouteSet,
		"/ops/restart",
		strlen("/ops/restart"),
		arrIndexes,
		DEMO_MAX_ROUTE,
		&iHitCount
	) > 0 ) {
		printf("matched route count = %u\n", (unsigned)iHitCount);
	}

	procFreeBundle(&tBundle);
	xrtUnit();
	return 0;
}
```


## 6. 这段代码最重要的 4 个边界

### 6.1 编码边界

这页不是先猜编码，再把业务逻辑写在“本机能看懂就行”的假设上。

它一开始就明确：

- `xrtFileReadAll(..., XRT_CP_AUTO, ...)`

把未知来源文件先统一进 UTF-8 主线。

也就是说：

- `charset` 负责“读对”
- 后面的 `regex` 和 `crypto` 才能稳定工作


### 6.2 解析边界

这里的 regex 只负责：

- 判断一行像不像 `key=value`
- 把 `key` 和 `value` 提出来

它不负责：

- 接管整份文件语法
- 维护多层状态机

这正是 regex 当前最稳的使用方式。


### 6.3 验签边界

这页刻意把签名范围定成：

- `hmac=` 之前的 UTF-8 文本字节

这样导入链就很清楚：

1. 先把文件统一成 UTF-8
2. 再确定 `hmac=` 这一行在哪里开始
3. 然后对前缀正文做 `HMAC-SHA256`

也就是说，认证主线是：

- 先统一文本语义
- 再校验来源

不是“读一边、拼一边、赌最后算出来还能对上”。


### 6.4 运行时边界

导入完成后，这页没有把 `route=` 继续留成裸字符串数组，而是马上：

- `xrtRegexSetCreate(...)`

把它编译成运行时规则集合。

这样后面的业务层拿到的就不是：

- 一堆还没编译的文本

而是：

- 直接可用的匹配对象


## 7. 为什么这页没有直接讲“发布端”

因为这页的核心问题是：

- 如何安全导入别人给的规则包

而不是：

- 如何设计一整套发布平台

当然，发布端也很简单：

1. 先生成不带 `hmac=` 行的 UTF-8 文本
2. 对正文调用 `xrtHMAC_SHA256()`
3. 再把结果用 `xrtHexEncode()` 写成最后一行 `hmac=...`

也就是说，发布和导入是同一条主线的两端；这页先把导入端讲稳。


## 8. 这条模型最适合什么项目

这条写法最适合：

- 规则包 / 策略包导入
- 人工可编辑、又需要来源认证的文本配置
- 需要把文本规则编译成运行时对象的网关或服务端程序
- 同时跑在 Windows / Linux 上的工具和服务

它不适合：

- 机密配置的保密存储
- 大型嵌套数据模型
- 完整协议或 DSL 解析器


## 9. 常见错误

### 9.1 错误一：用本机编码直接读，再赌业务层不会乱码

这会把同一份规则包在不同平台上读成不同结果。


### 9.2 错误二：验签前先把文本随手重排

只要签名覆盖范围和发布端不一致，最后就一定对不上。


### 9.3 错误三：把 regex 用成“整份文档解析器”

这超出了当前正则主线最稳的职责边界。


### 9.4 错误四：只做哈希，不做 HMAC

普通哈希只能说明“内容长这样”，不能说明“内容来自谁”。


### 9.5 错误五：导入后仍然把规则留成未编译字符串

这样后面的运行时匹配层还要再重复做一次编译和错误处理。


## 10. 建议继续阅读

- [Charset 入门](../guide/charset-intro.md)
- [Regex 入门](../guide/regex-intro.md)
- [Crypto 入门](../guide/crypto-intro.md)
- [Regex API](../api/api-regex.md)
- [Crypto API](../api/api-crypto.md)
- [用 `file + path + value + json` 构造配置系统](json-config-system.md)

---

**一句话总结：** 这条案例主线的关键不在“读到了一份文本文件”，而在“先把未知编码统一进 UTF-8，再用 regex 提取字段，再用 HMAC 验明来源，最后把文本规则编译成正式运行时对象”；这才是 `charset + regex + crypto` 在真实项目里最稳的一条组合写法。
