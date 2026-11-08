#define XPOP3_MODULE_POP3
#include <xpop3.h>
#include <string.h>

/* Exercise borrowed views, a Core-owned command and the consumer's Core error
 * channel through the installed public headers and the real static/shared lib. */
int main(void)
{
    xpop3replyview reply;
    xpop3uidlview uidl;
    size_t size = 0;
    str command;
    bool valid;
    char line[73];

    if (!xrtPop3ReplyParse(XRT_STR_LITERAL("+OK mailbox ready"), &reply) ||
        !reply.Ok || reply.Text.Size != sizeof("mailbox ready") - 1u ||
        memcmp(reply.Text.Data, "mailbox ready", reply.Text.Size) != 0) return 1;
    xrtClearError();
    command = xrtPop3Command(XRT_STR_LITERAL("QUIT"), XRT_STR_LITERAL(""), &size);
    valid = command != NULL && size == 6u && strcmp(command, "QUIT\r\n") == 0;
    xrtFree(command);
    if (!valid || xrtGetError() != NULL) return 2;

    line[0] = '1'; line[1] = ' ';
    memset(line + 2, 'x', sizeof(line) - 2u);
    valid = !xrtPop3UidlParse((xstrview){line, sizeof(line)}, &uidl) &&
        xrtErrorKind(xrtGetError()) == XERR_PROTOCOL &&
        xrtErrorCode(xrtGetError()) == XMAIL_ERROR_PROTOCOL;
    xrtClearError();
    return valid ? 0 : 3;
}
