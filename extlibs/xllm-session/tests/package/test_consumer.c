#define XLLM_SESSION_MODULE_XLLM_SESSION
#include <xllm-session.h>

int main(void)
{
    xllm_session_config config; xllmSessionConfigInit(&config);
    return xrtVersion() == NULL;
}
