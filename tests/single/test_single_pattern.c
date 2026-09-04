#ifdef XRT_MODULE_PATTERN
	#undef XRT_MODULE_PATTERN
#endif
#define XRT_MODULE_PATTERN
#define XRT_IMPLEMENTATION
#include "../../single/xrt.h"



int main(void)
{
	xpatternbuilder* pBuilder;
	xpatternspec Spec;
	xpattern* pPattern;
	xpatternmatch Match;
	xstrview Capture;

	#if !defined(XRT_FEATURE_PATTERN)
		#error "XRT_MODULE_PATTERN did not enable pattern"
	#endif

	pBuilder = xrtPatternBuilderCreate();
	if ( pBuilder == NULL ) {
		return 1;
	}
	memset(&Spec, 0, sizeof(Spec));
	Spec.Pattern = XRT_STR_LITERAL("/single/prefix{id}suffix");
	if ( xrtPatternBuilderAdd(pBuilder, &Spec) == XPATTERN_ID_INVALID ) {
		return 2;
	}
	pPattern = xrtPatternBuilderCompile(pBuilder);
	xrtPatternBuilderFree(pBuilder);
	if ( pPattern == NULL ) {
		return 3;
	}
	if ( xrtPatternMatch(
		pPattern,
		XRT_STR_LITERAL("/single/prefix9suffix"),
		&Capture,
		1u,
		&Match
	) != XPATTERN_MATCH ) {
		return 4;
	}
	if ( (Capture.Size != 1u) || (Capture.Data[0] != '9') ) {
		return 5;
	}
	xrtPatternRelease(pPattern);
	return 0;
}
