#include "../duplex/app_dispatch.h"
#include <stdio.h>

static bool handle(xrpcduplex* Endpoint, apprequest* Job, xstrview Document)
{
    if (xrpcDuplexReceive(Endpoint, Document, (xbytesview){0}) != XRPC_DUPLEX_ACCEPTED ||
        !appFinish(Endpoint, Job) || Job->Token) return false;
    xrpcduplexoutput* Output = NULL;
    uint64 Token;
    int Count = xrpcDuplexNext(Endpoint, &Output, &Token);
    if (Count != 1) return Count == 0;
    xstrview Wire = xrpcDuplexOutputWire(Output);
    bool OK = xrpcDuplexStarted(Endpoint, Token) &&
        fwrite(Wire.Data, 1, Wire.Size, stdout) == Wire.Size &&
        fputc('\n', stdout) != EOF && fflush(stdout) == 0 &&
        xrpcDuplexSent(Endpoint, Token);
    xrpcDuplexOutputRelease(Output);
    return OK;
}

int main(int Argc, char** Argv)
{
    xrpcduplex* Endpoint = NULL;
    xrpcduplexconfig Config;
    apprequest Job = {0};
    if (!xrpcDuplexConfigInit(&Config)) return 1;
    Config.OnRequest = appTakeRequest;
    Config.User = &Job;
    if (!xrpcDuplexCreate(&Config, &Endpoint)) return 1;
    bool OK = true;
    if (Argc == 2 && strcmp(Argv[1], "--serve") == 0) {
        char Line[sizeof(Job.Input) + 2]; /* LF and the fgets terminator. */
        while (OK && fgets(Line, sizeof(Line), stdin)) {
            /* This APP framing contract is one JSON document per LF record. */
            size_t Bytes = strlen(Line);
            OK = Bytes && Line[Bytes - 1] == '\n' &&
                handle(Endpoint, &Job, (xstrview){Line, Bytes - 1});
        }
        if (ferror(stdin)) OK = false;
    } else if (Argc == 1) {
        OK = handle(Endpoint, &Job,
            XRT_STR_LITERAL("{\"jsonrpc\":\"2.0\",\"id\":1,\"method\":\"echo\",\"params\":[42]}"));
    } else {
        OK = false;
    }
    bool Closed = xrpcDuplexClose(Endpoint, XRPC_CALL_CLOSED) &&
        xrpcDuplexStep(Endpoint) && xrpcDuplexDestroy(Endpoint);
    return OK && Closed ? 0 : 2;
}
