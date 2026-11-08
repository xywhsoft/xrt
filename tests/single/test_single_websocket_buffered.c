/* Ordinary local loopback: owned HTTP read-ahead crosses a frame header,
 * rejected attach retains input, then existing message/close checks run. */
#define TEST_SINGLE_WS_BUFFERED 1
#include "test_single_websocket_stream.c"
