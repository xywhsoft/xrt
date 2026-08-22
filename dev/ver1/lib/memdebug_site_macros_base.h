#ifndef XRT_MEMDEBUG_SITE_MACROS_BASE_H
#define XRT_MEMDEBUG_SITE_MACROS_BASE_H

#ifdef XRT_MEM_DEBUG
	#ifndef xrtMalloc
		#define xrtMalloc(iSize) __xrtMallocSite((iSize), __FILE__, __LINE__)
	#endif
	#ifndef xrtCalloc
		#define xrtCalloc(iNum, iSize) __xrtCallocSite((iNum), (iSize), __FILE__, __LINE__)
	#endif
	#ifndef xrtRealloc
		#define xrtRealloc(pMem, iSize) __xrtReallocSite((pMem), (iSize), __FILE__, __LINE__)
	#endif
	#ifndef xrtFree
		#define xrtFree(pMem) __xrtFreeSite((pMem), __FILE__, __LINE__)
	#endif
#endif

#endif
