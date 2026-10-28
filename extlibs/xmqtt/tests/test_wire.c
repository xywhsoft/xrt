#include "test.h"

static xbytesview mqttBytes(const void* Data, size_t Size) { return (xbytesview){(cbytes)Data, Size}; }

static void testMqttVbi(void)
{
	static const struct { uint32 Value; unsigned char Data[4]; size_t Size; } Cases[] = {
		{0,{0},1}, {127,{127},1}, {128,{128,1},2}, {321,{193,2},2}, {16383,{255,127},2},
		{16384,{128,128,1},3}, {2097151,{255,255,127},3}, {2097152,{128,128,128,1},4}, {268435455,{255,255,255,127},4}
	};
	for (size_t Index = 0; Index < sizeof(Cases)/sizeof(Cases[0]); ++Index) {
		unsigned char Output[6]; memset(Output, 0xa5, sizeof(Output)); size_t Written = 99;
		testRequire(xmqttVbiWrite(Output+1u, 4, Cases[Index].Value, &Written) && Written == Cases[Index].Size &&
			memcmp(Output+1u, Cases[Index].Data, Written) == 0 && Output[0] == 0xa5 && Output[Written+1u] == 0xa5, "MQTT VBI canonical wire fixture");
		for (size_t Prefix = 0; Prefix <= Written; ++Prefix) {
			uint32 Value = 12345; size_t Read = 54321;
			xmqttwirestatus Status = xmqttVbiRead(mqttBytes(Output+1u, Prefix), XMQTT_VERSION_5, &Value, &Read);
			testRequire(Prefix == Written ? (Status == XMQTT_WIRE_DONE && Value == Cases[Index].Value && Read == Written) :
				(Status == XMQTT_WIRE_MORE && Value == 12345 && Read == 54321), "MQTT fragmented VBI publication");
		}
	}
	/* Every two-byte prefix, checked with direct integer arithmetic. */
	for (unsigned Pair = 0; Pair < 65536u; ++Pair) {
		unsigned char Input[2] = {(unsigned char)(Pair/256u), (unsigned char)Pair};
		for (unsigned Version = 4; Version <= 5; ++Version) {
			uint32 Value = 12345; size_t Read = 54321;
			xmqttwirestatus Expected = Input[0] < 128u ? XMQTT_WIRE_DONE : Input[1] >= 128u ? XMQTT_WIRE_MORE :
				(Version == 5 && Input[1] == 0) ? XMQTT_WIRE_ERROR : XMQTT_WIRE_DONE;
			xmqttwirestatus Status = xmqttVbiRead(mqttBytes(Input, 2), (xmqttversion)Version, &Value, &Read);
			testRequire(Status == Expected, "exhaustive MQTT VBI prefix result");
			if (Status == XMQTT_WIRE_DONE) testRequire(Value == (Input[0] < 128u ? Input[0] : Input[0]-128u+128u*Input[1]) && Read == (Input[0] < 128u ? 1u : 2u), "MQTT VBI arithmetic oracle");
			else testRequire(Value == 12345 && Read == 54321, "MQTT incomplete/malformed VBI published outputs");
			xrtClearError();
		}
	}
	for (uint32 Number = 0; Number < 65536u; ++Number) {
		unsigned char Output[4]; size_t Written, Read; uint32 Value;
		testRequire(xmqttVbiWrite(Output, sizeof(Output), Number, &Written) && xmqttVbiRead(mqttBytes(Output, Written), XMQTT_VERSION_5, &Value, &Read) == XMQTT_WIRE_DONE &&
			Value == Number && Read == Written, "MQTT VBI exhaustive small round trip");
	}
	for (unsigned Count = 2; Count <= 4; ++Count) {
		unsigned char Input[5] = {128,128,128,0,0}; Input[Count-1u] = 0; uint32 Value = 12345; size_t Read = 54321;
		testRequire(xmqttVbiRead(mqttBytes(Input, Count+1u), XMQTT_VERSION_5, &Value, &Read) == XMQTT_WIRE_ERROR &&
			Value == 12345 && Read == 54321, "MQTT 5 accepted nonminimal VBI"); xrtClearError();
		testRequire(xmqttVbiRead(mqttBytes(Input, Count+1u), XMQTT_VERSION_311, &Value, &Read) == XMQTT_WIRE_DONE && Value == 0 && Read == Count, "MQTT 311 nonminimal compatibility");
	}
	unsigned char Bad[5] = {128,128,128,128,0}; uint32 Value = 12345; size_t Read = 54321;
	testRequire(xmqttVbiRead(mqttBytes(Bad, 4), XMQTT_VERSION_5, &Value, &Read) == XMQTT_WIRE_ERROR && Value == 12345 && Read == 54321,
		"MQTT fourth continuation bit waited for fifth byte"); xrtClearError();
}

static bool mqttHeaderAllowed(unsigned First, unsigned Version)
{
	static const unsigned char Allowed[] = {0x10,0x20,0x30,0x31,0x32,0x33,0x34,0x35,0x3a,0x3b,0x3c,0x3d,0x40,0x50,0x62,0x70,0x82,0x90,0xa2,0xb0,0xc0,0xd0,0xe0,0xf0};
	for (size_t Index = 0; Index < sizeof(Allowed); ++Index)
		if (First == Allowed[Index]) return First != 0xf0u || Version == 5;
	return false;
}

static void testMqttHeaders(void)
{
	for (unsigned Version = 4; Version <= 5; ++Version) {
		xmqttwireconfig Config; xmqttWireConfigInit(&Config); Config.Version = (xmqttversion)Version;
		for (unsigned First = 0; First < 256; ++First) {
			unsigned char Input[2] = {(unsigned char)First, 0}; xmqttfixedheader Header, Before; memset(&Header, 0xa5, sizeof(Header)); Before = Header;
			bool Allowed = mqttHeaderAllowed(First, Version); xmqttwirestatus Status = xmqttHeaderRead(mqttBytes(Input, 2), &Config, &Header);
			testRequire(Status == (Allowed ? XMQTT_WIRE_DONE : XMQTT_WIRE_ERROR), "MQTT fixed-header flag/version matrix");
			if (Allowed) testRequire(Header.Type == (xmqttpackettype)(First/16u) && Header.Flags == First%16u && Header.HeaderBytes == 2 && Header.PacketBytes == 2 && Header.RemainingLength == 0, "MQTT header descriptor");
			else testRequire(memcmp(&Header, &Before, sizeof(Header)) == 0, "MQTT invalid header published descriptor");
			xrtClearError();
			unsigned char Output[8], Saved[sizeof(Output)]; memset(Output, 0xa5, sizeof(Output)); memcpy(Saved, Output, sizeof(Output)); size_t Written = 54321;
			bool Encoded = xmqttHeaderWrite(Output+1u, 5, (xmqttpackettype)(First/16u), (uint8)(First%16u), 0, &Config, &Written);
			testRequire(Encoded == Allowed, "MQTT header writer flag matrix");
			if (Encoded) testRequire(Written == 2 && memcmp(Output+1u, Input, 2) == 0 && Output[0] == 0xa5 && Output[3] == 0xa5, "MQTT header wire/canary");
			else testRequire(Written == 54321 && memcmp(Output, Saved, sizeof(Output)) == 0, "MQTT header failure changed output");
			xrtClearError();
		}
	}
	unsigned char Maximum[] = {0x30,0xff,0xff,0xff,0x7f}; xmqttfixedheader Header, Before; memset(&Header, 0xa5, sizeof(Header)); Before = Header;
	testRequire(xmqttHeaderRead(mqttBytes(Maximum, sizeof(Maximum)), NULL, &Header) == XMQTT_WIRE_ERROR && memcmp(&Header, &Before, sizeof(Header)) == 0 &&
		xrtErrorCode(xrtGetError()) == XMQTT_WIRE_LIMIT, "MQTT advertised size bypassed cap before body buffering"); xrtClearError();
	xmqttwireconfig Config; xmqttWireConfigInit(&Config); Config.MaxPacketBytes = XMQTT_PACKET_MAX_BYTES;
	testRequire(xmqttHeaderRead(mqttBytes(Maximum, sizeof(Maximum)), &Config, &Header) == XMQTT_WIRE_DONE && Header.RemainingLength == XMQTT_VARIABLE_MAX &&
		Header.HeaderBytes == 5 && Header.PacketBytes == XMQTT_PACKET_MAX_BYTES, "MQTT protocol maximum header arithmetic");
	static const uint32 Lengths[] = {0,1,127,128,16383,16384,2097151,2097152,XMQTT_VARIABLE_MAX};
	for (size_t Index = 0; Index < sizeof(Lengths)/sizeof(Lengths[0]); ++Index) {
		unsigned char Output[5]; size_t Written;
		testRequire(xmqttHeaderWrite(Output, sizeof(Output), XMQTT_PACKET_PUBLISH, 2, Lengths[Index], &Config, &Written), "MQTT header length boundary encoding");
		for (size_t Prefix = 0; Prefix <= Written; ++Prefix) {
			Header = Before;
			xmqttwirestatus Status = xmqttHeaderRead(mqttBytes(Output, Prefix), &Config, &Header);
			testRequire(Prefix == Written ? (Status == XMQTT_WIRE_DONE && Header.PacketBytes == Written+Lengths[Index]) :
				(Status == XMQTT_WIRE_MORE && memcmp(&Header, &Before, sizeof(Header)) == 0), "MQTT fragmented fixed header");
		}
		Config.MaxPacketBytes = Written+Lengths[Index];
		testRequire(xmqttHeaderRead(mqttBytes(Output, Written), &Config, &Header) == XMQTT_WIRE_DONE, "MQTT exact total packet cap");
		if (Config.MaxPacketBytes > 2) {
			--Config.MaxPacketBytes;
			testRequire(xmqttHeaderRead(mqttBytes(Output, Written), &Config, &Header) == XMQTT_WIRE_ERROR && xrtErrorCode(xrtGetError()) == XMQTT_WIRE_LIMIT, "MQTT cap excluded header bytes"); xrtClearError();
		}
		Config.MaxPacketBytes = XMQTT_PACKET_MAX_BYTES;
	}
}

static void testMqttFrames(void)
{
	static unsigned char Input[16392]; static const size_t Lengths[] = {0,1,2,127,128,16383,16384};
	for (size_t Index = 0; Index < sizeof(Lengths)/sizeof(Lengths[0]); ++Index) {
		size_t HeaderBytes; testRequire(xmqttHeaderWrite(Input, 5, XMQTT_PACKET_PUBLISH, 0, (uint32)Lengths[Index], NULL, &HeaderBytes), "MQTT frame fixture");
		memset(Input+HeaderBytes, 0xff, Lengths[Index]); size_t Total = HeaderBytes+Lengths[Index]; Input[Total] = 0xc0; Input[Total+1u] = 0;
		for (size_t Prefix = 0; Prefix <= Total+2u; ++Prefix) {
			xmqttframe Frame, Saved; memset(&Frame, 0xa5, sizeof(Frame)); Saved = Frame;
			xmqttwirestatus Status = xmqttFrameRead(mqttBytes(Input, Prefix), NULL, &Frame);
			if (Prefix < Total) testRequire(Status == XMQTT_WIRE_MORE && memcmp(&Frame, &Saved, sizeof(Frame)) == 0, "MQTT partial frame published views");
			else testRequire(Status == XMQTT_WIRE_DONE && Frame.Header.HeaderBytes == HeaderBytes && Frame.Header.PacketBytes == Total &&
				Frame.Wire.Data == Input && Frame.Wire.Size == Total && Frame.Body.Data == Input+HeaderBytes && Frame.Body.Size == Lengths[Index], "MQTT borrowed/coalesced frame boundary");
		}
		xmqttframe Ping; testRequire(xmqttFrameRead(mqttBytes(Input+Total, 2), NULL, &Ping) == XMQTT_WIRE_DONE && Ping.Header.Type == XMQTT_PACKET_PINGREQ && Ping.Body.Size == 0, "MQTT coalesced suffix lost");
	}
	unsigned char Legacy[] = {0xc0,0x80,0x80,0x80,0}; xmqttwireconfig Config; xmqttWireConfigInit(&Config); Config.Version = XMQTT_VERSION_311; Config.MaxPacketBytes = 5;
	xmqttframe Frame; testRequire(xmqttFrameRead(mqttBytes(Legacy, sizeof(Legacy)), &Config, &Frame) == XMQTT_WIRE_DONE && Frame.Header.HeaderBytes == 5 && Frame.Wire.Size == 5, "MQTT legacy header bytes not counted");
	Config.MaxPacketBytes = 4; testRequire(xmqttFrameRead(mqttBytes(Legacy, sizeof(Legacy)), &Config, &Frame) == XMQTT_WIRE_ERROR && xrtErrorCode(xrtGetError()) == XMQTT_WIRE_LIMIT, "MQTT legacy size cap"); xrtClearError();
}

static size_t mqttUtf8Oracle(uint32 Scalar, unsigned char* Output)
{
	if (Scalar < 0x80u) { Output[0] = (unsigned char)Scalar; return 1; }
	if (Scalar < 0x800u) { Output[0] = (unsigned char)(0xc0u+Scalar/64u); Output[1] = (unsigned char)(0x80u+Scalar%64u); return 2; }
	if (Scalar < 0x10000u) {
		Output[0] = (unsigned char)(0xe0u+Scalar/4096u); Output[1] = (unsigned char)(0x80u+(Scalar/64u)%64u); Output[2] = (unsigned char)(0x80u+Scalar%64u); return 3;
	}
	Output[0] = (unsigned char)(0xf0u+Scalar/262144u); Output[1] = (unsigned char)(0x80u+(Scalar/4096u)%64u);
	Output[2] = (unsigned char)(0x80u+(Scalar/64u)%64u); Output[3] = (unsigned char)(0x80u+Scalar%64u); return 4;
}

static void testMqttUtf8(void)
{
	for (uint32 Scalar = 0; Scalar <= 0x10ffffu; ++Scalar) {
		if (Scalar >= 0xd800u && Scalar <= 0xdfffu) continue;
		unsigned char Bytes[4]; size_t Size = mqttUtf8Oracle(Scalar, Bytes), Offset = 12345;
		bool Valid = Scalar != 0, Strict = Valid && Scalar > 31u && !(Scalar >= 127u && Scalar <= 159u) &&
			!(Scalar >= 0xfdd0u && Scalar <= 0xfdefu) && Scalar%65536u < 65534u;
		testRequire(xmqttUtf8StringCheck((xstrview){(cstr)Bytes,Size}, 0, &Offset) == Valid && Offset == (Valid ? XRT_NPOS : 0), "MQTT Unicode scalar oracle"); xrtClearError();
		testRequire(xmqttUtf8StringCheck((xstrview){(cstr)Bytes,Size}, XMQTT_UTF8_REJECT_DISALLOWED, &Offset) == Strict && Offset == (Strict ? XRT_NPOS : 0), "MQTT disallowed Unicode scalar policy"); xrtClearError();
	}
	for (unsigned Pair = 0; Pair < 65536u; ++Pair) {
		unsigned char Bytes[2] = {(unsigned char)(Pair/256u),(unsigned char)Pair};
		bool Valid = (Bytes[0] > 0 && Bytes[0] < 128u && Bytes[1] > 0 && Bytes[1] < 128u) ||
			(Bytes[0] >= 0xc2u && Bytes[0] <= 0xdfu && Bytes[1] >= 0x80u && Bytes[1] <= 0xbfu);
		testRequire(xmqttUtf8StringCheck((xstrview){(cstr)Bytes,2}, 0, NULL) == Valid, "MQTT exhaustive two-byte UTF-8 grammar"); xrtClearError();
	}
	static const cstr Bad[] = {"\xed\xa0\x80", "\xe0\x80\x80", "\xf0\x80\x80\x80", "\xf4\x90\x80\x80", "\xf5\x80\x80\x80", "\xe4\xb8", "\xc0\x80"};
	for (size_t Index = 0; Index < sizeof(Bad)/sizeof(Bad[0]); ++Index) {
		char Text[16] = {'a','b'}; size_t Length = strlen(Bad[Index]); memcpy(Text+2u, Bad[Index], Length); size_t Offset = 12345;
		testRequire(!xmqttUtf8StringCheck((xstrview){Text,Length+2u}, 0, &Offset) && Offset == 2, "MQTT invalid UTF-8 offset"); xrtClearError();
	}
	char Null[] = {'a',0,'b'}; size_t Offset = 12345;
	testRequire(!xmqttUtf8StringCheck((xstrview){Null,3}, 0, &Offset) && Offset == 1, "MQTT NUL accepted"); xrtClearError();
	static const unsigned char Bom[] = {0xef,0xbb,0xbf,'a',0xef,0xbb,0xbf}; unsigned char Output[16]; size_t Written, Read; xstrview Text;
	testRequire(xmqttUtf8StringWrite(Output, sizeof(Output), (xstrview){(cstr)Bom,sizeof(Bom)}, XMQTT_UTF8_REJECT_DISALLOWED, &Written) &&
		xmqttUtf8StringRead(mqttBytes(Output,Written), XMQTT_UTF8_REJECT_DISALLOWED, &Text, &Read) == XMQTT_WIRE_DONE && Text.Size == sizeof(Bom) &&
		Text.Data == (cstr)Output+2u && memcmp(Text.Data,Bom,sizeof(Bom)) == 0, "MQTT BOM removed or text copied");
	testRequire(xmqttUtf8StringCheck((xstrview){NULL,0}, 0, &Offset) && Offset == XRT_NPOS, "MQTT empty UTF-8 string");
}

static void testMqttFields(void)
{
	static unsigned char Input[65540], Output[65540];
	memset(Input, 'a', 65536); size_t Written = 54321;
	testRequire(xmqttUtf8StringWrite(Output, sizeof(Output), (xstrview){(cstr)Input,65535}, 0, &Written) && Written == 65537 && Output[0] == 255 && Output[1] == 255, "MQTT uint16 string length boundary");
	for (size_t Prefix = 0; Prefix <= Written; ++Prefix) {
		xstrview Text = XRT_STR_LITERAL("sentinel"); size_t Read = 12345;
		xmqttwirestatus Status = xmqttUtf8StringRead(mqttBytes(Output,Prefix), 0, &Text, &Read);
		testRequire(Prefix == Written ? (Status == XMQTT_WIRE_DONE && Read == Written && Text.Data == (cstr)Output+2u && Text.Size == 65535) :
			(Status == XMQTT_WIRE_MORE && Read == 12345 && Text.Size == 8), "MQTT fragmented maximum string");
	}
	unsigned char Saved[32]; memset(Output, 0xa5, sizeof(Output)); memcpy(Saved, Output, sizeof(Saved)); Written = 54321;
	testRequire(!xmqttUtf8StringWrite(Output, sizeof(Output), (xstrview){(cstr)Input,65536}, 0, &Written) && Written == 54321 && memcmp(Output, Saved, sizeof(Saved)) == 0, "MQTT overlength string write changed output"); xrtClearError();
	size_t Offset = 12345; testRequire(!xmqttUtf8StringCheck((xstrview){(cstr)Input,65536}, 0, &Offset) && Offset == 65535, "MQTT overlength string check offset"); xrtClearError();
	for (size_t Index = 0; Index < 65535u; ++Index) Input[Index] = (unsigned char)Index;
	testRequire(xmqttBinaryWrite(Output, sizeof(Output), mqttBytes(Input,65535), &Written), "MQTT arbitrary binary write");
	xbytesview Binary; size_t Read;
	testRequire(xmqttBinaryRead(mqttBytes(Output,Written), &Binary, &Read) == XMQTT_WIRE_DONE && Binary.Data == Output+2u && Binary.Size == 65535 &&
		Read == Written && memcmp(Binary.Data,Input,65535) == 0, "MQTT arbitrary binary was converted/copied");
	xstrview Text = XRT_STR_LITERAL("sentinel"); Read = 12345;
	testRequire(xmqttUtf8StringRead(mqttBytes(Output,Written), 0, &Text, &Read) == XMQTT_WIRE_ERROR && Text.Size == 8 && Read == 12345, "MQTT binary treated as UTF-8 string"); xrtClearError();
	testRequire(xmqttBinaryWrite(Output, sizeof(Output), (xbytesview){NULL,0}, &Written) && Written == 2 && Output[0] == 0 && Output[1] == 0 &&
		xmqttBinaryRead(mqttBytes(Output,Written), &Binary, &Read) == XMQTT_WIRE_DONE && Binary.Size == 0 && Read == 2, "MQTT empty binary");
	unsigned char Coalesced[] = {0,1,'a',0,1,'b'};
	testRequire(xmqttUtf8StringRead(mqttBytes(Coalesced,sizeof(Coalesced)), 0, &Text, &Read) == XMQTT_WIRE_DONE && Read == 3 && Text.Size == 1 && Text.Data[0] == 'a', "MQTT field consumed coalesced suffix");
	unsigned char SplitScalar[] = {0,1,0xe4,0xb8,0xad}; Text = XRT_STR_LITERAL("sentinel"); Read = 12345;
	testRequire(xmqttUtf8StringRead(mqttBytes(SplitScalar,sizeof(SplitScalar)), 0, &Text, &Read) == XMQTT_WIRE_ERROR && Text.Size == 8 && Read == 12345,
		"MQTT UTF-8 scalar crossed its length-prefixed field boundary"); xrtClearError();
}

static void testMqttWriteFailures(void)
{
	unsigned char Output[64], Before[sizeof(Output)]; memset(Output,0xa5,sizeof(Output)); memcpy(Before,Output,sizeof(Output));
	xstrview Text = XRT_STR_LITERAL("a\xe4\xb8\xad");
	for (size_t Capacity = 0; Capacity < 6; ++Capacity) {
		size_t Written = 54321;
		testRequire(!xmqttUtf8StringWrite(Output,Capacity,Text,0,&Written) && Written == 54321 && memcmp(Output,Before,sizeof(Output)) == 0, "MQTT text capacity write was not atomic"); xrtClearError();
		testRequire(!xmqttBinaryWrite(Output,Capacity,mqttBytes(Text.Data,Text.Size),&Written) && Written == 54321 && memcmp(Output,Before,sizeof(Output)) == 0, "MQTT binary capacity write was not atomic"); xrtClearError();
		if (Capacity < 4) {
			testRequire(!xmqttVbiWrite(Output,Capacity,XMQTT_VARIABLE_MAX,&Written) && Written == 54321 && memcmp(Output,Before,sizeof(Output)) == 0, "MQTT VBI capacity atomicity"); xrtClearError();
		}
		if (Capacity < 5) {
			xmqttwireconfig Config; xmqttWireConfigInit(&Config); Config.MaxPacketBytes = XMQTT_PACKET_MAX_BYTES;
			testRequire(!xmqttHeaderWrite(Output,Capacity,XMQTT_PACKET_PUBLISH,0,XMQTT_VARIABLE_MAX,&Config,&Written) && Written == 54321 && memcmp(Output,Before,sizeof(Output)) == 0, "MQTT header capacity atomicity"); xrtClearError();
		}
	}
	size_t Written = 54321;
	testRequire(!xmqttVbiWrite(Output,sizeof(Output),XMQTT_VARIABLE_MAX+1u,&Written) && Written == 54321 && memcmp(Output,Before,sizeof(Output)) == 0, "MQTT VBI overflow atomicity"); xrtClearError();
	testRequire(!xmqttUtf8StringWrite(Output,sizeof(Output),XRT_STR_LITERAL("a\x00"),0,&Written) && Written == 54321 && memcmp(Output,Before,sizeof(Output)) == 0, "MQTT invalid UTF-8 write atomicity"); xrtClearError();
	testRequire(!xmqttHeaderWrite(Output,sizeof(Output),XMQTT_PACKET_PUBLISH,0,1024u*1024u,NULL,&Written) && Written == 54321 && memcmp(Output,Before,sizeof(Output)) == 0, "MQTT header advertised size atomicity"); xrtClearError();
	testRequire(!xmqttBinaryWrite(Output,sizeof(Output),mqttBytes(Output+3u,3),&Written) && Written == 54321 && memcmp(Output,Before,sizeof(Output)) == 0, "MQTT binary write source overlap"); xrtClearError();
	testRequire(!xmqttUtf8StringWrite(Output,sizeof(Output),XRT_STR_LITERAL("a"),0,(size_t*)Output) && memcmp(Output,Before,sizeof(Output)) == 0, "MQTT written overlaps wire"); xrtClearError();
	char Source[64] = "aaaa"; char Saved[sizeof(Source)]; memcpy(Saved,Source,sizeof(Source));
	testRequire(!xmqttUtf8StringWrite(Output,sizeof(Output),(xstrview){Source,4},0,(size_t*)Source) && memcmp(Source,Saved,sizeof(Source)) == 0 && memcmp(Output,Before,sizeof(Output)) == 0, "MQTT written overlaps source"); xrtClearError();
	testRequire(xmqttVbiWrite(Output,sizeof(Output),0,NULL) && xmqttHeaderWrite(Output,sizeof(Output),XMQTT_PACKET_PINGREQ,0,0,NULL,NULL) &&
		xmqttUtf8StringWrite(Output,sizeof(Output),XRT_STR_LITERAL("a"),0,NULL) && xmqttBinaryWrite(Output,sizeof(Output),(xbytesview){NULL,0},NULL), "MQTT optional written outputs");
}

static void testMqttApi(void)
{
	xmqttfixedheader Header, Saved; memset(&Header,0xa5,sizeof(Header)); Saved = Header;
	unsigned char Input[] = {0xc0,0}; xmqttwireconfig Config; xmqttWireConfigInit(&Config);
	for (unsigned Case = 0; Case < 5; ++Case) {
		xmqttWireConfigInit(&Config);
		if (Case == 0) Config.Size = 0; else if (Case == 1) Config.Version = (xmqttversion)3;
		else if (Case == 2) Config.MaxPacketBytes = 0; else if (Case == 3) Config.MaxPacketBytes = 1; else Config.MaxPacketBytes = (size_t)XMQTT_PACKET_MAX_BYTES+1u;
		testRequire(xmqttHeaderRead(mqttBytes(Input,2),&Config,&Header) == XMQTT_WIRE_ERROR && memcmp(&Header,&Saved,sizeof(Header)) == 0 && xrtErrorCode(xrtGetError()) == XMQTT_WIRE_ARGUMENT, "MQTT invalid config changed header"); xrtClearError();
	}
	testRequire(!xmqttWireConfigInit(NULL) && !xmqttWireConfigInit((xmqttwireconfig*)(uintptr_t)(UINTPTR_MAX-1u)), "MQTT invalid config output"); xrtClearError();
	testRequire(xmqttHeaderRead((xbytesview){NULL,1},NULL,&Header) == XMQTT_WIRE_ERROR && memcmp(&Header,&Saved,sizeof(Header)) == 0, "MQTT NULL input"); xrtClearError();
	testRequire(xmqttHeaderRead((xbytesview){(cbytes)(uintptr_t)(UINTPTR_MAX-1u),4},NULL,&Header) == XMQTT_WIRE_ERROR, "MQTT wrapping input"); xrtClearError();
	testRequire(xmqttHeaderRead(mqttBytes(Input,2),NULL,NULL) == XMQTT_WIRE_ERROR, "MQTT NULL header output"); xrtClearError();
	xmqttWireConfigInit(&Config); xmqttwireconfig ConfigSaved = Config;
	testRequire(xmqttHeaderRead(mqttBytes(Input,2),&Config,(xmqttfixedheader*)&Config) == XMQTT_WIRE_ERROR && memcmp(&Config,&ConfigSaved,sizeof(Config)) == 0, "MQTT header output overlaps config"); xrtClearError();
	union { xmqttframe Frame; xmqttfixedheader Header; unsigned char Data[128]; size_t Size; uint32 Value; } Storage;
	memset(&Storage,0xa5,sizeof(Storage)); Storage.Data[0] = 0xc0; Storage.Data[1] = 0; unsigned char Copy[sizeof(Storage)]; memcpy(Copy,&Storage,sizeof(Storage));
	testRequire(xmqttFrameRead(mqttBytes(Storage.Data,2),NULL,&Storage.Frame) == XMQTT_WIRE_ERROR && memcmp(Copy,&Storage,sizeof(Storage)) == 0, "MQTT frame output overlaps input"); xrtClearError();
	testRequire(xmqttHeaderRead(mqttBytes(Storage.Data,2),NULL,&Storage.Header) == XMQTT_WIRE_ERROR && memcmp(Copy,&Storage,sizeof(Storage)) == 0, "MQTT header output overlaps input"); xrtClearError();
	size_t Read = 54321; uint32 Value = 12345;
	testRequire(xmqttVbiRead(mqttBytes(Input,2),(xmqttversion)3,&Value,&Read) == XMQTT_WIRE_ERROR && Value == 12345 && Read == 54321, "MQTT invalid VBI version"); xrtClearError();
	testRequire(xmqttVbiRead(mqttBytes(Input,2),XMQTT_VERSION_5,&Storage.Value,&Storage.Size) == XMQTT_WIRE_ERROR && memcmp(Copy,&Storage,sizeof(Storage)) == 0, "MQTT VBI output overlap"); xrtClearError();
	xstrview Text = XRT_STR_LITERAL("sentinel");
	testRequire(xmqttUtf8StringRead(mqttBytes(Input,2),2u,&Text,&Read) == XMQTT_WIRE_ERROR && Text.Size == 8 && Read == 54321, "MQTT unknown UTF-8 policy"); xrtClearError();
	testRequire(!xmqttUtf8StringCheck(XRT_STR_LITERAL("a"),2u,&Read) && Read == 54321, "MQTT invalid check policy changed offset"); xrtClearError();
	testRequire(!xmqttUtf8StringCheck((xstrview){(cstr)Storage.Data,2},0,&Storage.Size) && memcmp(Copy,&Storage,sizeof(Storage)) == 0, "MQTT UTF-8 offset overlaps source"); xrtClearError();
	testRequire(!xmqttVbiWrite(NULL,4,0,&Read) && Read == 54321, "MQTT NULL write output"); xrtClearError();
	testRequire(!xmqttVbiWrite((ptr)(uintptr_t)(UINTPTR_MAX-1u),4,0,&Read) && Read == 54321, "MQTT wrapping write output"); xrtClearError();
	testRequire(!xmqttHeaderWrite(&Config,sizeof(Config),XMQTT_PACKET_PINGREQ,0,0,&Config,&Read) && memcmp(&Config,&ConfigSaved,sizeof(Config)) == 0, "MQTT header wire overlaps config"); xrtClearError();
	unsigned char Output[32]; memset(Output,0xa5,sizeof(Output));
	testRequire(!xmqttHeaderWrite(Output,sizeof(Output),XMQTT_PACKET_PINGREQ,0,0,&Config,&Config.Size) && Output[0] == 0xa5 && memcmp(&Config,&ConfigSaved,sizeof(Config)) == 0, "MQTT written-size overlaps config"); xrtClearError();
}

static uint32 mqttRandom(uint32* State)
{
	*State ^= *State << 13u; *State ^= *State >> 17u; *State ^= *State << 5u; return *State;
}

static void testMqttMutations(void)
{
	uint32 State = 0xaabbccddu;
	for (size_t Trial = 0; Trial < 20000; ++Trial) {
		unsigned char Input[256]; size_t Size = mqttRandom(&State)%sizeof(Input);
		for (size_t Index = 0; Index < Size; ++Index) Input[Index] = (unsigned char)mqttRandom(&State);
		xmqttwireconfig Config; xmqttWireConfigInit(&Config); Config.Version = (Trial&1u) ? XMQTT_VERSION_5 : XMQTT_VERSION_311; Config.MaxPacketBytes = 128;
		xmqttfixedheader Header, HBefore; xmqttframe Frame, FBefore; memset(&Header,0xa5,sizeof(Header)); HBefore = Header; memset(&Frame,0xa5,sizeof(Frame)); FBefore = Frame;
		xmqttwirestatus HStatus = xmqttHeaderRead(mqttBytes(Input,Size),&Config,&Header); xrtClearError();
		xmqttwirestatus FStatus = xmqttFrameRead(mqttBytes(Input,Size),&Config,&Frame);
		if (HStatus == XMQTT_WIRE_DONE) {
			testRequire(Header.HeaderBytes >= 2 && Header.HeaderBytes <= 5 && Header.PacketBytes <= 128 && Header.PacketBytes == Header.HeaderBytes+Header.RemainingLength, "MQTT mutation header bounds");
			testRequire(FStatus == (Size >= Header.PacketBytes ? XMQTT_WIRE_DONE : XMQTT_WIRE_MORE), "MQTT frame/header mutation disagreement");
		} else testRequire(memcmp(&Header,&HBefore,sizeof(Header)) == 0 && FStatus == HStatus, "MQTT mutation changed unpublished header");
		if (FStatus == XMQTT_WIRE_DONE) testRequire(Frame.Wire.Data == Input && Frame.Wire.Size <= Size && Frame.Body.Data == Input+Frame.Header.HeaderBytes && Frame.Body.Size == Frame.Header.RemainingLength, "MQTT mutation borrowed spans");
		else testRequire(memcmp(&Frame,&FBefore,sizeof(Frame)) == 0, "MQTT mutation published partial frame");
		xrtClearError();
	}
}

static void testMqttAllocation(void)
{
	static const unsigned char Packet[] = {0x32,0x0f,0,3,'a','/','b',0,1,0,'p','a','y','l','o','a','d'};
	unsigned char Output[128]; size_t Written, Read; uint32 Number; xmqttframe Frame; xstrview Text; xbytesview Binary;
	testRequire(xmqttFrameRead(mqttBytes(Packet,sizeof(Packet)),NULL,&Frame) == XMQTT_WIRE_DONE, "MQTT allocation fixture");
	xrtSetErrorInfo(XERR_VALUE,"xrt.test",777,"ambient"); const xerror* Ambient = xrtGetError();
	#if defined(XRT_FEATURE_MEMORY_STATS)
	xmemstats Before, After; xrtMemStatsEnable(true); xrtMemStatsGet(&Before);
	#endif
	for (size_t Index = 0; Index < 10000; ++Index) {
		testRequire(xmqttFrameRead(mqttBytes(Packet,sizeof(Packet)),NULL,&Frame) == XMQTT_WIRE_DONE &&
			xmqttUtf8StringRead(Frame.Body,0,&Text,&Read) == XMQTT_WIRE_DONE && xmqttUtf8StringCheck(Text,XMQTT_UTF8_REJECT_DISALLOWED,NULL) &&
			xmqttUtf8StringWrite(Output,sizeof(Output),Text,0,&Written) && xmqttBinaryRead(mqttBytes(Output,Written),&Binary,&Read) == XMQTT_WIRE_DONE &&
			xmqttBinaryWrite(Output,sizeof(Output),mqttBytes(Packet,3),&Written) && xmqttVbiWrite(Output,sizeof(Output),16384,&Written) &&
			xmqttVbiRead(mqttBytes(Output,Written),XMQTT_VERSION_5,&Number,&Read) == XMQTT_WIRE_DONE &&
			xmqttHeaderWrite(Output,sizeof(Output),XMQTT_PACKET_PINGREQ,0,0,NULL,&Written) && xrtGetError() == Ambient, "MQTT warm success changed ambient error");
	}
	#if defined(XRT_FEATURE_MEMORY_STATS)
	xrtMemStatsGet(&After); xrtMemStatsEnable(false);
	testRequire(After.BlockAllocCalls == Before.BlockAllocCalls && After.MallocCalls == Before.MallocCalls && After.CallocCalls == Before.CallocCalls && After.ReallocCalls == Before.ReallocCalls, "MQTT wire primitive allocated");
	#endif
	xrtClearError(); xallocationbudget* Budget = NULL; xallocationscope Scope = {0};
	testRequire(xrtAllocationBudgetCreate(1,&Budget) && xrtAllocationBudgetEnter(Budget,&Scope), "MQTT quota");
	testRequire(xmqttUtf8StringWrite(Output,sizeof(Output),XRT_STR_LITERAL("a\xe4\xb8\xad"),0,&Written) && xmqttUtf8StringRead(mqttBytes(Output,Written),0,&Text,&Read) == XMQTT_WIRE_DONE &&
		xmqttFrameRead(mqttBytes(Packet,sizeof(Packet)),NULL,&Frame) == XMQTT_WIRE_DONE, "MQTT wire success under quota");
	memset(Output,0xa5,sizeof(Output)); Written = 54321;
	testRequire(!xmqttUtf8StringWrite(Output,sizeof(Output),XRT_STR_LITERAL("a\x00"),0,&Written) && Written == 54321 && Output[0] == 0xa5 && xrtErrorKind(xrtGetError()) == XERR_MEMORY, "MQTT diagnostic OOM changed output");
	testRequire(xrtAllocationBudgetLeave(&Scope), "MQTT quota leave"); xallocationbudgetinfo Info = {0}; Info.Size = sizeof(Info);
	testRequire(xrtAllocationBudgetInfo(Budget,&Info) && Info.LiveBytes == 0 && Info.Rejections != 0, "MQTT wire quota leak");
	xrtClearError(); xrtAllocationBudgetFree(Budget);
}

int main(void)
{
	testMqttVbi(); testMqttHeaders(); testMqttFrames(); testMqttUtf8(); testMqttFields(); testMqttWriteFailures(); testMqttApi(); testMqttMutations(); testMqttAllocation();
	printf("[PASS] xmqtt wire: versions/VBI/flags/frame-bounds/Unicode/binary/atomicity/zero-heap/OOM\n"); return 0;
}
