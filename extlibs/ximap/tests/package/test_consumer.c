#define XIMAP_MODULE_ALL
#include <ximap.h>
#include <string.h>

/* Exercise borrowed views, Core-owned quoting and the consumer's error channel. */
int main(void)
{
    static const char line[] = "* OK ready";
    ximapresponseview response;
    ximapevent event, before;
    size_t size = 0;
    str quoted;
    if (!xrtImapResponseParse((xstrview){ line, sizeof(line) - 1u }, &response) ||
        response.Kind != XIMAP_RESPONSE_UNTAGGED || response.Status != XIMAP_STATUS_OK ||
        response.Source.Data != line || response.Text.Data != line + 5u ||
        response.Text.Size != sizeof("ready") - 1u) return 1;
    quoted = xrtImapQuote(XRT_STR_LITERAL("INBOX"), &size);
    if (quoted == NULL) return 2;
    if (size != 7u || memcmp(quoted, "\"INBOX\"", 8u) != 0) {
        xrtFree(quoted); return 3;
    }
    xrtFree(quoted);
    memset(&event, 0xA5, sizeof(event)); memcpy(&before, &event, sizeof(before));
    xrtClearError();
    if (xrtImapClientReceive(NULL, &event, -2, NULL) ||
        xrtErrorKind(xrtGetError()) != XERR_ARGUMENT ||
        memcmp(&event, &before, sizeof(event)) != 0) return 4;
    xrtClearError(); return 0;
}
