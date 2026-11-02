#include "test.h"

typedef struct legacyFixture { unsigned char Data[64]; size_t Size; xmqttdirection Direction; } legacyFixture;
static const legacyFixture legacyFixtures[]={
	{{0x10,13,0,4,'M','Q','T','T',4,0,0,30,0,1,'c'},15,XMQTT_CLIENT_TO_SERVER},
	{{0x10,12,0,4,'M','Q','T','T',4,2,0,0,0,0},14,XMQTT_CLIENT_TO_SERVER},
	{{0x10,27,0,4,'M','Q','T','T',4,0xf6,1,2,0,1,'c',0,1,'w',0,2,0,255,0,1,'u',0,2,255,0},29,XMQTT_CLIENT_TO_SERVER},
	{{0x20,2,0,0},4,XMQTT_SERVER_TO_CLIENT}, {{0x20,2,1,0},4,XMQTT_SERVER_TO_CLIENT}, {{0x20,2,0,5},4,XMQTT_SERVER_TO_CLIENT},
	{{0x30,5,0,1,'t',0,255},7,XMQTT_CLIENT_TO_SERVER},
	{{0x3b,7,0,1,'t',0,42,0,255},9,XMQTT_SERVER_TO_CLIENT},
	{{0x34,7,0,1,'t',255,255,255,0},9,XMQTT_CLIENT_TO_SERVER},
	{{0x40,2,0,42},4,XMQTT_CLIENT_TO_SERVER}, {{0x50,2,0,42},4,XMQTT_SERVER_TO_CLIENT},
	{{0x62,2,255,255},4,XMQTT_CLIENT_TO_SERVER}, {{0x70,2,0,42},4,XMQTT_SERVER_TO_CLIENT},
	{{0x82,6,0,42,0,1,'t',2},8,XMQTT_CLIENT_TO_SERVER},
	{{0x90,6,0,42,0,1,2,128},8,XMQTT_SERVER_TO_CLIENT},
	{{0xa2,5,0,42,0,1,'t'},7,XMQTT_CLIENT_TO_SERVER},
	{{0xb0,2,0,42},4,XMQTT_SERVER_TO_CLIENT},
	{{0xc0,0},2,XMQTT_CLIENT_TO_SERVER}, {{0xd0,0},2,XMQTT_SERVER_TO_CLIENT}, {{0xe0,0},2,XMQTT_CLIENT_TO_SERVER},
	/* '$share/' has no special shared-group grammar in standard 3.1.1. */
	{{0x82,14,0,42,0,9,'$','s','h','a','r','e','/','/','t',1},16,XMQTT_CLIENT_TO_SERVER},
	{{0x82,10,0,42,0,1,'a',0,0,1,'b',2},12,XMQTT_CLIENT_TO_SERVER}
};

static void legacyWithin(xbytesview Whole, const void* Data, size_t Size)
{
	if (!Data && !Size) return;
	uintptr_t Begin=(uintptr_t)Whole.Data, Part=(uintptr_t)Data;
	testRequire(Part>=Begin && Part-Begin<=Whole.Size && Size<=Whole.Size-(Part-Begin),"3.1.1 borrowed view escaped packet/scratch");
}

static void legacyPacketWithin(const xmqttpacket* Packet, const xmqttcodecbuffers* Buffers)
{
	xbytesview Wire=Packet->Wire; testRequire(Packet->Properties.Count==0 && Packet->Properties.Data==NULL,"3.1.1 synthesized properties");
	if (Packet->Type==XMQTT_PACKET_CONNECT) {
		const xmqttconnectpacket* Value=&Packet->Body.Connect; legacyWithin(Wire,Value->ClientId.Data,Value->ClientId.Size);
		testRequire(Value->WillProperties.Count==0 && Value->WillProperties.Data==NULL,"3.1.1 synthesized Will properties");
		if (Value->HasWill) { legacyWithin(Wire,Value->WillTopic.Data,Value->WillTopic.Size); legacyWithin(Wire,Value->WillPayload.Data,Value->WillPayload.Size); }
		if (Value->HasUsername) legacyWithin(Wire,Value->Username.Data,Value->Username.Size);
		if (Value->HasPassword) legacyWithin(Wire,Value->Password.Data,Value->Password.Size);
	} else if (Packet->Type==XMQTT_PACKET_PUBLISH) {
		legacyWithin(Wire,Packet->Body.Publish.Topic.Data,Packet->Body.Publish.Topic.Size); legacyWithin(Wire,Packet->Body.Publish.Payload.Data,Packet->Body.Publish.Payload.Size);
	} else if (Packet->Type==XMQTT_PACKET_SUBSCRIBE || Packet->Type==XMQTT_PACKET_UNSUBSCRIBE) {
		legacyWithin((xbytesview){(cbytes)Buffers->Topics,Buffers->TopicCapacity*sizeof(xmqttsubscription)},Packet->Body.Topics.Data,Packet->Body.Topics.Count*sizeof(xmqttsubscription));
		for (size_t Index=0; Index<Packet->Body.Topics.Count; ++Index) legacyWithin(Wire,Packet->Body.Topics.Data[Index].Filter.Data,Packet->Body.Topics.Data[Index].Filter.Size);
	} else if (Packet->Type==XMQTT_PACKET_SUBACK) legacyWithin(Wire,Packet->Body.Reasons.Data,Packet->Body.Reasons.Size);
	else if (Packet->Type==XMQTT_PACKET_UNSUBACK) testRequire(Packet->Body.Reasons.Size==0 && Packet->Body.Reasons.Data==NULL,"3.1.1 UNSUBACK reason list");
}

static void legacyWriteFailure(const xmqttpacket* Packet, xmqttdirection Direction, const xmqttcodecconfig* Config)
{
	unsigned char Output[512], Before[512]; memset(Output,0xa5,sizeof(Output)); memcpy(Before,Output,sizeof(Output)); size_t Size=777, Written=888;
	testRequire(!xmqttPacketSize311(Packet,Direction,Config,&Size) && Size==777,"failed 3.1.1 Size published"); xrtClearError();
	testRequire(!xmqttPacketWrite311(Output,sizeof(Output),Packet,Direction,Config,&Written) && Written==888 && !memcmp(Output,Before,sizeof(Output)),"failed 3.1.1 Write changed bytes/size"); xrtClearError();
}

static xmqttcodecissue legacyReadFailure(xbytesview Input, xmqttdirection Direction, const xmqttcodecconfig* Config, xmqttcodecstatus Expected)
{
	xmqttsubscription Topics[8]; xmqttcodecbuffers Buffers={NULL,0,Topics,8}; xmqttpacket Packet, Before;
	memset(&Packet,0xa5,sizeof(Packet)); memcpy(&Before,&Packet,sizeof(Packet)); xmqttcodecissue Issue={XMQTT_CODEC_DONE,XMQTT_CODEC_OK,777,0,0,0};
	xmqttcodecstatus Status=xmqttPacketRead311(Input,Direction,Config,&Buffers,&Packet,&Issue);
	if (Status!=Expected) fprintf(stderr,"3.1.1 got=%d expected=%d type=%u bytes=%zu\n",(int)Status,(int)Expected,Input.Size ? Input.Data[0]>>4 : 0,Input.Size);
	testRequire(Status==Expected && !memcmp(&Packet,&Before,sizeof(Packet)),"3.1.1 failure classification/publication");
	if (Expected<0) testRequire(Issue.Status==Expected && Issue.Offset<=Input.Size,"3.1.1 typed issue/offset");
	xrtClearError(); return Issue;
}

static void testLegacyFixtures(void)
{
	for (size_t Case=0; Case<sizeof(legacyFixtures)/sizeof(legacyFixtures[0]); ++Case) for (unsigned Width=1; Width<=4; ++Width) {
		const legacyFixture* Fixture=&legacyFixtures[Case]; unsigned char Wire[128], Output[128]; Wire[0]=Fixture->Data[0];
		Wire[1]=(unsigned char)(Fixture->Data[1]|(Width>1 ? 128u : 0u));
		for (unsigned Byte=1; Byte<Width; ++Byte) Wire[1+Byte]=Byte+1u==Width ? 0 : 128;
		memcpy(Wire+1u+Width,Fixture->Data+2,Fixture->Size-2u); size_t Size=Fixture->Size+Width-1u; Wire[Size]=255;
		xmqttsubscription Topics[8], BeforeTopics[8]; xmqttcodecbuffers Buffers={NULL,0,Topics,8}; xmqttpacket Packet, Before;
		for (size_t Prefix=0; Prefix<Size; ++Prefix) {
			memset(&Packet,0xa5,sizeof(Packet)); memcpy(&Before,&Packet,sizeof(Packet)); memset(Topics,0xa5,sizeof(Topics)); memcpy(BeforeTopics,Topics,sizeof(Topics));
			xmqttcodecissue Issue={XMQTT_CODEC_DONE,XMQTT_CODEC_OK,777,0,0,0}, BeforeIssue; memcpy(&BeforeIssue,&Issue,sizeof(Issue));
			testRequire(xmqttPacketRead311((xbytesview){Wire,Prefix},Fixture->Direction,NULL,&Buffers,&Packet,&Issue)==XMQTT_CODEC_MORE &&
				!memcmp(&Packet,&Before,sizeof(Packet)) && !memcmp(&Issue,&BeforeIssue,sizeof(Issue)) && !memcmp(Topics,BeforeTopics,sizeof(Topics)),"3.1.1 valid prefix changed outputs");
		}
		xmqttcodecissue Issue; size_t Required, Written;
		testRequire(xmqttPacketRead311((xbytesview){Wire,Size+1u},Fixture->Direction,NULL,&Buffers,&Packet,&Issue)==XMQTT_CODEC_DONE &&
			Packet.Type==(xmqttpackettype)(Wire[0]>>4) && Packet.Flags==(Wire[0]&15u) && Packet.Wire.Data==Wire && Packet.Wire.Size==Size &&
			Issue.Status==XMQTT_CODEC_DONE && Issue.Error==XMQTT_CODEC_OK && Issue.Offset==XRT_NPOS,"3.1.1 literal/header-width/coalescing");
		legacyPacketWithin(&Packet,&Buffers);
		testRequire(xmqttPacketSize311(&Packet,Fixture->Direction,NULL,&Required) && Required==Fixture->Size &&
			xmqttPacketWrite311(Output,sizeof(Output),&Packet,Fixture->Direction,NULL,&Written) && Written==Required && !memcmp(Output,Fixture->Data,Written),"3.1.1 canonical literal encoding");
		for (size_t Capacity=0; Capacity<Required; ++Capacity) {
			memset(Output,0xa5,sizeof(Output)); Written=888;
			testRequire(!xmqttPacketWrite311(Output,Capacity,&Packet,Fixture->Direction,NULL,&Written) && Written==888,"3.1.1 short write publication");
			for (size_t Index=0; Index<sizeof(Output); ++Index) testRequire(Output[Index]==0xa5,"3.1.1 short write modified bytes");
			xrtClearError();
		}
		if (Case==2) testRequire(Packet.Body.Connect.CleanStart && Packet.Body.Connect.HasWill && Packet.Body.Connect.WillQos==2 && Packet.Body.Connect.WillRetain &&
			Packet.Body.Connect.KeepAlive==258 && Packet.Body.Connect.WillPayload.Size==2 && Packet.Body.Connect.WillPayload.Data[1]==255 &&
			Packet.Body.Connect.HasUsername && Packet.Body.Connect.HasPassword && Packet.Body.Connect.Password.Data[0]==255,"3.1.1 complete CONNECT optionals");
		xmqttcodecconfig Config; xmqttCodecConfigInit(&Config); Config.MaxPacketBytes=Size; Config.MaxPropertyBytes=0; Config.MaxProperties=0;
		testRequire(xmqttPacketRead311((xbytesview){Wire,Size},Fixture->Direction,&Config,&Buffers,&Packet,NULL)==XMQTT_CODEC_DONE,"3.1.1 exact wire cap/zero property budgets");
		if (Size>2) { Config.MaxPacketBytes=Size-1u; legacyReadFailure((xbytesview){Wire,1u+Width},Fixture->Direction,&Config,XMQTT_CODEC_LOCAL_ERROR); }
	}
}

static void testLegacyRolesAndBoundaries(void)
{
	for (size_t Case=0; Case<sizeof(legacyFixtures)/sizeof(legacyFixtures[0]); ++Case) {
		const legacyFixture* Fixture=&legacyFixtures[Case]; unsigned Type=Fixture->Data[0]>>4;
		xmqttsubscription Topics[8]; xmqttcodecbuffers Buffers={NULL,0,Topics,8}; xmqttpacket Packet;
		for (unsigned Direction=1; Direction<=2; ++Direction) {
			bool Valid=(Type==1 || Type==8 || Type==10 || Type==12 || Type==14) ? Direction==1 : (Type==2 || Type==9 || Type==11 || Type==13) ? Direction==2 : true;
			testRequire(xmqttPacketRead311((xbytesview){Fixture->Data,Fixture->Size},(xmqttdirection)Direction,NULL,&Buffers,&Packet,NULL)==
				(Valid ? XMQTT_CODEC_DONE : XMQTT_CODEC_PROTOCOL_ERROR),"3.1.1 all-type role matrix"); xrtClearError();
		}
		for (unsigned Cut=0; Cut<Fixture->Data[1]; ++Cut) {
			unsigned char Wire[64], Output[64]; memcpy(Wire,Fixture->Data,Fixture->Size); Wire[1]=(unsigned char)Cut;
			xmqttcodecstatus Status=xmqttPacketRead311((xbytesview){Wire,Fixture->Size},Fixture->Direction,NULL,&Buffers,&Packet,NULL);
			testRequire(Status!=XMQTT_CODEC_MORE,"3.1.1 borrowed suffix to finish in-packet field");
			if (Status==XMQTT_CODEC_DONE) { size_t Written; testRequire(Packet.Wire.Size==Cut+2u,"3.1.1 truncated packet consumed suffix"); legacyPacketWithin(&Packet,&Buffers);
				testRequire(xmqttPacketWrite311(Output,sizeof(Output),&Packet,Fixture->Direction,NULL,&Written) && Written<=Packet.Wire.Size,"3.1.1 accepted short packet cannot encode"); }
			xrtClearError();
		}
	}
	/* Reject known impossible fixed body lengths before buffering any body. */
	const unsigned Types[]={2,4,5,6,7,11,12,13,14};
	for (size_t Index=0; Index<sizeof(Types)/sizeof(Types[0]); ++Index) for (unsigned Length=0; Length<=255; ++Length) {
		unsigned Type=Types[Index]; unsigned char Header[]={(unsigned char)((Type<<4)|(Type==6 ? 2u : 0u)),(unsigned char)((Length&127u)|(Length>=128 ? 128u : 0u)),1};
		xmqttdirection Direction=(Type==2 || Type==11 || Type==13) ? XMQTT_SERVER_TO_CLIENT : XMQTT_CLIENT_TO_SERVER;
		bool Empty=Type>=12; bool Valid=Length==(Empty ? 0u : 2u); xmqttpacket Packet;
		xmqttcodecstatus Expected=!Valid ? XMQTT_CODEC_MALFORMED : Empty ? XMQTT_CODEC_DONE : XMQTT_CODEC_MORE;
		testRequire(xmqttPacketRead311((xbytesview){Header,Length>=128 ? 3u : 2u},Direction,NULL,NULL,&Packet,NULL)==Expected,"3.1.1 fixed-body-length matrix"); xrtClearError();
	}
}

static void testLegacyFlagsAndCodes(void)
{
	for (unsigned Flags=0; Flags<=255; ++Flags) {
		unsigned char Wire[64]={0x10,0,0,4,'M','Q','T','T',4,(unsigned char)Flags,0,0,0,1,'c'}; size_t Size=15;
		bool Valid=!(Flags&1u) && (Flags&0x18u)!=0x18u && ((Flags&4u) || !(Flags&0x38u)) && (Flags&0xc0u)!=0x40u;
		if (Flags&4u) { Wire[Size++]=0; Wire[Size++]=1; Wire[Size++]='w'; Wire[Size++]=0; Wire[Size++]=1; Wire[Size++]=255; }
		if (Flags&128u) { Wire[Size++]=0; Wire[Size++]=0; }
		if (Flags&64u) { Wire[Size++]=0; Wire[Size++]=0; }
		Wire[1]=(unsigned char)(Size-2u); xmqttpacket Packet;
		testRequire(xmqttPacketRead311((xbytesview){Wire,Size},XMQTT_CLIENT_TO_SERVER,NULL,NULL,&Packet,NULL)==(Valid ? XMQTT_CODEC_DONE : XMQTT_CODEC_MALFORMED),"3.1.1 CONNECT flags/password-user dependency");
		if (Valid) { unsigned char Output[64]; size_t Written; testRequire(xmqttPacketWrite311(Output,sizeof(Output),&Packet,XMQTT_CLIENT_TO_SERVER,NULL,&Written) && Written==Size && !memcmp(Wire,Output,Size),"3.1.1 CONNECT flags encoding"); }
		xrtClearError();
	}
	for (unsigned Value=0; Value<=255; ++Value) {
		unsigned char Connack[]={0x20,2,0,(unsigned char)Value}, Suback[]={0x90,3,0,42,(unsigned char)Value}, Subscribe[]={0x82,6,0,42,0,1,'t',(unsigned char)Value};
		xmqttpacket Packet; xmqttsubscription Topic; xmqttcodecbuffers Buffers={NULL,0,&Topic,1};
		testRequire(xmqttPacketRead311((xbytesview){Connack,sizeof(Connack)},XMQTT_SERVER_TO_CLIENT,NULL,NULL,&Packet,NULL)==(Value<=5 ? XMQTT_CODEC_DONE : XMQTT_CODEC_PROTOCOL_ERROR),"3.1.1 CONNACK return-byte domain"); xrtClearError();
		testRequire(xmqttPacketRead311((xbytesview){Suback,sizeof(Suback)},XMQTT_SERVER_TO_CLIENT,NULL,NULL,&Packet,NULL)==(Value<=2 || Value==128 ? XMQTT_CODEC_DONE : XMQTT_CODEC_PROTOCOL_ERROR),"3.1.1 SUBACK return-byte domain"); xrtClearError();
		testRequire(xmqttPacketRead311((xbytesview){Subscribe,sizeof(Subscribe)},XMQTT_CLIENT_TO_SERVER,NULL,&Buffers,&Packet,NULL)==(Value<=2 ? XMQTT_CODEC_DONE : XMQTT_CODEC_MALFORMED),"3.1.1 subscription byte/reserved bits"); xrtClearError();
		for (unsigned Flag=0; Flag<=255; ++Flag) {
			Connack[2]=(unsigned char)Flag;
			xmqttcodecstatus Expected=Flag>1 ? XMQTT_CODEC_MALFORMED : Value>5 || (Flag && Value) ? XMQTT_CODEC_PROTOCOL_ERROR : XMQTT_CODEC_DONE;
			testRequire(xmqttPacketRead311((xbytesview){Connack,sizeof(Connack)},XMQTT_SERVER_TO_CLIENT,NULL,NULL,&Packet,NULL)==Expected,"3.1.1 all CONNACK flag/code combinations"); xrtClearError();
		}
	}
}

static void testLegacyInvalid(void)
{
	static const struct { unsigned char Data[32]; size_t Size; xmqttdirection Direction; xmqttcodecstatus Status; } Cases[]={
		{{0xf0,0},2,1,XMQTT_CODEC_MALFORMED}, {{0xe0,0},2,2,XMQTT_CODEC_PROTOCOL_ERROR},
		{{0x40,3,0,42,0},5,1,XMQTT_CODEC_MALFORMED}, {{0xb0,3,0,42,0},5,2,XMQTT_CODEC_MALFORMED},
		{{0x20,3,0,0,0},5,2,XMQTT_CODEC_MALFORMED}, {{0xe0,1,0},3,1,XMQTT_CODEC_MALFORMED},
		{{0x40,2,0,0},4,1,XMQTT_CODEC_PROTOCOL_ERROR}, {{0x32,5,0,1,'t',0,0},7,1,XMQTT_CODEC_PROTOCOL_ERROR},
		{{0x30,2,0,0},4,1,XMQTT_CODEC_PROTOCOL_ERROR}, {{0x30,3,0,1,'+'},5,1,XMQTT_CODEC_PROTOCOL_ERROR},
		{{0x30,3,0,1,0},5,1,XMQTT_CODEC_MALFORMED}, {{0x30,3,0,1,255},5,1,XMQTT_CODEC_MALFORMED},
		{{0x38,0},2,1,XMQTT_CODEC_MALFORMED}, {{0x82,2,0,42},4,1,XMQTT_CODEC_PROTOCOL_ERROR},
		{{0xa2,2,0,42},4,1,XMQTT_CODEC_PROTOCOL_ERROR}, {{0x90,2,0,42},4,2,XMQTT_CODEC_PROTOCOL_ERROR},
		{{0x82,5,0,42,0,0,0},7,1,XMQTT_CODEC_PROTOCOL_ERROR}, {{0x82,5,0,42,0,1,'t'},7,1,XMQTT_CODEC_MALFORMED},
		{{0xc0,0x80,0x80,0x80,0x80,0},6,1,XMQTT_CODEC_MALFORMED},
		{{0x10,12,0,4,'M','Q','T','T',4,0,0,0,0,0},14,1,XMQTT_CODEC_PROTOCOL_ERROR},
		{{0x10,13,0,4,'M','Q','T','T',5,2,0,0,0,1,'c'},15,1,XMQTT_CODEC_UNSUPPORTED}
	};
	for (size_t Index=0; Index<sizeof(Cases)/sizeof(Cases[0]); ++Index) {
		xmqttcodecissue Issue=legacyReadFailure((xbytesview){Cases[Index].Data,Cases[Index].Size},Cases[Index].Direction,NULL,Cases[Index].Status);
		if (Index==19) testRequire(Issue.Error==XMQTT_CODEC_CLIENT_ID && Issue.Offset==12,"3.1.1 empty persistent ClientId context");
	}
	xmqttpacket Packet; xmqttPacketInit(XMQTT_PACKET_CONNECT,&Packet); legacyWriteFailure(&Packet,XMQTT_CLIENT_TO_SERVER,NULL);
	Packet.Body.Connect.CleanStart=true; Packet.Body.Connect.HasPassword=true; legacyWriteFailure(&Packet,XMQTT_CLIENT_TO_SERVER,NULL);
	Packet.Body.Connect.HasPassword=false; Packet.Body.Connect.WillProperties.Count=1; legacyWriteFailure(&Packet,XMQTT_CLIENT_TO_SERVER,NULL);
	xmqttPacketInit(XMQTT_PACKET_UNSUBACK,&Packet); Packet.Identifier=42; Packet.Body.Reasons=(xbytesview){(cbytes)"x",1}; legacyWriteFailure(&Packet,XMQTT_SERVER_TO_CLIENT,NULL);
	xmqttPacketInit(XMQTT_PACKET_PUBACK,&Packet); Packet.Identifier=42; Packet.Reason=0x80; legacyWriteFailure(&Packet,XMQTT_CLIENT_TO_SERVER,NULL);
	xmqttPacketInit(XMQTT_PACKET_AUTH,&Packet); legacyWriteFailure(&Packet,XMQTT_CLIENT_TO_SERVER,NULL);
	xmqttPacketInit(XMQTT_PACKET_DISCONNECT,&Packet); legacyWriteFailure(&Packet,XMQTT_SERVER_TO_CLIENT,NULL); Packet.Reason=4; legacyWriteFailure(&Packet,XMQTT_CLIENT_TO_SERVER,NULL);
	xmqttPacketInit(XMQTT_PACKET_PUBLISH,&Packet); Packet.Body.Publish.Topic=XRT_STR_LITERAL("t"); Packet.Properties.Count=1; legacyWriteFailure(&Packet,XMQTT_CLIENT_TO_SERVER,NULL);
	Packet.Properties.Count=0; Packet.Identifier=42; legacyWriteFailure(&Packet,XMQTT_CLIENT_TO_SERVER,NULL);
	xmqttPacketInit(XMQTT_PACKET_SUBSCRIBE,&Packet); Packet.Identifier=42; xmqttsubscription Topics[]={{XRT_STR_LITERAL("a/+"),0},{XRT_STR_LITERAL("b"),4}};
	Packet.Body.Topics=(xmqttsubscriptionlist){Topics,2}; legacyWriteFailure(&Packet,XMQTT_CLIENT_TO_SERVER,NULL);
	/* A v5-looking byte at the start of a 3.1.1 payload is still opaque data. */
	unsigned char Data[]={0x30,8,0,1,'t',0,1,1,0,255};
	testRequire(xmqttPacketRead311((xbytesview){Data,sizeof(Data)},XMQTT_CLIENT_TO_SERVER,NULL,NULL,&Packet,NULL)==XMQTT_CODEC_DONE &&
		Packet.Body.Publish.Payload.Size==5 && Packet.Body.Publish.Payload.Data[4]==255,"3.1.1 guessed a payload protocol/property section");
}

static void testLegacyLimits(void)
{
	xmqttcodecconfig Config; xmqttCodecConfigInit(&Config); Config.MaxPayloadBytes=1;
	xmqttcodecissue Issue=legacyReadFailure((xbytesview){legacyFixtures[7].Data,legacyFixtures[7].Size},XMQTT_SERVER_TO_CLIENT,&Config,XMQTT_CODEC_LOCAL_ERROR);
	testRequire(Issue.Identifier==42 && Issue.Type==XMQTT_PACKET_PUBLISH && Issue.Error==XMQTT_CODEC_LIMIT,"3.1.1 payload limit context");
	xmqttpacket Packet; xmqttPacketInit(XMQTT_PACKET_PUBLISH,&Packet); Packet.Body.Publish.Topic=XRT_STR_LITERAL("t"); const unsigned char Data[]={0,255}; Packet.Body.Publish.Payload=(xbytesview){Data,2};
	legacyWriteFailure(&Packet,XMQTT_CLIENT_TO_SERVER,&Config);
	xmqttCodecConfigInit(&Config); Config.MaxTopics=0;
	legacyReadFailure((xbytesview){legacyFixtures[13].Data,legacyFixtures[13].Size},XMQTT_CLIENT_TO_SERVER,&Config,XMQTT_CODEC_LOCAL_ERROR);
	legacyReadFailure((xbytesview){legacyFixtures[14].Data,legacyFixtures[14].Size},XMQTT_SERVER_TO_CLIENT,&Config,XMQTT_CODEC_LOCAL_ERROR);
	xmqttsubscription Topics[2]; xmqttcodecbuffers Buffers={NULL,0,Topics,1};
	testRequire(xmqttPacketRead311((xbytesview){legacyFixtures[21].Data,legacyFixtures[21].Size},XMQTT_CLIENT_TO_SERVER,NULL,&Buffers,&Packet,&Issue)==XMQTT_CODEC_LOCAL_ERROR && Issue.Error==XMQTT_CODEC_CAPACITY,"3.1.1 late topic scratch exhaustion"); xrtClearError();
	Config.MaxPayloadBytes=0; Config.MaxPropertyBytes=0; Config.MaxProperties=0; size_t Required;
	xmqttPacketInit(XMQTT_PACKET_PINGREQ,&Packet); testRequire(xmqttPacketSize311(&Packet,XMQTT_CLIENT_TO_SERVER,&Config,&Required) && Required==2,"3.1.1 zero budgets reject PING");
	xmqttPacketInit(XMQTT_PACKET_UNSUBACK,&Packet); Packet.Identifier=42; testRequire(xmqttPacketSize311(&Packet,XMQTT_SERVER_TO_CLIENT,&Config,&Required) && Required==4,"3.1.1 UNSUBACK wrongly uses reason/property budgets");
	unsigned char Maximum[]={0x30,255,255,255,127}; xmqttCodecConfigInit(&Config); Config.MaxPacketBytes=XMQTT_PACKET_MAX_BYTES;
	testRequire(xmqttPacketRead311((xbytesview){Maximum,5},XMQTT_CLIENT_TO_SERVER,&Config,NULL,&Packet,NULL)==XMQTT_CODEC_MORE,"3.1.1 full four-byte Remaining Length range");
	Config.MaxPacketBytes=XMQTT_PACKET_MAX_BYTES-1u; legacyReadFailure((xbytesview){Maximum,5},XMQTT_CLIENT_TO_SERVER,&Config,XMQTT_CODEC_LOCAL_ERROR);
}

static void testLegacyLargeFields(void)
{
	const size_t Lengths[]={0,1,127,128,16383,16384,65535,65536,2097148,2097149,2097152};
	unsigned char* Data=(unsigned char*)malloc(2097152); unsigned char* Wire=(unsigned char*)malloc(3u*1024u*1024u);
	testRequire(Data && Wire,"3.1.1 test host allocations"); memset(Data,'x',2097152);
	xmqttcodecconfig Config; xmqttCodecConfigInit(&Config); Config.MaxPacketBytes=3u*1024u*1024u; Config.MaxPayloadBytes=2097152; Config.MaxPropertyBytes=0; Config.MaxProperties=0;
	xmqttpacket Packet, Parsed; size_t Written;
	for (size_t Index=0; Index<sizeof(Lengths)/sizeof(Lengths[0]); ++Index) {
		xmqttPacketInit(XMQTT_PACKET_PUBLISH,&Packet); Packet.Body.Publish.Topic=XRT_STR_LITERAL("t"); Packet.Body.Publish.Payload=(xbytesview){Data,Lengths[Index]};
		testRequire(xmqttPacketWrite311(Wire,3u*1024u*1024u,&Packet,XMQTT_CLIENT_TO_SERVER,&Config,&Written) &&
			xmqttPacketRead311((xbytesview){Wire,Written},XMQTT_CLIENT_TO_SERVER,&Config,NULL,&Parsed,NULL)==XMQTT_CODEC_DONE &&
			Parsed.Body.Publish.Payload.Size==Lengths[Index] && !memcmp(Parsed.Body.Publish.Payload.Data,Data,Lengths[Index]),"3.1.1 large packet/approved-errata range");
		size_t Prefixes=Lengths[Index]<=65536 ? Written : 8;
		for (size_t Prefix=0; Prefix<Prefixes; ++Prefix) testRequire(xmqttPacketRead311((xbytesview){Wire,Prefix},XMQTT_CLIENT_TO_SERVER,&Config,NULL,&Parsed,NULL)==XMQTT_CODEC_MORE,"3.1.1 large-prefix framing");
		testRequire(xmqttPacketRead311((xbytesview){Wire,Written-1u},XMQTT_CLIENT_TO_SERVER,&Config,NULL,&Parsed,NULL)==XMQTT_CODEC_MORE,"3.1.1 final missing payload byte");
	}
	xmqttPacketInit(XMQTT_PACKET_CONNECT,&Packet); Packet.Body.Connect.ClientId=(xstrview){(cstr)Data,65535};
	testRequire(xmqttPacketWrite311(Wire,3u*1024u*1024u,&Packet,XMQTT_CLIENT_TO_SERVER,&Config,&Written) &&
		xmqttPacketRead311((xbytesview){Wire,Written},XMQTT_CLIENT_TO_SERVER,&Config,NULL,&Parsed,NULL)==XMQTT_CODEC_DONE && Parsed.Body.Connect.ClientId.Size==65535,"3.1.1 maximum ClientId beyond 23 bytes");
	Packet.Body.Connect.ClientId.Size=65536; legacyWriteFailure(&Packet,XMQTT_CLIENT_TO_SERVER,&Config);
	Packet.Body.Connect.ClientId=XRT_STR_LITERAL("\xe2\x98\x83"); Packet.Body.Connect.HasWill=true; Packet.Body.Connect.WillTopic=XRT_STR_LITERAL("w"); Packet.Body.Connect.WillPayload=(xbytesview){Data,65535}; Data[77]=255;
	testRequire(xmqttPacketWrite311(Wire,3u*1024u*1024u,&Packet,XMQTT_CLIENT_TO_SERVER,&Config,&Written) &&
		xmqttPacketRead311((xbytesview){Wire,Written},XMQTT_CLIENT_TO_SERVER,&Config,NULL,&Parsed,NULL)==XMQTT_CODEC_DONE && Parsed.Body.Connect.WillPayload.Data[77]==255,"3.1.1 non-ASCII ClientId/binary Will");
	Packet.Body.Connect.WillPayload.Size=65536; legacyWriteFailure(&Packet,XMQTT_CLIENT_TO_SERVER,&Config);
	free(Wire); free(Data);
}

static void testLegacyAliasesAndApi(void)
{
	xmqttpacket Packet, Before; xmqttPacketInit(XMQTT_PACKET_PUBLISH,&Packet); Packet.Body.Publish.Topic=XRT_STR_LITERAL("t");
	unsigned char Wire[256], BeforeWire[256]; memset(Wire,'x',sizeof(Wire)); memcpy(BeforeWire,Wire,sizeof(Wire)); size_t Written=888;
	testRequire(!xmqttPacketSize311(&Packet,XMQTT_CLIENT_TO_SERVER,NULL,(size_t*)&Packet) && !xmqttPacketWrite311(&Packet,sizeof(Packet),&Packet,XMQTT_CLIENT_TO_SERVER,NULL,&Written),"3.1.1 output aliases Packet"); xrtClearError();
	xmqttcodecconfig Config; xmqttCodecConfigInit(&Config);
	testRequire(!xmqttPacketSize311(&Packet,XMQTT_CLIENT_TO_SERVER,&Config,&Config.MaxPacketBytes),"3.1.1 size aliases config"); xrtClearError();
	Packet.Body.Publish.Topic=(xstrview){(cstr)Wire,1};
	testRequire(!xmqttPacketWrite311(Wire,sizeof(Wire),&Packet,XMQTT_CLIENT_TO_SERVER,NULL,&Written) && Written==888 && !memcmp(Wire,BeforeWire,sizeof(Wire)),"3.1.1 output aliases topic"); xrtClearError();
	Packet.Body.Publish.Topic=XRT_STR_LITERAL("t"); Packet.Body.Publish.Payload=(xbytesview){Wire,2};
	testRequire(!xmqttPacketWrite311(Wire,sizeof(Wire),&Packet,XMQTT_CLIENT_TO_SERVER,NULL,&Written),"3.1.1 output aliases payload"); xrtClearError();
	Packet.Body.Publish.Payload=(xbytesview){NULL,1}; legacyWriteFailure(&Packet,XMQTT_CLIENT_TO_SERVER,NULL);
	Packet.Body.Publish.Payload=(xbytesview){(cbytes)(uintptr_t)(UINTPTR_MAX-1u),4}; legacyWriteFailure(&Packet,XMQTT_CLIENT_TO_SERVER,NULL);
	Packet.Body.Publish.Payload=(xbytesview){NULL,0}; union { size_t Align; unsigned char Data[256]; } Output;
	testRequire(!xmqttPacketWrite311(Output.Data,sizeof(Output.Data),&Packet,XMQTT_CLIENT_TO_SERVER,NULL,(size_t*)Output.Data),"3.1.1 Written aliases wire"); xrtClearError();
	legacyWriteFailure(NULL,XMQTT_CLIENT_TO_SERVER,NULL);
	xmqttPacketInit(XMQTT_PACKET_SUBSCRIBE,&Packet); Packet.Identifier=42; xmqttsubscription Topics[2]={{XRT_STR_LITERAL("a"),0},{XRT_STR_LITERAL("b"),1}}; Packet.Body.Topics=(xmqttsubscriptionlist){Topics,2};
	testRequire(!xmqttPacketSize311(&Packet,XMQTT_CLIENT_TO_SERVER,NULL,(size_t*)Topics) && !xmqttPacketWrite311(Topics,sizeof(Topics),&Packet,XMQTT_CLIENT_TO_SERVER,NULL,&Written),"3.1.1 output aliases topic descriptors"); xrtClearError();
	Config.MaxTopics=SIZE_MAX; Packet.Body.Topics.Count=SIZE_MAX; legacyWriteFailure(&Packet,XMQTT_CLIENT_TO_SERVER,&Config);
	xmqttPacketInit(XMQTT_PACKET_PINGREQ,&Packet); memcpy(&Before,&Packet,sizeof(Packet));
	xmqttcodecissue Issue={XMQTT_CODEC_DONE,XMQTT_CODEC_OK,777,0,0,0}, BeforeIssue; memcpy(&BeforeIssue,&Issue,sizeof(Issue));
	testRequire(xmqttPacketRead311((xbytesview){NULL,1},XMQTT_CLIENT_TO_SERVER,NULL,NULL,&Packet,&Issue)==XMQTT_CODEC_LOCAL_ERROR &&
		!memcmp(&Packet,&Before,sizeof(Packet)) && !memcmp(&Issue,&BeforeIssue,sizeof(Issue)),"3.1.1 API failure changed outputs"); xrtClearError();
	xmqttcodecbuffers Buffers={(struct xmqttproperty*)(uintptr_t)UINTPTR_MAX,SIZE_MAX,Topics,2};
	testRequire(xmqttPacketRead311((xbytesview){legacyFixtures[17].Data,2},XMQTT_CLIENT_TO_SERVER,NULL,&Buffers,&Packet,&Issue)==XMQTT_CODEC_DONE,"3.1.1 accessed ignored property scratch metadata");
	testRequire(xmqttPacketRead311((xbytesview){(cbytes)Topics,1},XMQTT_CLIENT_TO_SERVER,NULL,&Buffers,&Packet,&Issue)==XMQTT_CODEC_LOCAL_ERROR,"3.1.1 input/topic-scratch alias"); xrtClearError();
	Buffers.TopicCapacity=SIZE_MAX; testRequire(xmqttPacketRead311((xbytesview){NULL,0},XMQTT_CLIENT_TO_SERVER,NULL,&Buffers,&Packet,&Issue)==XMQTT_CODEC_LOCAL_ERROR,"3.1.1 topic capacity multiplication overflow"); xrtClearError();
	Buffers.TopicCapacity=2; testRequire(xmqttPacketRead311((xbytesview){NULL,0},XMQTT_CLIENT_TO_SERVER,NULL,&Buffers,(xmqttpacket*)&Buffers,&Issue)==XMQTT_CODEC_LOCAL_ERROR,"3.1.1 Packet overlaps Buffers metadata"); xrtClearError();
	for (unsigned Case=0; Case<5; ++Case) {
		xmqttCodecConfigInit(&Config); if (!Case) Config.Size=0; else if (Case==1) Config.MaxPacketBytes=1; else if (Case==2) Config.Utf8Flags=2;
		else if (Case==3) Config.MaxPayloadBytes=(size_t)XMQTT_VARIABLE_MAX+1u; else Config.MaxPropertyBytes=(size_t)XMQTT_VARIABLE_MAX+5u;
		legacyWriteFailure(&Packet,XMQTT_CLIENT_TO_SERVER,&Config);
		memcpy(&Issue,&BeforeIssue,sizeof(Issue)); testRequire(xmqttPacketRead311((xbytesview){NULL,0},XMQTT_CLIENT_TO_SERVER,&Config,NULL,&Packet,&Issue)==XMQTT_CODEC_LOCAL_ERROR &&
			!memcmp(&Issue,&BeforeIssue,sizeof(Issue)),"3.1.1 invalid config published Issue"); xrtClearError();
	}
}

static uint32 legacyRandom(uint32* State) { *State^=*State<<13u; *State^=*State>>17u; *State^=*State<<5u; return *State; }

static void testLegacyMutations(void)
{
	uint32 State=0x76192345u;
	for (size_t Trial=0; Trial<30000; ++Trial) {
		unsigned char Wire[256], Output[256], Again[256]; size_t Case=legacyRandom(&State)%(sizeof(legacyFixtures)/sizeof(legacyFixtures[0]));
		size_t Size=legacyFixtures[Case].Size; memcpy(Wire,legacyFixtures[Case].Data,Size); xmqttdirection Direction=legacyFixtures[Case].Direction;
		if (Trial%3u==0) { Size=legacyRandom(&State)%sizeof(Wire); for (size_t Index=0; Index<Size; ++Index) Wire[Index]=(unsigned char)legacyRandom(&State); Direction=(xmqttdirection)(1u+Trial%2u); }
		else for (unsigned Index=0, Changes=legacyRandom(&State)%5u; Index<Changes; ++Index) Wire[legacyRandom(&State)%Size]=(unsigned char)legacyRandom(&State);
		xmqttcodecconfig Config; xmqttCodecConfigInit(&Config); Config.MaxPacketBytes=256; Config.MaxTopics=16; Config.MaxPropertyBytes=0; Config.MaxProperties=0;
		xmqttsubscription Topics[16], Topics2[16], BeforeTopics[16]; xmqttcodecbuffers Buffers={NULL,0,Topics,16}, Buffers2={NULL,0,Topics2,16}; xmqttpacket Packet, Before, Parsed;
		memset(&Packet,0xa5,sizeof(Packet)); memcpy(&Before,&Packet,sizeof(Packet)); memset(Topics,0xa5,sizeof(Topics)); memcpy(BeforeTopics,Topics,sizeof(Topics));
		xmqttcodecstatus Status=xmqttPacketRead311((xbytesview){Wire,Size},Direction,&Config,&Buffers,&Packet,NULL);
		if (Status==XMQTT_CODEC_DONE) {
			size_t Written, Written2; testRequire(Packet.Wire.Data==Wire && Packet.Wire.Size<=Size,"3.1.1 mutation packet bounds"); legacyPacketWithin(&Packet,&Buffers);
			testRequire(xmqttPacketWrite311(Output,sizeof(Output),&Packet,Direction,&Config,&Written) &&
				xmqttPacketRead311((xbytesview){Output,Written},Direction,&Config,&Buffers2,&Parsed,NULL)==XMQTT_CODEC_DONE &&
				xmqttPacketWrite311(Again,sizeof(Again),&Parsed,Direction,&Config,&Written2) && Written==Written2 && !memcmp(Output,Again,Written),"3.1.1 accepted mutation has no stable canonical encoding");
		} else {
			testRequire(Status>=XMQTT_CODEC_UNSUPPORTED && Status<=XMQTT_CODEC_MORE && Status!=XMQTT_CODEC_PAYLOAD_ERROR && !memcmp(&Packet,&Before,sizeof(Packet)),"3.1.1 mutation status/publication");
			if (Status==XMQTT_CODEC_MORE) testRequire(!memcmp(Topics,BeforeTopics,sizeof(Topics)),"3.1.1 MORE changed topic scratch");
		}
		xrtClearError();
	}
}

static void testLegacyAllocation(void)
{
	const legacyFixture* Fixture=&legacyFixtures[7]; unsigned char Output[64]; xmqttpacket Packet; size_t Size, Written;
	xrtSetErrorInfo(XERR_VALUE,"xrt.test",777,"ambient"); const xerror* Ambient=xrtGetError();
	#if defined(XRT_FEATURE_MEMORY_STATS)
	xmemstats Before, After; xrtMemStatsEnable(true); xrtMemStatsGet(&Before);
	#endif
	for (size_t Index=0; Index<10000; ++Index) testRequire(xmqttPacketRead311((xbytesview){Fixture->Data,Fixture->Size-1u},Fixture->Direction,NULL,NULL,&Packet,NULL)==XMQTT_CODEC_MORE &&
		xmqttPacketRead311((xbytesview){Fixture->Data,Fixture->Size},Fixture->Direction,NULL,NULL,&Packet,NULL)==XMQTT_CODEC_DONE &&
		xmqttPacketSize311(&Packet,Fixture->Direction,NULL,&Size) && xmqttPacketWrite311(Output,sizeof(Output),&Packet,Fixture->Direction,NULL,&Written) &&
		Size==Written && Written==Fixture->Size && !memcmp(Output,Fixture->Data,Written) && xrtGetError()==Ambient,"3.1.1 warm cycle changed ambient/wire");
	#if defined(XRT_FEATURE_MEMORY_STATS)
	xrtMemStatsGet(&After); xrtMemStatsEnable(false); testRequire(After.BlockAllocCalls==Before.BlockAllocCalls && After.MallocCalls==Before.MallocCalls &&
		After.CallocCalls==Before.CallocCalls && After.ReallocCalls==Before.ReallocCalls,"3.1.1 successful codec allocated");
	#endif
	xrtClearError(); xallocationbudget* Budget=NULL; xallocationscope Scope={0}; testRequire(xrtAllocationBudgetCreate(1,&Budget) && xrtAllocationBudgetEnter(Budget,&Scope),"3.1.1 quota enter");
	testRequire(xmqttPacketRead311((xbytesview){Fixture->Data,Fixture->Size},Fixture->Direction,NULL,NULL,&Packet,NULL)==XMQTT_CODEC_DONE &&
		xmqttPacketWrite311(Output,sizeof(Output),&Packet,Fixture->Direction,NULL,&Written),"3.1.1 valid operation requires heap");
	const unsigned char Bad[][20]={{0xc1,0},{0x40,2,0,0},{0x30,3,0,1,255},{0x10,12,0,4,'M','Q','T','T',4,0,0,0,0,0},{0x10,12,0,4,'M','Q','T','T',5,2,0,0,0,0}};
	const size_t Sizes[]={2,4,5,14,14}; const xmqttcodecstatus Expected[]={XMQTT_CODEC_MALFORMED,XMQTT_CODEC_PROTOCOL_ERROR,XMQTT_CODEC_MALFORMED,XMQTT_CODEC_PROTOCOL_ERROR,XMQTT_CODEC_UNSUPPORTED};
	for (size_t Index=0; Index<5; ++Index) {
		xmqttcodecissue Issue; testRequire(xmqttPacketRead311((xbytesview){Bad[Index],Sizes[Index]},XMQTT_CLIENT_TO_SERVER,NULL,NULL,&Packet,&Issue)==Expected[Index] &&
			Issue.Status==Expected[Index] && xrtErrorKind(xrtGetError())==XERR_MEMORY,"3.1.1 own/nested diagnostic OOM hid typed failure");
		if (Index==3) testRequire(Issue.Error==XMQTT_CODEC_CLIENT_ID,"3.1.1 client-id rejection lost under OOM");
	}
	xmqttcodecconfig Config; xmqttCodecConfigInit(&Config); Config.MaxPacketBytes=2; xmqttcodecissue Issue;
	testRequire(xmqttPacketRead311((xbytesview){Fixture->Data,2},Fixture->Direction,&Config,NULL,&Packet,&Issue)==XMQTT_CODEC_LOCAL_ERROR &&
		Issue.Error==XMQTT_CODEC_LIMIT && xrtErrorKind(xrtGetError())==XERR_MEMORY,"3.1.1 header limit lost under OOM");
	testRequire(xrtAllocationBudgetLeave(&Scope),"3.1.1 quota leave"); xallocationbudgetinfo Info={0}; Info.Size=sizeof(Info);
	testRequire(xrtAllocationBudgetInfo(Budget,&Info) && Info.LiveBytes==0 && Info.Rejections!=0,"3.1.1 cold diagnostic quota leak"); xrtClearError(); xrtAllocationBudgetFree(Budget);
}

int main(void)
{
	testLegacyFixtures(); testLegacyRolesAndBoundaries(); testLegacyFlagsAndCodes(); testLegacyInvalid(); testLegacyLimits(); testLegacyLargeFields(); testLegacyAliasesAndApi(); testLegacyMutations(); testLegacyAllocation();
	printf("[PASS] xmqtt codec311: all14/errata/flags/codes/options/roles/prefixes/topics/binary/limits/aliases/atomicity/mutations/zero-heap/OOM\n"); return 0;
}
