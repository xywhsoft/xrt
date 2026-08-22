#include "../xrt.h"

#include <stdio.h>


typedef struct {
	int Value;
} weak_test_object;


typedef struct {
	const xrt_value_weak* Weak;
	volatile long* Start;
	volatile long* LockedCount;
} weak_thread_args;


static volatile long object_drop_count = 0;
static volatile long handle_drop_count = 0;


static void init_object(ptr object)
{
	weak_test_object* value = (weak_test_object*)object;
	value->Value = 42;
}


static void drop_object(ptr object)
{
	weak_test_object* value = (weak_test_object*)object;
	value->Value = 0;
	__xrtAtomicAddFetch32(&object_drop_count, 1);
}


static void drop_handle(ptr object)
{
	ptr* handle = (ptr*)object;
	if ( handle != NULL && *handle != NULL ) {
		*handle = NULL;
		__xrtAtomicAddFetch32(&handle_drop_count, 1);
	}
}


static const xrt_type_ops object_ops = {
	.init = init_object,
	.drop = drop_object
};


static const xrt_type_desc object_desc = {
	.TypeId = 0x5745414Bu,
	.Kind = XRT_TYPE_KIND_CLASS,
	.Name = "WeakTestObject",
	.NameSize = 14u,
	.AbiName = "weak_test_object",
	.AbiNameSize = 16u,
	.Size = sizeof(weak_test_object),
	.Align = sizeof(int),
	.Ops = &object_ops
};


static const xrt_type_ops handle_ops = {
	.drop = drop_handle
};


static const xrt_type_desc handle_desc = {
	.TypeId = 0x5745414Cu,
	.Kind = XRT_TYPE_KIND_HANDLE,
	.Name = "WeakTestHandle",
	.NameSize = 14u,
	.AbiName = "ptr",
	.AbiNameSize = 3u,
	.Size = sizeof(ptr),
	.Align = sizeof(ptr),
	.Ops = &handle_ops
};


static int fail(const char* message)
{
	fprintf(stderr, "test_value_weak: %s\n", message);
	return 1;
}


static uint32 lock_worker(ptr param)
{
	weak_thread_args* args = (weak_thread_args*)param;
	while ( *args->Start == 0 ) {
		xrtSleep(0);
	}
	for ( ;; ) {
		xvalue value = xvoWeakLock(args->Weak);
		if ( value == NULL ) {
			break;
		}
		__xrtAtomicAddFetch32(args->LockedCount, 1);
		xvoUnref(value);
	}
	return 0;
}


static int test_class_weak(void)
{
	xrt_value_weak weak;
	xrt_value_weak copied;
	xrt_value_weak cloned;
	xrt_value_weak moved;
	xvalue object;
	xvalue locked;

	xvoWeakInit(&weak);
	xvoWeakInit(&copied);
	xvoWeakInit(&cloned);
	xvoWeakInit(&moved);
	object_drop_count = 0;
	object = xvoCreateObject(&object_desc);
	if ( object == NULL || !xvoWeakSet(&weak, object) || xvoWeakExpired(&weak) ) {
		return fail("class weak initialization failed");
	}
	cloned = xvoWeakCreate(object);
	if ( cloned.Control == NULL || xvoWeakExpired(&cloned) ) {
		return fail("weak create failed");
	}
	xvoWeakUnit(&cloned);
	locked = xvoWeakLock(&weak);
	if ( locked != object || ((weak_test_object*)xvoGetRecordData(locked))->Value != 42 ) {
		xvoUnref(locked);
		return fail("class weak lock did not preserve object identity");
	}
	xvoUnref(locked);
	locked = xvoWeakLockValue(weak);
	if ( locked != object ) {
		xvoUnref(locked);
		return fail("value-form weak lock did not preserve object identity");
	}
	xvoUnref(locked);
	if ( !xvoWeakCopy(&copied, &weak) ) {
		return fail("weak copy failed");
	}
	cloned = xvoWeakClone(&weak);
	if ( cloned.Control == NULL || xvoWeakExpired(&cloned) ) {
		return fail("weak clone failed");
	}
	xvoWeakMove(&moved, &copied);
	if ( copied.Control != NULL || moved.Control == NULL ) {
		return fail("weak move did not clear the source");
	}
	xvoUnref(object);
	if ( object_drop_count != 1 || !xvoWeakExpired(&weak) ||
		 xvoWeakLock(&weak) != NULL || !xvoWeakExpired(&moved) ||
		 !xvoWeakExpired(&cloned) ) {
		return fail("expired class weak reference remained lockable");
	}
	xvoWeakUnit(&moved);
	xvoWeakUnit(&cloned);
	xvoWeakUnit(&copied);
	xvoWeakUnit(&weak);
	return 0;
}


static int test_native_handle_weak(void)
{
	xrt_value_weak weak;
	xvalue handle;
	ptr* handle_slot;
	int native_value = 7;

	xvoWeakInit(&weak);
	handle_drop_count = 0;
	handle = xvoCreateHandle(
		&handle_desc,
		&native_value,
		XRT_HANDLE_FLAG_OWNED | XRT_HANDLE_FLAG_WEAKABLE
	);
	if ( handle == NULL || !xvoIsShared(handle) || !xvoWeakSet(&weak, handle) ) {
		return fail("weakable owned handle initialization failed");
	}
	handle_slot = xvoGetHandleSlot(handle);
	if ( handle_slot == NULL ||
		 xvoGetHandleOwner((const ptr*)handle_slot) != handle ||
		 xvoGetHandleOwner(NULL) != NULL ) {
		xvoUnref(handle);
		xvoWeakUnit(&weak);
		return fail("typed handle owner lookup failed");
	}
	xvoUnref(handle);
	if ( handle_drop_count != 1 || !xvoWeakExpired(&weak) ) {
		return fail("weakable owned handle lifetime failed");
	}
	xvoWeakUnit(&weak);

	handle = xvoCreateHandle(&handle_desc, &native_value, XRT_HANDLE_FLAG_OWNED);
	if ( handle == NULL || xvoWeakSet(&weak, handle) ) {
		xvoUnref(handle);
		return fail("handle without weak lifecycle contract was accepted");
	}
	xvoUnref(handle);
	xvoWeakUnit(&weak);
	return 0;
}


static int test_lock_release_race(void)
{
	enum { THREAD_COUNT = 8 };
	xrt_value_weak weak;
	xthread threads[THREAD_COUNT];
	weak_thread_args args;
	xvalue object;
	volatile long start = 0;
	volatile long locked_count = 0;
	int i;

	xvoWeakInit(&weak);
	object_drop_count = 0;
	object = xvoCreateObject(&object_desc);
	if ( object == NULL || !xvoWeakSet(&weak, object) ) {
		return fail("race fixture initialization failed");
	}
	args.Weak = &weak;
	args.Start = &start;
	args.LockedCount = &locked_count;
	for ( i = 0; i < THREAD_COUNT; ++i ) {
		threads[i] = xrtThreadCreate(lock_worker, &args, 0);
		if ( threads[i] == NULL ) {
			return fail("race worker creation failed");
		}
	}
	__xrtAtomicAddFetch32(&start, 1);
	xrtSleep(10);
	xvoUnref(object);
	for ( i = 0; i < THREAD_COUNT; ++i ) {
		xrtThreadWait(threads[i]);
		xrtThreadDestroy(threads[i]);
	}
	if ( object_drop_count != 1 || locked_count == 0 ||
		 !xvoWeakExpired(&weak) || xvoWeakLock(&weak) != NULL ) {
		return fail("weak lock/final release race contract failed");
	}
	xvoWeakUnit(&weak);
	return 0;
}


int main(void)
{
	int result;

	if ( xrtInit() == NULL ) {
		return fail("xrtInit failed");
	}
	result = test_class_weak();
	if ( result == 0 ) {
		result = test_native_handle_weak();
	}
	if ( result == 0 ) {
		result = test_lock_release_race();
	}
	xrtUnit();
	if ( result == 0 ) {
		printf("test_value_weak: PASS\n");
	}
	return result;
}
