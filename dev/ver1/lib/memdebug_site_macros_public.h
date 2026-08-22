#ifndef XRT_MEMDEBUG_SITE_MACROS_PUBLIC_H
#define XRT_MEMDEBUG_SITE_MACROS_PUBLIC_H

#ifdef XRT_MEM_DEBUG
	#ifndef xrtMalloc
		#define xrtMalloc(iSize) xrtMallocDbg((iSize), __FILE__, __LINE__)
	#endif
	#ifndef xrtCalloc
		#define xrtCalloc(iNum, iSize) xrtCallocDbg((iNum), (iSize), __FILE__, __LINE__)
	#endif
	#ifndef xrtRealloc
		#define xrtRealloc(pMem, iSize) xrtReallocDbg((pMem), (iSize), __FILE__, __LINE__)
	#endif
	#ifndef xrtFree
		#define xrtFree(pMem) xrtFreeDbg((pMem), __FILE__, __LINE__)
	#endif

	#ifndef xrtArrayCreate
		#define xrtArrayCreate(iItemLength, iMode) xrtArrayCreateDbg((iItemLength), (iMode), __FILE__, __LINE__)
	#endif
	#ifndef xrtArrayInit
		#define xrtArrayInit(pArr, iItemLength, iMode) xrtArrayInitDbg((pArr), (iItemLength), (iMode), __FILE__, __LINE__)
	#endif
	#ifndef xrtArrayDestroy
		#define xrtArrayDestroy(pArr) xrtArrayDestroyDbg((pArr), __FILE__, __LINE__)
	#endif
	#ifndef xrtArrayUnit
		#define xrtArrayUnit(pArr) xrtArrayUnitDbg((pArr), __FILE__, __LINE__)
	#endif

	#ifndef xrtDictCreate
		#define xrtDictCreate(iItemLength, iMode) xrtDictCreateDbg((iItemLength), (iMode), __FILE__, __LINE__)
	#endif
	#ifndef xrtDictInit
		#define xrtDictInit(objHT, iItemLength, iMode) xrtDictInitDbg((objHT), (iItemLength), (iMode), __FILE__, __LINE__)
	#endif
	#ifndef xrtDictDestroy
		#define xrtDictDestroy(objHT) xrtDictDestroyDbg((objHT), __FILE__, __LINE__)
	#endif
	#ifndef xrtDictUnit
		#define xrtDictUnit(objHT) xrtDictUnitDbg((objHT), __FILE__, __LINE__)
	#endif
	#ifndef xrtDictSet
		#define xrtDictSet(objHT, sKey, iKeyLen, bNewRet) xrtDictSetDbg((objHT), (sKey), (iKeyLen), (bNewRet), __FILE__, __LINE__)
	#endif
	#ifndef xrtDictSetPtr
		#define xrtDictSetPtr(objHT, sKey, iKeyLen, pVal, ppOldVal) xrtDictSetPtrDbg((objHT), (sKey), (iKeyLen), (pVal), (ppOldVal), __FILE__, __LINE__)
	#endif
	#ifndef xrtDictRemove
		#define xrtDictRemove(objHT, sKey, iKeyLen) xrtDictRemoveDbg((objHT), (sKey), (iKeyLen), __FILE__, __LINE__)
	#endif
	#ifndef xrtDictRemovePtr
		#define xrtDictRemovePtr(objHT, sKey, iKeyLen) xrtDictRemovePtrDbg((objHT), (sKey), (iKeyLen), __FILE__, __LINE__)
	#endif

	#ifndef xrtListCreate
		#define xrtListCreate(iItemLength, iMode) xrtListCreateDbg((iItemLength), (iMode), __FILE__, __LINE__)
	#endif
	#ifndef xrtListInit
		#define xrtListInit(objList, iItemLength, iMode) xrtListInitDbg((objList), (iItemLength), (iMode), __FILE__, __LINE__)
	#endif
	#ifndef xrtListDestroy
		#define xrtListDestroy(objList) xrtListDestroyDbg((objList), __FILE__, __LINE__)
	#endif
	#ifndef xrtListUnit
		#define xrtListUnit(objList) xrtListUnitDbg((objList), __FILE__, __LINE__)
	#endif
	#ifndef xrtListSet
		#define xrtListSet(objList, iKey, bNewRet) xrtListSetDbg((objList), (iKey), (bNewRet), __FILE__, __LINE__)
	#endif
	#ifndef xrtListSetPtr
		#define xrtListSetPtr(objList, iKey, pVal, ppOldVal) xrtListSetPtrDbg((objList), (iKey), (pVal), (ppOldVal), __FILE__, __LINE__)
	#endif
	#ifndef xrtListRemove
		#define xrtListRemove(objList, iKey) xrtListRemoveDbg((objList), (iKey), __FILE__, __LINE__)
	#endif
	#ifndef xrtListRemovePtr
		#define xrtListRemovePtr(objList, iKey) xrtListRemovePtrDbg((objList), (iKey), __FILE__, __LINE__)
	#endif

	#ifndef xrtAVLTreeCreate
		#define xrtAVLTreeCreate(iItemLength, procComp, iMode) xrtAVLTreeCreateDbg((iItemLength), (procComp), (iMode), __FILE__, __LINE__)
	#endif
	#ifndef xrtAVLTreeInit
		#define xrtAVLTreeInit(objAVLT, iItemLength, procComp, iMode) xrtAVLTreeInitDbg((objAVLT), (iItemLength), (procComp), (iMode), __FILE__, __LINE__)
	#endif
	#ifndef xrtAVLTreeDestroy
		#define xrtAVLTreeDestroy(objAVLT) xrtAVLTreeDestroyDbg((objAVLT), __FILE__, __LINE__)
	#endif
	#ifndef xrtAVLTreeUnit
		#define xrtAVLTreeUnit(objAVLT) xrtAVLTreeUnitDbg((objAVLT), __FILE__, __LINE__)
	#endif

	#ifndef xrtDynStackCreate
		#define xrtDynStackCreate(iItemLength) xrtDynStackCreateDbg((iItemLength), __FILE__, __LINE__)
	#endif
	#ifndef xrtDynStackInit
		#define xrtDynStackInit(objSTK, iItemLength) xrtDynStackInitDbg((objSTK), (iItemLength), __FILE__, __LINE__)
	#endif
	#ifndef xrtDynStackDestroy
		#define xrtDynStackDestroy(objSTK) xrtDynStackDestroyDbg((objSTK), __FILE__, __LINE__)
	#endif
	#ifndef xrtDynStackUnit
		#define xrtDynStackUnit(objSTK) xrtDynStackUnitDbg((objSTK), __FILE__, __LINE__)
	#endif

	#ifndef xrtMemPoolCreate
		#define xrtMemPoolCreate(iCustom, iMode) xrtMemPoolCreateDbg((iCustom), (iMode), __FILE__, __LINE__)
	#endif
	#ifndef xrtMemPoolInit
		#define xrtMemPoolInit(objMP, iCustom, iMode) xrtMemPoolInitDbg((objMP), (iCustom), (iMode), __FILE__, __LINE__)
	#endif
	#ifndef xrtMemPoolDestroy
		#define xrtMemPoolDestroy(objMP) xrtMemPoolDestroyDbg((objMP), __FILE__, __LINE__)
	#endif
	#ifndef xrtMemPoolUnit
		#define xrtMemPoolUnit(objMP) xrtMemPoolUnitDbg((objMP), __FILE__, __LINE__)
	#endif

	#ifndef xrtFSMemPoolCreate
		#define xrtFSMemPoolCreate(iItemLength, iMode) xrtFSMemPoolCreateDbg((iItemLength), (iMode), __FILE__, __LINE__)
	#endif
	#ifndef xrtFSMemPoolInit
		#define xrtFSMemPoolInit(objMM, iItemLength, iMode) xrtFSMemPoolInitDbg((objMM), (iItemLength), (iMode), __FILE__, __LINE__)
	#endif
	#ifndef xrtFSMemPoolDestroy
		#define xrtFSMemPoolDestroy(objMM) xrtFSMemPoolDestroyDbg((objMM), __FILE__, __LINE__)
	#endif
	#ifndef xrtFSMemPoolUnit
		#define xrtFSMemPoolUnit(objMM) xrtFSMemPoolUnitDbg((objMM), __FILE__, __LINE__)
	#endif
#endif

#endif
