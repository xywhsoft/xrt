#include "../test.h"



typedef struct testatomicnested {
	uint8 Prefix;
	xatomic64 Value;
} testatomicnested;



static xatomic32 __testStatic32 = XRT_ATOMIC32_INIT(17u);
static xatomic64 __testStatic64 = XRT_ATOMIC64_INIT(UINT64_C(0x123456789abcdef0));
static xatomicptr __testStaticPtr = XRT_ATOMICPTR_INIT(NULL);



/* 验证 32 位原子整数的完整读改写合同。 */
static void testAtomic32(void)
{
	xatomic32 tValue;
	uint32 iExpected;

	testRequire(
		xrtAtomic32Load(&__testStatic32, XMEMORY_RELAXED) == 17u,
		"static atomic32 initializer mismatch"
	);
	xrtAtomic32Init(&tValue, 10u);
	testRequire(xrtAtomic32Load(&tValue, XMEMORY_ACQUIRE) == 10u, "atomic32 load mismatch");
	xrtAtomic32Store(&tValue, 20u, XMEMORY_RELEASE);
	testRequire(
		xrtAtomic32Exchange(&tValue, 30u, XMEMORY_ACQ_REL) == 20u,
		"atomic32 exchange mismatch"
	);

	iExpected = 29u;
	testRequire(
		!xrtAtomic32CompareExchange(
			&tValue,
			&iExpected,
			40u,
			XMEMORY_ACQ_REL,
			XMEMORY_ACQUIRE
		),
		"atomic32 mismatched CAS should fail"
	);
	testRequire(iExpected == 30u, "atomic32 CAS did not publish actual value");
	testRequire(
		xrtAtomic32CompareExchange(
			&tValue,
			&iExpected,
			40u,
			XMEMORY_ACQ_REL,
			XMEMORY_ACQUIRE
		),
		"atomic32 matching CAS failed"
	);
	testRequire(xrtAtomic32FetchAdd(&tValue, 2u, XMEMORY_RELAXED) == 40u, "atomic32 add mismatch");
	testRequire(xrtAtomic32FetchSub(&tValue, 3u, XMEMORY_RELAXED) == 42u, "atomic32 sub mismatch");

	xrtAtomic32Store(&tValue, 0xf0u, XMEMORY_RELAXED);
	testRequire(xrtAtomic32FetchAnd(&tValue, 0xccu, XMEMORY_RELAXED) == 0xf0u, "atomic32 and mismatch");
	testRequire(xrtAtomic32FetchOr(&tValue, 0x03u, XMEMORY_RELAXED) == 0xc0u, "atomic32 or mismatch");
	testRequire(xrtAtomic32FetchXor(&tValue, 0xffu, XMEMORY_RELAXED) == 0xc3u, "atomic32 xor mismatch");
	testRequire(xrtAtomic32Load(&tValue, XMEMORY_RELAXED) == 0x3cu, "atomic32 bit result mismatch");

	/* 非法顺序必须失败且不得写入对象。 */
	xrtClearError();
	xrtAtomic32Store(&tValue, 99u, XMEMORY_ACQUIRE);
	testRequire(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT, "atomic32 invalid order error mismatch");
	testRequire(xrtAtomic32Load(&tValue, XMEMORY_RELAXED) == 0x3cu, "invalid atomic32 store changed value");
}



/* 验证 64 位原子整数、回绕算术和对齐边界。 */
static void testAtomic64(void)
{
	xatomic64 tValue;
	xatomic64 arrAligned[2];
	testatomicnested Nested;
	uint64 iExpected;
	xatomic64* pMisaligned = (xatomic64*)(void*)((unsigned char*)arrAligned + 1u);

	testRequire(
		xrtAtomic64Load(&__testStatic64, XMEMORY_RELAXED) == UINT64_C(0x123456789abcdef0),
		"static atomic64 initializer mismatch"
	);
	xrtAtomic64Init(&tValue, UINT64_MAX);
	testRequire(
		xrtAtomic64FetchAdd(&tValue, 2u, XMEMORY_SEQ_CST) == UINT64_MAX,
		"atomic64 add old value mismatch"
	);
	testRequire(xrtAtomic64Load(&tValue, XMEMORY_RELAXED) == 1u, "atomic64 add wrap mismatch");
	testRequire(xrtAtomic64FetchSub(&tValue, 2u, XMEMORY_RELAXED) == 1u, "atomic64 sub mismatch");
	testRequire(xrtAtomic64Load(&tValue, XMEMORY_RELAXED) == UINT64_MAX, "atomic64 sub wrap mismatch");

	iExpected = UINT64_MAX;
	testRequire(
		xrtAtomic64CompareExchange(
			&tValue,
			&iExpected,
			UINT64_C(0x55aa55aa55aa55aa),
			XMEMORY_SEQ_CST,
			XMEMORY_SEQ_CST
		),
		"atomic64 CAS failed"
	);
	testRequire(
		xrtAtomic64Exchange(&tValue, 0u, XMEMORY_SEQ_CST) == UINT64_C(0x55aa55aa55aa55aa),
		"atomic64 exchange mismatch"
	);
	xrtAtomic64Store(&tValue, UINT64_C(0xf0f0f0f0f0f0f0f0), XMEMORY_RELAXED);
	testRequire(
		xrtAtomic64FetchAnd(&tValue, UINT64_C(0xcccccccccccccccc), XMEMORY_RELAXED) ==
		UINT64_C(0xf0f0f0f0f0f0f0f0),
		"atomic64 and mismatch"
	);
	testRequire(
		xrtAtomic64FetchOr(&tValue, UINT64_C(0x0303030303030303), XMEMORY_RELAXED) ==
		UINT64_C(0xc0c0c0c0c0c0c0c0),
		"atomic64 or mismatch"
	);
	testRequire(
		xrtAtomic64FetchXor(&tValue, UINT64_MAX, XMEMORY_RELAXED) ==
		UINT64_C(0xc3c3c3c3c3c3c3c3),
		"atomic64 xor mismatch"
	);

	xrtClearError();
	testRequire(
		xrtAtomic64Load(pMisaligned, XMEMORY_RELAXED) == 0u,
		"misaligned atomic64 load should fail"
	);
	testRequire(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT, "misaligned atomic64 error mismatch");

	xrtAtomic64Init(&Nested.Value, UINT64_C(0x123456789abcdef0));
	testRequire(
		(((uintptr_t)&Nested.Value & 7u) == 0u) &&
		(xrtAtomic64Load(&Nested.Value, XMEMORY_RELAXED) ==
		 UINT64_C(0x123456789abcdef0)),
		"nested stack atomic64 alignment mismatch"
	);

	#if defined(__TINYC__) && defined(_WIN32) && \
		(UINTPTR_MAX == UINT32_MAX)
		/* TinyCC x86 外层栈结构只保证 4 字节对齐，完整验证分片锁退化路径。 */
		xatomic64* pFourAligned = (xatomic64*)(void*)(
			(unsigned char*)arrAligned + 4u
		);

		testRequire(((uintptr_t)pFourAligned & 7u) == 4u,
			"TinyCC atomic64 fallback address mismatch");
		xrtAtomic64Init(pFourAligned, 10u);
		testRequire(xrtAtomic64Load(pFourAligned, XMEMORY_ACQUIRE) == 10u,
			"TinyCC atomic64 fallback init mismatch");
		xrtAtomic64Store(pFourAligned, 20u, XMEMORY_RELEASE);
		testRequire(xrtAtomic64Exchange(
			pFourAligned,
			30u,
			XMEMORY_ACQ_REL
		) == 20u, "TinyCC atomic64 fallback exchange mismatch");
		iExpected = 30u;
		testRequire(xrtAtomic64CompareExchange(
			pFourAligned,
			&iExpected,
			40u,
			XMEMORY_ACQ_REL,
			XMEMORY_ACQUIRE
		), "TinyCC atomic64 fallback CAS mismatch");
		testRequire(xrtAtomic64FetchAdd(
			pFourAligned,
			2u,
			XMEMORY_RELAXED
		) == 40u, "TinyCC atomic64 fallback add mismatch");
		testRequire(xrtAtomic64FetchSub(
			pFourAligned,
			1u,
			XMEMORY_RELAXED
		) == 42u, "TinyCC atomic64 fallback sub mismatch");
		testRequire(xrtAtomic64FetchAnd(
			pFourAligned,
			0x2au,
			XMEMORY_RELAXED
		) == 41u, "TinyCC atomic64 fallback and mismatch");
		testRequire(xrtAtomic64FetchOr(
			pFourAligned,
			0x10u,
			XMEMORY_RELAXED
		) == 40u, "TinyCC atomic64 fallback or mismatch");
		testRequire(xrtAtomic64FetchXor(
			pFourAligned,
			0x3au,
			XMEMORY_RELAXED
		) == 56u, "TinyCC atomic64 fallback xor mismatch");
		testRequire(xrtAtomic64Load(
			pFourAligned,
			XMEMORY_SEQ_CST
		) == 2u, "TinyCC atomic64 fallback final mismatch");
	#endif
}



/* 验证原子指针支持空值和标准 Expected 回写语义。 */
static void testAtomicPointer(void)
{
	xatomicptr tPointer;
	int pValues[] = { 10, 20, 30 };
	ptr pExpected;

	testRequire(
		xrtAtomicPtrLoad(&__testStaticPtr, XMEMORY_RELAXED) == NULL,
		"static atomic pointer initializer mismatch"
	);
	xrtAtomicPtrInit(&tPointer, &pValues[0]);
	testRequire(xrtAtomicPtrLoad(&tPointer, XMEMORY_ACQUIRE) == &pValues[0], "atomic pointer load mismatch");
	testRequire(
		xrtAtomicPtrExchange(&tPointer, &pValues[1], XMEMORY_ACQ_REL) == &pValues[0],
		"atomic pointer exchange mismatch"
	);
	pExpected = &pValues[2];
	testRequire(
		!xrtAtomicPtrCompareExchange(
			&tPointer,
			&pExpected,
			NULL,
			XMEMORY_RELEASE,
			XMEMORY_RELAXED
		),
		"mismatched atomic pointer CAS should fail"
	);
	testRequire(pExpected == &pValues[1], "atomic pointer CAS actual value mismatch");
	testRequire(
		xrtAtomicPtrCompareExchange(
			&tPointer,
			&pExpected,
			NULL,
			XMEMORY_RELEASE,
			XMEMORY_RELAXED
		),
		"atomic pointer NULL CAS failed"
	);
	testRequire(xrtAtomicPtrLoad(&tPointer, XMEMORY_ACQUIRE) == NULL, "atomic pointer NULL result mismatch");
}



/* 在测试存储区中构造一个确定不满足指定自然对齐的地址。 */
static void* testAtomicMisalignedAddress(
	unsigned char* pStorage,
	size_t iAlignment
)
{
	uintptr_t iAddress = (uintptr_t)pStorage;
	uintptr_t iAligned = (iAddress + iAlignment - 1u) &
		~(uintptr_t)(iAlignment - 1u);

	return (void*)(iAligned + 1u);
}



/* 验证空地址、未对齐地址和非法内存顺序都失败且不污染输出。 */
static void testAtomicErrors(void)
{
	xatomic32 tValue32 = XRT_ATOMIC32_INIT(7u);
	xatomicptr tPointer = XRT_ATOMICPTR_INIT(NULL);
	unsigned char arrStorage[sizeof(xatomicptr) + sizeof(ptr) + 1u];
	xatomic32* pMisaligned32 = (xatomic32*)testAtomicMisalignedAddress(
		arrStorage,
		4u
	);
	xatomicptr* pMisalignedPtr = (xatomicptr*)testAtomicMisalignedAddress(
		arrStorage,
		sizeof(ptr)
	);
	uint32 iExpected;

	/* 公共 API 支持的原子宽度只有 4、8 和指针宽度。 */
	testRequire(!xrtAtomicIsLockFree(0u), "zero-width atomic should not be lock-free");
	testRequire(!xrtAtomicIsLockFree(1u), "one-byte atomic should not be lock-free");
	testRequire(!xrtAtomicIsLockFree(2u), "two-byte atomic should not be lock-free");
	testRequire(!xrtAtomicIsLockFree(3u), "unsupported atomic width should not be lock-free");
	testRequire(!xrtAtomicIsLockFree(16u), "wide atomic should not be lock-free");

	/* 空地址和未对齐地址必须在接触对象前被拒绝。 */
	xrtClearError();
	xrtAtomic32Init(NULL, 1u);
	testRequire(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT, "atomic32 NULL init error mismatch");
	xrtClearError();
	testRequire(
		xrtAtomic32Load(pMisaligned32, XMEMORY_RELAXED) == 0u,
		"misaligned atomic32 load should fail"
	);
	testRequire(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT, "misaligned atomic32 error mismatch");
	xrtClearError();
	testRequire(
		xrtAtomicPtrLoad(pMisalignedPtr, XMEMORY_RELAXED) == NULL,
		"misaligned atomic pointer load should fail"
	);
	testRequire(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT, "misaligned atomic pointer error mismatch");

	/* 加载、存储和读改写各自只接受 C11 允许的内存顺序。 */
	xrtClearError();
	testRequire(
		xrtAtomic32Load(&tValue32, XMEMORY_RELEASE) == 0u,
		"atomic load with release order should fail"
	);
	testRequire(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT, "atomic load order error mismatch");
	xrtClearError();
	xrtAtomic32Store(&tValue32, 8u, XMEMORY_ACQUIRE);
	testRequire(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT, "atomic store order error mismatch");
	testRequire(
		xrtAtomic32Load(&tValue32, XMEMORY_RELAXED) == 7u,
		"invalid atomic store changed the object"
	);
	xrtClearError();
	testRequire(
		xrtAtomic32FetchAdd(&tValue32, 1u, (xmemoryorder)99) == 0u,
		"atomic RMW with unknown order should fail"
	);
	testRequire(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT, "atomic RMW order error mismatch");
	testRequire(
		xrtAtomic32Load(&tValue32, XMEMORY_RELAXED) == 7u,
		"invalid atomic RMW changed the object"
	);

	/* 比较交换的失败顺序不得包含 Release，也不得强于成功顺序。 */
	iExpected = 7u;
	xrtClearError();
	testRequire(
		!xrtAtomic32CompareExchange(
			&tValue32,
			&iExpected,
			9u,
			XMEMORY_RELEASE,
			XMEMORY_ACQUIRE
		),
		"atomic CAS with stronger failure order should fail"
	);
	testRequire(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT, "atomic CAS order error mismatch");
	testRequire(iExpected == 7u, "invalid atomic CAS changed Expected");
	testRequire(
		xrtAtomic32Load(&tValue32, XMEMORY_RELAXED) == 7u,
		"invalid atomic CAS changed the object"
	);
	xrtClearError();
	testRequire(
		!xrtAtomic32CompareExchange(
			&tValue32,
			NULL,
			9u,
			XMEMORY_RELAXED,
			XMEMORY_RELAXED
		),
		"atomic CAS with NULL Expected should fail"
	);
	testRequire(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT, "atomic CAS NULL Expected error mismatch");

	/* 指针操作与栅栏使用相同的顺序校验，成功调用不得清除已有错误。 */
	xrtClearError();
	xrtAtomicPtrStore(&tPointer, &tValue32, XMEMORY_ACQUIRE);
	testRequire(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT, "atomic pointer order error mismatch");
	testRequire(xrtAtomicPtrLoad(&tPointer, XMEMORY_RELAXED) == NULL,
		"invalid atomic pointer store changed the object");
	testRequire(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT,
		"successful atomic pointer load cleared the previous error");
	xrtClearError();
	xrtAtomicThreadFence((xmemoryorder)-1);
	testRequire(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT, "thread fence order error mismatch");
	xrtClearError();
	xrtAtomicSignalFence((xmemoryorder)99);
	testRequire(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT, "signal fence order error mismatch");
}



/* 运行原子标量、指针、栅栏和错误合同测试。 */
int main(void)
{
	testRequire(xrtAtomicIsLockFree(4u), "32-bit atomics should be lock-free on test target");
	testRequire(xrtAtomicIsLockFree(8u), "64-bit atomics should be lock-free on test target");
	testAtomic32();
	testAtomic64();
	testAtomicPointer();
	testAtomicErrors();
	xrtAtomicThreadFence(XMEMORY_SEQ_CST);
	xrtAtomicSignalFence(XMEMORY_ACQ_REL);
	xrtAtomicPause();
	printf("[PASS] atomic\n");
	return 0;
}
