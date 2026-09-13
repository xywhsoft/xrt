#include "../internal/xrt_jsonl.h"

#if defined(XRT_FEATURE_JSONL_CORE)
const xtextlinesformat __xrtJsonlFormat = { "xrt.jsonl", "xrt.json", XJSONL_ERROR_CONFIG };

XRT_API bool xrtJsonlErrorLocation(const xerror* pError, xjsonllocation* pLocation)
{
	xtextlineslocation Location;
	if ( pLocation == NULL ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	if ( !__xrtTextLinesErrorLocation(pError, &__xrtJsonlFormat, &Location) ) {
		return false;
	}
	pLocation->Offset = Location.Offset;
	pLocation->Line = Location.Line;
	pLocation->Column = Location.Column;
	pLocation->RecordIndex = Location.RecordIndex;
	return true;
}
#endif

