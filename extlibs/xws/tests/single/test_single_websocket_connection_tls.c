#define XWS_IMPLEMENTATION
#include "../../single/xws.h"



/*
	单头发布复用模块化 TLS 会话回归，确保两种发布形态保持同一组
	握手、短写 FIFO、大消息与关闭边界。
*/
#include "../websocket/test_connection_tls.c"
