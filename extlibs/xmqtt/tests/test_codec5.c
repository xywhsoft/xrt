#include "test.h"

typedef struct codecFixture { unsigned char Data[64]; size_t Size; xmqttdirection Direction; } codecFixture;
static const codecFixture codecFixtures[]={
	{{0x10,13,0,4,'M','Q','T','T',5,2,0,30,0,0,0},15,XMQTT_CLIENT_TO_SERVER},
	{{0x20,3,0,0,0},5,XMQTT_SERVER_TO_CLIENT},
	{{0x30,5,0,1,'t',0,'x'},7,XMQTT_CLIENT_TO_SERVER},
	{{0x32,7,0,1,'t',0,1,0,'x'},9,XMQTT_SERVER_TO_CLIENT},
	{{0x34,7,0,1,'t',0xff,0xff,0,'x'},9,XMQTT_CLIENT_TO_SERVER},
	{{0x40,2,0,1},4,XMQTT_CLIENT_TO_SERVER}, {{0x50,2,0,1},4,XMQTT_SERVER_TO_CLIENT},
	{{0x62,2,0,1},4,XMQTT_CLIENT_TO_SERVER}, {{0x70,2,0,1},4,XMQTT_SERVER_TO_CLIENT},
	{{0x82,7,0,1,0,0,1,'t',0},9,XMQTT_CLIENT_TO_SERVER},
	{{0x90,4,0,1,0,0},6,XMQTT_SERVER_TO_CLIENT},
	{{0xa2,6,0,1,0,0,1,'t'},8,XMQTT_CLIENT_TO_SERVER},
	{{0xb0,4,0,1,0,0},6,XMQTT_SERVER_TO_CLIENT},
	{{0xc0,0},2,XMQTT_CLIENT_TO_SERVER}, {{0xd0,0},2,XMQTT_SERVER_TO_CLIENT},
	{{0xe0,0},2,XMQTT_CLIENT_TO_SERVER}, {{0xf0,0},2,XMQTT_SERVER_TO_CLIENT},
	{{0xf0,6,0x18,4,21,0,1,'m'},8,XMQTT_CLIENT_TO_SERVER},
	/* Payload Format Indicator 1: embedded NUL is well-formed character data. */
	{{0x32,10,0,1,'t',0,42,2,1,1,'x',0},12,XMQTT_CLIENT_TO_SERVER},
	{{0x10,50,0,4,'M','Q','T','T',5,0xf6,1,2,
		14,17,0,0,0,30,21,0,1,'m',22,0,2,0,255,0,1,'c',
		7,24,0,0,0,30,1,1,0,1,'w',0,2,'x',0,0,1,'u',0,2,0,255},52,XMQTT_CLIENT_TO_SERVER},
	{{0x30,7,0,0,3,35,0,1,'x'},9,XMQTT_CLIENT_TO_SERVER},
	{{0x90,7,0,42,0,0,1,2,128},9,XMQTT_SERVER_TO_CLIENT},
	{{0xb0,6,0,42,0,0,17,145},8,XMQTT_SERVER_TO_CLIENT},
	{{0x82,16,0,42,0,0,1,'t',0,0,6,'$','S','Y','S','/','#',0x2e},18,XMQTT_CLIENT_TO_SERVER},
	{{0xa2,14,0,42,0,0,1,'t',0,6,'$','S','Y','S','/','#'},16,XMQTT_CLIENT_TO_SERVER}
};

static void codecViewWithin(xbytesview Whole, const void* Data, size_t Size)
{
	if (Data==NULL && !Size) return;
	uintptr_t Begin=(uintptr_t)Whole.Data, Part=(uintptr_t)Data;
	testRequire(Part>=Begin && Part-Begin<=Whole.Size && Size<=Whole.Size-(Part-Begin),"decoded borrowed view escaped its packet/scratch");
}

static void codecPropertiesWithin(xbytesview Wire, xmqttpropertylist Values, const xmqttcodecbuffers* Buffers)
{
	codecViewWithin((xbytesview){(cbytes)Buffers->Properties,Buffers->PropertyCapacity*sizeof(xmqttproperty)},Values.Data,Values.Count*sizeof(xmqttproperty));
	for (size_t Index=0; Index<Values.Count; ++Index) {
		const xmqttproperty* Value=&Values.Data[Index];
		if (Value->Type==XMQTT_PROPERTY_STRING) codecViewWithin(Wire,Value->Value.Text.Data,Value->Value.Text.Size);
		else if (Value->Type==XMQTT_PROPERTY_BINARY) codecViewWithin(Wire,Value->Value.Binary.Data,Value->Value.Binary.Size);
		else if (Value->Type==XMQTT_PROPERTY_STRING_PAIR) {
			codecViewWithin(Wire,Value->Value.Pair.Name.Data,Value->Value.Pair.Name.Size); codecViewWithin(Wire,Value->Value.Pair.Value.Data,Value->Value.Pair.Value.Size);
		}
	}
}

static void codecPacketWithin(const xmqttpacket* Packet, const xmqttcodecbuffers* Buffers)
{
	xbytesview Wire=Packet->Wire; codecPropertiesWithin(Wire,Packet->Properties,Buffers);
	if (Packet->Type==XMQTT_PACKET_PUBLISH) {
		codecViewWithin(Wire,Packet->Body.Publish.Topic.Data,Packet->Body.Publish.Topic.Size); codecViewWithin(Wire,Packet->Body.Publish.Payload.Data,Packet->Body.Publish.Payload.Size);
	} else if (Packet->Type==XMQTT_PACKET_CONNECT) {
		const xmqttconnectpacket* Value=&Packet->Body.Connect; codecViewWithin(Wire,Value->ClientId.Data,Value->ClientId.Size);
		if (Value->HasWill) {
			codecPropertiesWithin(Wire,Value->WillProperties,Buffers); codecViewWithin(Wire,Value->WillTopic.Data,Value->WillTopic.Size); codecViewWithin(Wire,Value->WillPayload.Data,Value->WillPayload.Size);
		}
		if (Value->HasUsername) codecViewWithin(Wire,Value->Username.Data,Value->Username.Size);
		if (Value->HasPassword) codecViewWithin(Wire,Value->Password.Data,Value->Password.Size);
	} else if (Packet->Type==XMQTT_PACKET_SUBSCRIBE || Packet->Type==XMQTT_PACKET_UNSUBSCRIBE) {
		codecViewWithin((xbytesview){(cbytes)Buffers->Topics,Buffers->TopicCapacity*sizeof(xmqttsubscription)},Packet->Body.Topics.Data,Packet->Body.Topics.Count*sizeof(xmqttsubscription));
		for (size_t Index=0; Index<Packet->Body.Topics.Count; ++Index) codecViewWithin(Wire,Packet->Body.Topics.Data[Index].Filter.Data,Packet->Body.Topics.Data[Index].Filter.Size);
	} else if (Packet->Type==XMQTT_PACKET_SUBACK || Packet->Type==XMQTT_PACKET_UNSUBACK) codecViewWithin(Wire,Packet->Body.Reasons.Data,Packet->Body.Reasons.Size);
}

static void codecWriteFailure(const xmqttpacket* Packet, xmqttdirection Direction, const xmqttcodecconfig* Config)
{
	unsigned char Output[512], Before[512]; memset(Output,0xa5,sizeof(Output)); memcpy(Before,Output,sizeof(Output)); size_t Size=777, Written=888;
	testRequire(!xmqttPacketSize5(Packet,Direction,Config,&Size) && Size==777,"codec size failure published output"); xrtClearError();
	testRequire(!xmqttPacketWrite5(Output,sizeof(Output),Packet,Direction,Config,&Written) && Written==888 && !memcmp(Output,Before,sizeof(Output)),"codec write failure was not atomic"); xrtClearError();
}

static xmqttcodecissue codecReadFailure(xbytesview Input, xmqttdirection Direction, const xmqttcodecconfig* Config, xmqttcodecstatus Expected)
{
	xmqttproperty Properties[16]; xmqttsubscription Topics[16]; xmqttcodecbuffers Buffers={Properties,16,Topics,16};
	xmqttpacket Packet, Before; memset(&Packet,0xa5,sizeof(Packet)); memcpy(&Before,&Packet,sizeof(Packet));
	xmqttcodecissue Issue={XMQTT_CODEC_DONE,XMQTT_CODEC_OK,777,0,0,0};
	xmqttcodecstatus Status=xmqttPacketRead5(Input,Direction,Config,&Buffers,&Packet,&Issue);
	if (Status!=Expected) fprintf(stderr,"read failure got %d expected %d first=%02x size=%zu\n",(int)Status,(int)Expected,Input.Size ? Input.Data[0] : 0,Input.Size);
	testRequire(Status==Expected && !memcmp(&Packet,&Before,sizeof(Packet)),"codec failure status/packet publication");
	if (Status<0) testRequire(Issue.Status==Expected && Issue.Offset<=Input.Size,"codec independent issue status/offset");
	xrtClearError(); return Issue;
}

static void testCodecFixtures(void)
{
	for (size_t Case=0; Case<sizeof(codecFixtures)/sizeof(codecFixtures[0]); ++Case) {
		const codecFixture* Fixture=&codecFixtures[Case]; unsigned char Wire[128]; memcpy(Wire,Fixture->Data,Fixture->Size); Wire[Fixture->Size]=255;
		xmqttproperty Properties[8], BeforeProperties[8]; xmqttsubscription Topics[8], BeforeTopics[8]; xmqttcodecbuffers Buffers={Properties,8,Topics,8};
		for (size_t Prefix=0; Prefix<Fixture->Size; ++Prefix) {
			xmqttpacket Packet, Before; memset(&Packet,0xa5,sizeof(Packet)); memcpy(&Before,&Packet,sizeof(Packet));
			memset(Properties,0xa5,sizeof(Properties)); memcpy(BeforeProperties,Properties,sizeof(Properties)); memset(Topics,0xa5,sizeof(Topics)); memcpy(BeforeTopics,Topics,sizeof(Topics));
			xmqttcodecissue Issue={XMQTT_CODEC_DONE,XMQTT_CODEC_OK,777,0,0,0}, BeforeIssue=Issue;
			testRequire(xmqttPacketRead5((xbytesview){Wire,Prefix},Fixture->Direction,NULL,&Buffers,&Packet,&Issue)==XMQTT_CODEC_MORE &&
				!memcmp(&Packet,&Before,sizeof(Packet)) && !memcmp(&Issue,&BeforeIssue,sizeof(Issue)) && !memcmp(Properties,BeforeProperties,sizeof(Properties)) &&
				!memcmp(Topics,BeforeTopics,sizeof(Topics)),"codec valid prefix changed outputs/scratch");
		}
		xmqttpacket Packet; xmqttcodecissue Issue;
		testRequire(xmqttPacketRead5((xbytesview){Wire,Fixture->Size+1u},Fixture->Direction,NULL,&Buffers,&Packet,&Issue)==XMQTT_CODEC_DONE &&
			Packet.Type==(xmqttpackettype)(Wire[0]>>4) && Packet.Flags==(Wire[0]&15u) && Packet.Wire.Data==Wire && Packet.Wire.Size==Fixture->Size &&
			Issue.Status==XMQTT_CODEC_DONE && Issue.Error==0 && Issue.Offset==XRT_NPOS,"codec literal type/consumption/issue");
		codecPacketWithin(&Packet,&Buffers);
		unsigned char Output[128]; size_t Size=0, Written=0;
		testRequire(xmqttPacketSize5(&Packet,Fixture->Direction,NULL,&Size) && Size==Fixture->Size &&
			xmqttPacketWrite5(Output,sizeof(Output),&Packet,Fixture->Direction,NULL,&Written) && Written==Size && !memcmp(Output,Wire,Size),"codec literal canonical encode");
		for (size_t Capacity=0; Capacity<Size; ++Capacity) {
			memset(Output,0xa5,sizeof(Output)); Written=888;
			testRequire(!xmqttPacketWrite5(Output,Capacity,&Packet,Fixture->Direction,NULL,&Written) && Written==888,"short packet write publication");
			for (size_t Index=0; Index<sizeof(Output); ++Index) testRequire(Output[Index]==0xa5,"short packet write modified wire");
			xrtClearError();
		}
		/* Borrowed views survive as long as the caller's wire/scratch. */
		if (Packet.Type==XMQTT_PACKET_PUBLISH) {
			testRequire(Packet.Body.Publish.Topic.Data==(cstr)Wire+4 && Packet.Body.Publish.Payload.Data+Packet.Body.Publish.Payload.Size==Wire+Fixture->Size,"PUBLISH borrowed lifetimes");
		}
		if (Case==19) testRequire(Packet.Body.Connect.HasWill && Packet.Body.Connect.WillQos==2 && Packet.Body.Connect.WillRetain &&
			Packet.Body.Connect.CleanStart && Packet.Body.Connect.KeepAlive==258 && Packet.Properties.Count==3 && Packet.Body.Connect.WillProperties.Count==2 &&
			Packet.Body.Connect.WillProperties.Data==Properties+3 && Packet.Body.Connect.Username.Size==1 && Packet.Body.Connect.Password.Size==2 &&
			Packet.Body.Connect.Password.Data[1]==255 && Packet.Body.Connect.WillPayload.Data[1]==0,"complete CONNECT optional field order/lifetimes");
	}
	/* Explicit zero-property encodings canonicalize to the abbreviated form. */
	const unsigned char Expanded[][6]={{0x40,4,0,1,0,0},{0x62,4,0,1,0,0},{0xe0,2,0,0}};
	const size_t Sizes[]={6,6,4}, Canonical[]={4,4,2};
	for (size_t Index=0; Index<3; ++Index) {
		xmqttpacket Packet; unsigned char Output[16]; size_t Written;
		testRequire(xmqttPacketRead5((xbytesview){Expanded[Index],Sizes[Index]},XMQTT_CLIENT_TO_SERVER,NULL,NULL,&Packet,NULL)==XMQTT_CODEC_DONE &&
			xmqttPacketWrite5(Output,sizeof(Output),&Packet,XMQTT_CLIENT_TO_SERVER,NULL,&Written) && Written==Canonical[Index],"omitted ACK/DISCONNECT canonicalization");
	}
}

static void testCodecRolesAndPacketBoundary(void)
{
	for (size_t Case=0; Case<sizeof(codecFixtures)/sizeof(codecFixtures[0]); ++Case) {
		const codecFixture* Fixture=&codecFixtures[Case]; unsigned Type=Fixture->Data[0]>>4;
		xmqttproperty Props[8]; xmqttsubscription Topics[8]; xmqttcodecbuffers Buffers={Props,8,Topics,8}; xmqttpacket Packet;
		for (unsigned Direction=1; Direction<=2; ++Direction) {
			bool Valid=(Type==1 || Type==8 || Type==10 || Type==12) ? Direction==1 :
				(Type==2 || Type==9 || Type==11 || Type==13) ? Direction==2 : !(Type==15 && Fixture->Data[1]==0 && Direction==1);
			testRequire(xmqttPacketRead5((xbytesview){Fixture->Data,Fixture->Size},(xmqttdirection)Direction,NULL,&Buffers,&Packet,NULL)==
				(Valid ? XMQTT_CODEC_DONE : XMQTT_CODEC_PROTOCOL_ERROR),"all control types sender-role matrix"); xrtClearError();
		}
		/* A shorter declared packet cannot borrow from the still-buffered suffix.
		 * PUBLISH payload cuts and abbreviated ACK cuts may be legal packets. */
		for (unsigned Cut=0; Cut<Fixture->Data[1]; ++Cut) {
			unsigned char Wire[64], Output[64]; memcpy(Wire,Fixture->Data,Fixture->Size); Wire[1]=(unsigned char)Cut;
			xmqttcodecstatus Status=xmqttPacketRead5((xbytesview){Wire,Fixture->Size},Fixture->Direction,NULL,&Buffers,&Packet,NULL);
			testRequire(Status!=XMQTT_CODEC_MORE,"coalesced suffix used to finish truncated in-packet field");
			if (Status==XMQTT_CODEC_DONE) {
				size_t Written; testRequire(Packet.Wire.Size==Cut+2u,"short declared packet consumed suffix"); codecPacketWithin(&Packet,&Buffers);
				testRequire(xmqttPacketWrite5(Output,sizeof(Output),&Packet,Fixture->Direction,NULL,&Written) && Written<=Packet.Wire.Size,"short accepted packet not encodable");
			}
			xrtClearError();
		}
	}
}

static bool codecReasonOracle(unsigned Type, unsigned Reason, unsigned Direction)
{
	/* Literal OASIS tables; deliberately no production helper/registry calls. */
	static const unsigned char Connack[]={0,128,129,130,131,132,133,134,135,136,137,138,140,144,149,151,153,154,155,156,157,159};
	static const unsigned char Puback[]={0,16,128,131,135,144,145,151,153}, Pubrel[]={0,146};
	static const unsigned char Suback[]={0,1,2,128,131,135,143,145,151,158,161,162}, Unsuback[]={0,17,128,131,135,143,145};
	static const unsigned char DiscBoth[]={0,128,129,130,131,144,147,148,149,150,151,152,153};
	static const unsigned char DiscServer[]={135,137,139,141,142,143,154,155,156,157,158,159,160,161,162};
	const unsigned char* Table=NULL; size_t Count=0;
	if (Type==2) { if (Direction!=2) return false; Table=Connack; Count=sizeof(Connack); }
	else if (Type==4 || Type==5) { Table=Puback; Count=sizeof(Puback); }
	else if (Type==6 || Type==7) { Table=Pubrel; Count=sizeof(Pubrel); }
	else if (Type==9 || Type==11) { if (Direction!=2) return false; Table=Type==9 ? Suback : Unsuback; Count=Type==9 ? sizeof(Suback) : sizeof(Unsuback); }
	else if (Type==15) return Reason==24 || (Reason==0 && Direction==2) || (Reason==25 && Direction==1);
	else {
		if (Reason==4 && Direction==1) return true;
		for (size_t Index=0; Index<sizeof(DiscServer); ++Index) if (Reason==DiscServer[Index]) return Direction==2;
		Table=DiscBoth; Count=sizeof(DiscBoth);
	}
	for (size_t Index=0; Index<Count; ++Index) if (Reason==Table[Index]) return true;
	return false;
}

static void testCodecReasons(void)
{
	const unsigned Types[]={2,4,5,6,7,9,11,14,15};
	for (size_t Case=0; Case<sizeof(Types)/sizeof(Types[0]); ++Case) for (unsigned Direction=1; Direction<=2; ++Direction) for (unsigned Reason=0; Reason<=255; ++Reason) {
		unsigned Type=Types[Case]; unsigned char Wire[16]={0}; Wire[0]=(unsigned char)((Type<<4)|(Type==6 ? 2u : 0u)); size_t Size;
		if (Type==2) { Wire[1]=3; Wire[3]=(unsigned char)Reason; Size=5; }
		else if (Type==9 || Type==11) { Wire[1]=4; Wire[3]=1; Wire[5]=(unsigned char)Reason; Size=6; }
		else if (Type==14) { Wire[1]=1; Wire[2]=(unsigned char)Reason; Size=3; }
		else if (Type==15) { Wire[1]=6; Wire[2]=(unsigned char)Reason; Wire[3]=4; Wire[4]=21; Wire[6]=1; Wire[7]='m'; Size=8; }
		else { Wire[1]=3; Wire[3]=1; Wire[4]=(unsigned char)Reason; Size=5; }
		xmqttpacket Packet; xmqttproperty Properties[2]; xmqttcodecbuffers Buffers={Properties,2,NULL,0}; bool Valid=codecReasonOracle(Type,Reason,Direction);
		xmqttcodecstatus Status=xmqttPacketRead5((xbytesview){Wire,Size},(xmqttdirection)Direction,NULL,&Buffers,&Packet,NULL);
		testRequire(Status==(Valid ? XMQTT_CODEC_DONE : XMQTT_CODEC_PROTOCOL_ERROR),"packet reason/direction exhaustive wire matrix");
		if (Valid) { size_t Required; testRequire(xmqttPacketSize5(&Packet,(xmqttdirection)Direction,NULL,&Required),"valid reason not encodable"); }
		xrtClearError();
	}
}

static void testCodecConnectFlags(void)
{
	for (unsigned Flags=0; Flags<=255; ++Flags) {
		unsigned char Wire[64]={0x10,0,0,4,'M','Q','T','T',5,(unsigned char)Flags,0,0,0,0,0}; size_t Size=15;
		bool Valid=!(Flags&1u) && (Flags&0x18u)!=0x18u && ((Flags&4u) || !(Flags&0x38u));
		if (Flags&4u) { Wire[Size++]=0; Wire[Size++]=0; Wire[Size++]=1; Wire[Size++]='w'; Wire[Size++]=0; Wire[Size++]=0; }
		if (Flags&0x80u) { Wire[Size++]=0; Wire[Size++]=0; }
		if (Flags&0x40u) { Wire[Size++]=0; Wire[Size++]=1; Wire[Size++]=255; }
		Wire[1]=(unsigned char)(Size-2u); xmqttpacket Packet;
		xmqttcodecstatus Status=xmqttPacketRead5((xbytesview){Wire,Size},XMQTT_CLIENT_TO_SERVER,NULL,NULL,&Packet,NULL);
		testRequire(Status==(Valid ? XMQTT_CODEC_DONE : XMQTT_CODEC_MALFORMED),"CONNECT flag byte grammar");
		if (Valid) { unsigned char Output[64]; size_t Written; testRequire(xmqttPacketWrite5(Output,sizeof(Output),&Packet,XMQTT_CLIENT_TO_SERVER,NULL,&Written) && Written==Size && !memcmp(Output,Wire,Size),"CONNECT flag-presence round trip"); }
		xrtClearError();
	}
	unsigned char WrongVersion[]={0x10,13,0,4,'M','Q','T','T',4,2,0,30,0,0,0};
	codecReadFailure((xbytesview){WrongVersion,sizeof(WrongVersion)},XMQTT_CLIENT_TO_SERVER,NULL,XMQTT_CODEC_UNSUPPORTED);
	WrongVersion[8]=5; WrongVersion[4]='x'; codecReadFailure((xbytesview){WrongVersion,sizeof(WrongVersion)},XMQTT_CLIENT_TO_SERVER,NULL,XMQTT_CODEC_UNSUPPORTED);
}

static void testCodecTopicOptions(void)
{
	const cstr Filters[]={"a/+/#","$share/g/a/+"};
	for (size_t Filter=0; Filter<2; ++Filter) for (unsigned Option=0; Option<=255; ++Option) {
		unsigned char Wire[64]={0x82,0,0,1,0,0,0}; size_t Length=strlen(Filters[Filter]); Wire[6]=(unsigned char)Length;
		memcpy(Wire+7,Filters[Filter],Length); Wire[7+Length]=(unsigned char)Option; Wire[1]=(unsigned char)(6u+Length);
		xmqttsubscription Topic; xmqttcodecbuffers Buffers={NULL,0,&Topic,1}; xmqttpacket Packet;
		bool BadBits=(Option&0xc0u) || (Option&3u)==3u || (Option&0x30u)==0x30u;
		xmqttcodecstatus Expected=BadBits ? XMQTT_CODEC_MALFORMED : Filter && (Option&4u) ? XMQTT_CODEC_PROTOCOL_ERROR : XMQTT_CODEC_DONE;
		testRequire(xmqttPacketRead5((xbytesview){Wire,8u+Length},XMQTT_CLIENT_TO_SERVER,NULL,&Buffers,&Packet,NULL)==Expected,"subscription options/shared No Local matrix");
		if (Expected==XMQTT_CODEC_DONE) { unsigned char Output[64]; size_t Written; testRequire(xmqttPacketWrite5(Output,sizeof(Output),&Packet,XMQTT_CLIENT_TO_SERVER,NULL,&Written) && Written==8u+Length && !memcmp(Output,Wire,Written),"valid subscription options bytes"); }
		xrtClearError();
	}
	const cstr Invalid[]={"","a+","a/#/b","$share//a","$share/g/","$share/+/a"};
	for (size_t Index=0; Index<sizeof(Invalid)/sizeof(Invalid[0]); ++Index) {
		xmqttpacket Packet; xmqttPacketInit(XMQTT_PACKET_SUBSCRIBE,&Packet); Packet.Identifier=1;
		xmqttsubscription Topic={{Invalid[Index],strlen(Invalid[Index])},0}; Packet.Body.Topics=(xmqttsubscriptionlist){&Topic,1};
		codecWriteFailure(&Packet,XMQTT_CLIENT_TO_SERVER,NULL);
	}
	xmqttpacket Packet; xmqttPacketInit(XMQTT_PACKET_SUBSCRIBE,&Packet); Packet.Identifier=1;
	xmqttsubscription Topics[]={{XRT_STR_LITERAL("a/+"),0},{XRT_STR_LITERAL("$share/g/b"),XMQTT_SUBSCRIBE_NO_LOCAL}};
	Packet.Body.Topics=(xmqttsubscriptionlist){Topics,2}; codecWriteFailure(&Packet,XMQTT_CLIENT_TO_SERVER,NULL);
}

static void testCodecInvalid(void)
{
	static const struct { unsigned char Data[32]; size_t Size; xmqttdirection Direction; xmqttcodecstatus Status; } Cases[]={
		{{0xc0,1},2,1,XMQTT_CODEC_MALFORMED}, {{0xf0,1},2,2,XMQTT_CODEC_MALFORMED},
		{{0x20,3,2,0,0},5,2,XMQTT_CODEC_MALFORMED}, {{0x20,3,1,0x80,0},5,2,XMQTT_CODEC_PROTOCOL_ERROR},
		{{0x40,2,0,0},4,1,XMQTT_CODEC_PROTOCOL_ERROR}, {{0x32,7,0,1,'t',0,0,0,'x'},9,1,XMQTT_CODEC_PROTOCOL_ERROR},
		{{0x30,4,0,1,'+',0},6,1,XMQTT_CODEC_PROTOCOL_ERROR}, {{0x30,4,0,1,0,0},6,1,XMQTT_CODEC_MALFORMED},
		{{0x30,3,0,0,0},5,1,XMQTT_CODEC_PROTOCOL_ERROR},
		{{0x32,9,0,1,'t',0,42,2,1,1,255},11,1,XMQTT_CODEC_PAYLOAD_ERROR},
		{{0x30,5,0,1,'t',1,7},7,1,XMQTT_CODEC_MALFORMED},
		{{0x30,6,0,1,'t',2,1,2},8,1,XMQTT_CODEC_PROTOCOL_ERROR},
		{{0x30,4,0,1,'t',2},6,1,XMQTT_CODEC_MALFORMED},
		{{0x30,5,0,2,'t',0,0},7,1,XMQTT_CODEC_MALFORMED},
		{{0x82,3,0,1,0},5,1,XMQTT_CODEC_PROTOCOL_ERROR}, {{0xa2,3,0,1,0},5,1,XMQTT_CODEC_PROTOCOL_ERROR},
		{{0x90,3,0,1,0},5,2,XMQTT_CODEC_PROTOCOL_ERROR}, {{0xb0,3,0,1,0},5,2,XMQTT_CODEC_PROTOCOL_ERROR},
		{{0xf0,2,0x18,0},4,1,XMQTT_CODEC_PROTOCOL_ERROR}, {{0xf0,0},2,1,XMQTT_CODEC_PROTOCOL_ERROR},
		{{0xe0,3,0,0,1},5,1,XMQTT_CODEC_MALFORMED}, {{0x40,1,1},3,1,XMQTT_CODEC_MALFORMED},
		{{0x30,3,0,5,0},5,1,XMQTT_CODEC_MALFORMED}, {{0x20,0},2,2,XMQTT_CODEC_MALFORMED}
	};
	for (size_t Index=0; Index<sizeof(Cases)/sizeof(Cases[0]); ++Index) {
		xmqttcodecissue Issue=codecReadFailure((xbytesview){Cases[Index].Data,Cases[Index].Size},Cases[Index].Direction,NULL,Cases[Index].Status);
		if (Cases[Index].Status==XMQTT_CODEC_PAYLOAD_ERROR) testRequire(Issue.Type==XMQTT_PACKET_PUBLISH && Issue.Identifier==42 && Issue.Offset==10,"payload error lacks confirmed routing metadata");
	}
	unsigned char Alias[]={0x30,6,0,0,3,35,0,1}; xmqttpacket Packet; xmqttproperty Properties[2]; xmqttcodecbuffers Buffers={Properties,2,NULL,0};
	testRequire(xmqttPacketRead5((xbytesview){Alias,sizeof(Alias)},XMQTT_CLIENT_TO_SERVER,NULL,&Buffers,&Packet,NULL)==XMQTT_CODEC_DONE && Packet.Body.Publish.Topic.Size==0 &&
		Packet.Properties.Count==1 && Properties[0].Value.Number==1,"empty topic with positive alias rejected syntactically");
	/* PFI flag itself is not the encoded-string policy. C0/NUL/control bytes are
	 * character data; malformed scalar sequences remain invalid. */
	const unsigned char Good[][5]={{0,1,0x7f,0,0},{0xef,0xbb,0xbf,0,0},{0xf4,0x8f,0xbf,0xbf,0}}; const size_t GoodSizes[]={3,3,4};
	xmqttPacketInit(XMQTT_PACKET_PUBLISH,&Packet); Packet.Body.Publish.Topic=XRT_STR_LITERAL("t"); Properties[0]=(xmqttproperty){0};
	Properties[0].Id=XMQTT_PROPERTY_PAYLOAD_FORMAT; Properties[0].Type=XMQTT_PROPERTY_BYTE; Properties[0].Value.Number=1; Packet.Properties=(xmqttpropertylist){Properties,1};
	for (size_t Index=0; Index<3; ++Index) {
		Packet.Body.Publish.Payload=(xbytesview){Good[Index],GoodSizes[Index]}; unsigned char Wire[64]; size_t Written;
		testRequire(xmqttPacketWrite5(Wire,sizeof(Wire),&Packet,XMQTT_CLIENT_TO_SERVER,NULL,&Written),"valid PFI character data rejected");
	}
	const unsigned char Bad[][4]={{0xc0,0x80,0,0},{0xed,0xa0,0x80,0},{0xf4,0x90,0x80,0x80},{0xe2,0x82,0,0}}; const size_t BadSizes[]={2,3,4,2};
	for (size_t Index=0; Index<4; ++Index) { Packet.Body.Publish.Payload=(xbytesview){Bad[Index],BadSizes[Index]}; codecWriteFailure(&Packet,XMQTT_CLIENT_TO_SERVER,NULL); }
}

static void testCodecLimits(void)
{
	xmqttpacket Packet; xmqttPacketInit(XMQTT_PACKET_CONNECT,&Packet); Packet.Body.Connect.HasWill=true; Packet.Body.Connect.WillTopic=XRT_STR_LITERAL("w");
	xmqttproperty Root={0}, Will={0}; Root.Id=XMQTT_PROPERTY_REQUEST_RESPONSE_INFO; Root.Type=XMQTT_PROPERTY_BYTE; Root.Value.Number=0;
	Will.Id=XMQTT_PROPERTY_PAYLOAD_FORMAT; Will.Type=XMQTT_PROPERTY_BYTE; Will.Value.Number=1;
	Packet.Properties=(xmqttpropertylist){&Root,1}; Packet.Body.Connect.WillProperties=(xmqttpropertylist){&Will,1};
	unsigned char Wire[256]; size_t Written; testRequire(xmqttPacketWrite5(Wire,sizeof(Wire),&Packet,XMQTT_CLIENT_TO_SERVER,NULL,&Written),"CONNECT aggregate fixture");
	xmqttcodecconfig Config; xmqttCodecConfigInit(&Config); Config.MaxPropertyBytes=6; Config.MaxProperties=2;
	xmqttproperty Properties[2]; xmqttcodecbuffers Buffers={Properties,2,NULL,0}; xmqttpacket Parsed;
	testRequire(xmqttPacketRead5((xbytesview){Wire,Written},XMQTT_CLIENT_TO_SERVER,&Config,&Buffers,&Parsed,NULL)==XMQTT_CODEC_DONE &&
		Parsed.Properties.Data==Properties && Parsed.Body.Connect.WillProperties.Data==Properties+1,"CONNECT aggregate properties/order");
	Config.MaxProperties=1; codecReadFailure((xbytesview){Wire,Written},XMQTT_CLIENT_TO_SERVER,&Config,XMQTT_CODEC_LOCAL_ERROR); codecWriteFailure(&Packet,XMQTT_CLIENT_TO_SERVER,&Config);
	Config.MaxProperties=2; Config.MaxPropertyBytes=5; codecReadFailure((xbytesview){Wire,Written},XMQTT_CLIENT_TO_SERVER,&Config,XMQTT_CODEC_LOCAL_ERROR); codecWriteFailure(&Packet,XMQTT_CLIENT_TO_SERVER,&Config);
	xmqttCodecConfigInit(&Config); Config.MaxPacketBytes=Written-1u;
	xmqttcodecissue Issue=codecReadFailure((xbytesview){Wire,2},XMQTT_CLIENT_TO_SERVER,&Config,XMQTT_CODEC_LOCAL_ERROR);
	testRequire(Issue.Error==XMQTT_CODEC_LIMIT && Issue.Offset==0,"header limit not known before body"); codecWriteFailure(&Packet,XMQTT_CLIENT_TO_SERVER,&Config);
	xmqttCodecConfigInit(&Config); Config.MaxPacketBytes=Written;
	testRequire(xmqttPacketRead5((xbytesview){Wire,Written},XMQTT_CLIENT_TO_SERVER,&Config,&Buffers,&Parsed,NULL)==XMQTT_CODEC_DONE,"exact packet byte cap rejected");
	xmqttPacketInit(XMQTT_PACKET_PINGREQ,&Packet); Config.MaxPropertyBytes=0; Config.MaxProperties=0; Config.MaxPayloadBytes=0; Config.MaxTopics=0; size_t Required;
	testRequire(xmqttPacketSize5(&Packet,XMQTT_CLIENT_TO_SERVER,&Config,&Required) && Required==2,"zero caps rejected property-free PING");
	xmqttPacketInit(XMQTT_PACKET_CONNACK,&Packet); codecWriteFailure(&Packet,XMQTT_SERVER_TO_CLIENT,&Config);
	unsigned char EmptyConnack[]={0x20,3,0,0,0}; codecReadFailure((xbytesview){EmptyConnack,5},XMQTT_SERVER_TO_CLIENT,&Config,XMQTT_CODEC_LOCAL_ERROR);
	/* Empty mandatory sections consume a length byte, even with zero properties. */
	Config.MaxPropertyBytes=1; testRequire(xmqttPacketSize5(&Packet,XMQTT_SERVER_TO_CLIENT,&Config,&Required) && Required==5,"empty mandatory property section cap");
	xmqttPacketInit(XMQTT_PACKET_PUBLISH,&Packet); Packet.Body.Publish.Topic=XRT_STR_LITERAL("t"); const unsigned char Data[]={'x'};
	Packet.Body.Publish.Payload=(xbytesview){Data,1}; codecWriteFailure(&Packet,XMQTT_CLIENT_TO_SERVER,&Config);
	codecReadFailure((xbytesview){codecFixtures[2].Data,codecFixtures[2].Size},XMQTT_CLIENT_TO_SERVER,&Config,XMQTT_CODEC_LOCAL_ERROR);
	xmqttCodecConfigInit(&Config); Config.MaxTopics=0;
	codecReadFailure((xbytesview){codecFixtures[9].Data,codecFixtures[9].Size},XMQTT_CLIENT_TO_SERVER,&Config,XMQTT_CODEC_LOCAL_ERROR);
	codecReadFailure((xbytesview){codecFixtures[10].Data,codecFixtures[10].Size},XMQTT_SERVER_TO_CLIENT,&Config,XMQTT_CODEC_LOCAL_ERROR);
	xmqttsubscription Topic={XRT_STR_LITERAL("t"),0}; xmqttPacketInit(XMQTT_PACKET_SUBSCRIBE,&Packet); Packet.Identifier=1; Packet.Body.Topics=(xmqttsubscriptionlist){&Topic,1};
	codecWriteFailure(&Packet,XMQTT_CLIENT_TO_SERVER,&Config);
	testRequire(xmqttPacketRead5((xbytesview){codecFixtures[9].Data,codecFixtures[9].Size},XMQTT_CLIENT_TO_SERVER,NULL,NULL,&Parsed,&Issue)==XMQTT_CODEC_LOCAL_ERROR && Issue.Error==XMQTT_CODEC_CAPACITY,"missing topic scratch classification"); xrtClearError();
}

static void testCodecBoundaries(void)
{
	const size_t Lengths[]={0,1,127,128,16383,16384,65535,65536};
	unsigned char* Data=(unsigned char*)malloc(65536); unsigned char* Wire=(unsigned char*)malloc(66000);
	testRequire(Data && Wire,"boundary test host allocation"); memset(Data,'x',65536);
	for (size_t Index=0; Index<sizeof(Lengths)/sizeof(Lengths[0]); ++Index) {
		xmqttpacket Packet, Parsed; xmqttPacketInit(XMQTT_PACKET_PUBLISH,&Packet); Packet.Body.Publish.Topic=XRT_STR_LITERAL("t"); Packet.Body.Publish.Payload=(xbytesview){Data,Lengths[Index]};
		size_t Written, Required; testRequire(xmqttPacketSize5(&Packet,XMQTT_CLIENT_TO_SERVER,NULL,&Required) &&
			xmqttPacketWrite5(Wire,66000,&Packet,XMQTT_CLIENT_TO_SERVER,NULL,&Written) && Required==Written,"boundary PUBLISH encode");
		for (size_t Prefix=0; Prefix<Written; ++Prefix)
			testRequire(xmqttPacketRead5((xbytesview){Wire,Prefix},XMQTT_CLIENT_TO_SERVER,NULL,NULL,&Parsed,NULL)==XMQTT_CODEC_MORE,"boundary packet prefix");
		testRequire(xmqttPacketRead5((xbytesview){Wire,Written},XMQTT_CLIENT_TO_SERVER,NULL,NULL,&Parsed,NULL)==XMQTT_CODEC_DONE &&
			Parsed.Body.Publish.Payload.Size==Lengths[Index] && !memcmp(Parsed.Body.Publish.Payload.Data,Data,Lengths[Index]),"boundary borrowed payload");
		xmqttcodecconfig Config; xmqttCodecConfigInit(&Config); Config.MaxPacketBytes=Written-1u;
		codecReadFailure((xbytesview){Wire,5},XMQTT_CLIENT_TO_SERVER,&Config,XMQTT_CODEC_LOCAL_ERROR); codecWriteFailure(&Packet,XMQTT_CLIENT_TO_SERVER,&Config);
	}
	xmqttpacket Packet, Parsed; xmqttPacketInit(XMQTT_PACKET_CONNECT,&Packet); Packet.Body.Connect.ClientId=(xstrview){(cstr)Data,65535}; size_t Written;
	testRequire(xmqttPacketWrite5(Wire,66000,&Packet,XMQTT_CLIENT_TO_SERVER,NULL,&Written) &&
		xmqttPacketRead5((xbytesview){Wire,Written},XMQTT_CLIENT_TO_SERVER,NULL,NULL,&Parsed,NULL)==XMQTT_CODEC_DONE && Parsed.Body.Connect.ClientId.Size==65535,"maximum ClientId field");
	Packet.Body.Connect.ClientId.Size=65536; codecWriteFailure(&Packet,XMQTT_CLIENT_TO_SERVER,NULL);
	/* Max binary Will payload; indicated UTF-8 includes U+0000. */
	Packet.Body.Connect.ClientId=(xstrview){NULL,0}; Packet.Body.Connect.HasWill=true; Packet.Body.Connect.WillTopic=XRT_STR_LITERAL("w"); Packet.Body.Connect.WillPayload=(xbytesview){Data,65535};
	xmqttproperty Will={0}; Will.Id=XMQTT_PROPERTY_PAYLOAD_FORMAT; Will.Type=XMQTT_PROPERTY_BYTE; Will.Value.Number=1; Packet.Body.Connect.WillProperties=(xmqttpropertylist){&Will,1}; Data[77]=0;
	xmqttproperty Props[2]; xmqttcodecbuffers Buffers={Props,2,NULL,0};
	testRequire(xmqttPacketWrite5(Wire,66000,&Packet,XMQTT_CLIENT_TO_SERVER,NULL,&Written) &&
		xmqttPacketRead5((xbytesview){Wire,Written},XMQTT_CLIENT_TO_SERVER,NULL,&Buffers,&Parsed,NULL)==XMQTT_CODEC_DONE && Parsed.Body.Connect.WillPayload.Size==65535,"maximum binary Will with NUL PFI data");
	Data[77]=255; codecWriteFailure(&Packet,XMQTT_CLIENT_TO_SERVER,NULL);
	/* Mutate the emitted PFI payload, preserving all lengths. */
	size_t Offset=(size_t)(Parsed.Body.Connect.WillPayload.Data-Wire); Wire[Offset+77u]=255;
	xmqttcodecissue Issue=codecReadFailure((xbytesview){Wire,Written},XMQTT_CLIENT_TO_SERVER,NULL,XMQTT_CODEC_PAYLOAD_ERROR); testRequire(Issue.Offset==Offset+77u,"Will payload UTF8 byte offset");
	free(Wire); free(Data);
}

static void testCodecApiAliases(void)
{
	xmqttpacket Packet, Before; xmqttPacketInit(XMQTT_PACKET_PUBLISH,&Packet); Packet.Body.Publish.Topic=XRT_STR_LITERAL("t");
	xmqttproperty Prop={0}; Prop.Id=XMQTT_PROPERTY_USER; Prop.Type=XMQTT_PROPERTY_STRING_PAIR; Prop.Value.Pair=(xmqttstringpair){XRT_STR_LITERAL("k"),XRT_STR_LITERAL("v")}; Packet.Properties=(xmqttpropertylist){&Prop,1};
	unsigned char Wire[256], BeforeWire[256]; memset(Wire,0xa5,sizeof(Wire)); memcpy(BeforeWire,Wire,sizeof(Wire)); size_t Written=888;
	testRequire(!xmqttPacketSize5(&Packet,XMQTT_CLIENT_TO_SERVER,NULL,(size_t*)&Packet) && !xmqttPacketWrite5(&Packet,sizeof(Packet),&Packet,XMQTT_CLIENT_TO_SERVER,NULL,&Written),"output aliases packet descriptor"); xrtClearError();
	testRequire(!xmqttPacketSize5(&Packet,XMQTT_CLIENT_TO_SERVER,NULL,(size_t*)&Prop) && !xmqttPacketWrite5(&Prop,sizeof(Prop),&Packet,XMQTT_CLIENT_TO_SERVER,NULL,&Written),"output aliases property vector"); xrtClearError();
	xmqttcodecconfig Config; xmqttCodecConfigInit(&Config); testRequire(!xmqttPacketSize5(&Packet,XMQTT_CLIENT_TO_SERVER,&Config,&Config.MaxPacketBytes),"size aliases config"); xrtClearError();
	Packet.Body.Publish.Topic=(xstrview){(cstr)Wire,1}; Wire[0]='t'; memcpy(BeforeWire,Wire,sizeof(Wire));
	testRequire(!xmqttPacketWrite5(Wire,sizeof(Wire),&Packet,XMQTT_CLIENT_TO_SERVER,NULL,&Written) && Written==888 && !memcmp(Wire,BeforeWire,sizeof(Wire)),"packet output aliases topic bytes"); xrtClearError();
	Packet.Body.Publish.Topic=XRT_STR_LITERAL("t"); Prop.Value.Pair.Value=(xstrview){(cstr)Wire,1};
	testRequire(!xmqttPacketWrite5(Wire,sizeof(Wire),&Packet,XMQTT_CLIENT_TO_SERVER,NULL,&Written),"packet output aliases property text"); xrtClearError();
	Prop.Value.Pair.Value=XRT_STR_LITERAL("v");
	union { size_t Alignment; unsigned char Data[256]; } Output;
	testRequire(!xmqttPacketWrite5(Output.Data,sizeof(Output.Data),&Packet,XMQTT_CLIENT_TO_SERVER,NULL,(size_t*)Output.Data),"Written aliases wire"); xrtClearError();
	Packet.Body.Publish.Payload=(xbytesview){NULL,1}; codecWriteFailure(&Packet,XMQTT_CLIENT_TO_SERVER,NULL);
	Packet.Body.Publish.Payload=(xbytesview){(cbytes)(uintptr_t)(UINTPTR_MAX-1u),4}; codecWriteFailure(&Packet,XMQTT_CLIENT_TO_SERVER,NULL);
	Packet.Body.Publish.Payload=(xbytesview){NULL,0}; Packet.Identifier=1; codecWriteFailure(&Packet,XMQTT_CLIENT_TO_SERVER,NULL); Packet.Identifier=0;
	Packet.Reason=1; codecWriteFailure(&Packet,XMQTT_CLIENT_TO_SERVER,NULL); Packet.Reason=0; Packet.SessionPresent=true; codecWriteFailure(&Packet,XMQTT_CLIENT_TO_SERVER,NULL);
	codecWriteFailure(NULL,XMQTT_CLIENT_TO_SERVER,NULL);
	xmqttsubscription Topic={XRT_STR_LITERAL("t"),0}; xmqttPacketInit(XMQTT_PACKET_SUBSCRIBE,&Packet); Packet.Identifier=1; Packet.Body.Topics=(xmqttsubscriptionlist){&Topic,SIZE_MAX};
	xmqttCodecConfigInit(&Config); Config.MaxTopics=SIZE_MAX; codecWriteFailure(&Packet,XMQTT_CLIENT_TO_SERVER,&Config);
	xmqttPacketInit(XMQTT_PACKET_CONNECT,&Packet); Packet.Body.Connect.WillQos=1; codecWriteFailure(&Packet,XMQTT_CLIENT_TO_SERVER,NULL);
	Packet.Body.Connect.WillQos=0; Packet.Body.Connect.Username=XRT_STR_LITERAL("u"); codecWriteFailure(&Packet,XMQTT_CLIENT_TO_SERVER,NULL);
	xmqttPacketInit(XMQTT_PACKET_PINGREQ,&Packet); memcpy(&Before,&Packet,sizeof(Packet));
	xmqttcodecissue Issue={XMQTT_CODEC_DONE,XMQTT_CODEC_OK,777,0,0,0}, BeforeIssue=Issue;
	xbytesview Input={codecFixtures[13].Data,codecFixtures[13].Size};
	testRequire(xmqttPacketRead5((xbytesview){NULL,1},XMQTT_CLIENT_TO_SERVER,NULL,NULL,&Packet,&Issue)==XMQTT_CODEC_LOCAL_ERROR &&
		!memcmp(&Packet,&Before,sizeof(Packet)) && !memcmp(&Issue,&BeforeIssue,sizeof(Issue)),"invalid API modified packet/issue"); xrtClearError();
	xmqttproperty Properties[4]; xmqttsubscription Topics[4]; xmqttcodecbuffers Buffers={Properties,4,Topics,4};
	testRequire(xmqttPacketRead5((xbytesview){(cbytes)Properties,1},XMQTT_CLIENT_TO_SERVER,NULL,&Buffers,&Packet,&Issue)==XMQTT_CODEC_LOCAL_ERROR,"read input/scratch alias"); xrtClearError();
	Buffers.TopicCapacity=SIZE_MAX; testRequire(xmqttPacketRead5(Input,XMQTT_CLIENT_TO_SERVER,NULL,&Buffers,&Packet,&Issue)==XMQTT_CODEC_LOCAL_ERROR,"scratch multiplication overflow"); xrtClearError();
	Buffers.TopicCapacity=4; Buffers.Topics=(xmqttsubscription*)Properties;
	testRequire(xmqttPacketRead5(Input,XMQTT_CLIENT_TO_SERVER,NULL,&Buffers,&Packet,&Issue)==XMQTT_CODEC_LOCAL_ERROR,"scratch outputs alias each other"); xrtClearError();
	Buffers.Topics=Topics;
	testRequire(xmqttPacketRead5(Input,XMQTT_CLIENT_TO_SERVER,NULL,&Buffers,(xmqttpacket*)&Buffers,&Issue)==XMQTT_CODEC_LOCAL_ERROR,"packet output aliases buffers metadata"); xrtClearError();
	for (unsigned Case=0; Case<6; ++Case) {
		xmqttCodecConfigInit(&Config); if (!Case) Config.Size=0; else if (Case==1) Config.MaxPacketBytes=1;
		else if (Case==2) Config.MaxPacketBytes=(size_t)XMQTT_PACKET_MAX_BYTES+1u; else if (Case==3) Config.MaxPayloadBytes=(size_t)XMQTT_VARIABLE_MAX+1u;
		else if (Case==4) Config.MaxPropertyBytes=(size_t)XMQTT_VARIABLE_MAX+5u; else Config.Utf8Flags=2;
		testRequire(xmqttPacketRead5(Input,XMQTT_CLIENT_TO_SERVER,&Config,NULL,&Packet,&Issue)==XMQTT_CODEC_LOCAL_ERROR && !memcmp(&Issue,&BeforeIssue,sizeof(Issue)),"invalid config changed issue"); xrtClearError();
	}
}

static uint32 codecRandom(uint32* State)
{
	*State^=*State<<13u; *State^=*State>>17u; *State^=*State<<5u; return *State;
}

static void testCodecMutations(void)
{
	uint32 State=0x76214892u;
	for (size_t Trial=0; Trial<30000; ++Trial) {
		unsigned char Wire[256], Output[256], Again[256]; size_t Case=codecRandom(&State)%(sizeof(codecFixtures)/sizeof(codecFixtures[0]));
		size_t Size=codecFixtures[Case].Size; memcpy(Wire,codecFixtures[Case].Data,Size); xmqttdirection Direction=codecFixtures[Case].Direction;
		if (Trial%3u==0) { Size=codecRandom(&State)%sizeof(Wire); for (size_t Index=0; Index<Size; ++Index) Wire[Index]=(unsigned char)codecRandom(&State); Direction=(xmqttdirection)(1u+Trial%2u); }
		else for (unsigned Index=0, Changes=codecRandom(&State)%5u; Index<Changes; ++Index) Wire[codecRandom(&State)%Size]=(unsigned char)codecRandom(&State);
		xmqttcodecconfig Config; xmqttCodecConfigInit(&Config); Config.MaxPacketBytes=256; Config.MaxPropertyBytes=128; Config.MaxProperties=16; Config.MaxTopics=16;
		xmqttproperty Props[16], Props2[16], BeforeProps[16]; xmqttsubscription Topics[16], Topics2[16], BeforeTopics[16];
		xmqttcodecbuffers Buffers={Props,16,Topics,16}, Buffers2={Props2,16,Topics2,16}; xmqttpacket Packet, Before, Parsed;
		memset(&Packet,0xa5,sizeof(Packet)); memcpy(&Before,&Packet,sizeof(Packet)); memset(Props,0xa5,sizeof(Props)); memcpy(BeforeProps,Props,sizeof(Props)); memset(Topics,0xa5,sizeof(Topics)); memcpy(BeforeTopics,Topics,sizeof(Topics));
		xmqttcodecstatus Status=xmqttPacketRead5((xbytesview){Wire,Size},Direction,&Config,&Buffers,&Packet,NULL);
		if (Status==XMQTT_CODEC_DONE) {
			size_t Written, Written2; testRequire(Packet.Wire.Data==Wire && Packet.Wire.Size<=Size && Packet.Properties.Count<=16,"mutated packet published bounds");
			codecPacketWithin(&Packet,&Buffers);
			testRequire(xmqttPacketWrite5(Output,sizeof(Output),&Packet,Direction,&Config,&Written) &&
				xmqttPacketRead5((xbytesview){Output,Written},Direction,&Config,&Buffers2,&Parsed,NULL)==XMQTT_CODEC_DONE &&
				xmqttPacketWrite5(Again,sizeof(Again),&Parsed,Direction,&Config,&Written2) && Written==Written2 && !memcmp(Output,Again,Written),"accepted mutation lacks stable canonical form");
		} else {
			testRequire(!memcmp(&Packet,&Before,sizeof(Packet)),"mutated packet partially published");
			if (Status==XMQTT_CODEC_MORE) testRequire(!memcmp(Props,BeforeProps,sizeof(Props)) && !memcmp(Topics,BeforeTopics,sizeof(Topics)),"mutated incomplete packet changed scratch");
		}
		xrtClearError();
	}
}

static void testCodecAllocation(void)
{
	const codecFixture* Fixture=&codecFixtures[18]; unsigned char Output[128]; xmqttproperty Properties[4]; xmqttcodecbuffers Buffers={Properties,4,NULL,0}; xmqttpacket Packet; size_t Size, Written;
	xrtSetErrorInfo(XERR_VALUE,"xrt.test",777,"ambient"); const xerror* Ambient=xrtGetError();
	#if defined(XRT_FEATURE_MEMORY_STATS)
	xmemstats Before, After; xrtMemStatsEnable(true); xrtMemStatsGet(&Before);
	#endif
	for (size_t Index=0; Index<10000; ++Index)
		testRequire(xmqttPacketRead5((xbytesview){Fixture->Data,Fixture->Size-1u},Fixture->Direction,NULL,&Buffers,&Packet,NULL)==XMQTT_CODEC_MORE &&
			xmqttPacketRead5((xbytesview){Fixture->Data,Fixture->Size},Fixture->Direction,NULL,&Buffers,&Packet,NULL)==XMQTT_CODEC_DONE &&
			xmqttPacketSize5(&Packet,Fixture->Direction,NULL,&Size) && xmqttPacketWrite5(Output,sizeof(Output),&Packet,Fixture->Direction,NULL,&Written) &&
			Size==Written && Written==Fixture->Size && !memcmp(Output,Fixture->Data,Written) && xrtGetError()==Ambient,"warm packet cycle allocations/ambient/wire");
	#if defined(XRT_FEATURE_MEMORY_STATS)
	xrtMemStatsGet(&After); xrtMemStatsEnable(false); testRequire(After.BlockAllocCalls==Before.BlockAllocCalls && After.MallocCalls==Before.MallocCalls &&
		After.CallocCalls==Before.CallocCalls && After.ReallocCalls==Before.ReallocCalls,"successful packet codec allocated");
	#endif
	xrtClearError(); xallocationbudget* Budget=NULL; xallocationscope Scope={0}; testRequire(xrtAllocationBudgetCreate(1,&Budget) && xrtAllocationBudgetEnter(Budget,&Scope),"codec quota enter");
	testRequire(xmqttPacketRead5((xbytesview){Fixture->Data,Fixture->Size},Fixture->Direction,NULL,&Buffers,&Packet,NULL)==XMQTT_CODEC_DONE &&
		xmqttPacketWrite5(Output,sizeof(Output),&Packet,Fixture->Direction,NULL,&Written),"valid packet codec depends on heap");
	const unsigned char Invalid[][16]={{0x40,2,0,0},{0xc1,0},{0x30,4,0,1,255,0},{0x30,6,0,1,'t',2,1,2},{0x32,9,0,1,'t',0,42,2,1,1,255}};
	const size_t Sizes[]={4,2,6,8,11}; const xmqttcodecstatus Statuses[]={XMQTT_CODEC_PROTOCOL_ERROR,XMQTT_CODEC_MALFORMED,XMQTT_CODEC_MALFORMED,XMQTT_CODEC_PROTOCOL_ERROR,XMQTT_CODEC_PAYLOAD_ERROR};
	for (size_t Index=0; Index<5; ++Index) {
		xmqttcodecissue Issue; testRequire(xmqttPacketRead5((xbytesview){Invalid[Index],Sizes[Index]},XMQTT_CLIENT_TO_SERVER,NULL,&Buffers,&Packet,&Issue)==Statuses[Index] &&
			Issue.Status==Statuses[Index] && xrtErrorKind(xrtGetError())==XERR_MEMORY,"diagnostic OOM hid own/nested packet classification");
	}
	xmqttcodecconfig Config; xmqttCodecConfigInit(&Config); Config.MaxPacketBytes=2; xmqttcodecissue Issue;
	testRequire(xmqttPacketRead5((xbytesview){Fixture->Data,2},Fixture->Direction,&Config,&Buffers,&Packet,&Issue)==XMQTT_CODEC_LOCAL_ERROR &&
		Issue.Error==XMQTT_CODEC_LIMIT && xrtErrorKind(xrtGetError())==XERR_MEMORY,"header size diagnostic OOM classification");
	testRequire(xrtAllocationBudgetLeave(&Scope),"codec quota leave"); xallocationbudgetinfo Info={0}; Info.Size=sizeof(Info);
	testRequire(xrtAllocationBudgetInfo(Budget,&Info) && Info.LiveBytes==0 && Info.Rejections!=0,"codec cold diagnostic quota leak"); xrtClearError(); xrtAllocationBudgetFree(Budget);
}

int main(void)
{
	testCodecFixtures(); testCodecRolesAndPacketBoundary(); testCodecReasons(); testCodecConnectFlags(); testCodecTopicOptions(); testCodecInvalid(); testCodecLimits(); testCodecBoundaries(); testCodecApiAliases(); testCodecMutations(); testCodecAllocation();
	printf("[PASS] xmqtt codec5: all15/directions/reasons/flags/topics/options/properties/PFI/limits/aliases/prefixes/atomicity/mutations/zero-heap/OOM\n"); return 0;
}
