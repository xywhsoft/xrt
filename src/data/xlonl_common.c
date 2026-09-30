#include "../internal/xrt_xlonl.h"

#if defined(XRT_FEATURE_XLONL_CORE)
const xtextlinesformat __xrtXlonlFormat = { "xrt.xlonl", "xrt.xlon", XXLONL_ERROR_CONFIG };

XRT_API bool xrtXlonlErrorLocation(const xerror* pError, xxlonllocation* pLocation)
{
	xtextlineslocation Location;
	if ( pLocation == NULL ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	if ( !__xrtTextLinesErrorLocation(pError, &__xrtXlonlFormat, &Location) ) {
		return false;
	}
	pLocation->Offset = Location.Offset;
	pLocation->Line = Location.Line;
	pLocation->Column = Location.Column;
	pLocation->RecordIndex = Location.RecordIndex;
	return true;
}
#endif

