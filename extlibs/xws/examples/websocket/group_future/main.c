#include <stdio.h>

#include <xws.h>



/* 展示空组广播也会返回可等待、可检查的完整操作对象。 */
int main(void)
{
	xwsgroup* pGroup = xrtWsGroupCreate(0);
	xwsgroupop* pOperation;

	if ( pGroup == NULL ) {
		return 1;
	}
	pOperation = xrtWsGroupTextAsync(
		pGroup,
		XRT_STR_LITERAL("service-ready")
	);
	if ( (pOperation == NULL) ||
		(xrtWsGroupOpWait(pOperation) != XWAIT_OK) ) {
		xrtWsGroupOpDestroy(pOperation);
		xrtWsGroupDestroy(pGroup);
		return 1;
	}
	printf(
		"members=%zu accepted=%zu rejected=%zu\n",
		xrtWsGroupOpCount(pOperation),
		xrtWsGroupOpAccepted(pOperation),
		xrtWsGroupOpRejected(pOperation)
	);
	xrtWsGroupOpDestroy(pOperation);
	xrtWsGroupDestroy(pGroup);
	return 0;
}
