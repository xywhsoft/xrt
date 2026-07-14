#include "../xrt.h"

#include <stdio.h>
#include <string.h>

static int check(int condition, const char* message)
{
	if ( !condition ) {
		fprintf(stderr, "FAIL: %s\n", message);
		return 0;
	}
	return 1;
}


int main(void)
{
	xtstack stack;
	xtavltree tree;
	xtqueue queue;
	xtqueue waitQueue;
	int64 first = 11;
	int64 second = 22;
	int64 number = 0;
	const char* keyA = "a";
	const char* keyB = "b";
	const char* valueA = "alpha";
	const char* valueB = "beta";
	const char* text = NULL;
	xtarray keys;
	int8 tiny = -7;
	int8 tinyOut = 0;
	uint16 wide = 60000u;
	uint16 wideOut = 0u;
	float real = 1.25f;
	float realOut = 0.0f;
	int32 truth = 1;
	int32 truthOut = 0;

	xrtInit();

	stack = xrtTypedStackCreate(xrtTypeInt());
	if ( !check(stack != NULL, "create typed stack") ||
		 !check(xrtTypedStackPush(stack, &first), "push first stack item") ||
		 !check(xrtTypedStackPush(stack, &second), "push second stack item") ||
		 !check(xrtTypedStackCount(stack) == 2u, "stack count") ||
		 !check(xrtTypedStackPeek(stack, &number) && number == second, "peek stack item") ||
		 !check(xrtTypedStackPop(stack, &number) && number == second, "pop stack item") ||
		 !check(xrtTypedStackPop(stack, &number) && number == first, "pop final stack item") ||
		 !check(!xrtTypedStackPop(stack, &number), "empty stack pop") ) {
		xrtTypedStackRelease(stack);
		xrtUnit();
		return 1;
	}
	xrtTypedStackRelease(stack);

	stack = xrtTypedStackCreate(xrtTypeInt8());
	if ( !check(stack != NULL && xrtTypedStackPush(stack, &tiny) &&
	             xrtTypedStackPop(stack, &tinyOut) && tinyOut == tiny,
	             "fixed int8 descriptor preserves width") ) {
		xrtTypedStackRelease(stack);
		xrtUnit();
		return 1;
	}
	xrtTypedStackRelease(stack);

	stack = xrtTypedStackCreate(xrtTypeUInt16());
	if ( !check(stack != NULL && xrtTypedStackPush(stack, &wide) &&
	             xrtTypedStackPop(stack, &wideOut) && wideOut == wide,
	             "fixed uint16 descriptor preserves width") ) {
		xrtTypedStackRelease(stack);
		xrtUnit();
		return 1;
	}
	xrtTypedStackRelease(stack);

	stack = xrtTypedStackCreate(xrtTypeFloat32());
	if ( !check(stack != NULL && xrtTypedStackPush(stack, &real) &&
	             xrtTypedStackPop(stack, &realOut) && realOut == real,
	             "fixed float32 descriptor preserves width") ) {
		xrtTypedStackRelease(stack);
		xrtUnit();
		return 1;
	}
	xrtTypedStackRelease(stack);

	stack = xrtTypedStackCreate(xrtTypeBool32());
	if ( !check(stack != NULL && xrtTypedStackPush(stack, &truth) &&
	             xrtTypedStackPop(stack, &truthOut) && truthOut == truth,
	             "32-bit language bool descriptor preserves width") ) {
		xrtTypedStackRelease(stack);
		xrtUnit();
		return 1;
	}
	xrtTypedStackRelease(stack);

	tree = xrtTypedAvlTreeCreate(xrtTypeString(), xrtTypeString(), XRT_OBJMODE_SHARED);
	if ( !check(tree != NULL, "create typed AVL tree") ||
		 !check(xrtTypedAvlTreeSet(tree, &keyB, &valueB), "set tree item b") ||
		 !check(xrtTypedAvlTreeSet(tree, &keyA, &valueA), "set tree item a") ||
		 !check(xrtTypedAvlTreeCount(tree) == 2u, "tree count") ||
		 !check(xrtTypedAvlTreeGet(tree, &keyA, &text) && text != NULL && strcmp(text, valueA) == 0, "get tree item") ) {
		xrtTypeDropValue(xrtTypeString(), &text);
		xrtTypedAvlTreeRelease(tree);
		xrtUnit();
		return 1;
	}
	xrtTypeDropValue(xrtTypeString(), &text);
	text = NULL;
	keys = xrtTypedAvlTreeKeys(tree, XRT_OBJMODE_LOCAL);
	if ( !check(keys != NULL && xrtTypedArrayCount(keys) == 2u, "collect ordered tree keys") ||
		 !check(strcmp(*(const char**)xrtTypedArrayGet(keys, 1u), "a") == 0, "first ordered tree key") ||
		 !check(strcmp(*(const char**)xrtTypedArrayGet(keys, 2u), "b") == 0, "last ordered tree key") ||
		 !check(xrtTypedAvlTreeRemove(tree, &keyA), "remove tree item") ||
		 !check(!xrtTypedAvlTreeContains(tree, &keyA), "removed tree item is absent") ) {
		xrtTypedArrayRelease(keys);
		xrtTypedAvlTreeRelease(tree);
		xrtUnit();
		return 1;
	}
	xrtTypedArrayRelease(keys);
	xrtTypedAvlTreeRelease(tree);

	queue = xrtTypedQueueCreate(XRT_TYPED_QUEUE_MPMC, xrtTypeString(), 8u);
	if ( !check(queue != NULL, "create typed MPMC queue") ||
		 !check(xrtTypedQueueTryPush(queue, &valueA) == XQUEUE_OK, "push typed queue") ||
		 !check(xrtTypedQueueTryPop(queue, &text) == XQUEUE_OK, "pop typed queue") ||
		 !check(text != NULL && strcmp(text, valueA) == 0, "typed queue preserves string") ) {
		xrtTypeDropValue(xrtTypeString(), &text);
		xrtTypedQueueRelease(queue);
		xrtUnit();
		return 1;
	}
	xrtTypeDropValue(xrtTypeString(), &text);
	text = NULL;
	xrtTypedQueueClose(queue);
	if ( !check(xrtTypedQueueTryPush(queue, &valueB) == XQUEUE_CLOSED, "closed typed queue rejects push") ) {
		xrtTypedQueueRelease(queue);
		xrtUnit();
		return 1;
	}
	xrtTypedQueueRelease(queue);

	waitQueue = xrtTypedQueueCreate(XRT_TYPED_QUEUE_WAIT, xrtTypeInt(), 4u);
	if ( !check(waitQueue != NULL, "create typed wait queue") ||
		 !check(xrtTypedQueuePopTimeout(waitQueue, &number, 1u) == XQUEUE_TIMEOUT, "typed wait queue timeout") ||
		 !check(xrtTypedQueueTryPush(waitQueue, &second) == XQUEUE_OK, "push typed wait queue") ||
		 !check(xrtTypedQueuePop(waitQueue, &number) == XQUEUE_OK && number == second, "pop typed wait queue") ) {
		xrtTypedQueueRelease(waitQueue);
		xrtUnit();
		return 1;
	}
	xrtTypedQueueRelease(waitQueue);

	xrtUnit();
	puts("typed special ok");
	return 0;
}
