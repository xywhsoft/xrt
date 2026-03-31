# Charset 入门：程序里的字符串、文件编码和平台宽字符不是一回事

> 目标：把“为什么会乱码”讲成几个清楚的边界。读完这页后，你应该明确知道：什么时候该继续把字符串留在 UTF-8，什么时候该转成 UTF-16 / UTF-32，文件读取为什么常用 `XRT_CP_AUTO`，以及输出为什么应该显式指定编码。

[返回教学文档](README.md)

---

## 1. 为什么 `charset` 总是在“程序快写完了”才出问题

很多程序前期看起来完全不需要编码知识：

- 配置能读
- 日志能写
- JSON 能发
- 模板能渲染

但一旦开始碰下面这些边界，乱码问题就会集中爆出来：

- 读别人给的旧文本文件
- 把文本写给 Excel、记事本、外部工具
- 在 Windows 和 Linux 之间搬数据
- 把 UTF-8 文本交给宽字符 API
- 从宽字符来源再回到 UTF-8 主线

所以这一页真正要讲的，不是“UTF-8 是什么”，而是 3 条更实用的规则：

1. 程序内部主线尽量统一为 UTF-8
2. 到平台边界、文件边界时再显式转换
3. 输入可以自动识别，输出最好明确指定


## 2. 先把 5 个边界分清楚

### 2.1 程序内部主线为什么优先 UTF-8

XRT 里大量字符串接口最终都更适合围绕 UTF-8 运转，例如：

- `xrtFileReadAll()` 读文本后返回 UTF-8
- `xrtStringifyJSON()` 返回 UTF-8
- 模板、HTTP、日志这些高层文本也更自然地接 UTF-8

所以更稳的心智模型是：

- 内存主线：尽量统一成 UTF-8
- 边界转换：需要时再转 UTF-16 / UTF-32 / OEM

这比在业务代码里到处混用不同宽度字符串稳定得多。


### 2.2 `UTF-16 / UTF-32` 不是“更高级”，而是不同边界

在 XRT 里：

- `xrtUTF8to16()` / `xrtUTF16to8()`
- `xrtUTF8to32()` / `xrtUTF32to8()`

解决的不是“哪种编码更好”，而是“你要和哪一层交接”。

最常见的现实情况是：

- Windows 宽字符环境更常靠近 UTF-16
- Linux 上 `wchar_t` 更常靠近 UTF-32

所以跨平台程序不应该把 `L"..."` 当成永远固定的一种宽度。  
更稳的做法是：从 UTF-8 文本出发，按需要显式转成 `u16str` 或 `u32str`。


### 2.3 文件编码和内存字符串不是一回事

这一点最容易被混淆。

- 内存里的业务字符串，你可以统一按 UTF-8 处理
- 磁盘上的文件，可能是 UTF-8、UTF-16、UTF-32、OEM，甚至带 BOM

所以文件边界要单独处理：

- 读取未知来源文本：优先 `XRT_CP_AUTO`
- 自己生成的文本文件：优先明确写 `XRT_CP_UTF8`
- 如果外部系统明确要求 UTF-16 / UTF-32，再显式写对应编码


### 2.4 `XRT_CP_AUTO` 更适合输入，不适合输出

这是最值得先记住的一条规则。

- 读文件时，`XRT_CP_AUTO` 很有价值
	- 让 XRT 帮你猜测输入文件编码
- 写文件时，`XRT_CP_AUTO` 没有教学价值
	- 因为输出应该是你自己决定的

也就是说：

- 输入边界：可以自动识别
- 输出边界：最好明确指定


### 2.5 `XRT_CP_OEM` 跨平台含义并不完全一样

`XRT_CP_OEM` 表示“本机字符集”。

但它在不同平台上的含义并不对称：

- Windows：通常是系统本机多字节代码页
- Linux：当前主线下固定按 UTF-8 理解

所以如果你想写稳定的跨平台教程、配置、日志、HTTP、JSON 主线，优先还是 UTF-8。  
`XRT_CP_OEM` 更适合放在“必须和本机传统工具兼容”的边界层。


## 3. 最小 DEMO：先把同一份 UTF-8 文本转成 UTF-16 / UTF-32，再转回来

第一步先不要碰文件。  
先只做一件事：确认你理解“程序内部统一 UTF-8，边界处按需转换”。

```c
#include "xrt.h"
#include <stdio.h>
#include <string.h>

int main(void)
{
	str sUtf8 = "你好，XRT 😀";
	u16str sUtf16 = NULL;
	u32str sUtf32 = NULL;
	str sFrom16 = NULL;
	str sFrom32 = NULL;
	size_t iUtf16Chars = 0;
	size_t iUtf32Chars = 0;
	size_t iRoundTrip16Bytes = 0;
	size_t iRoundTrip32Bytes = 0;
	int iRet = 1;

	xrtInit();

	sUtf16 = xrtUTF8to16((u8str)sUtf8, 0u, &iUtf16Chars);
	sUtf32 = xrtUTF8to32((u8str)sUtf8, 0u, &iUtf32Chars);
	if ( (sUtf16 == NULL) || (sUtf32 == NULL) ) {
		fprintf(stderr, "utf conversion failed\n");
		goto cleanup;
	}

	sFrom16 = xrtUTF16to8(sUtf16, 0u, &iRoundTrip16Bytes);
	sFrom32 = xrtUTF32to8(sUtf32, 0u, &iRoundTrip32Bytes);
	if ( (sFrom16 == NULL) || (sFrom32 == NULL) ) {
		fprintf(stderr, "round-trip conversion failed\n");
		goto cleanup;
	}

	printf("src utf8 bytes : %zu\n", strlen(sUtf8));
	printf("utf16 chars    : %zu\n", iUtf16Chars);
	printf("utf32 chars    : %zu\n", iUtf32Chars);
	printf("from utf16     : %s\n", sFrom16);
	printf("from utf32     : %s\n", sFrom32);
	printf("utf16 len fn   : %zu\n", u16len(sUtf16));
	printf("utf32 len fn   : %zu\n", u32len(sUtf32));
	printf("rt16 bytes     : %zu\n", iRoundTrip16Bytes);
	printf("rt32 bytes     : %zu\n", iRoundTrip32Bytes);

	iRet = 0;

cleanup:
	if ( sFrom32 != NULL ) {
		xrtFree(sFrom32);
	}
	if ( sFrom16 != NULL ) {
		xrtFree(sFrom16);
	}
	if ( sUtf32 != NULL ) {
		xrtFree(sUtf32);
	}
	if ( sUtf16 != NULL ) {
		xrtFree(sUtf16);
	}
	xrtUnit();
	return iRet;
}
```

这个最小例子最想让你先记住的，是下面 4 件事：

1. 业务字符串从 UTF-8 出发最稳
2. `xrtUTF8to16()` / `xrtUTF8to32()` 只在边界层出现
3. 转回来后仍然回到 UTF-8 主线
4. `u16len()` / `u32len()` 处理的是宽字符串长度，不是 UTF-8 字节数


## 4. 升级 DEMO：同一份文本分别写成 UTF-8 和 UTF-16 BOM 文件，再自动识别读回

第二步才开始把 `charset` 和 `file` 接起来。

这次要解决的真实问题是：

- 我自己输出时，能明确决定文件编码
- 我回头再读这些文件时，不想手工判断每个文件到底是什么编码

这正是：

- `xrtFileWriteAll()`
- `xrtFileReadAll()`
- `xrtOpen(..., XRT_CP_AUTO)`

要配合起来使用的场景。

```c
#include "xrt.h"
#include <stdio.h>

static void procPrintDetectedEncoding(const char* sLabel, const char* sPath)
{
	xfile hFile = NULL;

	hFile = xrtOpen((str)sPath, TRUE, XRT_CP_AUTO);
	if ( hFile == NULL ) {
		printf("%s open failed\n", sLabel);
		return;
	}

	printf("%s charset : %d\n", sLabel, hFile->Charset);
	printf("%s bom     : %d\n", sLabel, hFile->BOM);

	xrtClose(hFile);
}


int main(void)
{
	xrtGlobalData* pCore = NULL;
	str sRootDir = NULL;
	str sUtf8Path = NULL;
	str sUtf16Path = NULL;
	str sUtf8Text = NULL;
	str sUtf16Text = NULL;
	size_t iUtf8Size = 0;
	size_t iUtf16Size = 0;
	const char* sText = "清浅池塘边，重生破土的冲动";
	int iRet = 1;

	pCore = xrtInit();
	if ( pCore == NULL ) {
		return 1;
	}

	sRootDir = xrtPathJoin(3, pCore->AppPath, "build", "charset-demo");
	sUtf8Path = xrtPathJoin(4, pCore->AppPath, "build", "charset-demo", "sample-utf8.txt");
	sUtf16Path = xrtPathJoin(4, pCore->AppPath, "build", "charset-demo", "sample-utf16.txt");
	if ( (sRootDir == NULL) || (sUtf8Path == NULL) || (sUtf16Path == NULL) ) {
		fprintf(stderr, "build path failed\n");
		goto cleanup;
	}

	if ( xrtDirCreateAll(sRootDir) != TRUE ) {
		fprintf(stderr, "create charset demo dir failed\n");
		goto cleanup;
	}

	if ( xrtFileWriteAll(sUtf8Path, (str)sText, 0u, XRT_CP_UTF8) <= 0 ) {
		fprintf(stderr, "write utf8 file failed\n");
		goto cleanup;
	}
	if ( xrtFileWriteAll(sUtf16Path, (str)sText, 0u, XRT_CP_UTF16 | XRT_CP_BOM) <= 0 ) {
		fprintf(stderr, "write utf16 file failed\n");
		goto cleanup;
	}

	procPrintDetectedEncoding("utf8 file", sUtf8Path);
	procPrintDetectedEncoding("utf16 file", sUtf16Path);

	sUtf8Text = xrtFileReadAll(sUtf8Path, XRT_CP_AUTO, &iUtf8Size);
	sUtf16Text = xrtFileReadAll(sUtf16Path, XRT_CP_AUTO, &iUtf16Size);
	if ( (sUtf8Text == NULL) || (sUtf16Text == NULL) ) {
		fprintf(stderr, "auto read failed\n");
		goto cleanup;
	}

	printf("utf8 text  : %s\n", sUtf8Text);
	printf("utf16 text : %s\n", sUtf16Text);
	printf("utf8 size  : %llu\n", (unsigned long long)iUtf8Size);
	printf("utf16 size : %llu\n", (unsigned long long)iUtf16Size);

	iRet = 0;

cleanup:
	if ( sUtf16Text != NULL ) {
		xrtFree(sUtf16Text);
	}
	if ( sUtf8Text != NULL ) {
		xrtFree(sUtf8Text);
	}
	if ( sUtf16Path != NULL ) {
		xrtFree(sUtf16Path);
	}
	if ( sUtf8Path != NULL ) {
		xrtFree(sUtf8Path);
	}
	if ( sRootDir != NULL ) {
		xrtFree(sRootDir);
	}
	xrtUnit();
	return iRet;
}
```

这个升级版最关键的教学点有 4 个：

1. 输出编码要自己决定
	- 这次我们明确写了 `UTF-8`
	- 也明确写了 `UTF-16 + BOM`
2. 输入边界可以自动识别
	- `xrtFileReadAll(..., XRT_CP_AUTO, ...)` 最后统一回 UTF-8
3. 如果你还想看“它原文件到底是什么编码”
	- 用 `xrtOpen(..., XRT_CP_AUTO)` 观察 `xfile->Charset` 和 `xfile->BOM`
4. 同一份业务文本可以服务不同外部编码要求
	- 但程序内部不需要跟着切换整条主线


## 5. 什么时候该用 `xrtConvCharset()`

前面的两个 DEMO 用的是专用转换函数：

- `xrtUTF8to16()`
- `xrtUTF16to8()`
- `xrtUTF8to32()`
- `xrtUTF32to8()`

它们适合：

- 你明确知道自己只在 UTF-8 / UTF-16 / UTF-32 之间切换
- 你想把示例写得最直接、最清楚

如果你的边界更一般化，例如：

- 输入编码是变量
- 输出编码由配置决定
- 需要在 UTF-16 BE / UTF-32 BE 之间切换

就更适合用：

- `xrtConvCharset()`

可以先把它理解成：

- 专用函数：写最短路径
- 通用函数：做编码桥接器


## 6. 什么时候该用 `xrtDetectCharset()`

如果你已经拿到一段“原始字节”，还没进文件 helper，也还不知道编码，可以再往下走到：

- `xrtDetectCharset()`

它更适合这种场景：

- 自己从网络拿到一段原始响应体
- 自己读了二进制缓冲区
- 想先判断有没有 BOM，再决定怎么转换

但对大多数“读文本文件”场景，入门阶段不必先从它讲起。  
更稳的顺序是：

1. 先学会 `xrtFileReadAll(..., XRT_CP_AUTO, ...)`
2. 需要观察底层检测结果时，再补 `xrtOpen(..., XRT_CP_AUTO)` 或 `xrtDetectCharset()`


## 7. Windows / Linux 跨平台时最该记住的事

- 跨平台业务主线优先 UTF-8
- 不要把 `L"..."` 当成固定宽度的可移植表示
- Windows 边界更常碰到 UTF-16
- Linux 边界更常碰到 UTF-32 或直接 UTF-8
- 输出文件编码要自己决定，不要把“自动识别”当成输出策略


## 8. 常见错误

### 错误一：把 `wchar_t` 当成永远等于 UTF-16

这在跨平台代码里并不稳。  
更稳的做法是显式用 `u16str` / `u32str` 和对应转换函数。

### 错误二：读文件时强行写死某个编码

如果文件来源不稳定，优先 `XRT_CP_AUTO`。  
如果文件是你自己写出来的，再按你自己定义的编码去读。

### 错误三：写文件时也想“自动识别”

输出编码应该是设计决定，不是猜测结果。  
自己写出的文本文件，最好明确给出 `XRT_CP_UTF8`、`XRT_CP_UTF16 | XRT_CP_BOM` 这类目标编码。

### 错误四：把 UTF-8 字节数和 UTF-16 / UTF-32 字符数混为一谈

UTF-8 常讨论“字节数”。  
UTF-16 / UTF-32 示例里，常更接近“宽字符长度”。

### 错误五：把 `XRT_CP_OEM` 当成跨平台统一编码

它表达的是“本机字符集”，而不是“跨平台统一交换格式”。  
跨平台交换、配置、JSON、HTTP 主线仍然优先 UTF-8。


## 9. 建议继续阅读

- [Charset API](../api/api-charset.md)
- [File API](../api/api-file.md)
- [时间、路径与文件系统入门](time-path-file-intro.md)
- [xvalue 与 JSON 入门](xvalue-json-intro.md)
- [用 `file + path + value + json` 构造配置系统](../case/json-config-system.md)

---

**一句话总结：** `charset` 真正要解决的不是“背几种编码名字”，而是“让程序内部始终围绕 UTF-8 主线运转，在文件和平台边界上再做明确转换”。这条线一旦立住，乱码问题就会少掉一大半。
