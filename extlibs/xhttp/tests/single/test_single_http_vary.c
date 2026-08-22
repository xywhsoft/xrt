#define XHTTP_IMPLEMENTATION
#include "../../single/xhttp.h"

#include <stdio.h>



/* 验证单头文件保留完整 Vary 协议计划。 */
int main(void)
{
	static const xhttpfield Fields[] = {
		{
			XRT_STR_INIT("Vary"),
			XRT_STR_INIT("Accept-Encoding")
		},
		{
			XRT_STR_INIT("vary"),
			XRT_STR_INIT("Origin, *")
		}
	};
	xhttpvaryplan Plan;
	xhttpvaryitem Item;
	bool bPass;

	bPass = xrtHttpVaryPlan(
		Fields, 2, &Plan
	) && (Plan.FieldCount == 2) &&
		(Plan.ItemCount == 3) &&
		((Plan.Flags & XHTTP_VARY_MIXED) != 0) &&
		(xrtHttpVaryFind(
			Fields,
			2,
			XRT_STR_LITERAL("origin"),
			&Item
		 ) == XHTTP_NEXT_ITEM);
	printf(
		"%s single-http-vary\n",
		bPass ? "[PASS]" : "[FAIL]"
	);
	return bPass ? 0 : 1;
}
