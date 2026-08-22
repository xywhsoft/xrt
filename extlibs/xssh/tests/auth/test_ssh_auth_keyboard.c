#include "../test.h"



/* 验证 keyboard-interactive 请求的便捷和完整写法。 */
static void testSshKeyboardRequest(void)
{
	unsigned char arrPayload[256];
	xsshwriter Writer;
	xsshauthkeyboard Keyboard;

	testRequire(xrtSshWriterInit(&Writer, arrPayload, sizeof(arrPayload)) &&
		(xrtSshAuthKeyboardWrite(
			&Writer,
			XRT_STR_LITERAL("alice"),
			XRT_STR_LITERAL("pam,otp")
		) == XSSH_OK) && (xrtSshAuthKeyboardRead(
			(xbytesview){ arrPayload, Writer.Size },
			&Keyboard
		) == XSSH_OK) && testSshTextEqual(
		Keyboard.User,
		XRT_STR_LITERAL("alice")
	) && (Keyboard.Language.Size == 0u) && testSshTextEqual(
		Keyboard.Submethods,
		XRT_STR_LITERAL("pam,otp")
	), "ssh keyboard request mismatch");

	testRequire(xrtSshWriterInit(&Writer, arrPayload, sizeof(arrPayload)) &&
		(xrtSshAuthKeyboardWriteLanguage(
			&Writer,
			XRT_STR_LITERAL("bob"),
			XRT_STR_LITERAL("en-US"),
			XRT_STR_LITERAL("")
		) == XSSH_OK) && (xrtSshAuthKeyboardRead(
			(xbytesview){ arrPayload, Writer.Size },
			&Keyboard
		) == XSSH_OK) && testSshTextEqual(
		Keyboard.Language,
		XRT_STR_LITERAL("en-US")
	), "ssh keyboard language request mismatch");
}



/* 验证 challenge 的零项和无固定上限迭代。 */
static void testSshKeyboardChallenge(void)
{
	unsigned char arrPayload[512];
	xsshauthkeyboardprompt arrPrompts[16];
	xsshauthkeyboardprompt Prompt;
	xsshauthkeyboardchallenge Challenge;
	xsshwriter Writer;
	size_t i;

	for ( i = 0u; i < 16u; ++i ) {
		arrPrompts[i].Prompt = XRT_STR_LITERAL("Verification code:");
		arrPrompts[i].Echo = (i & 1u) != 0u;
	}
	testRequire(xrtSshWriterInit(&Writer, arrPayload, sizeof(arrPayload)) &&
		(xrtSshAuthKeyboardChallengeWrite(
			&Writer,
			XRT_STR_LITERAL("Two-factor authentication"),
			XRT_STR_LITERAL("Complete every prompt"),
			XRT_STR_LITERAL("en-US"),
			arrPrompts,
			16u
		) == XSSH_OK) && (xrtSshAuthKeyboardChallengeRead(
			(xbytesview){ arrPayload, Writer.Size },
			&Challenge
		) == XSSH_OK) && (Challenge.Count == 16u) &&
		testSshTextEqual(
			Challenge.Name,
			XRT_STR_LITERAL("Two-factor authentication")
		), "ssh keyboard challenge setup failed");
	for ( i = 0u; i < 16u; ++i ) {
		testRequire(xrtSshAuthKeyboardChallengeNext(
			&Challenge,
			&Prompt
		) && testSshTextEqual(
			Prompt.Prompt,
			XRT_STR_LITERAL("Verification code:")
		) && (Prompt.Echo == ((i & 1u) != 0u)),
		"ssh keyboard challenge item mismatch");
	}
	testRequire(!xrtSshAuthKeyboardChallengeNext(&Challenge, &Prompt),
		"ssh keyboard challenge iterated past end");

	testRequire(xrtSshWriterInit(&Writer, arrPayload, sizeof(arrPayload)) &&
		(xrtSshAuthKeyboardChallengeWrite(
			&Writer,
			XRT_STR_LITERAL("No questions"),
			XRT_STR_LITERAL(""),
			XRT_STR_LITERAL(""),
			NULL,
			0u
		) == XSSH_OK) && (xrtSshAuthKeyboardChallengeRead(
			(xbytesview){ arrPayload, Writer.Size },
			&Challenge
		) == XSSH_OK) && (Challenge.Count == 0u),
		"ssh keyboard zero-prompt challenge mismatch");
}



/* 验证 response 的空响应、零项和借用迭代。 */
static void testSshKeyboardResponse(void)
{
	unsigned char arrPayload[128];
	xstrview arrResponses[3];
	xstrview Response;
	xsshauthkeyboardresponses Responses;
	xsshwriter Writer;

	arrResponses[0] = XRT_STR_LITERAL("alice");
	arrResponses[1] = XRT_STR_LITERAL("");
	arrResponses[2] = XRT_STR_LITERAL("123456");
	testRequire(xrtSshWriterInit(&Writer, arrPayload, sizeof(arrPayload)) &&
		(xrtSshAuthKeyboardResponseWrite(
			&Writer,
			arrResponses,
			3u
		) == XSSH_OK) && (xrtSshAuthKeyboardResponseRead(
			(xbytesview){ arrPayload, Writer.Size },
			&Responses
		) == XSSH_OK) && (Responses.Count == 3u),
		"ssh keyboard response setup failed");
	testRequire(xrtSshAuthKeyboardResponseNext(&Responses, &Response) &&
		testSshTextEqual(Response, arrResponses[0]),
		"ssh keyboard first response mismatch");
	testRequire(xrtSshAuthKeyboardResponseNext(&Responses, &Response) &&
		(Response.Size == 0u),
		"ssh keyboard empty response mismatch");
	testRequire(xrtSshAuthKeyboardResponseNext(&Responses, &Response) &&
		testSshTextEqual(Response, arrResponses[2]) &&
		!xrtSshAuthKeyboardResponseNext(&Responses, &Response),
		"ssh keyboard final response mismatch");

	testRequire(xrtSshWriterInit(&Writer, arrPayload, sizeof(arrPayload)) &&
		(xrtSshAuthKeyboardResponseWrite(
			&Writer,
			NULL,
			0u
		) == XSSH_OK) && (xrtSshAuthKeyboardResponseRead(
			(xbytesview){ arrPayload, Writer.Size },
			&Responses
		) == XSSH_OK) && (Responses.Count == 0u),
		"ssh keyboard zero-response message mismatch");
}



/* 验证截断、尾随、UTF-8、容量和输入输出重叠边界。 */
static void testSshKeyboardBoundaries(void)
{
	static const char arrBadUtf8[] = { (char)0xc0, (char)0x80 };
	union {
		xsshauthkeyboardprompt Align;
		unsigned char Data[256];
	} Storage;
	unsigned char arrPayload[128];
	xsshauthkeyboardprompt Prompt;
	xsshauthkeyboardprompt* pOverlap;
	xsshauthkeyboardchallenge Challenge;
	xsshauthkeyboardresponses Responses;
	xsshwriter Writer;
	size_t iSize;

	Prompt.Prompt = XRT_STR_LITERAL("");
	Prompt.Echo = false;
	testRequire(xrtSshWriterInit(&Writer, arrPayload, sizeof(arrPayload)) &&
		(xrtSshAuthKeyboardChallengeWrite(
			&Writer,
			XRT_STR_LITERAL(""),
			XRT_STR_LITERAL(""),
			XRT_STR_LITERAL(""),
			&Prompt,
			1u
		) == XSSH_ERROR_ARGUMENT) && (Writer.Size == 0u),
		"ssh keyboard accepted empty prompt");

	Prompt.Prompt = (xstrview){ arrBadUtf8, sizeof(arrBadUtf8) };
	testRequire((xrtSshAuthKeyboardChallengeWrite(
		&Writer,
		XRT_STR_LITERAL(""),
		XRT_STR_LITERAL(""),
		XRT_STR_LITERAL(""),
		&Prompt,
		1u
	) == XSSH_ERROR_ARGUMENT) && (Writer.Size == 0u),
		"ssh keyboard accepted invalid UTF-8 prompt");

	pOverlap = (xsshauthkeyboardprompt*)Storage.Data;
	pOverlap->Prompt = XRT_STR_LITERAL("Code:");
	pOverlap->Echo = false;
	testRequire(xrtSshWriterInit(
		&Writer,
		Storage.Data,
		sizeof(Storage.Data)
	) && (xrtSshAuthKeyboardChallengeWrite(
		&Writer,
		XRT_STR_LITERAL(""),
		XRT_STR_LITERAL(""),
		XRT_STR_LITERAL(""),
		pOverlap,
		1u
	) == XSSH_ERROR_ARGUMENT) && (Writer.Size == 0u),
		"ssh keyboard accepted overlapping prompt descriptors");

	Prompt.Prompt = XRT_STR_LITERAL("Code:");
	Prompt.Echo = false;
	testRequire(xrtSshWriterInit(&Writer, arrPayload, sizeof(arrPayload)) &&
		(xrtSshAuthKeyboardChallengeWrite(
			&Writer,
			XRT_STR_LITERAL(""),
			XRT_STR_LITERAL(""),
			XRT_STR_LITERAL(""),
			&Prompt,
			1u
		) == XSSH_OK), "ssh keyboard boundary setup failed");
	iSize = Writer.Size;
	testRequire(xrtSshAuthKeyboardChallengeRead(
		(xbytesview){ arrPayload, iSize - 1u },
		&Challenge
	) == XSSH_NEED_MORE, "ssh keyboard truncated challenge was not incremental");
	arrPayload[iSize] = 0u;
	testRequire(xrtSshAuthKeyboardChallengeRead(
		(xbytesview){ arrPayload, iSize + 1u },
		&Challenge
	) == XSSH_ERROR_PROTOCOL, "ssh keyboard accepted challenge trailing data");

	testRequire(xrtSshWriterInit(&Writer, arrPayload, 8u) &&
		(xrtSshAuthKeyboardChallengeWrite(
			&Writer,
			XRT_STR_LITERAL(""),
			XRT_STR_LITERAL(""),
			XRT_STR_LITERAL(""),
			&Prompt,
			1u
		) == XSSH_ERROR_SPACE) && (Writer.Size == 0u),
		"ssh keyboard short challenge changed writer");

	arrPayload[0] = XSSH_MSG_USERAUTH_INFO_RESPONSE;
	arrPayload[1] = 0u;
	arrPayload[2] = 0u;
	arrPayload[3] = 0u;
	arrPayload[4] = 1u;
	testRequire(xrtSshAuthKeyboardResponseRead(
		(xbytesview){ arrPayload, 5u },
		&Responses
	) == XSSH_NEED_MORE, "ssh keyboard accepted missing response string");
}



/* 运行 keyboard-interactive 报文与边界测试。 */
int main(void)
{
	testSshKeyboardRequest();
	testSshKeyboardChallenge();
	testSshKeyboardResponse();
	testSshKeyboardBoundaries();
	return 0;
}
