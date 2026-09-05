/*
 * 范例：network/proxy_dial_http_connect —— 托管代理的 CONNECT 变体
 * ----------------------------------------------------------------
 * 演示 API：
 *   复用托管代理骨架，协议切为 HTTP CONNECT
 * 模块宏：XRT_MODULE_PROXY
 * 编译（单头形态，Windows）：
 *   gcc -O1 -DXRT_MODULE_ALL -I single -include xrt.h impl.c ${BS}
 *       examples/network/proxy_dial_http_connect/main.c -lws2_32 -liphlpapi
 * 用法：
 *   proxy_dial <proxy-host> <proxy-port> <target-host> <target-port>
 * 预期输出（无参数时）：
 *   usage: proxy_dial <proxy-host> ...
 *
 * 编译期开关 EXAMPLE_PROXY_HTTP_CONNECT 直接复用
 *   proxy_dial 的 main.c——同一骨架换协议实现，
 *   佐证代理层的协议抽象边界（测试 _iocp 变体同款手法）。
 */


/* 复用托管代理示例，只切换为 HTTP CONNECT 协议。 */
#define EXAMPLE_PROXY_HTTP_CONNECT 1
#include "../proxy_dial/main.c"
