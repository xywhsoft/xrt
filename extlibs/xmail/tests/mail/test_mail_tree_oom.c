#include "../test.h"



/* 验证当前没有活动逻辑分配，并排空调试器隔离队列。 */
static void testMailTreeNoLive(cstr sMessage)
{
	xmemdebugsnapshot Snapshot;

	xrtMemDebugSnapshot(&Snapshot);
	testRequire(Snapshot.LiveCount == 0, sMessage);
	testMemoryDebugDrain(sMessage);
}



/* 覆盖 MIME 树每一个 arena 分配失败点和最终恢复路径。 */
int main(void)
{
	static const char sPrefix[] =
		"Content-Type: multipart/mixed; boundary=outer\r\n"
		"\r\n"
		"--outer\r\n"
		"Content-Type: application/octet-stream; name=sample.bin\r\n"
		"Content-Disposition: attachment; filename=sample.bin\r\n"
		"Content-Transfer-Encoding: base64\r\n"
		"\r\n";
	static const char sSuffix[] = "\r\n--outer--\r\n";
	char arrMessage[sizeof(sPrefix) + 8192u + sizeof(sSuffix)];
	xstrview Message;
	uint64 iCovered = 0;
	bool bRecovered = false;
	xmailtree Tree;

	memcpy(arrMessage, sPrefix, sizeof(sPrefix) - 1u);
	memset(arrMessage + sizeof(sPrefix) - 1u, 'A', 8192u);
	memcpy(
		arrMessage + sizeof(sPrefix) - 1u + 8192u,
		sSuffix,
		sizeof(sSuffix) - 1u
	);
	Message = testMailViewN(
		arrMessage,
		(sizeof(sPrefix) - 1u) + 8192u + (sizeof(sSuffix) - 1u)
	);
	testRequire(xrtMailTreeParse(Message, NULL, &Tree),
		"mail tree OOM fixture is invalid");
	xrtMailTreeFree(&Tree);
	testRequire(xrtMemDebugEnable(true),
		"mail tree memory debug enable failed");
	for ( uint64 iFail = 0; iFail < 128u; iFail++ ) {
		memset(&Tree, 0, sizeof(Tree));
		Tree.PartCount = 777u;
		testRequire(xrtMemDebugFailAfter(iFail),
			"mail tree logical OOM injection failed");
		if ( xrtMailTreeParse(Message, NULL, &Tree) ) {
			testRequire(!xrtMemDebugFailTriggered(),
				"mail tree ignored a logical allocation failure");
			xrtMailTreeFree(&Tree);
			bRecovered = true;
		} else {
			testRequire(xrtMemDebugFailTriggered() &&
				(xrtErrorKind(xrtGetError()) == XERR_MEMORY) &&
				(Tree.Storage == NULL) && (Tree.PartCount == 777u),
				"mail tree logical OOM contract mismatch");
			iCovered++;
		}
		xrtMemDebugFailClear();
		xrtClearError();
		testMailTreeNoLive("mail tree logical OOM leaked storage");
		if ( bRecovered ) {
			break;
		}
	}
	testRequire(bRecovered && (iCovered != 0),
		"mail tree did not recover after complete OOM matrix");
	testRequire(xrtMemDebugEnable(false),
		"mail tree memory debug disable failed");
	printf("[PASS] mail tree logical OOM (%llu positions)\n",
		(unsigned long long)iCovered);
	return 0;
}
