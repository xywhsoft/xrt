/*
 * XRT Example - Task Group Scope
 * XRT 范例 - 任务组作用域
 *
 * Description / 说明:
 *   EN: Demonstrates task-group close/join flow and nested child-scope cancellation.
 *   CN: 演示任务组的 close/join 流程，以及父作用域关闭时对子作用域的取消传播。
 *
 * Build / 编译:
 *   tcc main.c -o ../../bin/future_task_group_scope.exe -lWs2_32 -lIPHLPAPI -lShell32
 *   gcc main.c -o ../../bin/future_task_group_scope -lm -lpthread
 */

#include "../../../xrt.c"
#include <stdio.h>


static int32 GroupThreadTask(ptr pArg, xfuture_result* pOut)
{
	int* pValue = (int*)pArg;

	if ( pOut == NULL || pValue == NULL ) {
		return XRT_NET_ERROR;
	}

	xrtSleep(30u);
	pOut->pValue = pValue;
	return XRT_NET_OK;
}


static const char* FutureStateText(xfuture_state iState)
{
	switch ( iState ) {
		case XFUTURE_PENDING: return "PENDING";
		case XFUTURE_RESOLVED: return "RESOLVED";
		case XFUTURE_REJECTED: return "REJECTED";
		case XFUTURE_CANCELLED: return "CANCELLED";
		case XFUTURE_CLOSED: return "CLOSED";
		default: return "UNKNOWN";
	}
}


int main(void)
{
	xtaskgroup* pGroup = NULL;
	xfuture* pTaskA = NULL;
	xfuture* pTaskB = NULL;
	xfuture* pJoin = NULL;
	xtaskgroup* pParent = NULL;
	xtaskgroup* pChild = NULL;
	xfuture* pLeaf = NULL;
	xpromise* pLeafPromise = NULL;
	xfuture* pParentJoin = NULL;
	int iValueA = 301;
	int iValueB = 302;
	int iRet = 0;

	xrtInit();

	pGroup = xTaskGroupCreate();
	if ( pGroup == NULL ) {
		printf("group_create = failed\n");
		iRet = 1;
		goto cleanup;
	}

	pTaskA = xTaskGroupRunThread(pGroup, GroupThreadTask, &iValueA, 0u);
	pTaskB = xTaskGroupRunThread(pGroup, GroupThreadTask, &iValueB, 0u);
	pJoin = xTaskGroupJoinFuture(pGroup);
	if ( pTaskA == NULL || pTaskB == NULL || pJoin == NULL ) {
		printf("group_run = failed\n");
		iRet = 1;
		goto cleanup;
	}

	printf("group_count_before_close = %d\n", xTaskGroupCount(pGroup));
	xTaskGroupClose(pGroup);
	if ( !xFutureWaitTimeout(pJoin, 2000) ) {
		printf("group_join = timeout\n");
		iRet = 1;
		goto cleanup;
	}

	printf("group_task_a = %d\n", *(int*)xFutureValue(pTaskA));
	printf("group_task_b = %d\n", *(int*)xFutureValue(pTaskB));
	printf("group_count_after_join = %d\n", xTaskGroupCount(pGroup));

	pParent = xTaskGroupCreate();
	pChild = xTaskGroupCreateChild(pParent);
	pLeaf = xFutureCreate();
	pLeafPromise = xPromiseCreate(pLeaf);
	pParentJoin = xTaskGroupJoinFuture(pParent);
	if ( pParent == NULL || pChild == NULL || pLeaf == NULL || pLeafPromise == NULL || pParentJoin == NULL ) {
		printf("nested_scope = failed\n");
		iRet = 1;
		goto cleanup;
	}
	if ( !xTaskGroupAddFuture(pChild, pLeaf) ) {
		printf("child_add = failed\n");
		iRet = 1;
		goto cleanup;
	}

	xTaskGroupClose(pParent);
	if ( !xFutureWaitTimeout(pParentJoin, 1000) ) {
		printf("parent_join = timeout\n");
		iRet = 1;
		goto cleanup;
	}

	printf("leaf_state_after_parent_close = %s\n", FutureStateText(xFutureState(pLeaf)));
	printf("leaf_status_after_parent_close = %d\n", (int)xFutureStatus(pLeaf));

cleanup:
	if ( pParentJoin != NULL ) {
		xFutureRelease(pParentJoin);
	}
	if ( pLeafPromise != NULL ) {
		xPromiseDestroy(pLeafPromise);
	}
	if ( pLeaf != NULL ) {
		xFutureRelease(pLeaf);
	}
	if ( pChild != NULL ) {
		xTaskGroupDestroy(pChild);
	}
	if ( pParent != NULL ) {
		xTaskGroupDestroy(pParent);
	}
	if ( pJoin != NULL ) {
		xFutureRelease(pJoin);
	}
	if ( pTaskB != NULL ) {
		xFutureRelease(pTaskB);
	}
	if ( pTaskA != NULL ) {
		xFutureRelease(pTaskA);
	}
	if ( pGroup != NULL ) {
		xTaskGroupDestroy(pGroup);
	}
	xrtUnit();
	return iRet;
}
