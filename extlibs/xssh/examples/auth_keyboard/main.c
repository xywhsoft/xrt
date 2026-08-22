#include <stdio.h>

#include <xssh.h>



/* 构建两步挑战并无分配迭代其提示。 */
int main(void)
{
	unsigned char arrPayload[256];
	xsshauthkeyboardprompt arrPrompts[2];
	xsshauthkeyboardprompt Prompt;
	xsshauthkeyboardchallenge Challenge;
	xsshwriter Writer;

	arrPrompts[0] = (xsshauthkeyboardprompt){
		XRT_STR_LITERAL("Password:"),
		false
	};
	arrPrompts[1] = (xsshauthkeyboardprompt){
		XRT_STR_LITERAL("One-time code:"),
		false
	};
	if ( !xrtSshWriterInit(&Writer, arrPayload, sizeof(arrPayload)) ||
		(xrtSshAuthKeyboardChallengeWrite(
			&Writer,
			XRT_STR_LITERAL("Login"),
			XRT_STR_LITERAL("Complete both prompts"),
			XRT_STR_LITERAL(""),
			arrPrompts,
			2u
		) != XSSH_OK) || (xrtSshAuthKeyboardChallengeRead(
			(xbytesview){ arrPayload, Writer.Size },
			&Challenge
		) != XSSH_OK) ) {
		return 1;
	}
	while ( xrtSshAuthKeyboardChallengeNext(&Challenge, &Prompt) ) {
		printf("%.*s\n", (int)Prompt.Prompt.Size, Prompt.Prompt.Data);
	}
	return 0;
}
