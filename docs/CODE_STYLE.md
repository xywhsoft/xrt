# xrt 代码风格规则

本文档基于 xrt 早期模块（charset、string、buffer、array、dict、list、value、time、base 等）的实际代码风格整理。

---

## 一、缩进与空白

| 规则 | 说明 |
|------|------|
| **Tab 缩进** | 所有缩进使用 Tab，不用空格。遇到空格缩进需转为 Tab |
| **CRLF 换行** | 统一使用 `\r\n`，单独的 `\r` 或 `\n` 需转换为 `\r\n` |
| **`#if` 等宏也缩进** | `#ifdef`/`#if`/`#else`/`#endif` 内部的代码块也添加 Tab 缩进 |
| **括号内外空格** | `if`、`for`、`while` 的小括号内外都加空格：`if ( condition ) {` |

### `#if` 缩进示例

```c
XXAPI ptr xrtMalloc(size_t iSize)
{
	#ifdef XRT_MEM_DEBUG
		return __xrtMallocSite(iSize, __FILE__, __LINE__);
	#else
		return __xrtMallocSite(iSize, NULL, 0);
	#endif
}
```

---

## 二、花括号规则

| 场景 | 风格 | 示例 |
|------|------|------|
| **函数定义** | `{` 另起一行 | `void Func()`<br>`{` |
| **if/for/while** | `{` 不换行，与语句同行，且 `{` 前加空格 | `if ( condition ) {` |
| **单行语句块** | 始终加 `{}`，不省略 | `if ( x ) { return; }` |
| **紧凑单行** | 简单逻辑可写在一行 | `if ( sText == NULL ) { return xCore.sNull; }` |

### 示例

```c
// 函数定义：花括号另起一行
XXAPI bool xrtIsLeapYear(int iYear)
{
	if ( (iYear % 400) == 0 ) {
		return TRUE;
	} else if ( (iYear % 100) == 0 ) {
		return FALSE;
	} else {
		if ( (iYear & 3) == 0 ) {
			return TRUE;
		} else {
			return FALSE;
		}
	}
}
```

---

## 三、表达式与运算

| 规则 | 示例 |
|------|------|
| **冗余括号** | 所有组合条件都加括号避免优先级问题 | `if ( (iCharLen == 0) \|\| (iCharLen > (iSize - iPos)) )` |
| **强制类型转换明确** | 显式转换，不隐式 | `(uint32)iAllocSize`、`(size_t)iCount` |
| **溢出检查** | 分配前检查整数溢出 | `if ( iAllocSize < iNeedSize \|\| iAllocSize > UINT32_MAX )` |

---

## 四、函数注释风格

### 4.1 现有风格（单行注释）

早期模块使用单行 `//` 注释放在函数前：

```c
// utf-8 转 utf-16（ 需使用 xrtFree 释放 ）
XXAPI u16str xrtUTF8to16(u8str sText, size_t iSize, size_t* iRetSize)
```

### 4.2 期望风格（详细块注释）

对于较复杂的函数，使用详细块注释：

```c
/*
	函数说明：将 UTF-8 编码字符串转换为 UTF-16 编码字符串
	参数表：
		sText：源 UTF-8 字符串
		iSize：源数据字节长度，0 表示自动计算
		iRetSize：返回转换后的字符数（可选）
	返回值：
		转换后的 UTF-16 字符串，需使用 xrtFree 释放
	特殊说明：
		不支持 5 字节及以上的 UTF-8 字符，超出部分使用 FFFD 替换码点代替
*/
XXAPI u16str xrtUTF8to16(u8str sText, size_t iSize, size_t* iRetSize)
```

块注释格式说明：
- `特殊说明` 没有则不写
- 参数说明使用 Tab 缩进
- 每个部分独占一行

---

## 五、函数内注释风格

| 规则 | 说明 |
|------|------|
| **步骤注释** | 每个功能步骤前加 `//` 注释说明 |
| **步骤间空行** | 较长函数的步骤块之间加空行分隔 |
| **算法密集处加行内注释** | 位运算、编码转换等逻辑处加行内注释 |
| **变量用途** | 算法函数中注明每个变量用途 |

### 典型示例

```c
XXAPI bool xrtBufferInsert(xbuffer pBuf, uint32 iPos, ptr pData, uint32 iSize, uint32 bStrMode)
{
	uint64 iNeedSize;
	uint64 iAllocSize;
	// 长度为 0 时自动计算数据长度
	if ( iSize == 0 ) {
		if ( bStrMode == XBUF_ANSI ) {
			iSize = strlen(pData);
		} else if ( bStrMode == XBUF_UTF16 ) {
			iSize = u16len(pData) * XBUF_UTF16;
		} else if ( bStrMode == XBUF_UTF32 ) {
			iSize = u32len(pData) * XBUF_UTF32;
		} else {
			return FALSE;
		}
	}
	// 分配内存
	iNeedSize = (uint64)iPos + (uint64)iSize + (uint64)bStrMode;
	if ( iNeedSize > UINT32_MAX ) {
		return FALSE;
	}
	if ( iNeedSize > pBuf->AllocLength ) {
		iAllocSize = iNeedSize + pBuf->AllocStep;
		if ( iAllocSize < iNeedSize || iAllocSize > UINT32_MAX ) {
			return FALSE;
		}
		if ( xrtBufferMalloc(pBuf, (uint32)iAllocSize) == 0 ) {
			return FALSE;
		}
	}
	// 复制数据
	if ( iSize ) {
		memcpy(&pBuf->Buffer[iPos], pData, iSize);
		pBuf->Length = iPos + iSize;
	}
	// 字符串模式自动添加 \0
	if ( bStrMode ) {
		for ( uint32 i = 0; i < bStrMode; i++ ) {
			pBuf->Buffer[pBuf->Length + i] = 0;
		}
	}
	return TRUE;
}
```

---

## 六、空行分隔规则

| 场景 | 空行数 | 说明 |
|------|--------|------|
| 完全耦合的函数（如回调） | 0 个空行 | 回调函数紧跟被回调的函数 |
| 同模块紧密相关函数 | 1 个空行 | 功能相近的函数之间 |
| 同模块不同功能集 | 3 个空行 | 一个模块内不同功能的分界 |
| 不同模块 | 5 个空行 | 需要明显区分的模块之间 |
| 严格区分的大模块 | 11 个空行 | 大文件中便于快速浏览定位 |

---

## 七、结构体风格

```c
// 结构体数组内存管理器数据结构
typedef struct {
	str Memory;              // 管理器内存指针
	uint32 ItemLength;       // 成员占用内存长度
	uint32 Count;            // 管理器中存在多少成员
	uint32 AllocCount;       // 已经申请的结构数量
	uint32 AllocStep;        // 预分配内存步长
	xrtOwnerInfo Owner;      // 所有权信息
} xarray_struct, *xarray;
```

规则：
- 结构体前加 `//` 注释说明用途
- 字段注释右对齐，使用 Tab 缩进到统一列
- 使用匈牙利命名法前缀（`i`=整数, `s`=字符串, `p`=指针, `b`=布尔, `obj`=对象等）
- `typedef` 定义结构体类型和指针类型在同一行

---

## 八、跨平台模式

```c
#if defined(_WIN32) || defined(_WIN64)
	// windows 方案
	return strnicmp(s1, s2, iSize);
#else
	// 其他平台方案
	return strncasecmp(s1, s2, iSize);
#endif
```

规则：
- 统一使用 `defined(_WIN32) || defined(_WIN64)` 判断 Windows 平台
- 注释标注 `// windows 方案` 和 `// 其他平台方案`
- `#if` 内部代码有 Tab 缩进

---

## 九、错误处理与防御性编程

| 模式 | 说明 | 示例 |
|------|------|------|
| **参数空检查** | 函数入口检查 `NULL` 参数 | `if ( sText == NULL ) { return xCore.sNull; }` |
| **溢出保护** | 乘法/加法前检查 `SIZE_MAX`、`UINT32_MAX` | `if ( iSize > ((SIZE_MAX / sizeof(unsigned short)) - 1u) )` |
| **错误设置** | 使用 `xrtSetError()` 设置错误信息 | `xrtSetError("memory allocate failed.", FALSE);` |
| **所有权检查** | 共享对象使用所有权检查 | `xrtOwnerCheckMutable()` / `xrtOwnerBeginMutable()` |
| **统一返回** | 字符串类函数空指针返回 `xCore.sNull` 而非 `NULL` | `return xCore.sNull;` |

---

## 十、命名规范

### 10.1 变量命名（匈牙利命名法）

| 前缀 | 类型 | 示例 |
|------|------|------|
| `i` | 整数 | `iSize`、`iCount`、`iPos` |
| `f` | 浮点数 | `fValue` |
| `s` | 字符串 | `sText`、`sFile` |
| `t` | 时间 | `tScope` |
| `b` | 布尔值 | `bCase`、`bSrcRevise` |
| `h` | 句柄 | `hFile` |
| `p` | 指针 | `pBuf`、`pNode` |
| `proc` | 函数 | `FreeProc`、`CompProc` |
| `arr` | 数组 | `arrDays` |
| `tbl` | 字典 | `tblConfig` |
| `lst` | 列表 | `lstItems` |
| `obj` | 对象 | `objHT`、`objList` |

### 10.2 函数命名

| 类型 | 前缀/后缀 | 示例 |
|------|-----------|------|
| 公开 API | `xrt` + 模块名 | `xrtArrayCreate`、`xrtCopyStr`、`xrtBufferInsert` |
| 内部函数 | `__xrt` + 模块名 | `__xrtBytesExtraUTF8`、`__xrtArrayUnit_NoLock` |
| 无锁版本 | `_NoLock` 后缀 | `__xrtArrayUnit_NoLock`、`__xrtDictUnit_NoLock` |
| 调试版本 | `Dbg` 后缀 | `xrtArrayCreateDbg` |
| 值对象 | `xvo` 前缀 | `xvoAddRef`、`xvoUnref` |
| 回调函数 | `_FreeProc` / `_CompProc` 后缀 | `Dict_CompProc`、`AVLHT32_FreeProc` |

---

## 十一、注释语言与标点

| 规则 | 说明 |
|------|------|
| **中文注释** | 所有注释使用中文 |
| **中文括号注释** | 补充说明使用全角中文括号加空格：`（ 需使用 xrtFree 释放 ）` |
| **英文命名** | 变量名、函数名使用英文 |
| **错误信息** | `xrtSetError` 的错误信息使用英文 |
