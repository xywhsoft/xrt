#include <xmqtt/packet.h>
#include "../internal/xmqtt_internal.h"
#include <xrt/memory.h>
#include <string.h>

#if defined(XMQTT_FEATURE_XMQTT_PACKET)

static bool xmqttPacketArgument(cstr Message)
{
	xrtSetErrorInfo(XERR_ARGUMENT,"xrt.mqtt",XMQTT_CODEC_ARGUMENT,Message); return false;
}

XMQTT_API bool xmqttPacketInit(xmqttpackettype Type, xmqttpacket* Packet)
{
	if (Type<XMQTT_PACKET_CONNECT || Type>XMQTT_PACKET_AUTH || Packet==NULL || !xrtMemRangeValid(Packet,sizeof(*Packet)))
		return xmqttPacketArgument("invalid MQTT packet initializer");
	*Packet=(xmqttpacket){0}; Packet->Type=Type;
	if (Type==XMQTT_PACKET_PUBREL || Type==XMQTT_PACKET_SUBSCRIBE || Type==XMQTT_PACKET_UNSUBSCRIBE) Packet->Flags=2;
	return true;
}

XMQTT_API bool xmqttCodecConfigInit(xmqttcodecconfig* Config)
{
	if (Config==NULL || !xrtMemRangeValid(Config,sizeof(*Config))) return xmqttPacketArgument("invalid MQTT codec config output");
	*Config=(xmqttcodecconfig){sizeof(*Config),1024u*1024u,1024u*1024u,1024u*1024u,256,1024,0}; return true;
}

XMQTT_INTERNAL bool xmqttInternalCodecSettings(xmqttdirection Direction, const xmqttcodecconfig* Config, xmqttcodecconfig* Copy)
{
	if ((Direction!=XMQTT_CLIENT_TO_SERVER && Direction!=XMQTT_SERVER_TO_CLIENT) || (Config!=NULL && !xrtMemRangeValid(Config,sizeof(*Config))))
		return xmqttPacketArgument("invalid MQTT codec direction/config span");
	if (Config==NULL) xmqttCodecConfigInit(Copy); else *Copy=*Config;
	if (Copy->Size!=sizeof(*Copy) || Copy->MaxPacketBytes<2 || Copy->MaxPacketBytes>XMQTT_PACKET_MAX_BYTES ||
		Copy->MaxPayloadBytes>XMQTT_VARIABLE_MAX || Copy->MaxPropertyBytes>XMQTT_VARIABLE_MAX+4u || (Copy->Utf8Flags&~XMQTT_UTF8_REJECT_DISALLOWED))
		return xmqttPacketArgument("invalid MQTT codec resource configuration");
	return true;
}

XMQTT_INTERNAL bool xmqttInternalRole(xmqttpackettype Type, xmqttdirection Direction)
{
	if (Type<XMQTT_PACKET_CONNECT || Type>XMQTT_PACKET_AUTH) return false;
	if (Type==XMQTT_PACKET_CONNECT || Type==XMQTT_PACKET_SUBSCRIBE || Type==XMQTT_PACKET_UNSUBSCRIBE || Type==XMQTT_PACKET_PINGREQ)
		return Direction==XMQTT_CLIENT_TO_SERVER;
	if (Type==XMQTT_PACKET_CONNACK || Type==XMQTT_PACKET_SUBACK || Type==XMQTT_PACKET_UNSUBACK || Type==XMQTT_PACKET_PINGRESP)
		return Direction==XMQTT_SERVER_TO_CLIENT;
	return true;
}

XMQTT_INTERNAL bool xmqttInternalReason(xmqttversion Version, xmqttpackettype Type, uint8 Reason, xmqttdirection Direction)
{
	if (Version==XMQTT_VERSION_311) {
		if (Type==XMQTT_PACKET_CONNACK) return Reason<=5;
		if (Type==XMQTT_PACKET_SUBACK) return Reason<=2 || Reason==0x80;
		return Reason==0;
	}
	static const uint8 Connack[]={0,0x80,0x81,0x82,0x83,0x84,0x85,0x86,0x87,0x88,0x89,0x8a,0x8c,0x90,0x95,0x97,0x99,0x9a,0x9b,0x9c,0x9d,0x9f};
	static const uint8 Puback[]={0,0x10,0x80,0x83,0x87,0x90,0x91,0x97,0x99};
	static const uint8 Pubrel[]={0,0x92};
	static const uint8 Suback[]={0,1,2,0x80,0x83,0x87,0x8f,0x91,0x97,0x9e,0xa1,0xa2};
	static const uint8 Unsuback[]={0,0x11,0x80,0x83,0x87,0x8f,0x91};
	static const uint8 DisconnectBoth[]={0,0x80,0x81,0x82,0x83,0x90,0x93,0x94,0x95,0x96,0x97,0x98,0x99};
	static const uint8 DisconnectServer[]={0x87,0x89,0x8b,0x8d,0x8e,0x8f,0x9a,0x9b,0x9c,0x9d,0x9e,0x9f,0xa0,0xa1,0xa2};
	const uint8* Values=NULL; size_t Count=0;
	if (Type==XMQTT_PACKET_CONNACK) { Values=Connack; Count=sizeof(Connack); }
	else if (Type==XMQTT_PACKET_PUBACK || Type==XMQTT_PACKET_PUBREC) { Values=Puback; Count=sizeof(Puback); }
	else if (Type==XMQTT_PACKET_PUBREL || Type==XMQTT_PACKET_PUBCOMP) { Values=Pubrel; Count=sizeof(Pubrel); }
	else if (Type==XMQTT_PACKET_SUBACK) { Values=Suback; Count=sizeof(Suback); }
	else if (Type==XMQTT_PACKET_UNSUBACK) { Values=Unsuback; Count=sizeof(Unsuback); }
	else if (Type==XMQTT_PACKET_AUTH) return Reason==0x18 || (Reason==0 && Direction==XMQTT_SERVER_TO_CLIENT) || (Reason==0x19 && Direction==XMQTT_CLIENT_TO_SERVER);
	else if (Type==XMQTT_PACKET_DISCONNECT) {
		if (Reason==4 && Direction==XMQTT_CLIENT_TO_SERVER) return true;
		for (size_t Index=0; Index<sizeof(DisconnectBoth); ++Index) if (Reason==DisconnectBoth[Index]) return true;
		if (Direction==XMQTT_SERVER_TO_CLIENT)
			for (size_t Index=0; Index<sizeof(DisconnectServer); ++Index) if (Reason==DisconnectServer[Index]) return true;
		return false;
	} else return Reason==0;
	for (size_t Index=0; Index<Count; ++Index) if (Reason==Values[Index]) return true;
	return false;
}

XMQTT_INTERNAL bool xmqttInternalTopic(xstrview Text, bool Filter, bool Shared, bool* IsShared)
{
	size_t Begin=0;
	if (IsShared!=NULL) *IsShared=false;
	if (!Text.Size) return false;
	if (Filter && Shared && Text.Size>=7 && memcmp(Text.Data,"$share/",7)==0) {
		size_t Slash=7;
		while (Slash<Text.Size && Text.Data[Slash]!='/') {
			if (Text.Data[Slash]=='+' || Text.Data[Slash]=='#') return false;
			++Slash;
		}
		if (Slash==7 || Slash==Text.Size || Slash+1u==Text.Size) return false;
		Begin=Slash+1u; if (IsShared!=NULL) *IsShared=true;
	}
	for (size_t Index=Begin; Index<Text.Size; ++Index) {
		if (Text.Data[Index]=='+') {
			if (!Filter || (Index!=Begin && Text.Data[Index-1u]!='/') || (Index+1u<Text.Size && Text.Data[Index+1u]!='/')) return false;
		} else if (Text.Data[Index]=='#') {
			if (!Filter || Index+1u!=Text.Size || (Index!=Begin && Text.Data[Index-1u]!='/')) return false;
		}
	}
	return true;
}
#endif
