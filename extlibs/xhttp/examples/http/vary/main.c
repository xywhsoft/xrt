#include <xhttp.h>

#include <stdio.h>



/* 展示重复 Vary 字段的协议汇总和底层遍历。 */
int main(void)
{
	static const xhttpfield Fields[] = {
		{
			XRT_STR_INIT("Vary"),
			XRT_STR_INIT("Accept-Encoding, Origin")
		},
		{
			XRT_STR_INIT("vary"),
			XRT_STR_INIT("User-Agent")
		}
	};
	xhttpvaryplan Plan;
	xhttpvarycursor Cursor;
	xhttpvaryitem Item;
	xhttpnext Next;

	if ( !xrtHttpVaryPlan(
		Fields,
		sizeof(Fields) / sizeof(Fields[0]),
		&Plan
	) ) {
		return 1;
	}
	printf(
		"fields=%zu names=%zu wildcard=%s\n",
		Plan.FieldCount,
		Plan.NameCount,
		(Plan.Flags & XHTTP_VARY_WILDCARD) != 0 ?
			"yes" : "no"
	);
	xrtHttpVaryCursorInit(&Cursor);
	while ( (Next = xrtHttpVaryNext(
		Fields,
		sizeof(Fields) / sizeof(Fields[0]),
		&Cursor,
		&Item
	)) == XHTTP_NEXT_ITEM ) {
		printf(
			"vary=%.*s\n",
			(int)Item.Name.Size,
			Item.Name.Data
		);
	}
	return Next == XHTTP_NEXT_END ? 0 : 1;
}
