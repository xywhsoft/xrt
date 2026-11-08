#define XRT_MODULE_JSON_READ
#include <xrt/features.h>
/* This public leaf header must supply its own xerror declaration. */
#include <xrt/json.h>

int main(void)
{
	xjsonreadconfig Config;
	xrtJsonReadConfigInit(&Config);
	const xerror* Error = xrtGetError();
	(void)Error;
	return Config.MaxDepth != 0 ? 0 : 1;
}
