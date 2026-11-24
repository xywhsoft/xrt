#include "../duplex/app_dispatch.h"
#include <xrt/io.h>
#include <stdio.h>
#if defined(_WIN32)
#include <fcntl.h>
#include <io.h>
#endif

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

static bool serve(xreader* Input, xrpcduplex* Endpoint, apprequest* Job)
{
#if defined(_WIN32)
    if (_setmode(_fileno(stdout), _O_BINARY) == -1) return false;
#endif
    char Record[sizeof(Job->Input) + 1]; /* Room for LF after a full record. */
    size_t Used = 0;
    for (;;) {
        size_t Read;
        if (!xrtReaderRead(Input, Record + Used, sizeof(Record) - Used, &Read)) return false;
        if (!Read) return Used == 0;
        size_t Search = Used; /* The previous read already searched this prefix. */
        size_t Start = 0;
        Used += Read;
        while (Search < Used) {
            const char* End = memchr(Record + Search, '\n', Used - Search);
            if (!End) break;
            size_t Bytes = (size_t)(End - (Record + Start));
            if (!handle(Endpoint, Job, (xstrview){Record + Start, Bytes})) return false;
            Start += Bytes + 1;
            Search = Start;
        }
        if (Start) {
            Used -= Start;
            if (Used) memmove(Record, Record + Start, Used);
        }
        if (Used == sizeof(Record)) return false;
    }
}

int main(int Argc, char** Argv)
{
    xrpcduplex* Endpoint = NULL;
    xrpcduplexconfig Config;
    apprequest Job = {0};
    if (!xrpcDuplexConfigInit(&Config)) return 1;
    Config.MaxIncoming = 1;
    Config.OnRequest = appTakeRequest;
    Config.User = &Job;
    if (!xrpcDuplexCreate(&Config, &Endpoint)) return 1;
    bool OK = true;
    if (Argc == 2 && strcmp(Argv[1], "--serve") == 0) {
        xreader* Input = xrtReaderStdin();
        OK = Input && serve(Input, Endpoint, &Job);
        if (Input && !xrtReaderDestroy(Input)) OK = false;
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
