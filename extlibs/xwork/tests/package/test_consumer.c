#define XWORK_MODULE_XWORK
#include <xwork.h>

int main(void)
{
    xwork_agent_config config; xworkAgentConfigInit(&config);
    return xrtVersion() == NULL;
}
