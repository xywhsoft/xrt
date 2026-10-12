#include "../../../dev/bench/bench_common.h"
#include <xws-client.h>
int main(int argc, char** argv) {
 size_t Count = argc > 1 ? (size_t)strtoull(argv[1], NULL, 10) : 10000;
 xwsclientconfig Config; xrtWsClientConfigInit(&Config);
 uint64_t Start = xbenchNowNs();
 for (size_t i = 0; i < Count; i++) { char Key[XWS_KEY_CAPACITY]; xhttprequest* pRequest = xrtWsClientRequestCreate(XRT_STR_LITERAL("ws://localhost/test"), &Config, Key); if (pRequest == NULL) return 1; xrtHttpRequestDestroy(pRequest); }
 double Seconds = (double)(xbenchNowNs() - Start) / 1e9;
 printf("client_request_per_sec=%.3f\n", (double)Count / (Seconds > 0 ? Seconds : 1e-9));
 return 0;
}
