#define XRT_IMPLEMENTATION
#include "../../single/xrt.h"



/*
	单头发布复用完整 WSS Upgrade 回归，验证 TLS、HTTP/1.1、压缩协商、
	前缀交接和 WebSocket 流在同一裁剪组合下保持一致。
*/
#include "../websocket/test_upgrade_handoff_tls.c"
