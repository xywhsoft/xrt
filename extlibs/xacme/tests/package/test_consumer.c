/* 单头发布包消费探针：全量选择 + 实现宏。 */
#define XACME_MODULE_ALL
#define XACME_IMPLEMENTATION
#include "xacme.h"

int main(void)
{
	return (xrtAcmeDnsProviderCount() > 0) ? 0 : 1;
}
