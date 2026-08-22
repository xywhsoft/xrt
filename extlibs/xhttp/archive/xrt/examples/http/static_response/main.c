#include <stdio.h>
#include <string.h>

#include <xrt.h>



/* 构建不依赖服务器对象的静态响应字段。 */
int main(void)
{
	xhttprepresentation Current;
	xhttpstaticresponseconfig Config;
	xhttpstaticplan Plan;
	xhttpstaticresponse Response;
	char Workspace[128];
	size_t iSize;
	size_t i;

	memset(&Current, 0, sizeof(Current));
	Current.Exists = true;
	memset(&Plan, 0, sizeof(Plan));
	Plan.Status = XHTTP_STATUS_OK;
	Plan.SendBody = true;
	Plan.AcceptRanges = true;
	Plan.CompleteLength = 1024;
	Plan.SelectedLength = 1024;
	xrtHttpStaticResponseConfigInit(&Config);
	Config.ContentType = XRT_STR_LITERAL("application/json");

	if ( !xrtHttpStaticResponseBuild(
		&Plan,
		NULL,
		&Current,
		&Config,
		Workspace,
		sizeof(Workspace),
		&iSize,
		&Response
	) ) {
		return 1;
	}
	printf("status: %u\n", (unsigned int)Response.Status);
	for ( i = 0; i < Response.FieldCount; i++ ) {
		printf(
			"%.*s: %.*s\n",
			(int)Response.Fields[i].Name.Size,
			Response.Fields[i].Name.Data,
			(int)Response.Fields[i].Value.Size,
			Response.Fields[i].Value.Data
		);
	}
	return 0;
}
