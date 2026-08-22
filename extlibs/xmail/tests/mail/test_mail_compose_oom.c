#include "../test.h"



/* 验证当前没有活动逻辑分配，并排空调试器隔离队列。 */
static void testMailComposeNoLive(cstr sMessage)
{
	xmemdebugsnapshot Snapshot;

	xrtMemDebugSnapshot(&Snapshot);
	testRequire(Snapshot.LiveCount == 0, sMessage);
	testMemoryDebugDrain(sMessage);
}



/* 覆盖 Compose 每一个逻辑分配失败点以及最终恢复路径。 */
int main(void)
{
	char arrText[2048];
	char arrFileName[1600];
	unsigned char arrAttachmentData[2048];
	xmailaddress To;
	xmailattachment Attachment;
	xmailmessage Message;
	size_t iSize = SIZE_MAX;
	uint64 iCovered = 0;
	bool bRecovered = false;

	memset(arrText, 'c', sizeof(arrText));
	memset(arrFileName, 'n', sizeof(arrFileName));
	memset(arrAttachmentData, 0xA5, sizeof(arrAttachmentData));
	xrtMailMessageInit(&Message);
	Message.From = (xmailaddress){
		XRT_STR_LITERAL("Sender"),
		XRT_STR_LITERAL("sender@example.com")
	};
	To = (xmailaddress){
		XRT_STR_LITERAL("Receiver"),
		XRT_STR_LITERAL("receiver@example.net")
	};
	Message.To = &To;
	Message.ToCount = 1u;
	Message.Subject = XRT_STR_LITERAL("compose OOM");
	Message.Text = testMailViewN(arrText, sizeof(arrText));
	Attachment = (xmailattachment){
		testMailViewN(arrFileName, sizeof(arrFileName)),
		XRT_STR_LITERAL("application/octet-stream"),
		XRT_STR_LITERAL(""),
		{ arrAttachmentData, sizeof(arrAttachmentData) },
		false
	};
	Message.Attachments = &Attachment;
	Message.AttachmentCount = 1u;

	{
		str sText = xrtMailCompose(&Message, &iSize);

		if ( sText == NULL ) {
			const xerror* pError = xrtGetError();

			fprintf(
				stderr,
				"[INFO] compose fixture kind=%d code=%d message=%s\n",
				(int)xrtErrorKind(pError),
				(int)xrtErrorCode(pError),
				xrtErrorMessage(pError)
			);
		}
		testRequire(sText != NULL, "mail compose OOM fixture is invalid");
		xrtFree(sText);
	}
	testRequire(xrtMemDebugEnable(true),
		"mail compose memory debug enable failed");
	for ( uint64 iFail = 0; iFail < 128u; iFail++ ) {
		str sText;

		testRequire(xrtMemDebugFailAfter(iFail),
			"mail compose logical OOM injection failed");
		sText = xrtMailCompose(&Message, &iSize);
		if ( sText != NULL ) {
			xrtFree(sText);
			testRequire(!xrtMemDebugFailTriggered(),
				"mail compose ignored a logical allocation failure");
			bRecovered = true;
		} else {
			if ( !xrtMemDebugFailTriggered() ||
				 (xrtErrorKind(xrtGetError()) != XERR_MEMORY) ) {
				fprintf(
					stderr,
					"[INFO] compose logical OOM offset=%llu kind=%d code=%d\n",
					(unsigned long long)iFail,
					(int)xrtErrorKind(xrtGetError()),
					(int)xrtErrorCode(xrtGetError())
				);
			}
			testRequire(xrtMemDebugFailTriggered() &&
				(xrtErrorKind(xrtGetError()) == XERR_MEMORY),
				"mail compose logical OOM error mismatch");
			iCovered++;
		}
		xrtMemDebugFailClear();
		xrtClearError();
		testMailComposeNoLive("mail compose logical OOM leaked storage");
		if ( bRecovered ) {
			break;
		}
	}
	testRequire(bRecovered && (iCovered != 0),
		"mail compose did not recover after complete OOM matrix");
	testRequire(xrtMemDebugEnable(false),
		"mail compose memory debug disable failed");
	printf("[PASS] mail compose logical OOM (%llu positions)\n",
		(unsigned long long)iCovered);
	return 0;
}
