#define XMQTT_MODULE_XMQTT_SERVER_AUTH
#include <xmqtt/features.h>
#if defined(XMQTT_FEATURE_XMQTT_CLIENT) || defined(XMQTT_FEATURE_XMQTT_SERVER_TLS) || defined(XMQTT_FEATURE_XMQTT_SERVER_WS)
#error "enhanced auth must select only the standalone broker closure"
#endif
#define XRT_IMPLEMENTATION
#include "../../../../single/xrt.h"
#define XMQTT_IMPLEMENTATION
#include "../../../../single/extlibs/xmqtt.h"
#include "../test.h"

int main(void)
{
	xmqttserver Server = {0};
	testRequire(xmqttServerConfigInit(&Server.Config) && xmqttServerAuthConfigInit(&Server.Auth),
		"broker auth defaults");
	xmqttserverauthstate Auth = {.Method = XRT_STR_INIT("method")};
	xmqttserverpeer Peer = {.Server = &Server, .Auth = &Auth, .PacketLimit = 4096, .Version = XMQTT_VERSION_5};
	unsigned char Proof[256] = {0};
	xbytesview Reply = {Proof, sizeof(Proof)};

	xallocationbudget* Budget;
	xallocationscope Scope = {0};
	testRequire(xrtAllocationBudgetCreate(1, &Budget) && xrtAllocationBudgetEnter(Budget, &Scope),
		"start allocation failpoint");
	testRequire(xmqttServerAuthReply(&Peer, true, Reply) == 0x97 && !Auth.Connack && !Peer.Session,
		"CONNACK allocation failure leaves session unpublished");
	testRequire(xrtAllocationBudgetLeave(&Scope), "leave allocation failpoint");
	xallocationbudgetinfo Memory = {.Size = sizeof(Memory)};
	testRequire(xrtAllocationBudgetInfo(Budget, &Memory) && Memory.Rejections && !Memory.LiveBytes,
		"failed final proof leaves no allocation");
	xrtAllocationBudgetFree(Budget);
	xrtClearError();

	Auth.Bytes = 0;
	Peer.PacketLimit = 64;
	testRequire(xmqttServerAuthReply(&Peer, true, Reply) == 0x95 && !Auth.Bytes && !Auth.Connack,
		"broker obeys peer maximum packet size before allocation");
	Peer.PacketLimit = 4096;
	Server.Auth.MaxExchangeBytes = 64;
	testRequire(xmqttServerAuthReply(&Peer, true, Reply) == 0x97 && !Auth.Bytes && !Auth.Connack,
		"broker charges final proof against aggregate exchange quota");
	Server.Auth.MaxExchangeBytes = 4096;
	testRequire(xmqttServerAuthReply(&Peer, true, Reply) == 0 &&
		Auth.Connack && Auth.SessionPresentOffset == 3 && Auth.Bytes == Auth.ConnackBytes && !Peer.Session,
		"final proof pre-encodes two-byte remaining length without session admission");
	xmqttServerAuthFreeConnack(&Auth);
	testRequire(!Auth.Connack, "encoded proof retired");
	Auth.Bytes = 0;

	Auth.Method = XRT_STR_LITERAL("");
	testRequire(xmqttServerAuthReply(&Peer, true, (xbytesview){Proof, 0}) == 0,
		"empty authentication method and explicitly empty proof");
	xmqttproperty Properties[8];
	xmqttsubscription Filters[1];
	const xmqttcodecbuffers Scratch = {Properties, 8, Filters, 1};
	xmqttpacket Packet;
	testRequire(xmqttPacketRead5((xbytesview){Auth.Connack, Auth.ConnackBytes}, XMQTT_SERVER_TO_CLIENT,
		NULL, &Scratch, &Packet, NULL) == XMQTT_CODEC_DONE && Packet.Properties.Count == 8 &&
		Properties[6].Id == XMQTT_PROPERTY_AUTH_METHOD && !Properties[6].Value.Text.Size &&
		Properties[7].Id == XMQTT_PROPERTY_AUTH_DATA && !Properties[7].Value.Binary.Size,
		"CONNACK preserves empty properties");
	xmqttServerAuthFreeConnack(&Auth);
	puts("[PASS] broker auth allocation rollback, bounds and empty proof");
	return 0;
}
