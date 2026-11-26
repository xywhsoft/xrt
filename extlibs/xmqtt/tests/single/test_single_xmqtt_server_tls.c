#define XMQTT_MODULE_XMQTT_SERVER_TLS
#define XMQTT_MODULE_XMQTT_CODEC311
#define XRT_MODULE_TLS_IDENTITY_RSA
#include <xmqtt/features.h>
#define XRT_IMPLEMENTATION
#include "../../../../single/xrt.h"
#define XMQTT_IMPLEMENTATION
#include "../../../../single/extlibs/xmqtt.h"
#define main xmqttTlsExampleMain
#include "../../examples/server/tls.c"
#undef main

int main(int Argc, char** Argv)
{
	if (Argc == 1) {
		unsigned char Wire[17] = {0};
		xmqttserveroutput Output = {.Limit = sizeof(xmqttserverwrite) + sizeof(Wire) - 1};
		xmqttserverpeer Peer = {.Output = &Output};
		xnetref Part = {.Data = Wire, .Size = sizeof(Wire)};
		if (!xmqttServerOutputFits(&Peer, sizeof(Wire) - 1) ||
			xmqttServerOutputFits(&Peer, sizeof(Wire)) ||
			xmqttServerQueueOutput(&Peer, &Part, 1, false) != XNET_RESULT_ERROR) {
			fputs("MQTT output admission must include the write node\n", stderr);
			return 1;
		}
	}
	return xmqttTlsExampleMain(Argc, Argv);
}
