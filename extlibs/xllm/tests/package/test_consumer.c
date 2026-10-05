#define XLLM_MODULE_XLLM
#include <xllm.h>

int main(void)
{
    xllm_client_config config; xllmClientConfigInit(&config);
    return xrtVersion() == NULL;
}
