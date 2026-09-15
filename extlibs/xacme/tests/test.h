#ifndef XACME_TEST_H
#define XACME_TEST_H

#include "../../../tests/test.h"
#include <xacme.h>

#include <stdio.h>
#include <stdlib.h>

/*
	测试输出根目录：XACME_TEST_ROOT 覆盖，默认 ./xacme-test-out。
	返回借用指针（静态缓冲），落盘类断言的路径前缀统一走这里，
	不使用任何开发者本机绝对路径。
*/
#if defined(__GNUC__)
__attribute__((unused))
#endif
static const char* testOutRoot(void)
{
	static char sRoot[300];
	if(sRoot[0] == '\0')
	{
		const char* sEnv = getenv("XACME_TEST_ROOT");
		snprintf(sRoot, sizeof(sRoot), "%s",
			((sEnv != NULL) && (sEnv[0] != '\0')) ? sEnv :
				"xacme-test-out");
	}
	return sRoot;
}

#endif
