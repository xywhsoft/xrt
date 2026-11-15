#define XRT_MODULE_WEBSOCKET_STREAM
#define XRT_IMPLEMENTATION
#include "../../single/xrt.h"
#if defined(XRT_FEATURE_WEBSOCKET_STREAM_WRITE_PROGRESS) || defined(XRT_FEATURE_TLS_STREAM) || defined(XRT_FEATURE_NET_TCP_WRITE_PROGRESS)
#error "ordinary WS must not force progress or TLS"
#endif
int main(void){xwsstreamconfig C;xrtWsStreamConfigInit(&C);return xrtWsStreamConfigValid(&C)?0:1;}
