#include "test.h"
#if !defined(XMQTT_SINGLE_HEADER)
#include "../src/internal/xmqtt_internal.h"
#endif

static void testPacketGrammar(void)
{
	static const struct { cstr Text; bool Filter, Shared; bool Valid, IsShared; } Cases[]={
		{"",true,true,false,false}, {"/",true,true,true,false}, {"a//b",true,true,true,false},
		{"a/+/#",true,true,true,false}, {"#",true,true,true,false}, {"+",true,true,true,false},
		{"a+",true,true,false,false}, {"+a",true,true,false,false}, {"a/#/b",true,true,false,false}, {"##",true,true,false,false},
		{"$share/g/a/+",true,true,true,true}, {"$share//a",true,true,false,false}, {"$share/g/",true,true,false,false},
		{"$share/+/a",true,true,false,false}, {"$share/g#/a",true,true,false,false}, {"$share/g//",true,true,true,true},
		{"$share/g/$share/h/+",true,true,true,true}, {"$share",true,true,true,false},
		{"$share/g/",true,false,true,false}, {"$share//a",true,false,true,false},
		{"a/+",false,true,false,false}, {"a/#",false,true,false,false}, {"/x",false,true,true,false},
		{"$share/g/a",false,true,true,false}
	};
	for (size_t Index=0; Index<sizeof(Cases)/sizeof(Cases[0]); ++Index) {
		bool Shared=false; const cstr Text=Cases[Index].Text;
		bool Valid=xmqttInternalTopic((xstrview){Text,strlen(Text)},Cases[Index].Filter,Cases[Index].Shared,&Shared);
		testRequire(Valid==Cases[Index].Valid && (!Valid || Shared==Cases[Index].IsShared),"topic level/shared-name grammar");
	}
	/* The helper handles only already-validated string grammar. Version/role
	 * reason acceptance is verified with independent packet bytes in codec5. */
	for (unsigned Type=1; Type<=15; ++Type) {
		xmqttpacket Packet; memset(&Packet,0xa5,sizeof(Packet));
		testRequire(xmqttPacketInit((xmqttpackettype)Type,&Packet) && Packet.Type==(xmqttpackettype)Type && Packet.Identifier==0 &&
			Packet.Properties.Count==0 && Packet.Wire.Data==NULL && Packet.Flags==((Type==6 || Type==8 || Type==10) ? 2 : 0),"packet representation initialization");
		xmqttpacket Before=Packet; testRequire(!xmqttPacketInit((xmqttpackettype)0,&Packet) && memcmp(&Before,&Packet,sizeof(Packet))==0,"invalid initializer modified packet"); xrtClearError();
	}
	testRequire(!xmqttPacketInit(XMQTT_PACKET_PINGREQ,NULL) && !xmqttCodecConfigInit(NULL),"NULL packet/config outputs"); xrtClearError();
	xmqttcodecconfig Config; testRequire(xmqttCodecConfigInit(&Config) && Config.Size==sizeof(Config) && Config.MaxPacketBytes==1048576u &&
		Config.MaxPayloadBytes==1048576u && Config.MaxPropertyBytes==1048576u && Config.MaxProperties==256 && Config.MaxTopics==1024,"documented config resource defaults");
}

int main(void)
{
	testPacketGrammar(); printf("[PASS] xmqtt packet: topic/filter/shared grammar and representation contracts\n"); return 0;
}
