#include "../test.h"



/* 验证 APPEND 公开结构的默认状态。 */
int main(void)
{
	ximapappendconfig Config;
	ximapappendresult Result;

	xrtImapAppendConfigInit(&Config);
	xrtImapAppendResultInit(&Result);
	testRequire((Config.Literal == XIMAP_LITERAL_AUTO) &&
		(Config.Size == 0) && !Result.Present &&
		(Result.UidValidity == 0) && (Result.Uid == 0),
		"IMAP APPEND defaults mismatch");
	return 0;
}
