#define XSMTP_MODULE_ALL
#include <xsmtp.h>
#include <string.h>



/* 验证借用视图、跨库拥有型内存及同一 Core 错误通道。 */
int main(void)
{
	static const char Line[] = "250-SIZE 10485760";
	xsmtpreplyline Reply;
	xsmtpreply Output, Before;
	size_t Size = 0;
	str Command;

	if ( !xrtSmtpReplyLineParse((xstrview){ Line, sizeof(Line) - 1u }, &Reply) ||
		(Reply.Code != 250) || !Reply.Continued ||
		(Reply.Source.Data != Line) || (Reply.Text.Data != Line + 4u) ||
		(Reply.Text.Size != sizeof("SIZE 10485760") - 1u) ) {
		return 1;
	}
	Command = xrtSmtpCommand(XRT_STR_LITERAL("NOOP"), XRT_STR_LITERAL(""), &Size);
	if ( Command == NULL ) {
		return 2;
	}
	if ( (Size != 6u) || (memcmp(Command, "NOOP\r\n", 6u) != 0) ) {
		xrtFree(Command);
		return 3;
	}
	xrtFree(Command);
	memset(&Output, 0xA5, sizeof(Output));
	memcpy(&Before, &Output, sizeof(Before));
	xrtClearError();
	if ( xrtSmtpClientReceive(NULL, &Output, -2, NULL) ||
		(xrtErrorKind(xrtGetError()) != XERR_ARGUMENT) ||
		(memcmp(&Output, &Before, sizeof(Output)) != 0) ) {
		return 4;
	}
	xrtClearError();
	return 0;
}
