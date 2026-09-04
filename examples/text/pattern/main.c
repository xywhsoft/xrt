#define XRT_MODULE_PATTERN
#include <xrt.h>

#include <stdio.h>



int main(void)
{
	xpatternbuilder* pBuilder = xrtPatternBuilderCreate();
	xpatternspec Spec = {
		XRT_STR_INIT("/users/{group}/item-{id}.json"),
		(ptr)"user",
		0,
		0
	};
	xpattern* pPattern;
	xpatternmatch Match;
	xstrview arrCapture[2];

	if ( (pBuilder == NULL) ||
		 (xrtPatternBuilderAdd(pBuilder, &Spec) == XPATTERN_ID_INVALID) ) {
		xrtPatternBuilderFree(pBuilder);
		return 1;
	}
	pPattern = xrtPatternBuilderCompile(pBuilder);
	xrtPatternBuilderFree(pBuilder);
	if ( pPattern == NULL ) {
		return 1;
	}
	if ( xrtPatternMatch(
		pPattern,
		XRT_STR_LITERAL("/users/admin/item-42.json"),
		arrCapture,
		2u,
		&Match
	) == XPATTERN_MATCH ) {
		printf(
			"kind=%s group=%.*s id=%.*s\n",
			(cstr)Match.Value,
			(int)arrCapture[0].Size,
			arrCapture[0].Data,
			(int)arrCapture[1].Size,
			arrCapture[1].Data
		);
	}
	xrtPatternRelease(pPattern);
	return 0;
}
