#include "../test.h"
int main(void)
{
	xerrordesc Desc = { 0 };
	xxsonllocation Location = { 9, 9, 9, 9 };
	xerror* pError;
	Desc.Kind = XERR_PROTOCOL;
	Desc.Code = XXSONL_ERROR_RECORD;
	Desc.Domain = "xrt.xsonl";
	Desc.Data = "offset=12;line=4;column=3;record=1";
	pError = xrtErrorBuild(&Desc);
	testRequire(pError != NULL && xrtXsonlErrorLocation(pError, &Location), "location missing");
	testRequire(Location.Offset == 12 && Location.Line == 4 && Location.Column == 3 &&
		Location.RecordIndex == 1, "location mismatch");
	testRequire(!xrtXsonlErrorLocation(NULL, &Location) && Location.Offset == 12, "location failure changed output");
	xrtErrorFree(pError);
	xrtClearError();
	printf("[PASS] XSONL core\n");
	return 0;
}

