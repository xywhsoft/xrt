#include "test.h"

static const struct { unsigned Id; xmqttpropertytype Type; cstr Scopes; } propertyCases[] = {
	{1,XMQTT_PROPERTY_BYTE," PUBLISH WILL "}, {2,XMQTT_PROPERTY_UINT32," PUBLISH WILL "},
	{3,XMQTT_PROPERTY_STRING," PUBLISH WILL "}, {8,XMQTT_PROPERTY_STRING," PUBLISH WILL "},
	{9,XMQTT_PROPERTY_BINARY," PUBLISH WILL "}, {11,XMQTT_PROPERTY_VARIABLE," PUBLISH SUBSCRIBE "},
	{17,XMQTT_PROPERTY_UINT32," CONNECT CONNACK DISCONNECT "}, {18,XMQTT_PROPERTY_STRING," CONNACK "},
	{19,XMQTT_PROPERTY_UINT16," CONNACK "}, {21,XMQTT_PROPERTY_STRING," CONNECT CONNACK AUTH "},
	{22,XMQTT_PROPERTY_BINARY," CONNECT CONNACK AUTH "}, {23,XMQTT_PROPERTY_BYTE," CONNECT "},
	{24,XMQTT_PROPERTY_UINT32," WILL "}, {25,XMQTT_PROPERTY_BYTE," CONNECT "}, {26,XMQTT_PROPERTY_STRING," CONNACK "},
	{28,XMQTT_PROPERTY_STRING," CONNACK DISCONNECT "},
	{31,XMQTT_PROPERTY_STRING," CONNACK PUBACK PUBREC PUBREL PUBCOMP SUBACK UNSUBACK DISCONNECT AUTH "},
	{33,XMQTT_PROPERTY_UINT16," CONNECT CONNACK "}, {34,XMQTT_PROPERTY_UINT16," CONNECT CONNACK "},
	{35,XMQTT_PROPERTY_UINT16," PUBLISH "}, {36,XMQTT_PROPERTY_BYTE," CONNACK "}, {37,XMQTT_PROPERTY_BYTE," CONNACK "},
	{38,XMQTT_PROPERTY_STRING_PAIR," CONNECT CONNACK PUBLISH PUBACK PUBREC PUBREL PUBCOMP SUBSCRIBE SUBACK UNSUBSCRIBE UNSUBACK DISCONNECT AUTH WILL "},
	{39,XMQTT_PROPERTY_UINT32," CONNECT CONNACK "}, {40,XMQTT_PROPERTY_BYTE," CONNACK "},
	{41,XMQTT_PROPERTY_BYTE," CONNACK "}, {42,XMQTT_PROPERTY_BYTE," CONNACK "}
};

static xmqttproperty propertyFixture(unsigned Id, xmqttpropertytype Type)
{
	static const unsigned char Binary[] = {0,255}; xmqttproperty Property = {0}; Property.Id = (xmqttpropertyid)Id; Property.Type = Type;
	if (Type == XMQTT_PROPERTY_BYTE) Property.Value.Number = 1;
	else if (Type == XMQTT_PROPERTY_UINT16) Property.Value.Number = 0x0102u;
	else if (Type == XMQTT_PROPERTY_UINT32) Property.Value.Number = 0x01020304u;
	else if (Type == XMQTT_PROPERTY_VARIABLE) Property.Value.Number = 321;
	else if (Type == XMQTT_PROPERTY_STRING) Property.Value.Text = XRT_STR_LITERAL("x");
	else if (Type == XMQTT_PROPERTY_BINARY) Property.Value.Binary = (xbytesview){Binary,2};
	else Property.Value.Pair = (xmqttstringpair){XRT_STR_LITERAL("k"),XRT_STR_LITERAL("v")};
	return Property;
}

/* Literal field fixtures, independent of the production property registry. */
static size_t propertyFixtureWire(unsigned char* Output, unsigned Id, xmqttpropertytype Type)
{
	Output[0] = (unsigned char)Id;
	if (Type == XMQTT_PROPERTY_BYTE) { Output[1]=1; return 2; }
	if (Type == XMQTT_PROPERTY_UINT16) { Output[1]=1; Output[2]=2; return 3; }
	if (Type == XMQTT_PROPERTY_UINT32) { Output[1]=1; Output[2]=2; Output[3]=3; Output[4]=4; return 5; }
	if (Type == XMQTT_PROPERTY_VARIABLE) { Output[1]=193; Output[2]=2; return 3; }
	if (Type == XMQTT_PROPERTY_STRING) { Output[1]=0; Output[2]=1; Output[3]='x'; return 4; }
	if (Type == XMQTT_PROPERTY_BINARY) { Output[1]=0; Output[2]=2; Output[3]=0; Output[4]=255; return 5; }
	memcpy(Output+1,"\x00\x01k\x00\x01v",6); return 7;
}

static bool propertyDirection(unsigned Scope, unsigned Direction)
{
	if (Scope==1 || Scope==8 || Scope==10 || Scope==16) return Direction==1;
	if (Scope==2 || Scope==9 || Scope==11) return Direction==2;
	return true;
}

static void propertyReadFail(xbytesview Input, xmqttpropertyscope Scope, xmqttdirection Direction,
	const xmqttpropertyconfig* Config, size_t Capacity, xmqttpropertystatus Expected)
{
	xmqttproperty Scratch[8]; testRequire(Capacity<=8,"property fail fixture capacity");
	xmqttpropertydocument Doc = {77,88,{NULL,0}};
	testRequire(xmqttPropertiesRead(Input,Scope,Direction,Config,Scratch,Capacity,&Doc)==Expected && Doc.Count==77 && Doc.Consumed==88 && Doc.Raw.Data==NULL,
		"MQTT property failure classification/publication"); xrtClearError();
}

static void propertyWriteFail(const xmqttproperty* Properties, size_t Count, xmqttpropertyscope Scope,
	xmqttdirection Direction, const xmqttpropertyconfig* Config)
{
	unsigned char Output[512], Before[sizeof(Output)]; memset(Output,0xa5,sizeof(Output)); memcpy(Before,Output,sizeof(Output));
	size_t Size=12345, Written=54321;
	testRequire(!xmqttPropertiesSize(Properties,Count,Scope,Direction,Config,&Size) && Size==12345,"MQTT property failed size publication"); xrtClearError();
	testRequire(!xmqttPropertiesWrite(Output,sizeof(Output),Properties,Count,Scope,Direction,Config,&Written) && Written==54321 &&
		memcmp(Output,Before,sizeof(Output))==0,"MQTT property failed write was not atomic"); xrtClearError();
}

static void testPropertyMatrix(void)
{
	static const cstr Names[17]={NULL,"CONNECT","CONNACK","PUBLISH","PUBACK","PUBREC","PUBREL","PUBCOMP","SUBSCRIBE","SUBACK","UNSUBSCRIBE","UNSUBACK",NULL,NULL,"DISCONNECT","AUTH","WILL"};
	for (size_t Case=0; Case<sizeof(propertyCases)/sizeof(propertyCases[0]); ++Case) {
		for (unsigned Scope=1; Scope<=16; ++Scope) {
			if (!Names[Scope]) continue;
			char Token[32]; snprintf(Token,sizeof(Token)," %s ",Names[Scope]);
			for (unsigned Direction=1; Direction<=2; ++Direction) {
				xmqttproperty Input[3]; Input[0]=propertyFixture(propertyCases[Case].Id,propertyCases[Case].Type); size_t Count=1;
				unsigned char Wire[64]; size_t Bytes=propertyFixtureWire(Wire+1,propertyCases[Case].Id,propertyCases[Case].Type);
				if ((Scope==15 && Input[0].Id!=21) || (Scope==1 && Input[0].Id==22)) {
					Input[1]=propertyFixture(21,XMQTT_PROPERTY_STRING); Input[1].Value.Text=XRT_STR_LITERAL("m"); Count=2;
					memcpy(Wire+1u+Bytes,"\x15\x00\x01m",4); Bytes+=4;
				}
				Wire[0]=(unsigned char)Bytes; ++Bytes;
				xmqttpropertystatus Expected=!propertyDirection(Scope,Direction) ? XMQTT_PROPERTIES_LOCAL_ERROR :
					strstr(propertyCases[Case].Scopes,Token)==NULL ? XMQTT_PROPERTIES_MALFORMED :
					((Input[0].Id==11 && Scope==3 && Direction==1) || (Input[0].Id==17 && Scope==14 && Direction==2)) ? XMQTT_PROPERTIES_PROTOCOL_ERROR : XMQTT_PROPERTIES_DONE;
				xmqttproperty Parsed[8]; xmqttpropertydocument Doc={77,88,{NULL,0}};
				xmqttpropertystatus Status=xmqttPropertiesRead((xbytesview){Wire,Bytes},(xmqttpropertyscope)Scope,(xmqttdirection)Direction,NULL,Parsed,8,&Doc);
				testRequire(Status==Expected,"MQTT property 27-id/scope/direction matrix");
				if (Expected==XMQTT_PROPERTIES_DONE) {
					testRequire(Doc.Count==Count && Doc.Consumed==Bytes && Doc.Raw.Data==Wire && Parsed[0].Id==Input[0].Id && Parsed[0].Type==Input[0].Type,"MQTT property descriptor matrix");
					if (Parsed[0].Type<=XMQTT_PROPERTY_VARIABLE) testRequire(Parsed[0].Value.Number==Input[0].Value.Number,"MQTT property numeric endian fixture");
					else if (Parsed[0].Type==XMQTT_PROPERTY_STRING) testRequire(Parsed[0].Value.Text.Size==1 && Parsed[0].Value.Text.Data[0]=='x',"MQTT property string fixture");
					else if (Parsed[0].Type==XMQTT_PROPERTY_BINARY) testRequire(Parsed[0].Value.Binary.Size==2 && Parsed[0].Value.Binary.Data[0]==0 && Parsed[0].Value.Binary.Data[1]==255,"MQTT binary property fixture");
					else testRequire(Parsed[0].Value.Pair.Name.Data[0]=='k' && Parsed[0].Value.Pair.Value.Data[0]=='v',"MQTT string-pair fixture");
					unsigned char Output[64]; memset(Output,0xa5,sizeof(Output)); size_t Required, Written;
					testRequire(xmqttPropertiesSize(Input,Count,(xmqttpropertyscope)Scope,(xmqttdirection)Direction,NULL,&Required) && Required==Bytes &&
						xmqttPropertiesWrite(Output+1u,Bytes,Input,Count,(xmqttpropertyscope)Scope,(xmqttdirection)Direction,NULL,&Written) && Written==Bytes &&
						memcmp(Output+1u,Wire,Bytes)==0 && Output[0]==0xa5 && Output[Bytes+1u]==0xa5,"MQTT property exact wire fixture/canary");
					bool Repeat=Input[0].Id==38 || (Input[0].Id==11 && Scope==3);
					Input[Count++]=Input[0];
					bool Valid=xmqttPropertiesSize(Input,Count,(xmqttpropertyscope)Scope,(xmqttdirection)Direction,NULL,&Required);
					testRequire(Valid==Repeat,"MQTT singleton/repeat matrix"); xrtClearError();
					unsigned char Duplicate[64]; memcpy(Duplicate,Wire,Bytes);
					size_t Extra=propertyFixtureWire(Duplicate+Bytes,propertyCases[Case].Id,propertyCases[Case].Type);
					Duplicate[0]=(unsigned char)(Bytes+Extra-1u); Doc=(xmqttpropertydocument){77,88,{NULL,0}};
					Status=xmqttPropertiesRead((xbytesview){Duplicate,Bytes+Extra},(xmqttpropertyscope)Scope,(xmqttdirection)Direction,NULL,Parsed,8,&Doc);
					testRequire(Repeat ? (Status==XMQTT_PROPERTIES_DONE && Doc.Count==Count) : (Status==XMQTT_PROPERTIES_PROTOCOL_ERROR && Doc.Count==77),"MQTT decoded singleton/repeat matrix"); xrtClearError();
				} else {
					testRequire(Doc.Count==77 && Doc.Consumed==88,"MQTT invalid property document changed"); xrtClearError();
					propertyWriteFail(Input,Count,(xmqttpropertyscope)Scope,(xmqttdirection)Direction,NULL);
				}
			}
		}
	}
}

static void testPropertyInvalid(void)
{
	static const struct { unsigned char Data[16]; size_t Size; xmqttpropertystatus Status; } Cases[]={
		{{1,0},2,XMQTT_PROPERTIES_MALFORMED}, {{1,7},2,XMQTT_PROPERTIES_MALFORMED}, {{1,127},2,XMQTT_PROPERTIES_MALFORMED},
		{{2,0x81,0},3,XMQTT_PROPERTIES_MALFORMED}, {{0x80,0},2,XMQTT_PROPERTIES_MALFORMED},
		{{0x80,0x80,0x80,0x80},4,XMQTT_PROPERTIES_MALFORMED}, {{1,0x80},2,XMQTT_PROPERTIES_MALFORMED},
		{{1,1},2,XMQTT_PROPERTIES_MALFORMED}, {{2,1,2},3,XMQTT_PROPERTIES_PROTOCOL_ERROR},
		{{4,1,1,1,0},5,XMQTT_PROPERTIES_PROTOCOL_ERROR}, {{3,35,0,0},4,XMQTT_PROPERTIES_PROTOCOL_ERROR},
		{{2,11,0},3,XMQTT_PROPERTIES_PROTOCOL_ERROR}, {{3,11,0x80,0},4,XMQTT_PROPERTIES_MALFORMED},
		{{2,11,0x80},3,XMQTT_PROPERTIES_MALFORMED}, {{3,3,0,1},4,XMQTT_PROPERTIES_MALFORMED},
		{{4,3,0,1,0},5,XMQTT_PROPERTIES_MALFORMED}, {{4,3,0,1,255},5,XMQTT_PROPERTIES_MALFORMED},
		{{3,8,0,0},4,XMQTT_PROPERTIES_PROTOCOL_ERROR}, {{4,8,0,1,'+'},5,XMQTT_PROPERTIES_PROTOCOL_ERROR},
		{{4,8,0,1,'#'},5,XMQTT_PROPERTIES_PROTOCOL_ERROR}, {{4,38,0,1,'k'},5,XMQTT_PROPERTIES_MALFORMED},
		/* Trailing packet payload cannot complete a truncated property string. */
		{{3,3,0,2,'a','b'},6,XMQTT_PROPERTIES_MALFORMED}, {{4,3,0,1,0xe4,0xb8,0xad},7,XMQTT_PROPERTIES_MALFORMED}
	};
	for (size_t Index=0; Index<sizeof(Cases)/sizeof(Cases[0]); ++Index)
		propertyReadFail((xbytesview){Cases[Index].Data,Cases[Index].Size},XMQTT_PROPERTIES_PUBLISH,XMQTT_SERVER_TO_CLIENT,NULL,8,Cases[Index].Status);
	unsigned char DataOnly[]={3,22,0,0};
	propertyReadFail((xbytesview){DataOnly,sizeof(DataOnly)},XMQTT_PROPERTIES_CONNECT,XMQTT_CLIENT_TO_SERVER,NULL,8,XMQTT_PROPERTIES_PROTOCOL_ERROR);
	unsigned char Empty[]={0}; propertyReadFail((xbytesview){Empty,1},XMQTT_PROPERTIES_AUTH,XMQTT_SERVER_TO_CLIENT,NULL,8,XMQTT_PROPERTIES_PROTOCOL_ERROR);
	/* CONNACK authentication depends on prior CONNECT state, not a synthetic
	 * mandatory Method invented by this standalone section codec. */
	xmqttproperty Parsed[4]; xmqttpropertydocument Doc;
	testRequire(xmqttPropertiesRead((xbytesview){DataOnly,sizeof(DataOnly)},XMQTT_PROPERTIES_CONNACK,XMQTT_SERVER_TO_CLIENT,NULL,Parsed,4,&Doc)==XMQTT_PROPERTIES_DONE,"CONNACK auth data overconstrained without session context");
	static const unsigned BoolIds[]={1,23,25,36,37,40,41,42};
	for (size_t Index=0; Index<sizeof(BoolIds)/sizeof(BoolIds[0]); ++Index) {
		xmqttpropertyscope Scope=BoolIds[Index]==1 ? XMQTT_PROPERTIES_PUBLISH : (BoolIds[Index]==23 || BoolIds[Index]==25) ? XMQTT_PROPERTIES_CONNECT : XMQTT_PROPERTIES_CONNACK;
		xmqttdirection Direction=Scope==XMQTT_PROPERTIES_CONNECT ? XMQTT_CLIENT_TO_SERVER : XMQTT_SERVER_TO_CLIENT;
		for (unsigned Number=0; Number<256; ++Number) {
			unsigned char Wire[]={2,(unsigned char)BoolIds[Index],(unsigned char)Number};
			xmqttpropertystatus Status=xmqttPropertiesRead((xbytesview){Wire,3},Scope,Direction,NULL,Parsed,4,&Doc);
			testRequire(Status==(Number<2 ? XMQTT_PROPERTIES_DONE : XMQTT_PROPERTIES_PROTOCOL_ERROR),"MQTT boolean/maximum-QoS value domain"); xrtClearError();
		}
	}
	static const struct { unsigned Id; xmqttpropertytype Type; xmqttpropertyscope Scope; } Positive[]={
		{11,XMQTT_PROPERTY_VARIABLE,XMQTT_PROPERTIES_PUBLISH},{33,XMQTT_PROPERTY_UINT16,XMQTT_PROPERTIES_CONNACK},
		{35,XMQTT_PROPERTY_UINT16,XMQTT_PROPERTIES_PUBLISH},{39,XMQTT_PROPERTY_UINT32,XMQTT_PROPERTIES_CONNACK}
	};
	for (size_t Index=0; Index<sizeof(Positive)/sizeof(Positive[0]); ++Index) {
		xmqttproperty Property=propertyFixture(Positive[Index].Id,Positive[Index].Type); Property.Value.Number=0;
		propertyWriteFail(&Property,1,Positive[Index].Scope,XMQTT_SERVER_TO_CLIENT,NULL);
	}
	xmqttproperty Property=propertyFixture(19,XMQTT_PROPERTY_UINT16); Property.Value.Number=65536;
	propertyWriteFail(&Property,1,XMQTT_PROPERTIES_CONNACK,XMQTT_SERVER_TO_CLIENT,NULL);
	Property=propertyFixture(11,XMQTT_PROPERTY_VARIABLE); Property.Value.Number=XMQTT_VARIABLE_MAX+1u;
	propertyWriteFail(&Property,1,XMQTT_PROPERTIES_PUBLISH,XMQTT_SERVER_TO_CLIENT,NULL);
	Property=propertyFixture(34,XMQTT_PROPERTY_UINT16); Property.Value.Number=0; size_t Required;
	testRequire(xmqttPropertiesSize(&Property,1,XMQTT_PROPERTIES_CONNACK,XMQTT_SERVER_TO_CLIENT,NULL,&Required),"zero topic alias maximum is legal");
	Property=propertyFixture(39,XMQTT_PROPERTY_UINT32); Property.Value.Number=UINT32_MAX;
	testRequire(xmqttPropertiesSize(&Property,1,XMQTT_PROPERTIES_CONNECT,XMQTT_CLIENT_TO_SERVER,NULL,&Required),"full uint32 maximum packet property narrowed");
}

static void testPropertyLimits(void)
{
	static unsigned char Data[65536], Wire[65550]; memset(Data,0xab,sizeof(Data));
	xmqttproperty Property=propertyFixture(9,XMQTT_PROPERTY_BINARY); xmqttpropertyconfig Config; xmqttPropertiesConfigInit(&Config);
	static const size_t Lengths[]={124,125,16380,16381,65535};
	for (size_t Index=0; Index<sizeof(Lengths)/sizeof(Lengths[0]); ++Index) {
		Property.Value.Binary=(xbytesview){Data,Lengths[Index]}; size_t Required, Written;
		testRequire(xmqttPropertiesSize(&Property,1,XMQTT_PROPERTIES_PUBLISH,XMQTT_SERVER_TO_CLIENT,NULL,&Required) &&
			xmqttPropertiesWrite(Wire,sizeof(Wire),&Property,1,XMQTT_PROPERTIES_PUBLISH,XMQTT_SERVER_TO_CLIENT,NULL,&Written) && Required==Written,"MQTT property length transition fixture");
		for (size_t Prefix=0; Prefix<Written; ++Prefix) {
			xmqttproperty Parsed, Before; memset(&Parsed,0xa5,sizeof(Parsed)); memcpy(&Before,&Parsed,sizeof(Parsed)); xmqttpropertydocument Doc={77,88,{NULL,0}};
			testRequire(xmqttPropertiesRead((xbytesview){Wire,Prefix},XMQTT_PROPERTIES_PUBLISH,XMQTT_SERVER_TO_CLIENT,NULL,&Parsed,1,&Doc)==XMQTT_PROPERTIES_MORE &&
				Doc.Count==77 && Doc.Consumed==88 && memcmp(&Parsed,&Before,sizeof(Parsed))==0,"MQTT property prefix published scratch/document");
		}
		xmqttproperty Parsed; xmqttpropertydocument Doc;
		testRequire(xmqttPropertiesRead((xbytesview){Wire,Written},XMQTT_PROPERTIES_PUBLISH,XMQTT_SERVER_TO_CLIENT,NULL,&Parsed,1,&Doc)==XMQTT_PROPERTIES_DONE &&
			Doc.Consumed==Written && Parsed.Value.Binary.Size==Lengths[Index] && memcmp(Parsed.Value.Binary.Data,Data,Lengths[Index])==0,"MQTT maximum binary property view");
		Config.MaxBytes=Written; testRequire(xmqttPropertiesSize(&Property,1,XMQTT_PROPERTIES_PUBLISH,XMQTT_SERVER_TO_CLIENT,&Config,&Required),"MQTT exact property byte cap");
		Config.MaxBytes=Written-1u; propertyWriteFail(&Property,1,XMQTT_PROPERTIES_PUBLISH,XMQTT_SERVER_TO_CLIENT,&Config);
		/* Size rejection is early, even without the advertised body. */
		propertyReadFail((xbytesview){Wire,4},XMQTT_PROPERTIES_PUBLISH,XMQTT_SERVER_TO_CLIENT,&Config,1,XMQTT_PROPERTIES_LOCAL_ERROR);
		xmqttPropertiesConfigInit(&Config);
	}
	Property.Value.Binary=(xbytesview){Data,65536}; propertyWriteFail(&Property,1,XMQTT_PROPERTIES_PUBLISH,XMQTT_SERVER_TO_CLIENT,NULL);
	unsigned char Empty[]={0}; xmqttpropertydocument Doc;
	Config.MaxProperties=0; Config.MaxBytes=1;
	testRequire(xmqttPropertiesRead((xbytesview){Empty,1},XMQTT_PROPERTIES_CONNECT,XMQTT_CLIENT_TO_SERVER,&Config,NULL,0,&Doc)==XMQTT_PROPERTIES_DONE && Doc.Count==0,
		"MQTT zero count limit rejected empty section");
	unsigned char Output=255; size_t Written;
	testRequire(xmqttPropertiesWrite(&Output,1,NULL,0,XMQTT_PROPERTIES_CONNECT,XMQTT_CLIENT_TO_SERVER,&Config,&Written) && Output==0 && Written==1,"MQTT empty property section exact one-byte buffer");
	Property=propertyFixture(1,XMQTT_PROPERTY_BYTE); propertyWriteFail(&Property,1,XMQTT_PROPERTIES_PUBLISH,XMQTT_SERVER_TO_CLIENT,&Config);
	unsigned char Single[]={2,1,1};
	propertyReadFail((xbytesview){Single,3},XMQTT_PROPERTIES_PUBLISH,XMQTT_SERVER_TO_CLIENT,NULL,0,XMQTT_PROPERTIES_LOCAL_ERROR);
	xmqttPropertiesConfigInit(&Config); Config.MaxProperties=0;
	propertyReadFail((xbytesview){Single,3},XMQTT_PROPERTIES_PUBLISH,XMQTT_SERVER_TO_CLIENT,&Config,1,XMQTT_PROPERTIES_LOCAL_ERROR);
	unsigned char Control[]={4,3,0,1,1}; Config.Utf8Flags=XMQTT_UTF8_REJECT_DISALLOWED; Config.MaxProperties=1;
	propertyReadFail((xbytesview){Control,5},XMQTT_PROPERTIES_PUBLISH,XMQTT_SERVER_TO_CLIENT,&Config,1,XMQTT_PROPERTIES_MALFORMED);
	Property=propertyFixture(3,XMQTT_PROPERTY_STRING); Property.Value.Text=XRT_STR_LITERAL("\x01"); propertyWriteFail(&Property,1,XMQTT_PROPERTIES_PUBLISH,XMQTT_SERVER_TO_CLIENT,&Config);
	xmqttPropertiesConfigInit(&Config); Config.MaxBytes=1; Property.Value.Text=XRT_STR_LITERAL("\xff"); size_t Required=12345;
	testRequire(!xmqttPropertiesSize(&Property,1,XMQTT_PROPERTIES_PUBLISH,XMQTT_SERVER_TO_CLIENT,&Config,&Required) && Required==12345 &&
		xrtErrorCode(xrtGetError())==XMQTT_PROPERTY_LIMIT,"MQTT known byte-budget failure did not precede UTF-8 validation"); xrtClearError();
}

static void testPropertyAtomicity(void)
{
	xmqttproperty Input[3]={0}; Input[0]=propertyFixture(38,XMQTT_PROPERTY_STRING_PAIR); Input[1]=Input[0]; Input[1].Value.Pair.Value=XRT_STR_LITERAL("v2");
	Input[2]=propertyFixture(11,XMQTT_PROPERTY_VARIABLE); size_t Size;
	testRequire(xmqttPropertiesSize(Input,3,XMQTT_PROPERTIES_PUBLISH,XMQTT_SERVER_TO_CLIENT,NULL,&Size),"MQTT atomic fixture");
	unsigned char Output[256], Before[sizeof(Output)]; memset(Output,0xa5,sizeof(Output)); memcpy(Before,Output,sizeof(Output));
	for (size_t Capacity=0; Capacity<Size; ++Capacity) {
		size_t Written=54321;
		testRequire(!xmqttPropertiesWrite(Output,Capacity,Input,3,XMQTT_PROPERTIES_PUBLISH,XMQTT_SERVER_TO_CLIENT,NULL,&Written) && Written==54321 &&
			memcmp(Output,Before,sizeof(Output))==0,"MQTT property writer capacity atomicity"); xrtClearError();
	}
	xmqttproperty Saved[3]; memcpy(Saved,Input,sizeof(Input));
	testRequire(!xmqttPropertiesWrite(Input,sizeof(Input),Input,3,XMQTT_PROPERTIES_PUBLISH,XMQTT_SERVER_TO_CLIENT,NULL,NULL) && memcmp(Input,Saved,sizeof(Input))==0,"MQTT property wire overlaps descriptors"); xrtClearError();
	testRequire(!xmqttPropertiesSize(Input,3,XMQTT_PROPERTIES_PUBLISH,XMQTT_SERVER_TO_CLIENT,NULL,&Input[0].Value.Pair.Name.Size) && memcmp(Input,Saved,sizeof(Input))==0,"MQTT property Required overlaps descriptors"); xrtClearError();
	testRequire(!xmqttPropertiesWrite(Output,sizeof(Output),Input,3,XMQTT_PROPERTIES_PUBLISH,XMQTT_SERVER_TO_CLIENT,NULL,(size_t*)Output) && memcmp(Output,Before,sizeof(Output))==0,"MQTT property Written overlaps wire"); xrtClearError();
	xmqttpropertyconfig Config; xmqttPropertiesConfigInit(&Config); xmqttpropertyconfig ConfigBefore=Config;
	testRequire(!xmqttPropertiesWrite(&Config,sizeof(Config),Input,1,XMQTT_PROPERTIES_PUBLISH,XMQTT_SERVER_TO_CLIENT,&Config,NULL) && memcmp(&Config,&ConfigBefore,sizeof(Config))==0,"MQTT property wire overlaps config"); xrtClearError();
	for (unsigned View=0; View<4; ++View) {
		char Data[256]="abc"; char SavedData[sizeof(Data)]; memcpy(SavedData,Data,sizeof(Data)); xmqttproperty Property;
		if (View==0) { Property=propertyFixture(3,XMQTT_PROPERTY_STRING); Property.Value.Text=(xstrview){Data,3}; }
		else if (View==1) { Property=propertyFixture(9,XMQTT_PROPERTY_BINARY); Property.Value.Binary=(xbytesview){(cbytes)Data,3}; }
		else { Property=propertyFixture(38,XMQTT_PROPERTY_STRING_PAIR); if (View==2) Property.Value.Pair.Name=(xstrview){Data,3}; else Property.Value.Pair.Value=(xstrview){Data,3}; }
		testRequire(!xmqttPropertiesWrite(Data,sizeof(Data),&Property,1,XMQTT_PROPERTIES_PUBLISH,XMQTT_SERVER_TO_CLIENT,NULL,NULL) && memcmp(Data,SavedData,sizeof(Data))==0,"MQTT property wire overlaps active view"); xrtClearError();
		testRequire(!xmqttPropertiesSize(&Property,1,XMQTT_PROPERTIES_PUBLISH,XMQTT_SERVER_TO_CLIENT,NULL,(size_t*)Data) && memcmp(Data,SavedData,sizeof(Data))==0,"MQTT Required overlaps active view"); xrtClearError();
	}
	testRequire(xmqttPropertiesWrite(Output,sizeof(Output),Input,3,XMQTT_PROPERTIES_PUBLISH,XMQTT_SERVER_TO_CLIENT,NULL,NULL),"MQTT optional property Written");
	xmqttproperty Parsed[4]; xmqttpropertydocument Doc;
	testRequire(xmqttPropertiesRead((xbytesview){Output,Size},XMQTT_PROPERTIES_PUBLISH,XMQTT_SERVER_TO_CLIENT,NULL,Parsed,4,&Doc)==XMQTT_PROPERTIES_DONE && Doc.Count==3 &&
		Parsed[0].Value.Pair.Value.Size==1 && Parsed[1].Value.Pair.Value.Size==2,"MQTT repeated User Property order collapsed");
	Input[0]=propertyFixture(11,XMQTT_PROPERTY_VARIABLE); Input[1]=Input[0];
	testRequire(xmqttPropertiesWrite(Output,sizeof(Output),Input,2,XMQTT_PROPERTIES_PUBLISH,XMQTT_SERVER_TO_CLIENT,NULL,&Size) &&
		xmqttPropertiesRead((xbytesview){Output,Size},XMQTT_PROPERTIES_PUBLISH,XMQTT_SERVER_TO_CLIENT,NULL,Parsed,4,&Doc)==XMQTT_PROPERTIES_DONE && Doc.Count==2 &&
		Parsed[0].Value.Number==321 && Parsed[1].Value.Number==321,"MQTT repeated equal subscription identifiers lost");
	propertyWriteFail(Input,2,XMQTT_PROPERTIES_SUBSCRIBE,XMQTT_CLIENT_TO_SERVER,NULL);
}

static void testPropertyApi(void)
{
	xmqttproperty Property=propertyFixture(1,XMQTT_PROPERTY_BYTE); Property.Type=XMQTT_PROPERTY_STRING;
	propertyWriteFail(&Property,1,XMQTT_PROPERTIES_PUBLISH,XMQTT_SERVER_TO_CLIENT,NULL);
	Property=propertyFixture(0,XMQTT_PROPERTY_BYTE); propertyWriteFail(&Property,1,XMQTT_PROPERTIES_PUBLISH,XMQTT_SERVER_TO_CLIENT,NULL);
	Property=propertyFixture(3,XMQTT_PROPERTY_STRING); Property.Value.Text=(xstrview){NULL,1}; propertyWriteFail(&Property,1,XMQTT_PROPERTIES_PUBLISH,XMQTT_SERVER_TO_CLIENT,NULL);
	Property.Value.Text=(xstrview){(cstr)(uintptr_t)(UINTPTR_MAX-1u),4}; propertyWriteFail(&Property,1,XMQTT_PROPERTIES_PUBLISH,XMQTT_SERVER_TO_CLIENT,NULL);
	propertyWriteFail(NULL,1,XMQTT_PROPERTIES_PUBLISH,XMQTT_SERVER_TO_CLIENT,NULL); propertyWriteFail(&Property,SIZE_MAX,XMQTT_PROPERTIES_PUBLISH,XMQTT_SERVER_TO_CLIENT,NULL);
	unsigned char Empty[]={0};
	propertyReadFail((xbytesview){NULL,1},XMQTT_PROPERTIES_PUBLISH,XMQTT_SERVER_TO_CLIENT,NULL,1,XMQTT_PROPERTIES_LOCAL_ERROR);
	propertyReadFail((xbytesview){(cbytes)(uintptr_t)(UINTPTR_MAX-1u),4},XMQTT_PROPERTIES_PUBLISH,XMQTT_SERVER_TO_CLIENT,NULL,1,XMQTT_PROPERTIES_LOCAL_ERROR);
	propertyReadFail((xbytesview){Empty,1},(xmqttpropertyscope)12,XMQTT_SERVER_TO_CLIENT,NULL,1,XMQTT_PROPERTIES_LOCAL_ERROR);
	propertyReadFail((xbytesview){Empty,1},XMQTT_PROPERTIES_CONNECT,XMQTT_SERVER_TO_CLIENT,NULL,1,XMQTT_PROPERTIES_LOCAL_ERROR);
	xmqttpropertyconfig Config; xmqttpropertydocument Doc={77,88,{NULL,0}}; xmqttproperty Parsed[4];
	for (unsigned Case=0; Case<5; ++Case) {
		xmqttPropertiesConfigInit(&Config); if (Case==0) Config.Size=0; else if (Case==1) Config.MaxBytes=0;
		else if (Case==2) Config.MaxBytes=(size_t)XMQTT_PROPERTY_SECTION_MAX+1u; else if (Case==3) Config.Utf8Flags=2; else Config.Size++;
		propertyReadFail((xbytesview){Empty,1},XMQTT_PROPERTIES_PUBLISH,XMQTT_SERVER_TO_CLIENT,&Config,1,XMQTT_PROPERTIES_LOCAL_ERROR);
	}
	testRequire(!xmqttPropertiesConfigInit(NULL),"MQTT NULL property config output"); xrtClearError();
	testRequire(xmqttPropertiesRead((xbytesview){Empty,1},XMQTT_PROPERTIES_PUBLISH,XMQTT_SERVER_TO_CLIENT,NULL,Parsed,SIZE_MAX,&Doc)==XMQTT_PROPERTIES_LOCAL_ERROR && Doc.Count==77,"MQTT scratch multiplication overflow"); xrtClearError();
	testRequire(xmqttPropertiesRead((xbytesview){(cbytes)Parsed,1},XMQTT_PROPERTIES_PUBLISH,XMQTT_SERVER_TO_CLIENT,NULL,Parsed,4,&Doc)==XMQTT_PROPERTIES_LOCAL_ERROR && Doc.Count==77,"MQTT property scratch overlaps input"); xrtClearError();
	testRequire(xmqttPropertiesRead((xbytesview){Empty,1},XMQTT_PROPERTIES_PUBLISH,XMQTT_SERVER_TO_CLIENT,NULL,Parsed,4,(xmqttpropertydocument*)Parsed)==XMQTT_PROPERTIES_LOCAL_ERROR,"MQTT property outputs overlap"); xrtClearError();
	Property=propertyFixture(1,XMQTT_PROPERTY_BYTE); size_t Size=12345;
	testRequire(!xmqttPropertiesSize(&Property,1,XMQTT_PROPERTIES_PUBLISH,XMQTT_SERVER_TO_CLIENT,NULL,NULL),"MQTT NULL Required"); xrtClearError();
	testRequire(!xmqttPropertiesWrite(NULL,8,&Property,1,XMQTT_PROPERTIES_PUBLISH,XMQTT_SERVER_TO_CLIENT,NULL,&Size) && Size==12345,"MQTT NULL property wire output"); xrtClearError();
}

static uint32 propertyRandom(uint32* State)
{
	*State^=*State<<13u; *State^=*State>>17u; *State^=*State<<5u; return *State;
}

static void propertySpan(xbytesview Whole, xbytesview Part)
{
	testRequire(Part.Data!=NULL && (uintptr_t)Part.Data>=(uintptr_t)Whole.Data && (uintptr_t)Part.Data-(uintptr_t)Whole.Data<=Whole.Size &&
		Part.Size<=Whole.Size-((uintptr_t)Part.Data-(uintptr_t)Whole.Data),"MQTT property view not borrowed/bounded");
}

static void testPropertyMutations(void)
{
	static const unsigned char Seed[]={15,1,1,11,193,2,38,0,1,'k',0,1,'v',3,0,0}; uint32 State=0x81234567u;
	for (size_t Trial=0; Trial<20000; ++Trial) {
		unsigned char Input[256], Output[256]; size_t Size=sizeof(Seed); memcpy(Input,Seed,Size);
		if (Trial%3u==0) { Size=propertyRandom(&State)%sizeof(Input); for (size_t Index=0; Index<Size; ++Index) Input[Index]=(unsigned char)propertyRandom(&State); }
		else for (unsigned Change=0, Changes=propertyRandom(&State)%5u; Change<Changes; ++Change) Input[propertyRandom(&State)%Size]=(unsigned char)propertyRandom(&State);
		xmqttpropertyconfig Config; xmqttPropertiesConfigInit(&Config); Config.MaxBytes=128; Config.MaxProperties=16;
		xmqttproperty Scratch[16], Before[16]; memset(Scratch,0xa5,sizeof(Scratch)); memcpy(Before,Scratch,sizeof(Scratch)); xmqttpropertydocument Doc={77,88,{NULL,0}};
		xmqttpropertystatus Status=xmqttPropertiesRead((xbytesview){Input,Size},XMQTT_PROPERTIES_PUBLISH,XMQTT_SERVER_TO_CLIENT,&Config,Scratch,16,&Doc);
		if (Status==XMQTT_PROPERTIES_DONE) {
			testRequire(Doc.Count<=16 && Doc.Consumed<=Size && Doc.Raw.Data==Input && Doc.Raw.Size==Doc.Consumed,"MQTT property mutation document bounds");
			for (size_t Index=0; Index<Doc.Count; ++Index) {
				if (Scratch[Index].Type==XMQTT_PROPERTY_STRING) propertySpan(Doc.Raw,(xbytesview){(cbytes)Scratch[Index].Value.Text.Data,Scratch[Index].Value.Text.Size});
				else if (Scratch[Index].Type==XMQTT_PROPERTY_BINARY) propertySpan(Doc.Raw,Scratch[Index].Value.Binary);
				else if (Scratch[Index].Type==XMQTT_PROPERTY_STRING_PAIR) {
					propertySpan(Doc.Raw,(xbytesview){(cbytes)Scratch[Index].Value.Pair.Name.Data,Scratch[Index].Value.Pair.Name.Size});
					propertySpan(Doc.Raw,(xbytesview){(cbytes)Scratch[Index].Value.Pair.Value.Data,Scratch[Index].Value.Pair.Value.Size});
				}
			}
			size_t Written;
			testRequire(xmqttPropertiesWrite(Output,sizeof(Output),Scratch,Doc.Count,XMQTT_PROPERTIES_PUBLISH,XMQTT_SERVER_TO_CLIENT,&Config,&Written) &&
				Written==Doc.Consumed && memcmp(Output,Input,Written)==0,"MQTT property accepted mutation lost exact bytes/order");
		} else {
			testRequire(Doc.Count==77 && Doc.Consumed==88,"MQTT property mutation partial publication");
			if (Status==XMQTT_PROPERTIES_MORE) testRequire(memcmp(Scratch,Before,sizeof(Scratch))==0,"MQTT property MORE changed scratch");
		}
		xrtClearError();
	}
}

static void testPropertyAllocation(void)
{
	unsigned char Wire[]={6,1,1,3,0,1,'x'}, Output[64]; xmqttproperty Parsed[4]; xmqttpropertydocument Doc; size_t Size, Written;
	xrtSetErrorInfo(XERR_VALUE,"xrt.test",777,"ambient"); const xerror* Ambient=xrtGetError();
	#if defined(XRT_FEATURE_MEMORY_STATS)
	xmemstats Before, After; xrtMemStatsEnable(true); xrtMemStatsGet(&Before);
	#endif
	for (size_t Index=0; Index<10000; ++Index) {
		testRequire(xmqttPropertiesRead((xbytesview){Wire,2},XMQTT_PROPERTIES_PUBLISH,XMQTT_SERVER_TO_CLIENT,NULL,Parsed,4,&Doc)==XMQTT_PROPERTIES_MORE &&
			xmqttPropertiesRead((xbytesview){Wire,sizeof(Wire)},XMQTT_PROPERTIES_PUBLISH,XMQTT_SERVER_TO_CLIENT,NULL,Parsed,4,&Doc)==XMQTT_PROPERTIES_DONE &&
			xmqttPropertiesSize(Parsed,Doc.Count,XMQTT_PROPERTIES_PUBLISH,XMQTT_SERVER_TO_CLIENT,NULL,&Size) &&
			xmqttPropertiesWrite(Output,sizeof(Output),Parsed,Doc.Count,XMQTT_PROPERTIES_PUBLISH,XMQTT_SERVER_TO_CLIENT,NULL,&Written) && Written==Size &&
			memcmp(Output,Wire,sizeof(Wire))==0 && xrtGetError()==Ambient,"MQTT property warm cycle altered error or wire");
	}
	#if defined(XRT_FEATURE_MEMORY_STATS)
	xrtMemStatsGet(&After); xrtMemStatsEnable(false);
	testRequire(After.BlockAllocCalls==Before.BlockAllocCalls && After.MallocCalls==Before.MallocCalls && After.CallocCalls==Before.CallocCalls && After.ReallocCalls==Before.ReallocCalls,"MQTT properties allocated on success/MORE");
	#endif
	xrtClearError(); xallocationbudget* Budget=NULL; xallocationscope Scope={0};
	testRequire(xrtAllocationBudgetCreate(1,&Budget) && xrtAllocationBudgetEnter(Budget,&Scope),"MQTT property quota");
	testRequire(xmqttPropertiesRead((xbytesview){Wire,sizeof(Wire)},XMQTT_PROPERTIES_PUBLISH,XMQTT_SERVER_TO_CLIENT,NULL,Parsed,4,&Doc)==XMQTT_PROPERTIES_DONE &&
		xmqttPropertiesWrite(Output,sizeof(Output),Parsed,Doc.Count,XMQTT_PROPERTIES_PUBLISH,XMQTT_SERVER_TO_CLIENT,NULL,&Written),"MQTT properties success under quota");
	unsigned char Protocol[]={2,1,2}, Malformed[]={1,7}, Utf8[]={4,3,0,1,255}, Valid[]={2,1,1}; Doc=(xmqttpropertydocument){77,88,{NULL,0}};
	testRequire(xmqttPropertiesRead((xbytesview){Protocol,3},XMQTT_PROPERTIES_PUBLISH,XMQTT_SERVER_TO_CLIENT,NULL,Parsed,4,&Doc)==XMQTT_PROPERTIES_PROTOCOL_ERROR && xrtErrorKind(xrtGetError())==XERR_MEMORY && Doc.Count==77,"MQTT diagnostic OOM hid protocol error");
	testRequire(xmqttPropertiesRead((xbytesview){Malformed,2},XMQTT_PROPERTIES_PUBLISH,XMQTT_SERVER_TO_CLIENT,NULL,Parsed,4,&Doc)==XMQTT_PROPERTIES_MALFORMED && xrtErrorKind(xrtGetError())==XERR_MEMORY,"MQTT diagnostic OOM hid malformed wire");
	testRequire(xmqttPropertiesRead((xbytesview){Utf8,5},XMQTT_PROPERTIES_PUBLISH,XMQTT_SERVER_TO_CLIENT,NULL,Parsed,4,&Doc)==XMQTT_PROPERTIES_MALFORMED && xrtErrorKind(xrtGetError())==XERR_MEMORY,"MQTT nested wire diagnostic OOM hid malformed UTF-8");
	testRequire(xmqttPropertiesRead((xbytesview){Valid,3},XMQTT_PROPERTIES_PUBLISH,XMQTT_SERVER_TO_CLIENT,NULL,NULL,0,&Doc)==XMQTT_PROPERTIES_LOCAL_ERROR && xrtErrorKind(xrtGetError())==XERR_MEMORY,"MQTT diagnostic OOM hid local capacity");
	testRequire(xrtAllocationBudgetLeave(&Scope),"MQTT property quota leave"); xallocationbudgetinfo Info={0}; Info.Size=sizeof(Info);
	testRequire(xrtAllocationBudgetInfo(Budget,&Info) && Info.LiveBytes==0 && Info.Rejections!=0,"MQTT property quota leak"); xrtClearError(); xrtAllocationBudgetFree(Budget);
}

int main(void)
{
	testPropertyMatrix(); testPropertyInvalid(); testPropertyLimits(); testPropertyAtomicity(); testPropertyApi(); testPropertyMutations(); testPropertyAllocation();
	printf("[PASS] xmqtt properties: all27/scopes/directions/duplicates/types/ranges/UTF8/limits/atomicity/zero-heap/OOM\n"); return 0;
}
