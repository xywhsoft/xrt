#include "../internal/xrt_xsonl.h"

#if defined(XRT_FEATURE_XSONL_CORE)
const xtextlinesformat __xrtXsonlFormat = { "xrt.xsonl", "xrt.xson", XXSONL_ERROR_CONFIG };

XRT_API bool xrtXsonlErrorLocation(const xerror* pError, xxsonllocation* pLocation)
{
	xtextlineslocation Location;
	if ( pLocation == NULL ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	if ( !__xrtTextLinesErrorLocation(pError, &__xrtXsonlFormat, &Location) ) {
		return false;
	}
	pLocation->Offset = Location.Offset;
	pLocation->Line = Location.Line;
	pLocation->Column = Location.Column;
	pLocation->RecordIndex = Location.RecordIndex;
	return true;
}
#endif

