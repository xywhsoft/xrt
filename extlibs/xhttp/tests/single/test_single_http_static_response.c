#define XHTTP_IMPLEMENTATION
#include "../../single/xhttp.h"

#include <string.h>



/* 单头发布必须保留静态响应字段构建能力。 */
int main(void)
{
	xhttprepresentation Current;
	xhttpstaticresponseconfig Config;
	xhttpstaticplan Plan;
	xhttpstaticresponse Response;
	char Workspace[64];
	size_t iSize;

	memset(&Current, 0, sizeof(Current));
	Current.Exists = true;
	memset(&Plan, 0, sizeof(Plan));
	Plan.Status = XHTTP_STATUS_OK;
	Plan.SendBody = true;
	Plan.CompleteLength = 7;
	Plan.SelectedLength = 7;
	xrtHttpStaticResponseConfigInit(&Config);
	Config.ContentType = XRT_STR_LITERAL("text/plain");

	if ( !xrtHttpStaticResponseBuild(
		&Plan,
		NULL,
		&Current,
		&Config,
		Workspace,
		sizeof(Workspace),
		&iSize,
		&Response
	) || (Response.Status != XHTTP_STATUS_OK) ||
		(Response.FieldCount != 2) ||
		(Response.BodyLength != 7) ) {
		return 1;
	}
	return 0;
}
