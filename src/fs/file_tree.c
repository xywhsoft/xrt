#include "../internal/xrt_dir.h"

#include <stdio.h>

#if (defined(_WIN32) || defined(_WIN64)) && defined(__TINYC__)
	/* TinyCC 的旧 Win32 头缺少 Vista 起提供的序数宽字符串比较原型。 */
	int WINAPI CompareStringOrdinal(const wchar_t* sLeft, int iLeft,
		const wchar_t* sRight, int iRight, BOOL bIgnoreCase);
#endif



#if defined(XRT_FEATURE_FILE_TREE)

/* 新目标在同目录私有暂存后再一次性发布。 */
#define XRT_TREE_STAGE_ATTEMPTS 128u



/* 复制回调共享源根、目标根和覆盖策略。 */
typedef struct __xrt_tree_copy {
	cstr Source;
	cstr Target;
	uint32 Flags;
} __xrt_tree_copy;



/* 删除回调只需要知道是否保留根目录。 */
typedef struct __xrt_tree_remove {
	bool KeepRoot;
} __xrt_tree_remove;



/* 设置目录树模块结构化错误。 */
static void __xrtTreeError(xerrkind Kind, xtreeerror Code,
	cstr sOperation, cstr sMessage)
{
	xerrordesc Desc;
	xerror* pError;

	memset(&Desc, 0, sizeof(Desc));
	Desc.Kind = Kind;
	Desc.Domain = "xrt.tree";
	Desc.Code = (int32)Code;
	Desc.Operation = sOperation;
	Desc.Message = sMessage;
	pError = xrtErrorBuild(&Desc);
	if ( pError != NULL ) {
		__xrtErrorSetOwned(pError);
	}
}



/* 初始化目录树复制选项。 */
XRT_API void xrtTreeCopyOptionsInit(xtreecopyoptions* pOptions)
{
	if ( pOptions != NULL ) {
		pOptions->Flags = 0u;
	}
}



/* 展开并验证高级复制选项。 */
static bool __xrtTreeOptions(const xtreecopyoptions* pInput,
	xtreecopyoptions* pOptions)
{
	const uint32 iKnown = XTREE_COPY_MERGE | XTREE_COPY_REPLACE |
		XTREE_COPY_FOLLOW_LINKS | XTREE_COPY_SKIP_LINKS |
		XTREE_COPY_ONE_FILESYSTEM | XTREE_COPY_SKIP_SPECIAL |
		XTREE_COPY_METADATA;

	if ( pInput == NULL ) {
		xrtTreeCopyOptionsInit(pOptions);
	} else {
		*pOptions = *pInput;
	}
	if ( ((pOptions->Flags & ~iKnown) != 0u) ||
		 (((pOptions->Flags & XTREE_COPY_FOLLOW_LINKS) != 0u) &&
		  ((pOptions->Flags & XTREE_COPY_SKIP_LINKS) != 0u)) ||
		 (((pOptions->Flags & XTREE_COPY_REPLACE) != 0u) &&
		  ((pOptions->Flags & XTREE_COPY_MERGE) == 0u)) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	return true;
}



/* 为新目标排他创建同目录暂存树，避免暴露或误删半成品。 */
static str __xrtTreeStage(cstr sTarget)
{
	str sParent = xrtPathParent(sTarget);
	uint32 iAttempt;

	if ( sParent == NULL ) {
		return NULL;
	}
	for ( iAttempt = 0u; iAttempt < XRT_TREE_STAGE_ATTEMPTS; iAttempt++ ) {
		char sName[40];
		str sStage;
		uint64 iRandom;

		if ( !xrtSecureRandom(&iRandom, sizeof(iRandom)) ) {
			xrtFree(sParent);
			return NULL;
		}
		(void)snprintf(sName, sizeof(sName), ".xrt-tree-%016llx.tmp",
			(unsigned long long)iRandom);
		sStage = xrtPathJoin(sParent, sName);
		if ( sStage == NULL ) {
			xrtFree(sParent);
			return NULL;
		}
		if ( xrtDirCreateMode(sStage, 0777u) ) {
			xrtFree(sParent);
			return sStage;
		}
		if ( (xrtGetError() == NULL) ||
			 (xrtErrorKind(xrtGetError()) != XERR_EXISTS) ) {
			xrtFree(sStage);
			xrtFree(sParent);
			return NULL;
		}
		xrtClearError();
		xrtFree(sStage);
	}
	xrtFree(sParent);
	__xrtTreeError(XERR_AGAIN, XTREE_ERROR_TARGET, "copy",
		"failed to reserve a private tree staging directory");
	return NULL;
}



/* 清理私有暂存树，同时保留触发复制失败的原始错误。 */
static void __xrtTreeStageCleanup(cstr sStage)
{
	xerror* pError = xrtTakeError();

	(void)xrtDirRemoveAll(sStage);
	xrtClearError();
	if ( pError != NULL ) {
		__xrtErrorSetOwned(pError);
	}
}



/* 判断绝对目标是否位于绝对源目录内部。 */
static bool __xrtTreeDescendant(cstr sSource, cstr sTarget, bool* pDescendant)
{
	#if defined(_WIN32) || defined(_WIN64)
		uint16* pSource = __xrtPathToWide(sSource, NULL);
		uint16* pTarget;
		size_t iSource;
		size_t iTarget;
		int iEqual;

		if ( pSource == NULL ) {
			return false;
		}
		pTarget = __xrtPathToWide(sTarget, NULL);
		if ( pTarget == NULL ) {
			xrtFree(pSource);
			return false;
		}
		iSource = wcslen((const wchar_t*)pSource);
		iTarget = wcslen((const wchar_t*)pTarget);
		while ( (iSource != 0u) &&
			 ((pSource[iSource - 1u] == L'\\') ||
			  (pSource[iSource - 1u] == L'/')) ) {
			iSource--;
		}
		if ( (iSource > (size_t)INT_MAX) ||
			 (iTarget > (size_t)INT_MAX) ) {
			xrtFree(pTarget);
			xrtFree(pSource);
			__xrtErrorSetSizeOverflow();
			return false;
		}
		iEqual = (iTarget >= iSource) ?
			CompareStringOrdinal(
				(const wchar_t*)pSource, (int)iSource,
				(const wchar_t*)pTarget, (int)iSource, TRUE) : 0;
		*pDescendant = (iEqual == CSTR_EQUAL) &&
			((iTarget == iSource) ||
			 (pTarget[iSource] == L'\\') ||
			 (pTarget[iSource] == L'/'));
		xrtFree(pTarget);
		xrtFree(pSource);
		return true;
	#else
		str sRelative = xrtPathRel(sSource, sTarget);
		xstrview Relative;
		xpathparts Parts;
		bool bParent;

		if ( sRelative == NULL ) {
			return false;
		}
		Relative = xrtStrView(sRelative);
		if ( !xrtPathParse(Relative, XPATH_NATIVE, &Parts) ) {
			xrtFree(sRelative);
			return false;
		}
		bParent = (Relative.Size >= 2u) && (Relative.Data[0] == '.') &&
			 (Relative.Data[1] == '.') &&
			 ((Relative.Size == 2u) ||
			  (Relative.Data[2] == xrtPathSep()));
		*pDescendant =
			((Parts.Flags & XPATH_FLAG_ABSOLUTE) == 0u) && !bParent;
		xrtFree(sRelative);
		return true;
	#endif
}



/* 解析目标最近存在的父级，避免符号链接祖先把目标重新指回源树。 */
static str __xrtTreePhysicalTarget(cstr sTarget)
{
	xfileinfo Info;
	bool bExists;
	str sCurrent;
	str sSuffix;
	str sPhysical;
	str sResult;

	if ( !__xrtFilePathInfo(sTarget, false, &bExists, &Info) ) {
		return NULL;
	}
	if ( bExists && (Info.Type == XFILE_TYPE_DIRECTORY) ) {
		return xrtPathReal(sTarget);
	}
	sCurrent = xrtPathParent(sTarget);
	sSuffix = xrtPathName(sTarget);
	if ( (sCurrent == NULL) || (sSuffix == NULL) ) {
		xrtFree(sCurrent);
		xrtFree(sSuffix);
		return NULL;
	}
	for ( ;; ) {
		str sParent;
		str sName;
		str sJoined;

		if ( !__xrtFilePathInfo(sCurrent, false, &bExists, &Info) ) {
			xrtFree(sCurrent);
			xrtFree(sSuffix);
			return NULL;
		}
		if ( bExists ) {
			break;
		}
		sParent = xrtPathParent(sCurrent);
		sName = xrtPathName(sCurrent);
		if ( (sParent == NULL) || (sName == NULL) ) {
			xrtFree(sParent);
			xrtFree(sName);
			xrtFree(sCurrent);
			xrtFree(sSuffix);
			return NULL;
		}
		if ( (sParent[0] == '\0') || (sName[0] == '\0') ||
			 (strcmp(sParent, sCurrent) == 0) ) {
			xrtFree(sParent);
			xrtFree(sName);
			xrtFree(sCurrent);
			xrtFree(sSuffix);
			__xrtTreeError(XERR_NOT_FOUND, XTREE_ERROR_TARGET, "copy",
				"the tree copy target has no existing parent");
			return NULL;
		}
		sJoined = xrtPathJoin(sName, sSuffix);
		xrtFree(sName);
		if ( sJoined == NULL ) {
			xrtFree(sParent);
			xrtFree(sCurrent);
			xrtFree(sSuffix);
			return NULL;
		}
		xrtFree(sCurrent);
		xrtFree(sSuffix);
		sCurrent = sParent;
		sSuffix = sJoined;
	}
	sPhysical = xrtPathReal(sCurrent);
	xrtFree(sCurrent);
	if ( sPhysical == NULL ) {
		xrtFree(sSuffix);
		return NULL;
	}
	sResult = xrtPathJoin(sPhysical, sSuffix);
	xrtFree(sPhysical);
	xrtFree(sSuffix);
	return sResult;
}



/* 把平台可表达的访问时间、修改时间和权限属性应用到复制目标。 */
static bool __xrtTreeMetadata(const xwalkentry* pEntry,
	cstr sTarget, bool bPhysicalLink)
{
	const xtime* pAccessed =
		((pEntry->Info.Available & XFILE_INFO_ACCESS_TIME) != 0u) ?
		&pEntry->Info.Accessed : NULL;
	const xtime* pModified =
		((pEntry->Info.Available & XFILE_INFO_MODIFY_TIME) != 0u) ?
		&pEntry->Info.Modified : NULL;

	if ( ((pAccessed != NULL) || (pModified != NULL)) &&
		 !xrtPathSetTimes(sTarget, !bPhysicalLink, pAccessed, pModified) ) {
		return false;
	}
	if ( bPhysicalLink ) {
		return true;
	}
	#if defined(_WIN32) || defined(_WIN64)
		{
			const uint32 iMask = FILE_ATTRIBUTE_READONLY |
				FILE_ATTRIBUTE_HIDDEN | FILE_ATTRIBUTE_SYSTEM |
				FILE_ATTRIBUTE_ARCHIVE | FILE_ATTRIBUTE_TEMPORARY |
				FILE_ATTRIBUTE_OFFLINE | FILE_ATTRIBUTE_NOT_CONTENT_INDEXED;
			uint32 iAttributes = pEntry->Info.Attributes & iMask;

			if ( iAttributes == 0u ) {
				iAttributes = FILE_ATTRIBUTE_NORMAL;
			}
			return xrtPathSetAttributes(sTarget, iAttributes);
		}
	#else
		if ( (pEntry->Info.Available & XFILE_INFO_MODE) != 0u ) {
			return xrtPathSetMode(sTarget, true, pEntry->Info.Mode);
		}
		return true;
	#endif
}



/* 把源遍历路径映射到目标目录中的拥有路径。 */
static str __xrtTreeTargetPath(const __xrt_tree_copy* pCopy,
	const xwalkentry* pEntry)
{
	str sRelative;
	str sTarget;

	if ( pEntry->Depth == 0u ) {
		return xrtStrDup(pCopy->Target);
	}
	sRelative = xrtPathRel(pCopy->Source, pEntry->Path);
	if ( sRelative == NULL ) {
		return NULL;
	}
	sTarget = xrtPathJoin(pCopy->Target, sRelative);
	xrtFree(sRelative);
	return sTarget;
}



/* 删除一个已知对象自身，目录则递归删除。 */
static bool __xrtTreeDeleteObject(cstr sPath, const xfileinfo* pInfo)
{
	if ( pInfo->Type == XFILE_TYPE_DIRECTORY ) {
		return xrtDirRemoveAll(sPath);
	}
	if ( pInfo->Type == XFILE_TYPE_LINK ) {
		return xrtLinkDelete(sPath);
	}
	return xrtFileDelete(sPath);
}



/* 确保目标目录满足合并和替换策略。 */
static bool __xrtTreeEnsureDirectory(cstr sPath, uint32 iFlags)
{
	xfileinfo Info;
	bool bExists;

	if ( !__xrtFilePathInfo(sPath, false, &bExists, &Info) ) {
		return false;
	}
	if ( !bExists ) {
		return xrtDirCreate(sPath);
	}
	if ( Info.Type == XFILE_TYPE_DIRECTORY ) {
		if ( (iFlags & XTREE_COPY_MERGE) != 0u ) {
			return true;
		}
		__xrtTreeError(XERR_EXISTS, XTREE_ERROR_TARGET, "copy",
			"the target directory already exists");
		return false;
	}
	if ( (iFlags & XTREE_COPY_REPLACE) == 0u ) {
		__xrtTreeError(XERR_EXISTS, XTREE_ERROR_TARGET, "copy",
			"a non-directory object occupies the target directory path");
		return false;
	}
	if ( !__xrtTreeDeleteObject(sPath, &Info) ) {
		return false;
	}
	return xrtDirCreate(sPath);
}



/* 为复制符号链接准备不存在的目标名称。 */
static bool __xrtTreePrepareLink(cstr sPath, uint32 iFlags)
{
	xfileinfo Info;
	bool bExists;

	if ( !__xrtFilePathInfo(sPath, false, &bExists, &Info) ) {
		return false;
	}
	if ( !bExists ) {
		return true;
	}
	if ( (iFlags & XTREE_COPY_REPLACE) == 0u ) {
		__xrtTreeError(XERR_EXISTS, XTREE_ERROR_TARGET, "copy-link",
			"the symbolic link target path already exists");
		return false;
	}
	return __xrtTreeDeleteObject(sPath, &Info);
}



/* 复制有效普通文件前处理目录、链接或特殊对象占用的目标名称。 */
static bool __xrtTreeCopyFile(cstr sSource, cstr sTarget,
	uint32 iFlags, bool bFollowSource)
{
	xfileinfo Info;
	bool bExists;
	bool bReplace = (iFlags & XTREE_COPY_REPLACE) != 0u;

	if ( !__xrtFilePathInfo(sTarget, false, &bExists, &Info) ) {
		return false;
	}
	if ( bExists && (Info.Type != XFILE_TYPE_FILE) ) {
		if ( !bReplace ) {
			__xrtTreeError(XERR_EXISTS, XTREE_ERROR_TARGET, "copy-file",
				"a non-file object occupies the target file path");
			return false;
		}
		if ( !__xrtTreeDeleteObject(sTarget, &Info) ) {
			return false;
		}
		bExists = false;
	}
	return __xrtFileCopy(sSource, sTarget,
		bReplace && bExists, bFollowSource);
}



/* 复制一个符号链接并保留其存储目标文本。 */
static bool __xrtTreeCopyLink(const xwalkentry* pEntry,
	cstr sTarget, uint32 iFlags)
{
	str sLinkTarget;
	bool bDirectory = false;
	bool bResult;

	if ( !__xrtTreePrepareLink(sTarget, iFlags) ) {
		return false;
	}
	sLinkTarget = xrtLinkRead(pEntry->Path);
	if ( sLinkTarget == NULL ) {
		return false;
	}
	#if defined(_WIN32) || defined(_WIN64)
		bDirectory =
			(pEntry->Info.Attributes & FILE_ATTRIBUTE_DIRECTORY) != 0u;
	#endif
	bResult = xrtLinkCreate(sLinkTarget, sTarget, bDirectory);
	xrtFree(sLinkTarget);
	if ( bResult && ((iFlags & XTREE_COPY_METADATA) != 0u) ) {
		bResult = __xrtTreeMetadata(pEntry, sTarget, true);
	}
	return bResult;
}



/* 把一个 walk 事件转换为对应目标树操作。 */
static xwalkcontrol __xrtTreeCopyProc(const xwalkentry* pEntry, ptr pUserData)
{
	__xrt_tree_copy* pCopy = (__xrt_tree_copy*)pUserData;
	str sTarget = __xrtTreeTargetPath(pCopy, pEntry);
	bool bResult = true;

	if ( sTarget == NULL ) {
		return XWALK_ERROR;
	}
	if ( (pEntry->Flags & XWALK_ENTRY_CYCLE) != 0u ) {
		xrtFree(sTarget);
		__xrtTreeError(XERR_VALUE, XTREE_ERROR_LINK_CYCLE, "copy",
			"following symbolic links encountered a directory cycle");
		return XWALK_ERROR;
	}
	if ( pEntry->Event == XWALK_ENTER ) {
		bResult = __xrtTreeEnsureDirectory(sTarget,
			pCopy->Flags | ((pEntry->Depth == 0u) ? 0u : XTREE_COPY_MERGE));
	} else if ( pEntry->Event == XWALK_LEAVE ) {
		if ( (pCopy->Flags & XTREE_COPY_METADATA) != 0u ) {
			bResult = __xrtTreeMetadata(pEntry, sTarget, false);
		}
	} else if ( pEntry->Event == XWALK_ITEM ) {
		if ( (pEntry->Flags & XWALK_ENTRY_LINK) != 0u ) {
			if ( (pCopy->Flags & XTREE_COPY_SKIP_LINKS) != 0u ) {
				bResult = true;
			} else if ( (pCopy->Flags & XTREE_COPY_FOLLOW_LINKS) == 0u ) {
				bResult = __xrtTreeCopyLink(pEntry, sTarget, pCopy->Flags);
			} else if ( pEntry->Info.Type == XFILE_TYPE_FILE ) {
				bResult = __xrtTreeCopyFile(pEntry->Path, sTarget,
					pCopy->Flags, true);
				if ( bResult &&
					 ((pCopy->Flags & XTREE_COPY_METADATA) != 0u) ) {
					bResult = __xrtTreeMetadata(pEntry, sTarget, false);
				}
			} else if ( (pCopy->Flags &
				XTREE_COPY_SKIP_SPECIAL) == 0u ) {
				__xrtTreeError(XERR_UNSUPPORTED,
					XTREE_ERROR_SPECIAL, "copy",
					"the followed link targets an unsupported special object");
				bResult = false;
			}
		} else if ( pEntry->Info.Type == XFILE_TYPE_FILE ) {
			bResult = __xrtTreeCopyFile(pEntry->Path, sTarget,
				pCopy->Flags, false);
			if ( bResult && ((pCopy->Flags & XTREE_COPY_METADATA) != 0u) ) {
				bResult = __xrtTreeMetadata(pEntry, sTarget, false);
			}
		} else if ( (pCopy->Flags & XTREE_COPY_SKIP_SPECIAL) == 0u ) {
			__xrtTreeError(XERR_UNSUPPORTED, XTREE_ERROR_SPECIAL, "copy",
				"the source tree contains an unsupported special object");
			bResult = false;
		}
	}
	xrtFree(sTarget);
	return bResult ? XWALK_CONTINUE : XWALK_ERROR;
}



/* 使用高级选项复制目录树。 */
XRT_API bool xrtFileTreeCopy(cstr sSource, cstr sTarget,
	const xtreecopyoptions* pInput, xwalkstats* pStats)
{
	xtreecopyoptions Options;
	xwalkoptions WalkOptions;
	__xrt_tree_copy Copy;
	xfileinfo PhysicalSource;
	xfileinfo SourceInfo;
	str sSourceAbs;
	str sTargetAbs;
	str sSourceReal;
	str sTargetReal;
	str sStage = NULL;
	bool bDescendant;
	bool bTargetExisted;
	xfileinfo TargetInfo;
	bool bResult;
	xwalkstats Stats;

	if ( !__xrtTreeOptions(pInput, &Options) ||
		 (sSource == NULL) || (sSource[0] == '\0') ||
		 (sTarget == NULL) || (sTarget[0] == '\0') ) {
		if ( (sSource == NULL) || (sSource[0] == '\0') ||
			 (sTarget == NULL) || (sTarget[0] == '\0') ) {
			__xrtErrorSetInvalidArgument();
		}
		return false;
	}
	if ( !xrtPathStat(sSource, false, &PhysicalSource) ) {
		return false;
	}
	SourceInfo = PhysicalSource;
	if ( (PhysicalSource.Type == XFILE_TYPE_LINK) &&
		 ((Options.Flags & XTREE_COPY_FOLLOW_LINKS) != 0u) &&
		 !xrtPathStat(sSource, true, &SourceInfo) ) {
		return false;
	}
	if ( SourceInfo.Type != XFILE_TYPE_DIRECTORY ) {
		__xrtTreeError(XERR_TYPE, XTREE_ERROR_SOURCE, "copy",
			"the tree copy source is not a directory");
		return false;
	}
	sSourceAbs = xrtPathAbs(sSource);
	sTargetAbs = xrtPathAbs(sTarget);
	if ( (sSourceAbs == NULL) || (sTargetAbs == NULL) ) {
		xrtFree(sSourceAbs);
		xrtFree(sTargetAbs);
		return false;
	}
	sSourceReal = xrtPathReal(sSourceAbs);
	sTargetReal = __xrtTreePhysicalTarget(sTargetAbs);
	if ( (sSourceReal == NULL) || (sTargetReal == NULL) ) {
		xrtFree(sSourceReal);
		xrtFree(sTargetReal);
		xrtFree(sSourceAbs);
		xrtFree(sTargetAbs);
		return false;
	}
	if ( !__xrtTreeDescendant(sSourceReal, sTargetReal, &bDescendant) ) {
		xrtFree(sSourceReal);
		xrtFree(sTargetReal);
		xrtFree(sSourceAbs);
		xrtFree(sTargetAbs);
		return false;
	}
	xrtFree(sSourceReal);
	xrtFree(sTargetReal);
	if ( bDescendant ) {
		xrtFree(sSourceAbs);
		xrtFree(sTargetAbs);
		__xrtTreeError(XERR_ARGUMENT, XTREE_ERROR_DESCENDANT, "copy",
			"the tree copy target is the source or its descendant");
		return false;
	}
	if ( !__xrtFilePathInfo(sTargetAbs, false,
		&bTargetExisted, &TargetInfo) ) {
		xrtFree(sSourceAbs);
		xrtFree(sTargetAbs);
		return false;
	}
	if ( bTargetExisted && (TargetInfo.Type == XFILE_TYPE_DIRECTORY) &&
		 __xrtFileInfoSame(&SourceInfo, &TargetInfo) ) {
		xrtFree(sSourceAbs);
		xrtFree(sTargetAbs);
		__xrtTreeError(XERR_ARGUMENT, XTREE_ERROR_DESCENDANT, "copy",
			"the source and target refer to the same directory");
		return false;
	}
	if ( !bTargetExisted ) {
		sStage = __xrtTreeStage(sTargetAbs);
		if ( sStage == NULL ) {
			xrtFree(sSourceAbs);
			xrtFree(sTargetAbs);
			return false;
		}
	}
	Copy.Source = sSourceAbs;
	Copy.Target = sStage != NULL ? sStage : sTargetAbs;
	Copy.Flags = Options.Flags |
		(sStage != NULL ? XTREE_COPY_MERGE : 0u);
	xrtWalkOptionsInit(&WalkOptions);
	if ( (Options.Flags & XTREE_COPY_FOLLOW_LINKS) != 0u ) {
		WalkOptions.Flags |= XWALK_FOLLOW_LINKS;
	}
	if ( (Options.Flags & XTREE_COPY_ONE_FILESYSTEM) != 0u ) {
		WalkOptions.Flags |= XWALK_ONE_FILESYSTEM;
	}
	bResult = xrtFileWalk(sSourceAbs, &WalkOptions,
		__xrtTreeCopyProc, &Copy, pStats != NULL ? &Stats : NULL);
	if ( bResult && (sStage != NULL) ) {
		bResult = xrtPathRename(sStage, sTargetAbs, false);
	}
	if ( !bResult && (sStage != NULL) ) {
		__xrtTreeStageCleanup(sStage);
	}
	xrtFree(sStage);
	xrtFree(sSourceAbs);
	xrtFree(sTargetAbs);
	if ( bResult && (pStats != NULL) ) {
		*pStats = Stats;
	}
	return bResult;
}



/* 常用目录复制。 */
XRT_API bool xrtDirCopy(cstr sSource, cstr sTarget, bool bReplace)
{
	xtreecopyoptions Options;

	xrtTreeCopyOptionsInit(&Options);
	if ( bReplace ) {
		Options.Flags = XTREE_COPY_MERGE | XTREE_COPY_REPLACE;
	}
	return xrtFileTreeCopy(sSource, sTarget, &Options, NULL);
}



/* 删除 walk 回调按后序移除条目和目录。 */
static xwalkcontrol __xrtTreeRemoveProc(const xwalkentry* pEntry, ptr pUserData)
{
	__xrt_tree_remove* pRemove = (__xrt_tree_remove*)pUserData;
	bool bResult = true;

	if ( pEntry->Event == XWALK_ITEM ) {
		bResult = ((pEntry->Flags & XWALK_ENTRY_LINK) != 0u) ?
			xrtLinkDelete(pEntry->Path) : xrtFileDelete(pEntry->Path);
	} else if ( (pEntry->Event == XWALK_LEAVE) &&
		 (!pRemove->KeepRoot || (pEntry->Depth != 0u)) ) {
		bResult = xrtDirRemove(pEntry->Path);
	}
	return bResult ? XWALK_CONTINUE : XWALK_ERROR;
}



/* 判断路径是否是不能递归删除的文件系统根。 */
static bool __xrtTreeIsRoot(cstr sPath, bool* pRoot)
{
	str sAbsolute = xrtPathAbs(sPath);

	if ( sAbsolute == NULL ) {
		return false;
	}
	*pRoot = xrtPathIsRoot(sAbsolute);
	xrtFree(sAbsolute);
	return true;
}



/* 后序删除目录树或只清空其内容。 */
XRT_API bool xrtFileTreeRemove(cstr sPath, bool bKeepRoot,
	xwalkstats* pStats)
{
	xfileinfo Info;
	bool bRoot;
	__xrt_tree_remove Remove;

	if ( (sPath == NULL) || (sPath[0] == '\0') ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	if ( !__xrtTreeIsRoot(sPath, &bRoot) ) {
		return false;
	}
	if ( bRoot ) {
		__xrtTreeError(XERR_PERMISSION, XTREE_ERROR_ROOT, "remove",
			"recursive removal of a filesystem root is forbidden");
		return false;
	}
	if ( !xrtPathStat(sPath, false, &Info) ) {
		return false;
	}
	if ( Info.Type != XFILE_TYPE_DIRECTORY ) {
		__xrtTreeError(XERR_TYPE, XTREE_ERROR_SOURCE, "remove",
			"the recursive removal path is not a directory");
		return false;
	}
	Remove.KeepRoot = bKeepRoot;
	return xrtFileWalk(sPath, NULL,
		__xrtTreeRemoveProc, &Remove, pStats);
}



/* 递归删除一个目录及全部内容。 */
XRT_API bool xrtDirRemoveAll(cstr sPath)
{
	return xrtFileTreeRemove(sPath, false, NULL);
}



/* 删除目录全部内容但保留目录自身。 */
XRT_API bool xrtDirClean(cstr sPath)
{
	return xrtFileTreeRemove(sPath, true, NULL);
}



/* 优先同卷改名，跨卷时复制成功后删除源目录树。 */
XRT_API bool xrtDirMove(cstr sSource, cstr sTarget, bool bReplace)
{
	xfileinfo SourceInfo;
	xfileinfo TargetInfo;
	bool bTargetExists;

	if ( (sSource == NULL) || (sSource[0] == '\0') ||
		 (sTarget == NULL) || (sTarget[0] == '\0') ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	if ( !xrtPathStat(sSource, false, &SourceInfo) ) {
		return false;
	}
	if ( SourceInfo.Type != XFILE_TYPE_DIRECTORY ) {
		__xrtTreeError(XERR_TYPE, XTREE_ERROR_SOURCE, "move",
			"the directory move source is not a directory");
		return false;
	}
	if ( !__xrtFilePathInfo(sTarget, false,
		&bTargetExists, &TargetInfo) ) {
		return false;
	}
	if ( bTargetExists ) {
		if ( (TargetInfo.Type == XFILE_TYPE_DIRECTORY) &&
			 __xrtFileInfoSame(&SourceInfo, &TargetInfo) ) {
			__xrtTreeError(XERR_ARGUMENT, XTREE_ERROR_DESCENDANT, "move",
				"the source and target refer to the same directory");
			return false;
		}
		if ( !bReplace ) {
			__xrtTreeError(XERR_EXISTS, XTREE_ERROR_TARGET, "move",
				"the directory move target already exists");
			return false;
		}
		if ( !xrtDirCopy(sSource, sTarget, true) ) {
			return false;
		}
		return xrtDirRemoveAll(sSource);
	}
	if ( xrtPathRename(sSource, sTarget, false) ) {
		return true;
	}
	if ( !__xrtFileCrossDevice() &&
		 ((xrtGetError() == NULL) ||
		  (xrtErrorKind(xrtGetError()) != XERR_UNSUPPORTED)) ) {
		return false;
	}
	xrtClearError();
	if ( !xrtDirCopy(sSource, sTarget, false) ) {
		return false;
	}
	return xrtDirRemoveAll(sSource);
}



/* 统计目录根和指定深度内的全部对象。 */
XRT_API bool xrtDirStats(cstr sPath, bool bRecursive, xwalkstats* pStats)
{
	xfileinfo Info;
	xwalkoptions Options;

	if ( pStats == NULL ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	if ( !xrtPathStat(sPath, false, &Info) ) {
		return false;
	}
	if ( Info.Type != XFILE_TYPE_DIRECTORY ) {
		__xrtTreeError(XERR_TYPE, XTREE_ERROR_SOURCE, "stats",
			"the directory statistics path is not a directory");
		return false;
	}
	xrtWalkOptionsInit(&Options);
	if ( !bRecursive ) {
		Options.MaxDepth = 1u;
	}
	return xrtFileWalk(sPath, &Options, NULL, NULL, pStats);
}



/* 返回目录树中普通文件的总字节数。 */
XRT_API bool xrtDirSize(cstr sPath, bool bRecursive, uint64* pSize)
{
	xwalkstats Stats;

	if ( pSize == NULL ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	if ( !xrtDirStats(sPath, bRecursive, &Stats) ) {
		return false;
	}
	*pSize = Stats.Bytes;
	return true;
}



/* 创建缺失目录，或清空已有目录并保留根。 */
XRT_API bool xrtDirEnsureEmpty(cstr sPath)
{
	xfileinfo Info;
	bool bExists;

	if ( (sPath == NULL) || (sPath[0] == '\0') ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	if ( !__xrtFilePathInfo(sPath, false, &bExists, &Info) ) {
		return false;
	}
	if ( !bExists ) {
		return xrtDirCreateAll(sPath);
	}
	if ( Info.Type != XFILE_TYPE_DIRECTORY ) {
		__xrtTreeError(XERR_TYPE, XTREE_ERROR_TARGET, "ensure-empty",
			"a non-directory object occupies the requested directory path");
		return false;
	}
	return xrtDirClean(sPath);
}

#endif
