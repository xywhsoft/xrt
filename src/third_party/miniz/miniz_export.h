#ifndef XRT_THIRD_PARTY_MINIZ_EXPORT_H
#define XRT_THIRD_PARTY_MINIZ_EXPORT_H

/* XRT compiles each selected miniz codec once behind its own trimming boundary. */
#define MINIZ_EXPORT

/*
	XRT 的流式编码器按需创建，但仍可能被大量并发响应短暂持有。
	低内存布局把 tdefl 状态从约 312 KiB 降到约 164 KiB，压缩率只轻微变化。
*/
#ifndef TDEFL_LESS_MEMORY
	#define TDEFL_LESS_MEMORY 1
#endif

#ifndef MZ_FORCEINLINE
	#if defined(_MSC_VER)
		#define MZ_FORCEINLINE __forceinline
	#elif defined(__GNUC__) || defined(__clang__)
		#define MZ_FORCEINLINE inline __attribute__((always_inline))
	#else
		#define MZ_FORCEINLINE inline
	#endif
#endif

#ifndef MINIZ_USE_UNALIGNED_LOADS_AND_STORES
	#define MINIZ_USE_UNALIGNED_LOADS_AND_STORES 0
#endif

#ifndef MINIZ_LITTLE_ENDIAN
	#if defined(_WIN32) || defined(_WIN64) || defined(__i386__) || defined(__x86_64__) || \
		defined(__aarch64__) || defined(_M_IX86) || defined(_M_X64) || defined(_M_ARM64)
		#define MINIZ_LITTLE_ENDIAN 1
	#else
		#define MINIZ_LITTLE_ENDIAN 0
	#endif
#endif

#ifndef MINIZ_HAS_64BIT_REGISTERS
	#if defined(_WIN64) || defined(__x86_64__) || defined(__aarch64__) || defined(_M_X64) || defined(_M_ARM64)
		#define MINIZ_HAS_64BIT_REGISTERS 1
	#else
		#define MINIZ_HAS_64BIT_REGISTERS 0
	#endif
#endif

#endif
