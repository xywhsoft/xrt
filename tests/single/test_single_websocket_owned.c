/* Native owned Data, ready-work/HTTP-prefix handoff, ordinary message and
 * clean-close loops. No external traffic or security-specialist vectors. */
#define TEST_SINGLE_WS_BUFFERED 1
#define TEST_SINGLE_WS_OWNED 1
#include "test_single_websocket_stream.c"
