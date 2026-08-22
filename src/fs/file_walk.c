#include "../internal/xrt_dir.h"



#if defined(XRT_FEATURE_FILE_WALK)

/* 显式遍历栈帧避免目录深度消耗 C 调用栈。 */
typedef struct __xrt_walk_frame {
	str Path;
	xfileinfo Info;
	xdir Dir;
	size_t Depth;
	uint32 Flags;
	bool Entered;
	bool NoDescend;
} __xrt_walk_frame;



/* 遍历过程中的动态栈。 */
typedef struct __xrt_walk_stack {
	__xrt_walk_frame* Items;
	size_t Count;
	size_t Capacity;
} __xrt_walk_stack;



/* 设置遍历层自己的结构化错误。 */
static void __xrtWalkError(xerrkind Kind, xwalkerror Code,
	cstr sOperation, cstr sMessage)
{
	xerrordesc Desc;
	xerror* pError;

	memset(&Desc, 0, sizeof(Desc));
	Desc.Kind = Kind;
	Desc.Domain = "xrt.walk";
	Desc.Code = (int32)Code;
	Desc.Operation = sOperation;
	Desc.Message = sMessage;
	pError = xrtErrorBuild(&Desc);
	if ( pError != NULL ) {
		__xrtErrorSetOwned(pError);
	}
}



/* 初始化遍历选项。 */
XRT_API void xrtWalkOptionsInit(xwalkoptions* pOptions)
{
	if ( pOptions == NULL ) {
		return;
	}
	pOptions->Flags = 0u;
	pOptions->MaxDepth = XRT_NPOS;
	pOptions->OnError = NULL;
}



/* 展开空选项并检查已知标志。 */
static bool __xrtWalkOptions(const xwalkoptions* pInput,
	xwalkoptions* pOptions)
{
	const uint32 iKnown = XWALK_FOLLOW_LINKS | XWALK_ONE_FILESYSTEM;

	if ( pInput == NULL ) {
		xrtWalkOptionsInit(pOptions);
	} else {
		*pOptions = *pInput;
	}
	if ( (pOptions->Flags & ~iKnown) != 0u ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	return true;
}



/* 只有路径环境错误允许用户跳过，程序和资源错误必须终止。 */
static bool __xrtWalkRecoverable(const xerror* pError)
{
	xerrkind Kind;

	if ( pError == NULL ) {
		return false;
	}
	Kind = xrtErrorKind(pError);
	return (Kind == XERR_IO) || (Kind == XERR_NOT_FOUND) ||
		(Kind == XERR_TYPE) || (Kind == XERR_PERMISSION) ||
		(Kind == XERR_AGAIN) || (Kind == XERR_UNSUPPORTED);
}



/* 把当前路径环境错误交给可选回调并处理错误所有权。 */
static xwalkerroraction __xrtWalkHandleError(
	const xwalkoptions* pOptions, cstr sPath, ptr pUserData)
{
	xerror* pFailure = xrtTakeError();
	xerror* pReplacement;
	xwalkerroraction Action;

	if ( pFailure == NULL ) {
		__xrtWalkError(XERR_INTERNAL, XWALK_ERROR_CALLBACK, "on-error",
			"a file walk operation failed without setting an error");
		pFailure = xrtTakeError();
	}
	if ( (pOptions->OnError == NULL) || !__xrtWalkRecoverable(pFailure) ) {
		if ( pFailure != NULL ) {
			__xrtErrorSetOwned(pFailure);
		}
		return XWALK_ERROR_ABORT;
	}
	Action = pOptions->OnError(sPath, pFailure, pUserData);
	pReplacement = xrtTakeError();
	if ( (Action < XWALK_ERROR_ABORT) || (Action > XWALK_ERROR_STOP) ) {
		xrtErrorFree(pFailure);
		xrtErrorFree(pReplacement);
		__xrtWalkError(XERR_VALUE, XWALK_ERROR_CALLBACK, "on-error",
			"the file walk error callback returned an invalid action");
		return XWALK_ERROR_ABORT;
	}
	if ( Action == XWALK_ERROR_ABORT ) {
		if ( pReplacement != NULL ) {
			xrtErrorFree(pFailure);
			__xrtErrorSetOwned(pReplacement);
		} else if ( pFailure != NULL ) {
			__xrtErrorSetOwned(pFailure);
		}
	} else {
		xrtErrorFree(pFailure);
		xrtErrorFree(pReplacement);
	}
	return Action;
}



/* 成功恢复调用前错误，失败则保留本次遍历错误。 */
static bool __xrtWalkFinish(bool bSuccess, xerror* pPrevious)
{
	if ( bSuccess ) {
		xrtClearError();
		if ( pPrevious != NULL ) {
			__xrtErrorSetOwned(pPrevious);
		}
	} else {
		xrtErrorFree(pPrevious);
	}
	return bSuccess;
}



/* 给遍历栈追加一个拥有路径的帧。 */
static bool __xrtWalkPush(__xrt_walk_stack* pStack,
	const __xrt_walk_frame* pFrame)
{
	if ( pStack->Count == pStack->Capacity ) {
		size_t iCapacity = (pStack->Capacity == 0u) ? 16u :
			(pStack->Capacity * 2u);
		__xrt_walk_frame* pItems;

		if ( (iCapacity <= pStack->Capacity) ||
			 (iCapacity > (SIZE_MAX / sizeof(*pItems))) ) {
			__xrtErrorSetSizeOverflow();
			return false;
		}
		pItems = (__xrt_walk_frame*)xrtRealloc(pStack->Items,
			iCapacity * sizeof(*pItems));
		if ( pItems == NULL ) {
			return false;
		}
		pStack->Items = pItems;
		pStack->Capacity = iCapacity;
	}
	pStack->Items[pStack->Count++] = *pFrame;
	return true;
}



/* 关闭并释放全部未完成帧，按调用场景保留主错误或旧错误。 */
static bool __xrtWalkCleanup(__xrt_walk_stack* pStack, bool bFailure)
{
	xerror* pSaved = xrtTakeError();
	xerror* pCloseError = NULL;
	bool bResult = true;

	while ( pStack->Count != 0u ) {
		__xrt_walk_frame* pFrame = &pStack->Items[pStack->Count - 1u];

		if ( (pFrame->Dir != NULL) && !xrtDirClose(pFrame->Dir) ) {
			bResult = false;
			if ( pCloseError == NULL ) {
				pCloseError = xrtTakeError();
			} else {
				xrtClearError();
			}
		}
		xrtFree(pFrame->Path);
		pStack->Count--;
	}
	xrtFree(pStack->Items);
	pStack->Items = NULL;
	pStack->Capacity = 0;
	if ( bFailure && (pSaved != NULL) ) {
		xrtErrorFree(pCloseError);
		__xrtErrorSetOwned(pSaved);
	} else if ( pCloseError != NULL ) {
		xrtErrorFree(pSaved);
		__xrtErrorSetOwned(pCloseError);
	} else if ( pSaved != NULL ) {
		__xrtErrorSetOwned(pSaved);
	}
	return bResult;
}



/* 安全增加统计计数，避免极端目录树静默回绕。 */
static bool __xrtWalkAdd(uint64* pValue, uint64 iAdd)
{
	if ( iAdd > (UINT64_MAX - *pValue) ) {
		__xrtWalkError(XERR_RANGE, XWALK_ERROR_OVERFLOW, "stats",
			"file walk statistics overflowed");
		return false;
	}
	*pValue += iAdd;
	return true;
}



/* 按物理链接身份和有效对象类型累计一个对象。 */
static bool __xrtWalkCount(xwalkstats* pStats,
	const xfileinfo* pInfo, uint32 iFlags)
{
	if ( !__xrtWalkAdd(&pStats->Items, 1u) ) {
		return false;
	}
	if ( (iFlags & XWALK_ENTRY_LINK) != 0u ) {
		if ( !__xrtWalkAdd(&pStats->Links, 1u) ) {
			return false;
		}
	}
	if ( pInfo->Type == XFILE_TYPE_DIRECTORY ) {
		return __xrtWalkAdd(&pStats->Directories, 1u);
	}
	if ( pInfo->Type == XFILE_TYPE_FILE ) {
		if ( !__xrtWalkAdd(&pStats->Files, 1u) ) {
			return false;
		}
		if ( (pInfo->Available & XFILE_INFO_SIZE) != 0u ) {
			return __xrtWalkAdd(&pStats->Bytes, pInfo->Size);
		}
		return true;
	}
	if ( pInfo->Type == XFILE_TYPE_LINK ) {
		return true;
	}
	return __xrtWalkAdd(&pStats->Others, 1u);
}



/* 构造借用遍历条目并调用用户回调。 */
static xwalkcontrol __xrtWalkEmit(const __xrt_walk_frame* pFrame,
	xwalkevent Event, xwalkproc pProc, ptr pUserData)
{
	xwalkentry Entry;
	xpathparts Parts;
	xwalkcontrol Control;
	xerror* pBefore;
	xerror* pAfter;

	if ( pProc == NULL ) {
		return XWALK_CONTINUE;
	}
	memset(&Entry, 0, sizeof(Entry));
	Entry.Path = pFrame->Path;
	Entry.Info = pFrame->Info;
	Entry.Event = Event;
	Entry.Flags = pFrame->Flags;
	Entry.Depth = pFrame->Depth;
	if ( xrtPathParse(xrtStrView(pFrame->Path), XPATH_NATIVE, &Parts) ) {
		Entry.Parent = Parts.Parent;
		Entry.Name = Parts.Name;
	}
	pBefore = xrtTakeError();
	Control = pProc(&Entry, pUserData);
	pAfter = xrtTakeError();
	if ( (Control < XWALK_CONTINUE) || (Control > XWALK_ERROR) ||
		 ((Control == XWALK_SKIP) && (Event != XWALK_ENTER)) ) {
		xrtErrorFree(pBefore);
		xrtErrorFree(pAfter);
		__xrtWalkError(XERR_VALUE, XWALK_ERROR_CALLBACK, "callback",
			"the file walk callback returned an invalid control value");
		return XWALK_ERROR;
	}
	if ( Control == XWALK_ERROR ) {
		xrtErrorFree(pBefore);
		if ( pAfter != NULL ) {
			__xrtErrorSetOwned(pAfter);
		} else {
			__xrtWalkError(XERR_INTERNAL, XWALK_ERROR_CALLBACK, "callback",
				"the file walk callback failed without setting an error");
		}
		return Control;
	}
	xrtErrorFree(pAfter);
	if ( pBefore != NULL ) {
		__xrtErrorSetOwned(pBefore);
	}
	return Control;
}



/* 跟随物理链接时查询有效目标元数据。 */
static bool __xrtWalkResolve(cstr sPath, const xfileinfo* pPhysical,
	const xwalkoptions* pOptions, xfileinfo* pInfo, uint32* pFlags)
{
	*pInfo = *pPhysical;
	if ( pPhysical->Type != XFILE_TYPE_LINK ) {
		return true;
	}
	*pFlags |= XWALK_ENTRY_LINK;
	if ( (pOptions->Flags & XWALK_FOLLOW_LINKS) == 0u ) {
		return true;
	}
	return xrtPathStat(sPath, true, pInfo);
}



/* 判断目录身份是否已经出现在当前祖先链中。 */
static bool __xrtWalkCycle(const __xrt_walk_stack* pStack,
	const xfileinfo* pInfo)
{
	size_t i;

	if ( (pInfo->Available & XFILE_INFO_IDENTITY) == 0u ) {
		return false;
	}
	for ( i = 0; i < pStack->Count; i++ ) {
		const xfileinfo* pAncestor = &pStack->Items[i].Info;

		if ( ((pAncestor->Available & XFILE_INFO_IDENTITY) != 0u) &&
			 (pAncestor->Device == pInfo->Device) &&
			 (pAncestor->Identity == pInfo->Identity) ) {
			return true;
		}
	}
	return false;
}



/* 为一个已解析目录构造下降帧并标注环或跨文件系统。 */
static bool __xrtWalkDirectoryFrame(__xrt_walk_stack* pStack,
	str sPath, const xfileinfo* pInfo, size_t iDepth, uint32 iFlags,
	const xwalkoptions* pOptions, uint64 iRootDevice)
{
	__xrt_walk_frame Frame;

	memset(&Frame, 0, sizeof(Frame));
	Frame.Path = sPath;
	Frame.Info = *pInfo;
	Frame.Depth = iDepth;
	Frame.Flags = iFlags;
	if ( ((pOptions->Flags & (XWALK_FOLLOW_LINKS |
		XWALK_ONE_FILESYSTEM)) != 0u) &&
		 ((pInfo->Available & XFILE_INFO_IDENTITY) == 0u) ) {
		__xrtWalkError(XERR_UNSUPPORTED, XWALK_ERROR_IDENTITY, "identity",
			"safe link or filesystem traversal requires directory identity");
		return false;
	}
	if ( __xrtWalkCycle(pStack, pInfo) ) {
		Frame.Flags |= XWALK_ENTRY_CYCLE;
		Frame.NoDescend = true;
	}
	if ( ((pOptions->Flags & XWALK_ONE_FILESYSTEM) != 0u) &&
		 (pInfo->Device != iRootDevice) ) {
		Frame.Flags |= XWALK_ENTRY_CROSS_FILESYSTEM;
		Frame.NoDescend = true;
	}
	if ( !__xrtWalkPush(pStack, &Frame) ) {
		return false;
	}
	return true;
}



/* 处理一个非目录对象并立即释放其拥有路径。 */
static xwalkcontrol __xrtWalkItem(str sPath, const xfileinfo* pInfo,
	size_t iDepth, uint32 iFlags, xwalkproc pProc, ptr pUserData,
	xwalkstats* pStats)
{
	__xrt_walk_frame Frame;
	xwalkcontrol Control;

	memset(&Frame, 0, sizeof(Frame));
	Frame.Path = sPath;
	Frame.Info = *pInfo;
	Frame.Depth = iDepth;
	Frame.Flags = iFlags;
	if ( !__xrtWalkCount(pStats, pInfo, iFlags) ) {
		xrtFree(sPath);
		return XWALK_ERROR;
	}
	Control = __xrtWalkEmit(&Frame, XWALK_ITEM, pProc, pUserData);
	xrtFree(sPath);
	return Control;
}



/* 深度优先遍历一个文件系统对象。 */
XRT_API bool xrtFileWalk(cstr sPath, const xwalkoptions* pInput,
	xwalkproc pProc, ptr pUserData, xwalkstats* pStats)
{
	xwalkoptions Options;
	xwalkstats Stats;
	__xrt_walk_stack Stack;
	xfileinfo Physical;
	xfileinfo Effective;
	uint32 iRootFlags = 0u;
	uint64 iRootDevice = 0u;
	str sRoot;
	bool bSuccess = false;
	xerror* pPrevious;

	memset(&Stats, 0, sizeof(Stats));
	memset(&Stack, 0, sizeof(Stack));
	if ( (sPath == NULL) || (sPath[0] == '\0') ||
		 !__xrtWalkOptions(pInput, &Options) ) {
		if ( (sPath == NULL) || (sPath[0] == '\0') ) {
			__xrtErrorSetInvalidArgument();
		}
		return false;
	}
	pPrevious = xrtTakeError();
	if ( !xrtPathStat(sPath, false, &Physical) ||
		 !__xrtWalkResolve(sPath, &Physical, &Options,
			&Effective, &iRootFlags) ) {
		xwalkerroraction Action = __xrtWalkHandleError(
			&Options, sPath, pUserData);

		if ( Action == XWALK_ERROR_ABORT ) {
			return __xrtWalkFinish(false, pPrevious);
		}
		Stats.Stopped = Action == XWALK_ERROR_STOP;
		if ( pStats != NULL ) {
			*pStats = Stats;
		}
		return __xrtWalkFinish(true, pPrevious);
	}
	if ( xrtUtf8Valid(xrtStrView(sPath), NULL) ) {
		iRootFlags |= XWALK_ENTRY_UTF8;
	}
	sRoot = xrtStrDup(sPath);
	if ( sRoot == NULL ) {
		return __xrtWalkFinish(false, pPrevious);
	}
	if ( (Options.Flags & XWALK_ONE_FILESYSTEM) != 0u ) {
		if ( (Effective.Available & XFILE_INFO_IDENTITY) == 0u ) {
			xrtFree(sRoot);
			__xrtWalkError(XERR_UNSUPPORTED, XWALK_ERROR_IDENTITY, "identity",
				"one-filesystem traversal requires root identity");
			return __xrtWalkFinish(false, pPrevious);
		}
		iRootDevice = Effective.Device;
	}
	if ( Effective.Type != XFILE_TYPE_DIRECTORY ) {
		xwalkcontrol Control = __xrtWalkItem(sRoot, &Effective, 0u,
			iRootFlags, pProc, pUserData, &Stats);

		if ( Control == XWALK_ERROR ) {
			return __xrtWalkFinish(false, pPrevious);
		}
		Stats.Stopped = Control == XWALK_STOP;
		if ( pStats != NULL ) {
			*pStats = Stats;
		}
		return __xrtWalkFinish(true, pPrevious);
	}
	if ( !__xrtWalkDirectoryFrame(&Stack, sRoot, &Effective, 0u,
		iRootFlags, &Options, iRootDevice) ) {
		xrtFree(sRoot);
		(void)__xrtWalkCleanup(&Stack, true);
		return __xrtWalkFinish(false, pPrevious);
	}

	while ( Stack.Count != 0u ) {
		__xrt_walk_frame* pFrame = &Stack.Items[Stack.Count - 1u];

		if ( !pFrame->Entered ) {
			xwalkcontrol Control;

			pFrame->Entered = true;
			if ( !__xrtWalkCount(&Stats, &pFrame->Info, pFrame->Flags) ) {
				break;
			}
			Control = __xrtWalkEmit(pFrame, XWALK_ENTER, pProc, pUserData);
			if ( Control == XWALK_ERROR ) {
				break;
			}
			if ( Control == XWALK_STOP ) {
				Stats.Stopped = true;
				bSuccess = __xrtWalkCleanup(&Stack, false);
				goto done;
			}
			if ( Control == XWALK_SKIP ) {
				pFrame->NoDescend = true;
			}
			if ( pFrame->Depth >= Options.MaxDepth ) {
				pFrame->NoDescend = true;
			}
			if ( !pFrame->NoDescend ) {
				pFrame->Dir = xrtDirOpen(pFrame->Path, 0u);
				if ( pFrame->Dir == NULL ) {
					xwalkerroraction Action = __xrtWalkHandleError(
						&Options, pFrame->Path, pUserData);

					if ( Action == XWALK_ERROR_ABORT ) {
						break;
					}
					if ( Action == XWALK_ERROR_STOP ) {
						Stats.Stopped = true;
						bSuccess = __xrtWalkCleanup(&Stack, false);
						goto done;
					}
					pFrame->NoDescend = true;
				}
			}
			continue;
		}
		if ( pFrame->Dir != NULL ) {
			xdirentry Entry;
			xdirnext Next = xrtDirNext(pFrame->Dir, &Entry);

			if ( Next == XDIR_NEXT_ERROR ) {
				xwalkerroraction Action = __xrtWalkHandleError(
					&Options, pFrame->Path, pUserData);

				if ( Action == XWALK_ERROR_ABORT ) {
					break;
				}
				if ( Action == XWALK_ERROR_STOP ) {
					Stats.Stopped = true;
					bSuccess = __xrtWalkCleanup(&Stack, false);
					goto done;
				}
				Next = XDIR_NEXT_END;
			}
			if ( Next == XDIR_NEXT_ITEM ) {
				str sChild = xrtDirEntryPath(pFrame->Dir, &Entry);
				xfileinfo ChildPhysical;
				xfileinfo ChildInfo;
				uint32 iChildFlags = ((Entry.Flags & XDIR_ENTRY_UTF8) != 0u) ?
					XWALK_ENTRY_UTF8 : 0u;

				if ( sChild == NULL ) {
					break;
				}
				if ( !xrtPathStat(sChild, false, &ChildPhysical) ||
					 !__xrtWalkResolve(sChild, &ChildPhysical, &Options,
						&ChildInfo, &iChildFlags) ) {
					xwalkerroraction Action = __xrtWalkHandleError(
						&Options, sChild, pUserData);

					if ( Action == XWALK_ERROR_STOP ) {
						xrtFree(sChild);
						Stats.Stopped = true;
						bSuccess = __xrtWalkCleanup(&Stack, false);
						goto done;
					}
					xrtFree(sChild);
					if ( Action == XWALK_ERROR_ABORT ) {
						break;
					}
					continue;
				}
				if ( pFrame->Depth == SIZE_MAX ) {
					xrtFree(sChild);
					__xrtErrorSetSizeOverflow();
					break;
				}
				if ( ChildInfo.Type == XFILE_TYPE_DIRECTORY ) {
					if ( !__xrtWalkDirectoryFrame(&Stack, sChild,
						&ChildInfo, pFrame->Depth + 1u, iChildFlags,
						&Options, iRootDevice) ) {
						xrtFree(sChild);
						break;
					}
				} else {
					xwalkcontrol Control = __xrtWalkItem(sChild,
						&ChildInfo, pFrame->Depth + 1u, iChildFlags,
						pProc, pUserData, &Stats);

					if ( Control == XWALK_ERROR ) {
						break;
					}
					if ( Control == XWALK_STOP ) {
						Stats.Stopped = true;
						bSuccess = __xrtWalkCleanup(&Stack, false);
						goto done;
					}
				}
				continue;
			}
			if ( !xrtDirClose(pFrame->Dir) ) {
				pFrame->Dir = NULL;
				{
					xwalkerroraction Action = __xrtWalkHandleError(
						&Options, pFrame->Path, pUserData);

					if ( Action == XWALK_ERROR_ABORT ) {
						break;
					}
					if ( Action == XWALK_ERROR_STOP ) {
						Stats.Stopped = true;
						bSuccess = __xrtWalkCleanup(&Stack, false);
						goto done;
					}
				}
			}
			pFrame->Dir = NULL;
		}
		{
			xwalkcontrol Control = __xrtWalkEmit(pFrame,
				XWALK_LEAVE, pProc, pUserData);

			if ( Control == XWALK_ERROR ) {
				break;
			}
			if ( Control == XWALK_STOP ) {
				Stats.Stopped = true;
				bSuccess = __xrtWalkCleanup(&Stack, false);
				goto done;
			}
		}
		xrtFree(pFrame->Path);
		Stack.Count--;
	}
	if ( Stack.Count == 0u ) {
		xrtFree(Stack.Items);
		Stack.Items = NULL;
		bSuccess = true;
	} else {
		(void)__xrtWalkCleanup(&Stack, true);
	}

done:
	if ( bSuccess && (pStats != NULL) ) {
		*pStats = Stats;
	}
	return __xrtWalkFinish(bSuccess, pPrevious);
}

#endif
