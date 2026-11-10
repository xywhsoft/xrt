/* Same combined QoS1 echo, binding once and then using empty-topic aliases. */
#define XMQTT_BENCH_ALIAS
#define XMQTT_BENCH_SESSION
#define XMQTT_BENCH_INBOX
#include "bench_client_receive.c"
