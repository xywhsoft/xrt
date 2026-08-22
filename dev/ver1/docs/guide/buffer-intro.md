# Buffer 入门：什么时候你需要一块会长大的字节区，而不是数组和字符串

> 目标：把 `xrtBufferCreate()`、`xrtBufferAppend()`、`xrtBufferInsert()`、`xrtBufferMalloc()`、`xrtBufferInit()` / `xrtBufferUnit()` 讲成第 2 阶段的第一条容器主线。读完这页后，你应该明确知道：什么时候你的问题本质上是“按字节拼内容”，为什么这时应该先用 `buffer`，以及当前实现里 `append` 和 `insert` 的边界到底是什么。

[返回教学文档](README.md)

---

## 1. 为什么 `buffer` 应该是第 2 阶段第一篇

当程序刚从“基础工具”进入“容器与数据结构”阶段时，最先遇到的往往不是：

- 排序
- 树
- 字典
- 复杂对象池

而是更朴素的问题：

- 我想把一段一段文本拼起来
- 我想构造一份二进制帧
- 我想把若干块数据连续写进一块可增长内存
- 我不想每次都 `xrtFormat()` 一次、拷贝一次、再分配一次

这类问题的本质其实都一样：

- 我需要一块会长大的字节区

这就是 `buffer` 的位置。  
它不是“字符串库替代品”，也不是“结构体数组”。它更像：

- 一个自增长的 byte builder

只要你先把这条线走顺，后面再看：

- `array`
- `dict`
- `list`
- `template`
- HTTP / JSON 拼装

都会更容易理解。


## 2. 先把 6 个边界分清楚

### 2.1 `buffer` 解决的是“字节拼装”，不是“元素管理”

`buffer` 很适合：

- 文本拼接
- CSV / SQL / HTML / JSON 输出
- 原始二进制帧构造
- 网络报文、文件块、协议头 + payload 拼装

`buffer` 不擅长：

- 按元素插入删除并保持结构语义
- 管理一组固定大小结构体
- 管理对象引用集合

所以更直接地说：

- 你在想“字节和片段怎么连续写进去”
	- 先想 `buffer`
- 你在想“第 N 个元素、排序、删除、遍历”
	- 更可能该去看 `array / list / dict`


### 2.2 `xrtBufferAppend()` 是主路径，`xrtBufferInsert()` 不是文本编辑器的“插入”

这是这一页最重要的边界。

当前实现里：

- `xrtBufferAppend()`
	- 就是在末尾继续写
	- 这是最自然、最稳的主线
- `xrtBufferInsert()`
	- 是“从指定字节偏移开始写”
	- **不会自动把后面的内容整体后移**
	- 当前实现还会把 `Length` 更新为 `iPos + iSize`

所以不要把 `xrtBufferInsert()` 想成：

- 文本编辑器里的 splice
- 数组里的真正中间插入

它更适合：

- 你明确知道写入偏移
- 你就是要覆盖那个位置
- 你在自己控制整块字节区布局

而不是直接拿来做：

- “往现有字符串中间塞一段文本，还保留尾巴”


### 2.3 `Length` 是有效数据长度，不包含字符串终止符

`buffer` 里有两个最关键的字段：

- `Buffer`
	- 实际内存
- `Length`
	- 当前有效数据长度

如果你使用字符串模式：

- `XBUF_UTF8`
- `XBUF_ANSI`
- `XBUF_UTF16`
- `XBUF_UTF32`

库会自动补终止符，但：

- `Length` 只算有效内容
- 终止符不计入 `Length`

这也是为什么 `buffer` 既能拿来做文本，也能拿来做二进制。


### 2.4 `iSize == 0` 只适合字符串模式，不适合二进制模式

当前实现里：

- `iSize == 0`
	- 会根据字符串模式自动算长度

也就是说：

- `XBUF_UTF8 / ANSI`
	- 会走 `strlen()`
- `XBUF_UTF16`
	- 会走 `u16len()`
- `XBUF_UTF32`
	- 会走 `u32len()`

但如果你写的是：

- `XBUF_BINARY`

那 `iSize` 就必须自己给。  
因为二进制数据本来就没有“自动找到结尾”这件事。


### 2.5 `xrtBufferCreate()` / `Destroy()` 和 `xrtBufferInit()` / `Unit()` 是两套生命周期

这两组 API 解决的是两个不同场景：

- `xrtBufferCreate()` / `xrtBufferDestroy()`
	- 适合堆上单独创建一个 `xbuffer`
- `xrtBufferInit()` / `xrtBufferUnit()`
	- 适合把 `xbuffer_struct` 嵌进自己的结构体或放在栈上

所以不要混用：

- 堆上创建的对象，最后 `Destroy`
- 自己持有结构体本体的对象，最后 `Unit`


### 2.6 `xrtBufferMalloc()` 不只是“扩容”，还是“预分配”和“裁剪”

它至少有 3 个典型用法：

- 预分配
	- 先给一块足够大的内存，减少循环里反复扩容
- 裁剪
	- 用完后把多余空间收掉
- 清空
	- `xrtBufferMalloc(pBuf, 0)` 或 `xrtBufferUnit()`

这对大循环、报文拼装、日志缓冲都很有价值。


## 3. 最小 DEMO：先把它当成文本构建器用起来

第一步不要先碰二进制。  
先把最常见、也最容易写顺的一条主线走通：

- 一段一段追加文本
- 最后补一个 UTF-8 终止符
- 直接写成文件

```c
#include "xrt.h"
#include <stdio.h>

int main(void)
{
	xrtGlobalData* pCore = NULL;
	xbuffer pBuf = NULL;
	str sOutDir = NULL;
	str sOutPath = NULL;
	str sNow = NULL;
	int iRet = 1;

	pCore = xrtInit();
	if ( pCore == NULL ) {
		return 1;
	}

	pBuf = xrtBufferCreate(256);
	if ( pBuf == NULL ) {
		goto cleanup;
	}

	sOutDir = xrtPathJoin(3, pCore->AppPath, "build", "buffer-demo");
	sOutPath = xrtPathJoin(4, pCore->AppPath, "build", "buffer-demo", "startup-report.txt");
	sNow = xrtTimeToStr(xrtNow(), XRT_TIME_FORMAT_DATETIME);
	if ( (sOutDir == NULL) || (sOutPath == NULL) || (sNow == NULL) ) {
		fprintf(stderr, "prepare output path failed\n");
		goto cleanup;
	}

	if ( xrtDirCreateAll(sOutDir) != TRUE ) {
		fprintf(stderr, "create output dir failed\n");
		goto cleanup;
	}

	if (
		!xrtBufferAppend(pBuf, "time    : ", 0, XBUF_BINARY) ||
		!xrtBufferAppend(pBuf, sNow, 0, XBUF_BINARY) ||
		!xrtBufferAppend(pBuf, "\r\n", 2, XBUF_BINARY) ||
		!xrtBufferAppend(pBuf, "module  : buffer\r\n", 18, XBUF_BINARY) ||
		!xrtBufferAppend(pBuf, "status  : ready\r\n", 17, XBUF_BINARY) ||
		!xrtBufferAppend(pBuf, "", 0, XBUF_UTF8)
	) {
		fprintf(stderr, "build report failed\n");
		goto cleanup;
	}

	if ( xrtFileWriteAll(sOutPath, pBuf->Buffer, pBuf->Length, XRT_CP_UTF8) <= 0 ) {
		fprintf(stderr, "write report failed\n");
		goto cleanup;
	}

	printf("report path : %s\n", sOutPath);
	printf("length      : %u\n", pBuf->Length);
	printf("%s", pBuf->Buffer);
	iRet = 0;

cleanup:
	if ( sNow != NULL ) {
		xrtFree(sNow);
	}
	if ( sOutPath != NULL ) {
		xrtFree(sOutPath);
	}
	if ( sOutDir != NULL ) {
		xrtFree(sOutDir);
	}
	if ( pBuf != NULL ) {
		xrtBufferDestroy(pBuf);
	}
	xrtUnit();
	return iRet;
}
```

这个最小例子最想让你先记住的是：

1. 文本拼装时，`append` 是绝对主路径
2. 中间片段都可以按 `XBUF_BINARY` 追加
3. 最后再补一次 `XBUF_UTF8` 终止符就够了
4. `pBuf->Length` 适合直接作为文件写入长度


## 4. 升级 DEMO：用它构造一份二进制帧

当你已经会用 `buffer` 拼文本，下一步最值得学的是：

- 它其实首先是一块字节区
- 所以它也很适合构造协议头 + payload

这时重点就不再是 `\0`，而是：

- 先写头
- 再写 payload
- 最后回头补头里的长度字段

```c
#include "xrt.h"
#include <stdio.h>

typedef struct {
	uint32 u32Magic;
	uint32 u32PayloadLen;
	uint16 u16Version;
	uint16 u16Flags;
} DemoFrameHeader;


static void procDumpHex(const uint8* pData, uint32 u32Size)
{
	uint32 i = 0;

	for ( i = 0; i < u32Size; i++ ) {
		printf("%02X ", pData[i]);
		if ( (i + 1u) % 16u == 0u ) {
			printf("\n");
		}
	}
	if ( u32Size % 16u != 0u ) {
		printf("\n");
	}
}


int main(void)
{
	xrtGlobalData* pCore = NULL;
	xbuffer pBuf = NULL;
	DemoFrameHeader tHeader = {0x58525442u, 0u, 1u, 0u};
	uint8 arrPayload[] = {0x10, 0x20, 0x30, 0x40, 0x50};
	str sOutDir = NULL;
	str sPacketPath = NULL;
	int iRet = 1;

	pCore = xrtInit();
	if ( pCore == NULL ) {
		return 1;
	}

	pBuf = xrtBufferCreate(64);
	if ( pBuf == NULL ) {
		goto cleanup;
	}

	if ( !xrtBufferMalloc(pBuf, 128) ) {
		fprintf(stderr, "prealloc packet buffer failed\n");
		goto cleanup;
	}

	if (
		!xrtBufferAppend(pBuf, &tHeader, sizeof(tHeader), XBUF_BINARY) ||
		!xrtBufferAppend(pBuf, arrPayload, sizeof(arrPayload), XBUF_BINARY)
	) {
		fprintf(stderr, "build packet failed\n");
		goto cleanup;
	}

	((DemoFrameHeader*)pBuf->Buffer)->u32PayloadLen = pBuf->Length - sizeof(DemoFrameHeader);

	sOutDir = xrtPathJoin(3, pCore->AppPath, "build", "buffer-demo");
	sPacketPath = xrtPathJoin(4, pCore->AppPath, "build", "buffer-demo", "demo-frame.bin");
	if ( (sOutDir == NULL) || (sPacketPath == NULL) ) {
		fprintf(stderr, "build packet path failed\n");
		goto cleanup;
	}

	if ( xrtDirCreateAll(sOutDir) != TRUE ) {
		fprintf(stderr, "create output dir failed\n");
		goto cleanup;
	}

	if ( xrtFilePutAll(sPacketPath, pBuf->Buffer, pBuf->Length) <= 0 ) {
		fprintf(stderr, "write packet failed\n");
		goto cleanup;
	}

	printf("packet path  : %s\n", sPacketPath);
	printf("packet bytes : %u\n", pBuf->Length);
	procDumpHex((const uint8*)pBuf->Buffer, pBuf->Length);
	iRet = 0;

cleanup:
	if ( sPacketPath != NULL ) {
		xrtFree(sPacketPath);
	}
	if ( sOutDir != NULL ) {
		xrtFree(sOutDir);
	}
	if ( pBuf != NULL ) {
		xrtBufferDestroy(pBuf);
	}
	xrtUnit();
	return iRet;
}
```

这个升级例子真正想教会你的事有 6 个：

1. `buffer` 首先是字节区，不只是字符串构建器
2. 二进制输出应该走 `xrtFilePutAll()`，不要混成文本写盘
3. `xrtBufferMalloc()` 适合在已知大概大小时先预分配
4. `Length` 就是当前有效二进制长度
5. 回头补协议头字段时，直接操作 `Buffer` 通常比误用“插入”更稳
6. 文本和二进制这两条线，本质上是同一个 `buffer` 模型


## 5. 什么时候该用 `xrtBufferInit()` / `xrtBufferUnit()`

如果 `buffer` 只是某个对象的一个成员，通常没必要再单独 `Create()` 一个管理器。

这时更自然的写法是：

```c
#include "xrt.h"

typedef struct {
	char sName[32];
	xbuffer_struct tBody;
} DemoBuilder;


void procInitBuilder(DemoBuilder* pBuilder)
{
	memset(pBuilder, 0, sizeof(*pBuilder));
	xrtBufferInit(&pBuilder->tBody, 256);
}


void procUnitBuilder(DemoBuilder* pBuilder)
{
	xrtBufferUnit(&pBuilder->tBody);
}
```

这组 API 的核心价值是：

- 管理器结构体由你自己持有
- `buffer` 只负责内部那块会增长的内存

这在：

- 长生命周期对象
- 状态机
- HTTP 请求/响应上下文
- 模板或解析器上下文

里都很常见。


## 6. 它和 `array`、字符串拼接、`xrtFormat()` 的区别

### 6.1 和 `array` 的区别

- `buffer`
	- 按字节连续写
	- 你自己解释这段字节是什么意思
- `array`
	- 按固定大小元素管理
	- 你关心的是第几个元素、排序、删除、遍历


### 6.2 和反复 `xrtFormat()` / `strcat` 的区别

如果你不断这样写：

- 格式化一小段
- 重新分配
- 拷贝旧字符串
- 再拼新字符串

成本很快就会堆起来。

`buffer` 更适合：

- 片段很多
- 总长度逐步增长
- 你想把分配和拷贝次数压下去


## 7. 如果只有一条主线程，会卡在哪里

`buffer` 本身不是阻塞 I/O，但它会在主线程里消耗：

- 分配
- 扩容
- 拷贝
- 格式化

所以最常见的问题不是“线程卡住等网络”，而是：

- 在大循环里反复扩容
- 对大块内容不停重建
- 该预分配的时候没预分配

这也是为什么：

- 小片段少量拼接
	- 直接用就行
- 大量循环拼接
	- 先考虑 `xrtBufferMalloc()`


## 8. Windows / Linux 跨平台时最该记住的事

- `buffer` 里存的是原始字节，不自带换行和编码语义
- 文本换行要自己决定用 `\n` 还是 `\r\n`
- 文本写盘用 `xrtFileWriteAll()`，二进制写盘用 `xrtFilePutAll()`
- UTF-8 / UTF-16 / UTF-32 的终止符长度不同，要明确模式


## 9. 常见错误

### 错误一：把 `xrtBufferInsert()` 当成真正的中间插入

当前实现不会自动把尾部整体后移。  
如果你要“保留尾巴再往中间塞文本”，通常应该换一种构造方式。

### 错误二：二进制模式下还把 `iSize` 写成 `0`

`XBUF_BINARY` 没有自动长度推断。  
二进制长度必须你自己给。

### 错误三：以为字符串终止符会计入 `Length`

`Length` 是有效内容长度，不是“含终止符总占用”。

### 错误四：把二进制内容交给 `xrtFileWriteAll()`

文本和二进制写盘是两条不同 API，别混。

### 错误五：对嵌入式 `xbuffer_struct` 调 `xrtBufferDestroy()`

如果结构体本体不是 `Create()` 出来的，就应该 `Unit()`，不是 `Destroy()`。

### 错误六：大量追加却从不预分配

循环里追加大量片段时，忘记预分配通常就是在白白增加扩容和拷贝次数。


## 10. 建议继续阅读

- [Buffer API](../api/api-buffer.md)
- [Array 入门：什么时候该用连续结构体数组，而不是 buffer 和指针数组](array-intro.md)
- [Array API](../api/api-array.md)
- [Math 与 Hash 入门：随机数、约等于和稳定指纹不是一回事](math-hash-intro.md)
- [时间、路径与文件系统入门：先把程序落到真实世界](time-path-file-intro.md)
- [Template 入门：什么时候该用模板，而不是拼字符串](template-intro.md)

---

**一句话总结：** `buffer` 最适合承担“把一段一段字节稳定拼成一块连续内容”的角色；一旦你先把这条线走顺，后面的文本输出、二进制协议和更高层容器都会清楚很多。 
