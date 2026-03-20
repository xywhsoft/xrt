#ifndef XRT_TEST_XNET_INTERNAL_ENV_H
#define XRT_TEST_XNET_INTERNAL_ENV_H

/*
 * xnet/xhttp 内部实现测试环境：
 * - 直接把当前源码版 xrt.c 拉进测试 TU
 * - 让依赖内部实现细节的网络测试与当前实现同编译
 * - 避免单头文件接口滞后于源码主线
 */

#include "../xrt.c"

#endif
