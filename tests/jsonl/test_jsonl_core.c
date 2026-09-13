#include "../test.h"
int main(void)
{
	xerrordesc Desc = { 0 };
	xjsonllocation Location = { 9, 9, 9, 9 };
	xerror* pError;
	Desc.Kind = XERR_PROTOCOL;
	Desc.Code = XJSONL_ERROR_RECORD;
	Desc.Domain = "xrt.jsonl";
	Desc.Data = "offset=12;line=4;column=3;record=1";
	pError = xrtErrorBuild(&Desc);
	testRequire(pError != NULL && xrtJsonlErrorLocation(pError, &Location), "location missing");
	testRequire(Location.Offset == 12 && Location.Line == 4 && Location.Column == 3 &&
		Location.RecordIndex == 1, "location mismatch");
	testRequire(!xrtJsonlErrorLocation(NULL, &Location) && Location.Offset == 12, "location failure changed output");
	xrtErrorFree(pError);
	xrtClearError();
	printf("[PASS] JSONL core\n");
	return 0;
}

