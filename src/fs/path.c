#include "../internal/xrt_path.h"



#if defined(XRT_FEATURE_PATH)

#define XRT_PATH_ITER_STATE UINT32_C(0x50544849)



/* 路径根的内部表示同时保存消费长度和语义。 */
typedef struct __xrt_path_root {
	size_t Size;
	xpathroot Kind;
	bool Rooted;
	bool Absolute;
} __xrt_path_root;



/* 设置路径模块结构化错误。 */
void __xrtPathSetError(xerrkind Kind, xpatherror Code,
	cstr sOperation, cstr sMessage, int iSystemCode)
{
	xerrordesc tDesc;
	xerror* pError;

	memset(&tDesc, 0, sizeof(tDesc));
	tDesc.Kind = Kind;
	tDesc.Domain = "xrt.path";
	tDesc.Code = (int32)Code;
	tDesc.SystemCode = (int32)iSystemCode;
	tDesc.Operation = sOperation;
	tDesc.Message = sMessage;
	pError = xrtErrorBuild(&tDesc);
	if ( pError != NULL ) {
		__xrtErrorSetOwned(pError);
	}
}



/* 把 native 风格映射为当前平台的明确风格。 */
static bool __xrtPathStyle(xpathstyle Style, xpathstyle* pStyle)
{
	if ( Style == XPATH_NATIVE ) {
		#if defined(_WIN32) || defined(_WIN64)
			*pStyle = XPATH_WINDOWS;
		#else
			*pStyle = XPATH_POSIX;
		#endif
		return true;
	}
	if ( (Style != XPATH_POSIX) && (Style != XPATH_WINDOWS) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	*pStyle = Style;
	return true;
}



/* 判断指定风格下的路径分隔符。 */
static bool __xrtPathIsSep(char iChar, xpathstyle Style)
{
	return (iChar == '/') || ((Style == XPATH_WINDOWS) && (iChar == '\\'));
}



/* 返回指定风格的规范分隔符。 */
static char __xrtPathSepFor(xpathstyle Style)
{
	return Style == XPATH_WINDOWS ? '\\' : '/';
}



/* 检查借用路径视图和路径内不允许出现的零字节。 */
static bool __xrtPathViewValid(xstrview Path, cstr sOperation)
{
	if ( (Path.Data == NULL) && (Path.Size != 0) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	if ( (Path.Size != 0) && (memchr(Path.Data, 0, Path.Size) != NULL) ) {
		__xrtPathSetError(XERR_VALUE, XPATH_ERROR_FORMAT, sOperation,
			"path contains an embedded null byte", 0);
		return false;
	}
	return true;
}



/* 从必须存在的零结尾路径创建视图。 */
static bool __xrtPathText(cstr sPath, xstrview* pPath)
{
	if ( (sPath == NULL) || (pPath == NULL) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	*pPath = xrtStrView(sPath);
	return true;
}



/* 按 ASCII 规则判断 Windows 前缀字符，不读取 locale。 */
static bool __xrtPathAsciiEqual(char iLeft, char iRight)
{
	unsigned char iA = (unsigned char)iLeft;
	unsigned char iB = (unsigned char)iRight;

	if ( (iA >= (unsigned char)'A') && (iA <= (unsigned char)'Z') ) {
		iA = (unsigned char)(iA + ('a' - 'A'));
	}
	if ( (iB >= (unsigned char)'A') && (iB <= (unsigned char)'Z') ) {
		iB = (unsigned char)(iB + ('a' - 'A'));
	}
	return iA == iB;
}



/* 尝试读取 UNC 的 server/share 根，起点位于 server 开头。 */
static bool __xrtPathUncRoot(xstrview Path, size_t iStart,
	xpathstyle Style, size_t* pEnd)
{
	size_t i = iStart;
	size_t iServer;
	size_t iShare;

	iServer = i;
	while ( (i < Path.Size) && !__xrtPathIsSep(Path.Data[i], Style) ) {
		i++;
	}
	if ( (i == iServer) || (i == Path.Size) ) {
		return false;
	}
	while ( (i < Path.Size) && __xrtPathIsSep(Path.Data[i], Style) ) {
		i++;
	}
	iShare = i;
	while ( (i < Path.Size) && !__xrtPathIsSep(Path.Data[i], Style) ) {
		i++;
	}
	if ( i == iShare ) {
		return false;
	}
	while ( (i < Path.Size) && __xrtPathIsSep(Path.Data[i], Style) ) {
		i++;
	}
	*pEnd = i;
	return true;
}



/* 分解 Windows 驱动器、UNC、设备和根相对前缀。 */
static __xrt_path_root __xrtPathWindowsRoot(xstrview Path)
{
	__xrt_path_root Root = { 0, XPATH_ROOT_NONE, false, false };
	size_t iEnd;

	/* 扩展长度和设备命名空间都以两个分隔符、标记和分隔符开始。 */
	if ( (Path.Size >= 4) && __xrtPathIsSep(Path.Data[0], XPATH_WINDOWS) &&
		 __xrtPathIsSep(Path.Data[1], XPATH_WINDOWS) &&
		 ((Path.Data[2] == '?') || (Path.Data[2] == '.')) &&
		 __xrtPathIsSep(Path.Data[3], XPATH_WINDOWS) ) {
		Root.Size = 4;
		Root.Kind = XPATH_ROOT_DEVICE;
		Root.Rooted = true;
		Root.Absolute = true;

		if ( (Path.Data[2] == '?') && (Path.Size >= 8) &&
			 __xrtPathAsciiEqual(Path.Data[4], 'U') &&
			 __xrtPathAsciiEqual(Path.Data[5], 'N') &&
			 __xrtPathAsciiEqual(Path.Data[6], 'C') &&
			 __xrtPathIsSep(Path.Data[7], XPATH_WINDOWS) &&
			 __xrtPathUncRoot(Path, 8, XPATH_WINDOWS, &iEnd) ) {
			Root.Size = iEnd;
		} else if ( (Path.Size >= 6) &&
			 (((Path.Data[4] >= 'A') && (Path.Data[4] <= 'Z')) ||
			  ((Path.Data[4] >= 'a') && (Path.Data[4] <= 'z'))) &&
			 (Path.Data[5] == ':') ) {
			Root.Size = 6;
			while ( (Root.Size < Path.Size) &&
				__xrtPathIsSep(Path.Data[Root.Size], XPATH_WINDOWS) ) {
				Root.Size++;
			}
		}
		return Root;
	}

	/* 完整 UNC 根必须同时包含非空 server 和 share。 */
	if ( (Path.Size >= 2) && __xrtPathIsSep(Path.Data[0], XPATH_WINDOWS) &&
		 __xrtPathIsSep(Path.Data[1], XPATH_WINDOWS) &&
		 __xrtPathUncRoot(Path, 2, XPATH_WINDOWS, &iEnd) ) {
		Root.Size = iEnd;
		Root.Kind = XPATH_ROOT_UNC;
		Root.Rooted = true;
		Root.Absolute = true;
		return Root;
	}

	/* C:foo 是驱动器相对路径，只有 C:\foo 才是完整绝对路径。 */
	if ( (Path.Size >= 2) &&
		 (((Path.Data[0] >= 'A') && (Path.Data[0] <= 'Z')) ||
		  ((Path.Data[0] >= 'a') && (Path.Data[0] <= 'z'))) &&
		 (Path.Data[1] == ':') ) {
		Root.Size = 2;
		Root.Kind = XPATH_ROOT_DRIVE_RELATIVE;
		Root.Rooted = true;
		if ( (Path.Size >= 3) && __xrtPathIsSep(Path.Data[2], XPATH_WINDOWS) ) {
			Root.Size = 3;
			while ( (Root.Size < Path.Size) &&
				__xrtPathIsSep(Path.Data[Root.Size], XPATH_WINDOWS) ) {
				Root.Size++;
			}
			Root.Kind = XPATH_ROOT_DRIVE;
			Root.Absolute = true;
		}
		return Root;
	}

	/* 单分隔符根依赖当前驱动器，因此带根但不是完整绝对路径。 */
	if ( (Path.Size != 0) && __xrtPathIsSep(Path.Data[0], XPATH_WINDOWS) ) {
		Root.Size = 1;
		while ( (Root.Size < Path.Size) &&
			__xrtPathIsSep(Path.Data[Root.Size], XPATH_WINDOWS) ) {
			Root.Size++;
		}
		Root.Kind = XPATH_ROOT_WINDOWS;
		Root.Rooted = true;
	}
	return Root;
}



/* 分解指定风格的根，不验证输入视图。 */
static __xrt_path_root __xrtPathRoot(xstrview Path, xpathstyle Style)
{
	__xrt_path_root Root = { 0, XPATH_ROOT_NONE, false, false };

	if ( Style == XPATH_WINDOWS ) {
		return __xrtPathWindowsRoot(Path);
	}
	if ( (Path.Size != 0) && (Path.Data[0] == '/') ) {
		Root.Size = 1;
		while ( (Root.Size < Path.Size) && (Path.Data[Root.Size] == '/') ) {
			Root.Size++;
		}
		Root.Kind = XPATH_ROOT_POSIX;
		Root.Rooted = true;
		Root.Absolute = true;
	}
	return Root;
}



/* 按指定风格零分配地分解路径。 */
XRT_API bool xrtPathParse(xstrview Path, xpathstyle Style, xpathparts* pParts)
{
	xpathparts Parts;
	__xrt_path_root Root;
	size_t iEnd;
	size_t iNameStart;
	size_t iParentEnd;
	size_t iDot = XRT_NPOS;

	if ( pParts == NULL ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	if ( !__xrtPathStyle(Style, &Style) ||
		 !__xrtPathViewValid(Path, "parse") ) {
		return false;
	}

	memset(&Parts, 0, sizeof(Parts));
	Root = __xrtPathRoot(Path, Style);
	Parts.Root = xrtStrSlice(Path, 0, Root.Size);
	Parts.RootKind = Root.Kind;
	if ( Root.Rooted ) {
		Parts.Flags |= XPATH_FLAG_ROOTED;
	}
	if ( Root.Absolute ) {
		Parts.Flags |= XPATH_FLAG_ABSOLUTE;
	}
	if ( (Path.Size > Root.Size) &&
		 __xrtPathIsSep(Path.Data[Path.Size - 1], Style) ) {
		Parts.Flags |= XPATH_FLAG_TRAILING_SEPARATOR;
	}

	/* 尾部分隔符不产生虚假的空名称，但根本身仍保持空名称。 */
	iEnd = Path.Size;
	while ( (iEnd > Root.Size) && __xrtPathIsSep(Path.Data[iEnd - 1], Style) ) {
		iEnd--;
	}
	if ( iEnd == Root.Size ) {
		Parts.Parent = Parts.Root;
		*pParts = Parts;
		return true;
	}

	iNameStart = Root.Size;
	iParentEnd = Root.Size;
	for ( size_t i = iEnd; i > Root.Size; i-- ) {
		if ( __xrtPathIsSep(Path.Data[i - 1], Style) ) {
			iNameStart = i;
			iParentEnd = i - 1;
			while ( (iParentEnd > Root.Size) &&
				__xrtPathIsSep(Path.Data[iParentEnd - 1], Style) ) {
				iParentEnd--;
			}
			break;
		}
	}
	Parts.Parent = xrtStrSlice(Path, 0, iParentEnd);
	Parts.Name = xrtStrSlice(Path, iNameStart, iEnd - iNameStart);
	Parts.Stem = Parts.Name;

	/* 单点、双点和首字符点都不是扩展名分隔符。 */
	if ( !((Parts.Name.Size == 1) && (Parts.Name.Data[0] == '.')) &&
		 !((Parts.Name.Size == 2) && (Parts.Name.Data[0] == '.') &&
		   (Parts.Name.Data[1] == '.')) ) {
		for ( size_t i = iEnd; i > iNameStart; i-- ) {
			if ( Path.Data[i - 1] == '.' ) {
				iDot = i - 1;
				break;
			}
		}
	}
	if ( (iDot != XRT_NPOS) && (iDot > iNameStart) ) {
		Parts.Stem = xrtStrSlice(Path, iNameStart, iDot - iNameStart);
		Parts.Ext = xrtStrSlice(Path, iDot, iEnd - iDot);
	}
	*pParts = Parts;
	return true;
}



/* 初始化借用输入的路径组件迭代器。 */
XRT_API bool xrtPathIterInit(xpathiter* pIterator,
	xstrview Path, xpathstyle Style)
{
	xpathiter Iterator;
	__xrt_path_root Root;

	if ( pIterator == NULL ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	if ( !__xrtPathStyle(Style, &Style) ||
		 !__xrtPathViewValid(Path, "iterate") ) {
		return false;
	}
	memset(&Iterator, 0, sizeof(Iterator));
	Root = __xrtPathRoot(Path, Style);
	Iterator.Path = Path;
	Iterator.RootSize = Root.Size;
	Iterator.Style = Style;
	Iterator.State = XRT_PATH_ITER_STATE;
	*pIterator = Iterator;
	return true;
}



/* 返回路径中的下一个根、点、双点或普通名称组件。 */
XRT_API bool xrtPathNext(xpathiter* pIterator, xpathcomponent* pComponent)
{
	size_t iStart;

	if ( pComponent == NULL ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	memset(pComponent, 0, sizeof(xpathcomponent));
	if ( pIterator == NULL ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	if ( (pIterator->State != XRT_PATH_ITER_STATE) ||
		 ((pIterator->Path.Data == NULL) && (pIterator->Path.Size != 0)) ||
		 (pIterator->Position > pIterator->Path.Size) ||
		 (pIterator->RootSize > pIterator->Path.Size) ||
		 ((pIterator->Style != XPATH_POSIX) &&
		  (pIterator->Style != XPATH_WINDOWS)) ) {
		__xrtErrorSetInvalidState();
		return false;
	}

	/* 根作为首个独立组件返回，保留输入中的原始拼写。 */
	if ( (pIterator->Position == 0) && (pIterator->RootSize != 0) ) {
		pComponent->Text = xrtStrSlice(
			pIterator->Path, 0, pIterator->RootSize);
		pComponent->Kind = XPATH_COMPONENT_ROOT;
		pIterator->Position = pIterator->RootSize;
		return true;
	}

	/* 重复分隔符不产生空组件。 */
	while ( (pIterator->Position < pIterator->Path.Size) &&
		 __xrtPathIsSep(pIterator->Path.Data[pIterator->Position],
			pIterator->Style) ) {
		pIterator->Position++;
	}
	iStart = pIterator->Position;
	while ( (pIterator->Position < pIterator->Path.Size) &&
		 !__xrtPathIsSep(pIterator->Path.Data[pIterator->Position],
			pIterator->Style) ) {
		pIterator->Position++;
	}
	if ( iStart == pIterator->Position ) {
		return false;
	}

	pComponent->Text = xrtStrSlice(pIterator->Path, iStart,
		pIterator->Position - iStart);
	if ( (pComponent->Text.Size == 1) &&
		 (pComponent->Text.Data[0] == '.') ) {
		pComponent->Kind = XPATH_COMPONENT_CURRENT;
	} else if ( (pComponent->Text.Size == 2) &&
		 (pComponent->Text.Data[0] == '.') &&
		 (pComponent->Text.Data[1] == '.') ) {
		pComponent->Kind = XPATH_COMPONENT_PARENT;
	} else {
		pComponent->Kind = XPATH_COMPONENT_NORMAL;
	}
	return true;
}



/* 返回本机路径分隔符。 */
XRT_API char xrtPathSep(void)
{
	#if defined(_WIN32) || defined(_WIN64)
		return '\\';
	#else
		return '/';
	#endif
}



/* 返回本机路径列表分隔符。 */
XRT_API char xrtPathListSep(void)
{
	#if defined(_WIN32) || defined(_WIN64)
		return ';';
	#else
		return ':';
	#endif
}



/* 分解本机路径并复制指定字段。 */
static str __xrtPathCopyPart(cstr sPath, int iPart)
{
	xpathparts Parts;
	xstrview Path;
	xstrview Result;

	if ( !__xrtPathText(sPath, &Path) ||
		 !xrtPathParse(Path, XPATH_NATIVE, &Parts) ) {
		return NULL;
	}
	if ( iPart == 0 ) {
		Result = Parts.Name;
	} else if ( iPart == 1 ) {
		Result = Parts.Stem;
	} else if ( iPart == 2 ) {
		Result = Parts.Ext;
	} else {
		Result = Parts.Parent;
	}
	return xrtStrDupView(Result);
}



/* 复制本机路径的末级名称。 */
XRT_API str xrtPathName(cstr sPath)
{
	return __xrtPathCopyPart(sPath, 0);
}



/* 复制本机路径的末级名称主干。 */
XRT_API str xrtPathStem(cstr sPath)
{
	return __xrtPathCopyPart(sPath, 1);
}



/* 复制本机路径的最后一个扩展名。 */
XRT_API str xrtPathExt(cstr sPath)
{
	return __xrtPathCopyPart(sPath, 2);
}



/* 复制本机路径的父路径。 */
XRT_API str xrtPathParent(cstr sPath)
{
	return __xrtPathCopyPart(sPath, 3);
}



/* 判断本机路径是否完整绝对。 */
XRT_API bool xrtPathIsAbs(cstr sPath)
{
	xpathparts Parts;
	xstrview Path;

	if ( !__xrtPathText(sPath, &Path) ||
		 !xrtPathParse(Path, XPATH_NATIVE, &Parts) ) {
		return false;
	}
	return (Parts.Flags & XPATH_FLAG_ABSOLUTE) != 0;
}



/* 判断本机路径是否恰好为一个完整文件系统根。 */
XRT_API bool xrtPathIsRoot(cstr sPath)
{
	xpathparts Parts;
	xstrview Path;

	if ( !__xrtPathText(sPath, &Path) ||
		 !xrtPathParse(Path, XPATH_NATIVE, &Parts) ) {
		return false;
	}
	return ((Parts.Flags & XPATH_FLAG_ABSOLUTE) != 0u) &&
		xrtStrEmpty(Parts.Name) && (Parts.Parent.Size == Parts.Root.Size);
}



/* 判断本机路径是否带根。 */
XRT_API bool xrtPathIsRooted(cstr sPath)
{
	xpathparts Parts;
	xstrview Path;

	if ( !__xrtPathText(sPath, &Path) ||
		 !xrtPathParse(Path, XPATH_NATIVE, &Parts) ) {
		return false;
	}
	return (Parts.Flags & XPATH_FLAG_ROOTED) != 0;
}



/* 按 ASCII 大小写不敏感规则比较完整的 Windows 短名称。 */
static bool __xrtPathWindowsAsciiName(xstrview Name, cstr sExpected)
{
	size_t iSize = strlen(sExpected);

	if ( Name.Size != iSize ) {
		return false;
	}
	for ( size_t i = 0; i < iSize; i++ ) {
		if ( !__xrtPathAsciiEqual(Name.Data[i], sExpected[i]) ) {
			return false;
		}
	}
	return true;
}



/* 判断有限长度的 Windows 设备名基础部分。 */
static bool __xrtPathWindowsDeviceBase(
	const uint8* pBase,
	size_t iSize
)
{
	xstrview Base = {
		(const char*)pBase,
		iSize
	};

	if ( __xrtPathWindowsAsciiName(Base, "con") ||
		 __xrtPathWindowsAsciiName(Base, "prn") ||
		 __xrtPathWindowsAsciiName(Base, "aux") ||
		 __xrtPathWindowsAsciiName(Base, "nul") ||
		 __xrtPathWindowsAsciiName(Base, "conin$") ||
		 __xrtPathWindowsAsciiName(Base, "conout$") ) {
		return true;
	}
	if ( Base.Size == 4 ) {
		bool bPrefix =
			(__xrtPathAsciiEqual(Base.Data[0], 'c') &&
			 __xrtPathAsciiEqual(Base.Data[1], 'o') &&
			 __xrtPathAsciiEqual(Base.Data[2], 'm')) ||
			(__xrtPathAsciiEqual(Base.Data[0], 'l') &&
			 __xrtPathAsciiEqual(Base.Data[1], 'p') &&
			 __xrtPathAsciiEqual(Base.Data[2], 't'));

		return bPrefix && (Base.Data[3] >= '1') && (Base.Data[3] <= '9');
	}
	if ( Base.Size == 5 ) {
		bool bPrefix =
			(__xrtPathAsciiEqual(Base.Data[0], 'c') &&
			 __xrtPathAsciiEqual(Base.Data[1], 'o') &&
			 __xrtPathAsciiEqual(Base.Data[2], 'm')) ||
			(__xrtPathAsciiEqual(Base.Data[0], 'l') &&
			 __xrtPathAsciiEqual(Base.Data[1], 'p') &&
			 __xrtPathAsciiEqual(Base.Data[2], 't'));

		return bPrefix && ((unsigned char)Base.Data[3] == 0xC2u) &&
			(((unsigned char)Base.Data[4] == 0xB9u) ||
			  ((unsigned char)Base.Data[4] == 0xB2u) ||
			  ((unsigned char)Base.Data[4] == 0xB3u));
	}
	return false;
}



/* 判断 Windows 设备保留名，扩展名不会解除保留语义。 */
static bool __xrtPathWindowsDevice(xstrview Segment)
{
	size_t iBase = 0;

	while ( (iBase < Segment.Size) && (Segment.Data[iBase] != '.') ) {
		iBase++;
	}
	while ( (iBase != 0) && (Segment.Data[iBase - 1] == ' ') ) {
		iBase--;
	}
	return (iBase <= 7u) && __xrtPathWindowsDeviceBase(
		(const uint8*)Segment.Data,
		iBase
	);
}



/* 判断路径能否在不逃出基目录的前提下进行纯词法拼接。 */
XRT_API bool xrtPathIsLocal(xstrview Path, xpathstyle Style)
{
	xpathiter Iterator;
	xpathcomponent Component;
	size_t iDepth = 0;

	if ( !xrtPathIterInit(&Iterator, Path, Style) ) {
		return false;
	}
	if ( Path.Size == 0 ) {
		return false;
	}
	while ( xrtPathNext(&Iterator, &Component) ) {
		if ( Component.Kind == XPATH_COMPONENT_ROOT ) {
			return false;
		}
		if ( Component.Kind == XPATH_COMPONENT_CURRENT ) {
			continue;
		}
		if ( Component.Kind == XPATH_COMPONENT_PARENT ) {
			if ( iDepth == 0 ) {
				return false;
			}
			iDepth--;
			continue;
		}

		/* Windows 还必须排除卷语法、数据流和设备保留名。 */
		if ( Iterator.Style == XPATH_WINDOWS ) {
			if ( (memchr(Component.Text.Data, ':',
				 Component.Text.Size) != NULL) ||
				 __xrtPathWindowsDevice(Component.Text) ) {
				return false;
			}
		}
		iDepth++;
	}
	return true;
}



/* 把输入根复制为规范分隔符形式。 */
static size_t __xrtPathWriteRoot(str sOutput, xstrview Path,
	__xrt_path_root Root, xpathstyle Style)
{
	char iSep = __xrtPathSepFor(Style);
	size_t iPosition = 0;
	bool bKeepDouble = (Root.Kind == XPATH_ROOT_UNC) ||
		(Root.Kind == XPATH_ROOT_DEVICE);

	for ( size_t i = 0; i < Root.Size; i++ ) {
		char iChar = Path.Data[i];

		if ( __xrtPathIsSep(iChar, Style) ) {
			iChar = iSep;
			if ( (iPosition != 0) && (sOutput[iPosition - 1] == iSep) &&
				 !(bKeepDouble && (iPosition == 1)) ) {
				continue;
			}
		}
		sOutput[iPosition++] = iChar;
	}
	return iPosition;
}



/* 判断输出中的最后一个规范段是否为双点。 */
static bool __xrtPathLastIsDotDot(cstr sOutput, size_t iPosition,
	size_t iRootSize, char iSep)
{
	size_t iStart = iPosition;

	while ( (iStart > iRootSize) && (sOutput[iStart - 1] != iSep) ) {
		iStart--;
	}
	return ((iPosition - iStart) == 2) &&
		(sOutput[iStart] == '.') && (sOutput[iStart + 1] == '.');
}



/* 弹出最后一个普通段；根和保留的双点段不会被弹出。 */
static bool __xrtPathPop(str sOutput, size_t* pPosition,
	size_t iRootSize, char iSep)
{
	size_t iStart = *pPosition;

	if ( (*pPosition <= iRootSize) ||
		 __xrtPathLastIsDotDot(sOutput, *pPosition, iRootSize, iSep) ) {
		return false;
	}
	while ( (iStart > iRootSize) && (sOutput[iStart - 1] != iSep) ) {
		iStart--;
	}
	*pPosition = iStart;
	if ( (*pPosition > iRootSize) && (sOutput[*pPosition - 1] == iSep) ) {
		(*pPosition)--;
	}
	return true;
}



/* 向规范输出追加一个已经识别的普通段。 */
static void __xrtPathAppendSegment(str sOutput, size_t* pPosition,
	size_t iRootSize, xpathroot RootKind, char iSep,
	cstr sSegment, size_t iSize)
{
	bool bDriveRelative = (RootKind == XPATH_ROOT_DRIVE_RELATIVE) &&
		(*pPosition == iRootSize);

	if ( (*pPosition != 0) && (sOutput[*pPosition - 1] != iSep) &&
		 !bDriveRelative ) {
		sOutput[(*pPosition)++] = iSep;
	}
	memmove(sOutput + *pPosition, sSegment, iSize);
	*pPosition += iSize;
}



/* 把已验证路径清理到容量足够的输出，可与输入原地重叠。 */
static size_t __xrtPathCleanWrite(str sResult, xstrview Path,
	__xrt_path_root Root, xpathstyle Style)
{
	size_t iPosition;
	size_t iRootSize;
	size_t i = 0;
	char iSep;

	/* Windows 设备命名空间禁止普通路径规范化，必须原样传递。 */
	if ( Root.Kind == XPATH_ROOT_DEVICE ) {
		memmove(sResult, Path.Data, Path.Size);
		sResult[Path.Size] = 0;
		return Path.Size;
	}
	iSep = __xrtPathSepFor(Style);
	iPosition = __xrtPathWriteRoot(sResult, Path, Root, Style);
	iRootSize = iPosition;
	i = Root.Size;

	while ( i < Path.Size ) {
		size_t iStart;
		size_t iSize;
		bool bDotDot;

		while ( (i < Path.Size) && __xrtPathIsSep(Path.Data[i], Style) ) {
			i++;
		}
		iStart = i;
		while ( (i < Path.Size) && !__xrtPathIsSep(Path.Data[i], Style) ) {
			i++;
		}
		iSize = i - iStart;
		if ( (iSize == 0) ||
			 ((iSize == 1) && (Path.Data[iStart] == '.')) ) {
			continue;
		}
		bDotDot = (iSize == 2) && (Path.Data[iStart] == '.') &&
			(Path.Data[iStart + 1] == '.');
		if ( bDotDot ) {
			bool bMayEscape = (Root.Kind == XPATH_ROOT_NONE) ||
				(Root.Kind == XPATH_ROOT_DRIVE_RELATIVE);

			if ( __xrtPathPop(sResult, &iPosition, iRootSize, iSep) ) {
				continue;
			}
			if ( !bMayEscape ) {
				continue;
			}
		}
		__xrtPathAppendSegment(sResult, &iPosition, iRootSize,
			Root.Kind, iSep, Path.Data + iStart, iSize);
	}

	if ( iPosition == 0 ) {
		sResult[iPosition++] = '.';
	}
	sResult[iPosition] = 0;
	return iPosition;
}



/* 纯词法清理路径。 */
XRT_API str xrtPathClean(xstrview Path, xpathstyle Style)
{
	__xrt_path_root Root;
	str sResult;
	size_t iCapacity;

	if ( !__xrtPathStyle(Style, &Style) ||
		 !__xrtPathViewValid(Path, "clean") ) {
		return NULL;
	}
	if ( Path.Size == SIZE_MAX ) {
		__xrtErrorSetSizeOverflow();
		return NULL;
	}
	iCapacity = Path.Size != 0 ? Path.Size : 1u;
	sResult = (str)xrtMalloc(iCapacity + 1u);
	if ( sResult == NULL ) {
		return NULL;
	}
	Root = __xrtPathRoot(Path, Style);
	(void)__xrtPathCleanWrite(sResult, Path, Root, Style);
	return sResult;
}



/* 判断拼接缓冲是否恰好为 Windows 驱动器相对前缀。 */
static bool __xrtPathDrivePrefix(const xstrbuf* pBuffer, xpathstyle Style)
{
	return (Style == XPATH_WINDOWS) && (pBuffer->Size == 2) &&
		(((pBuffer->Data[0] >= 'A') && (pBuffer->Data[0] <= 'Z')) ||
		 ((pBuffer->Data[0] >= 'a') && (pBuffer->Data[0] <= 'z'))) &&
		(pBuffer->Data[1] == ':');
}



/* 追加带根项；Windows 根相对项继承已有卷或 UNC 前缀。 */
static bool __xrtPathBuildRooted(xstrbuf* pBuffer, xstrview Part,
	const xpathparts* pParts, xpathstyle Style)
{
	if ( (Style == XPATH_WINDOWS) &&
		 (pParts->RootKind == XPATH_ROOT_WINDOWS) &&
		 (pBuffer->Size != 0) ) {
		xpathparts Current;
		xstrview CurrentPath = xrtStrBufView(pBuffer);

		if ( !xrtPathParse(CurrentPath, Style, &Current) ) {
			return false;
		}
		if ( (Current.RootKind == XPATH_ROOT_DRIVE_RELATIVE) ||
			 (Current.RootKind == XPATH_ROOT_DRIVE) ||
			 (Current.RootKind == XPATH_ROOT_UNC) ||
			 (Current.RootKind == XPATH_ROOT_DEVICE) ) {
			char iSep = __xrtPathSepFor(Style);
			xstrview Tail = xrtStrSlice(Part, pParts->Root.Size, XRT_NPOS);

			if ( !xrtStrBufResize(pBuffer, Current.Root.Size) ) {
				return false;
			}
			if ( (pBuffer->Size == 0) ||
				 !__xrtPathIsSep(pBuffer->Data[pBuffer->Size - 1], Style) ) {
				if ( !xrtStrBufAppendByte(pBuffer, iSep) ) {
					return false;
				}
			}
			return xrtStrBufAppend(pBuffer, Tail);
		}
	}
	xrtStrBufClear(pBuffer);
	return xrtStrBufAppend(pBuffer, Part);
}



/* 按指定风格拼接并清理一组路径。 */
XRT_API str xrtPathBuild(const xstrview* arrParts, size_t iCount, xpathstyle Style)
{
	xstrbuf Buffer;
	str sJoined;
	__xrt_path_root Root;
	char iSep;

	if ( ((arrParts == NULL) && (iCount != 0)) ||
		 !__xrtPathStyle(Style, &Style) ) {
		if ( (arrParts == NULL) && (iCount != 0) ) {
			__xrtErrorSetInvalidArgument();
		}
		return NULL;
	}
	xrtStrBufInit(&Buffer);
	iSep = __xrtPathSepFor(Style);

	for ( size_t i = 0; i < iCount; i++ ) {
		xpathparts Parts;

		if ( !__xrtPathViewValid(arrParts[i], "build") ||
			 !xrtPathParse(arrParts[i], Style, &Parts) ) {
			xrtStrBufFree(&Buffer);
			return NULL;
		}
		if ( arrParts[i].Size == 0 ) {
			continue;
		}
		if ( (Parts.Flags & XPATH_FLAG_ROOTED) != 0 ) {
			if ( !__xrtPathBuildRooted(&Buffer, arrParts[i], &Parts, Style) ) {
				xrtStrBufFree(&Buffer);
				return NULL;
			}
		} else if ( (Buffer.Size != 0) &&
			 !__xrtPathIsSep(Buffer.Data[Buffer.Size - 1], Style) &&
			 !__xrtPathDrivePrefix(&Buffer, Style) ) {
			if ( !xrtStrBufAppendByte(&Buffer, iSep) ) {
				xrtStrBufFree(&Buffer);
				return NULL;
			}
		}
		if ( ((Parts.Flags & XPATH_FLAG_ROOTED) == 0) &&
			 !xrtStrBufAppend(&Buffer, arrParts[i]) ) {
			xrtStrBufFree(&Buffer);
			return NULL;
		}
	}

	if ( Buffer.Size == 0 ) {
		xrtStrBufFree(&Buffer);
		return xrtPathClean((xstrview){ NULL, 0 }, Style);
	}
	sJoined = xrtStrBufTake(&Buffer);
	if ( sJoined == NULL ) {
		return NULL;
	}
	Root = __xrtPathRoot(xrtStrView(sJoined), Style);
	(void)__xrtPathCleanWrite(sJoined, xrtStrView(sJoined), Root, Style);
	return sJoined;
}



/* 按本机风格拼接两个路径。 */
XRT_API str xrtPathJoin(cstr sLeft, cstr sRight)
{
	xstrview arrParts[2];

	if ( (sLeft == NULL) || (sRight == NULL) ) {
		__xrtErrorSetInvalidArgument();
		return NULL;
	}
	arrParts[0] = xrtStrView(sLeft);
	arrParts[1] = xrtStrView(sRight);
	return xrtPathBuild(arrParts, 2, XPATH_NATIVE);
}



/* 比较已经规范化的根；Windows 根按 ASCII 大小写不敏感比较。 */
static bool __xrtPathRootEqual(xstrview Left, xstrview Right, xpathstyle Style)
{
	if ( Left.Size != Right.Size ) {
		return false;
	}
	for ( size_t i = 0; i < Left.Size; i++ ) {
		char iLeft = Left.Data[i];
		char iRight = Right.Data[i];

		if ( __xrtPathIsSep(iLeft, Style) ) {
			iLeft = __xrtPathSepFor(Style);
		}
		if ( __xrtPathIsSep(iRight, Style) ) {
			iRight = __xrtPathSepFor(Style);
		}
		if ( Style == XPATH_WINDOWS ) {
			if ( !__xrtPathAsciiEqual(iLeft, iRight) ) {
				return false;
			}
		} else if ( iLeft != iRight ) {
			return false;
		}
	}
	return true;
}



/* 比较规范路径段；Windows 词法语义按 ASCII 大小写不敏感。 */
static bool __xrtPathSegmentEqual(xstrview Left, xstrview Right,
	xpathstyle Style)
{
	if ( Left.Size != Right.Size ) {
		return false;
	}
	if ( Style != XPATH_WINDOWS ) {
		return xrtStrEqual(Left, Right);
	}
	for ( size_t i = 0; i < Left.Size; i++ ) {
		if ( !__xrtPathAsciiEqual(Left.Data[i], Right.Data[i]) ) {
			return false;
		}
	}
	return true;
}



/* 读取规范路径的下一个非空段。 */
static bool __xrtPathNextSegment(xstrview Path, xpathstyle Style,
	size_t* pPosition, xstrview* pSegment)
{
	size_t i = *pPosition;
	size_t iStart;

	while ( (i < Path.Size) && __xrtPathIsSep(Path.Data[i], Style) ) {
		i++;
	}
	iStart = i;
	while ( (i < Path.Size) && !__xrtPathIsSep(Path.Data[i], Style) ) {
		i++;
	}
	*pPosition = i;
	if ( i == iStart ) {
		return false;
	}
	*pSegment = xrtStrSlice(Path, iStart, i - iStart);
	return true;
}



/* 判断规范相对路径是否只是当前目录点号。 */
static bool __xrtPathIsCurrent(xstrview Path, const xpathparts* pParts)
{
	return (pParts->RootKind == XPATH_ROOT_NONE) &&
		(Path.Size == 1) && (Path.Data[0] == '.');
}



/* 统计规范相对路径开头无法折叠的双点组件。 */
static size_t __xrtPathLeadingParents(xstrview Path,
	const xpathparts* pParts, xpathstyle Style)
{
	size_t iPosition = pParts->Root.Size;
	size_t iCount = 0;
	xstrview Segment;

	while ( __xrtPathNextSegment(Path, Style, &iPosition, &Segment) ) {
		if ( !xrtStrEqual(Segment, XRT_STR_LITERAL("..")) ) {
			break;
		}
		iCount++;
	}
	return iCount;
}



/* 纯词法计算相对路径。 */
XRT_API str xrtPathRelative(xstrview Base, xstrview Target, xpathstyle Style)
{
	str sBase;
	str sTarget;
	xstrview BaseView;
	xstrview TargetView;
	xpathparts BaseParts;
	xpathparts TargetParts;
	size_t iBase;
	size_t iTarget;
	xstrbuf Result;
	char iSep;

	if ( !__xrtPathStyle(Style, &Style) ||
		 !__xrtPathViewValid(Base, "relative") ||
		 !__xrtPathViewValid(Target, "relative") ) {
		return NULL;
	}
	sBase = xrtPathClean(Base, Style);
	if ( sBase == NULL ) {
		return NULL;
	}
	sTarget = xrtPathClean(Target, Style);
	if ( sTarget == NULL ) {
		xrtFree(sBase);
		return NULL;
	}
	BaseView = xrtStrView(sBase);
	TargetView = xrtStrView(sTarget);
	(void)xrtPathParse(BaseView, Style, &BaseParts);
	(void)xrtPathParse(TargetView, Style, &TargetParts);
	if ( (BaseParts.RootKind != TargetParts.RootKind) ||
		 !__xrtPathRootEqual(BaseParts.Root, TargetParts.Root, Style) ) {
		__xrtPathSetError(XERR_VALUE, XPATH_ERROR_ROOT, "relative",
			"paths have different roots", 0);
		xrtFree(sBase);
		xrtFree(sTarget);
		return NULL;
	}
	if ( BaseParts.RootKind == XPATH_ROOT_DEVICE ) {
		__xrtPathSetError(XERR_UNSUPPORTED, XPATH_ERROR_ROOT, "relative",
			"device namespace paths do not have ordinary relative semantics", 0);
		xrtFree(sBase);
		xrtFree(sTarget);
		return NULL;
	}
	if ( ((BaseParts.RootKind == XPATH_ROOT_NONE) ||
		 (BaseParts.RootKind == XPATH_ROOT_DRIVE_RELATIVE)) &&
		 (__xrtPathLeadingParents(TargetView, &TargetParts, Style) <
		  __xrtPathLeadingParents(BaseView, &BaseParts, Style)) ) {
		__xrtPathSetError(XERR_VALUE, XPATH_ERROR_ROOT, "relative",
			"target cannot be expressed from an unresolved parent base", 0);
		xrtFree(sBase);
		xrtFree(sTarget);
		return NULL;
	}

	iBase = __xrtPathIsCurrent(BaseView, &BaseParts) ?
		BaseView.Size : BaseParts.Root.Size;
	iTarget = __xrtPathIsCurrent(TargetView, &TargetParts) ?
		TargetView.Size : TargetParts.Root.Size;
	for ( ;; ) {
		size_t iBaseBefore = iBase;
		size_t iTargetBefore = iTarget;
		xstrview BaseSegment;
		xstrview TargetSegment;
		bool bBase = __xrtPathNextSegment(BaseView, Style, &iBase, &BaseSegment);
		bool bTarget = __xrtPathNextSegment(TargetView, Style,
			&iTarget, &TargetSegment);

		if ( !bBase || !bTarget ) {
			if ( bBase ) {
				iBase = iBaseBefore;
			}
			if ( bTarget ) {
				iTarget = iTargetBefore;
			}
			break;
		}
		if ( !__xrtPathSegmentEqual(BaseSegment, TargetSegment, Style) ) {
			iBase = iBaseBefore;
			iTarget = iTargetBefore;
			break;
		}
	}

	xrtStrBufInit(&Result);
	iSep = __xrtPathSepFor(Style);
	for ( ;; ) {
		xstrview Segment;

		if ( !__xrtPathNextSegment(BaseView, Style, &iBase, &Segment) ) {
			break;
		}
		if ( (Result.Size != 0) && !xrtStrBufAppendByte(&Result, iSep) ) {
			goto fail;
		}
		if ( !xrtStrBufAppend(&Result, XRT_STR_LITERAL("..")) ) {
			goto fail;
		}
	}
	for ( ;; ) {
		xstrview Segment;

		if ( !__xrtPathNextSegment(TargetView, Style, &iTarget, &Segment) ) {
			break;
		}
		if ( (Result.Size != 0) && !xrtStrBufAppendByte(&Result, iSep) ) {
			goto fail;
		}
		if ( !xrtStrBufAppend(&Result, Segment) ) {
			goto fail;
		}
	}
	xrtFree(sBase);
	xrtFree(sTarget);
	if ( Result.Size == 0 ) {
		xrtStrBufFree(&Result);
		return xrtStrDup(".");
	}
	return xrtStrBufTake(&Result);

fail:
	xrtStrBufFree(&Result);
	xrtFree(sBase);
	xrtFree(sTarget);
	return NULL;
}



/* 替换本机路径的末级名称。 */
XRT_API str xrtPathWithName(cstr sPath, cstr sName)
{
	xstrview Path;
	xpathparts Parts;
	xstrview arrParts[2];

	if ( !__xrtPathText(sPath, &Path) || (sName == NULL) ||
		 !xrtPathParse(Path, XPATH_NATIVE, &Parts) ) {
		if ( sName == NULL ) {
			__xrtErrorSetInvalidArgument();
		}
		return NULL;
	}
	arrParts[0] = Parts.Parent;
	arrParts[1] = xrtStrView(sName);
	return xrtPathBuild(arrParts, 2, XPATH_NATIVE);
}



/* 替换本机路径的最后一个扩展名。 */
XRT_API str xrtPathWithExt(cstr sPath, cstr sExtension)
{
	xstrview Path;
	xpathparts Parts;
	size_t iPrefix;
	size_t iExtension;
	size_t iDot;
	str sResult;

	if ( !__xrtPathText(sPath, &Path) || (sExtension == NULL) ||
		 !xrtPathParse(Path, XPATH_NATIVE, &Parts) ) {
		if ( sExtension == NULL ) {
			__xrtErrorSetInvalidArgument();
		}
		return NULL;
	}
	if ( Parts.Name.Size == 0 ) {
		__xrtPathSetError(XERR_VALUE, XPATH_ERROR_FORMAT, "with-ext",
			"path has no final name", 0);
		return NULL;
	}
	iExtension = strlen(sExtension);
	for ( size_t i = 0; i < iExtension; i++ ) {
		if ( (sExtension[i] == '/') || (sExtension[i] == '\\') ) {
			__xrtPathSetError(XERR_VALUE, XPATH_ERROR_FORMAT, "with-ext",
				"extension contains a path separator", 0);
			return NULL;
		}
	}
	iPrefix = (size_t)(Parts.Stem.Data - Path.Data) + Parts.Stem.Size;
	iDot = (iExtension != 0) && (sExtension[0] != '.') ? 1u : 0u;
	if ( iPrefix > (SIZE_MAX - iDot) ||
		 (iPrefix + iDot) > (SIZE_MAX - iExtension) ||
		 (iPrefix + iDot + iExtension) == SIZE_MAX ) {
		__xrtErrorSetSizeOverflow();
		return NULL;
	}
	sResult = (str)xrtMalloc(iPrefix + iDot + iExtension + 1u);
	if ( sResult == NULL ) {
		return NULL;
	}
	memcpy(sResult, Path.Data, iPrefix);
	if ( iDot != 0 ) {
		sResult[iPrefix] = '.';
	}
	if ( iExtension != 0 ) {
		memcpy(sResult + iPrefix + iDot, sExtension, iExtension);
	}
	sResult[iPrefix + iDot + iExtension] = 0;
	return sResult;
}



#if defined(XRT_FEATURE_PATH_SAFE)

/* 初始化可移植路径段的流式检查状态。 */
void __xrtPathSafeSegmentInit(
	xrt_path_safe_segment* pState
)
{
	if ( pState != NULL ) {
		memset(pState, 0, sizeof(*pState));
	}
}



/* 流式收集设备名基础部分并拒绝跨平台非法字节。 */
bool __xrtPathSafeSegmentFeed(
	xrt_path_safe_segment* pState,
	uint8 iValue
)
{
	if ( (pState == NULL) || pState->Invalid ||
		(pState->Size == SIZE_MAX) ) {
		return false;
	}
	if ( (iValue < 0x20u) || (iValue == 0x7Fu) ||
		 (iValue == (uint8)'/') || (iValue == (uint8)'\\') ||
		 (iValue == (uint8)'<') || (iValue == (uint8)'>') ||
		 (iValue == (uint8)':') || (iValue == (uint8)'"') ||
		 (iValue == (uint8)'|') || (iValue == (uint8)'?') ||
		 (iValue == (uint8)'*') ) {
		pState->Invalid = true;
		return false;
	}
	if ( pState->Size == 0 ) {
		pState->First = iValue;
	}
	pState->Last = iValue;
	pState->Size++;

	/* 扩展名不解除设备名语义，只保存第一个点之前的基础名。 */
	if ( !pState->BaseDone ) {
		if ( iValue == (uint8)'.' ) {
			pState->BaseDone = true;
		} else {
			if ( pState->BaseSize < sizeof(pState->Base) ) {
				pState->Base[pState->BaseSize] = iValue;
			}
			if ( pState->BaseSize != SIZE_MAX ) {
				pState->BaseSize++;
			}
			if ( iValue != (uint8)' ' ) {
				pState->BaseTrimmedSize = pState->BaseSize;
			}
		}
	}
	return true;
}



/* 完成点段、尾部空格和 Windows 设备保留名检查。 */
bool __xrtPathSafeSegmentFinish(
	const xrt_path_safe_segment* pState
)
{
	if ( (pState == NULL) || pState->Invalid ||
		(pState->Size == 0) ||
		((pState->Size == 1) && (pState->First == (uint8)'.')) ||
		((pState->Size == 2) && (pState->First == (uint8)'.') &&
		 (pState->Last == (uint8)'.')) ||
		(pState->Last == (uint8)'.') ||
		(pState->Last == (uint8)' ') ) {
		return false;
	}
	return (pState->BaseTrimmedSize > sizeof(pState->Base)) ||
		!__xrtPathWindowsDeviceBase(
			pState->Base,
			pState->BaseTrimmedSize
		);
}



_Static_assert(
	sizeof(xrt_path_safe_segment) <= XPATH_SAFE_SEGMENT_STORAGE_SIZE,
	"XPATH_SAFE_SEGMENT_STORAGE_SIZE is too small"
);



/* 初始化公开的固定存储路径段检查器。 */
XRT_API void xrtPathSafeSegmentInit(xpathsafesegment* pState)
{
	if ( __xrtRangeValid(pState, sizeof(*pState)) ) {
		memset(pState, 0, sizeof(*pState));
		__xrtPathSafeSegmentInit((xrt_path_safe_segment*)pState);
	}
}



/* 向公开路径段检查器加入一个已解码字节。 */
XRT_API bool xrtPathSafeSegmentFeed(
	xpathsafesegment* pState,
	uint8 iValue
)
{
	if ( !__xrtRangeValid(pState, sizeof(*pState)) ) {
		return false;
	}
	return __xrtPathSafeSegmentFeed(
		(xrt_path_safe_segment*)pState,
		iValue
	);
}



/* 完成公开路径段检查器并返回可移植性结论。 */
XRT_API bool xrtPathSafeSegmentFinish(
	const xpathsafesegment* pState
)
{
	if ( !__xrtRangeValid(pState, sizeof(*pState)) ) {
		return false;
	}
	return __xrtPathSafeSegmentFinish(
		(const xrt_path_safe_segment*)pState
	);
}



/* 检查一个完整的可移植归档路径段。 */
static bool __xrtPathSafeSegment(xstrview Segment)
{
	xrt_path_safe_segment State;
	size_t i;

	__xrtPathSafeSegmentInit(&State);
	for ( i = 0; i < Segment.Size; i++ ) {
		if ( !__xrtPathSafeSegmentFeed(
			&State,
			(uint8)Segment.Data[i]
		) ) {
			return false;
		}
	}
	return __xrtPathSafeSegmentFinish(&State);
}



/* 检查跨 Windows/POSIX 可移植的 UTF-8 相对归档条目。 */
XRT_API bool xrtPathIsSafeEntry(xstrview Path, bool bDirectory)
{
	size_t iPosition = 0;

	if ( !__xrtPathViewValid(Path, "safe-entry") || (Path.Size == 0) ||
		 !xrtUtf8Valid(Path, NULL) || (Path.Data[0] == '/') ||
		 (Path.Data[0] == '\\') ) {
		return false;
	}
	if ( Path.Data[Path.Size - 1] == '/' ) {
		if ( !bDirectory ) {
			return false;
		}
		Path.Size--;
		if ( Path.Size == 0 ) {
			return false;
		}
	}
	while ( iPosition < Path.Size ) {
		size_t iStart = iPosition;

		while ( (iPosition < Path.Size) && (Path.Data[iPosition] != '/') ) {
			iPosition++;
		}
		if ( !__xrtPathSafeSegment(
			(xstrview){ Path.Data + iStart, iPosition - iStart }) ) {
			return false;
		}
		if ( iPosition < Path.Size ) {
			iPosition++;
			if ( iPosition == Path.Size ) {
				return false;
			}
		}
	}
	return true;
}

#endif

#undef XRT_PATH_ITER_STATE

#endif
