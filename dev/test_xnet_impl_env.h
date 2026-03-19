#ifndef XRT_TEST_XNET_IMPL_ENV_DEV_H
#define XRT_TEST_XNET_IMPL_ENV_DEV_H

/*
 * 测试实现环境：
 * - 直接把当前源码版 xrt.c 拉进测试 TU
 * - 让 test/ 下依赖内部 xnet/xhttp 实现细节的测试可以和当前实现同编译
 * - 避免单头文件滞后于源码主线
 */

#include "../test/test_xnet_impl_env.h"

#endif
