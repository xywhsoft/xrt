#include <xmqtt/properties.h>
#include "../internal/xmqtt_internal.h"
#include <xrt/memory.h>
#include <string.h>

#if defined(XMQTT_FEATURE_XMQTT_PROPERTIES)

typedef struct xmqttpropertyspec { xmqttpropertytype Type; uint32 Scopes; } xmqttpropertyspec;
enum { MP_CONNECT=1u<<1, MP_CONNACK=1u<<2, MP_PUBLISH=1u<<3, MP_PUBACKS=0xfu<<4,
	MP_SUBSCRIBE=1u<<8, MP_SUBACK=1u<<9, MP_UNSUBSCRIBE=1u<<10, MP_UNSUBACK=1u<<11,
	MP_DISCONNECT=1u<<14, MP_AUTH=1u<<15, MP_WILL=1u<<16,
	MP_USER=MP_CONNECT|MP_CONNACK|MP_PUBLISH|MP_PUBACKS|MP_SUBSCRIBE|MP_SUBACK|MP_UNSUBSCRIBE|MP_UNSUBACK|MP_DISCONNECT|MP_AUTH|MP_WILL,
	MP_REASON=MP_CONNACK|MP_PUBACKS|MP_SUBACK|MP_UNSUBACK|MP_DISCONNECT|MP_AUTH };

/* Direct indexed standard registry; zero entries are unassigned ids. */
static const xmqttpropertyspec xmqttPropertySpecs[43] = {
	[1]={XMQTT_PROPERTY_BYTE,MP_PUBLISH|MP_WILL}, [2]={XMQTT_PROPERTY_UINT32,MP_PUBLISH|MP_WILL},
	[3]={XMQTT_PROPERTY_STRING,MP_PUBLISH|MP_WILL}, [8]={XMQTT_PROPERTY_STRING,MP_PUBLISH|MP_WILL},
	[9]={XMQTT_PROPERTY_BINARY,MP_PUBLISH|MP_WILL}, [11]={XMQTT_PROPERTY_VARIABLE,MP_PUBLISH|MP_SUBSCRIBE},
	[17]={XMQTT_PROPERTY_UINT32,MP_CONNECT|MP_CONNACK|MP_DISCONNECT}, [18]={XMQTT_PROPERTY_STRING,MP_CONNACK},
	[19]={XMQTT_PROPERTY_UINT16,MP_CONNACK}, [21]={XMQTT_PROPERTY_STRING,MP_CONNECT|MP_CONNACK|MP_AUTH},
	[22]={XMQTT_PROPERTY_BINARY,MP_CONNECT|MP_CONNACK|MP_AUTH}, [23]={XMQTT_PROPERTY_BYTE,MP_CONNECT},
	[24]={XMQTT_PROPERTY_UINT32,MP_WILL}, [25]={XMQTT_PROPERTY_BYTE,MP_CONNECT}, [26]={XMQTT_PROPERTY_STRING,MP_CONNACK},
	[28]={XMQTT_PROPERTY_STRING,MP_CONNACK|MP_DISCONNECT}, [31]={XMQTT_PROPERTY_STRING,MP_REASON},
	[33]={XMQTT_PROPERTY_UINT16,MP_CONNECT|MP_CONNACK}, [34]={XMQTT_PROPERTY_UINT16,MP_CONNECT|MP_CONNACK},
	[35]={XMQTT_PROPERTY_UINT16,MP_PUBLISH}, [36]={XMQTT_PROPERTY_BYTE,MP_CONNACK}, [37]={XMQTT_PROPERTY_BYTE,MP_CONNACK},
	[38]={XMQTT_PROPERTY_STRING_PAIR,MP_USER}, [39]={XMQTT_PROPERTY_UINT32,MP_CONNECT|MP_CONNACK},
	[40]={XMQTT_PROPERTY_BYTE,MP_CONNACK}, [41]={XMQTT_PROPERTY_BYTE,MP_CONNACK}, [42]={XMQTT_PROPERTY_BYTE,MP_CONNACK}
};

static xmqttpropertystatus xmqttPropertiesFail(xmqttpropertystatus Status, xmqttpropertyerror Code, cstr Message)
{
	xerrkind Kind = Code == XMQTT_PROPERTY_ARGUMENT ? XERR_ARGUMENT :
		(Code == XMQTT_PROPERTY_LIMIT || Code == XMQTT_PROPERTY_CAPACITY) ? XERR_RANGE : XERR_PROTOCOL;
	xrtSetErrorInfo(Kind,"xrt.mqtt",(int32)Code,Message); return Status;
}

static bool xmqttPropertiesSpan(const void* Data, size_t Size)
{
	return (Data != NULL || Size == 0) && xrtMemRangeValid(Data,Size);
}

static bool xmqttPropertiesSettings(xmqttpropertyscope Scope, xmqttdirection Direction,
	const xmqttpropertyconfig* Config, xmqttpropertyconfig* Copy)
{
	if (!((Scope >= XMQTT_PROPERTIES_CONNECT && Scope <= XMQTT_PROPERTIES_UNSUBACK) ||
		(Scope >= XMQTT_PROPERTIES_DISCONNECT && Scope <= XMQTT_PROPERTIES_WILL)) ||
		(Direction != XMQTT_CLIENT_TO_SERVER && Direction != XMQTT_SERVER_TO_CLIENT) ||
		((Scope == XMQTT_PROPERTIES_CONNECT || Scope == XMQTT_PROPERTIES_WILL || Scope == XMQTT_PROPERTIES_SUBSCRIBE || Scope == XMQTT_PROPERTIES_UNSUBSCRIBE) && Direction != XMQTT_CLIENT_TO_SERVER) ||
		((Scope == XMQTT_PROPERTIES_CONNACK || Scope == XMQTT_PROPERTIES_SUBACK || Scope == XMQTT_PROPERTIES_UNSUBACK) && Direction != XMQTT_SERVER_TO_CLIENT) ||
		(Config != NULL && !xrtMemRangeValid(Config,sizeof(*Config)))) goto Invalid;
	if (Config == NULL) xmqttPropertiesConfigInit(Copy); else *Copy = *Config;
	if (Copy->Size != sizeof(*Copy) || !Copy->MaxBytes || Copy->MaxBytes > XMQTT_PROPERTY_SECTION_MAX || (Copy->Utf8Flags & ~XMQTT_UTF8_REJECT_DISALLOWED)) goto Invalid;
	return true;
Invalid:
	xmqttPropertiesFail(XMQTT_PROPERTIES_LOCAL_ERROR,XMQTT_PROPERTY_ARGUMENT,"invalid MQTT property scope, direction or configuration"); return false;
}

XMQTT_API bool xmqttPropertiesConfigInit(xmqttpropertyconfig* Config)
{
	if (Config == NULL || !xrtMemRangeValid(Config,sizeof(*Config))) {
		xmqttPropertiesFail(XMQTT_PROPERTIES_LOCAL_ERROR,XMQTT_PROPERTY_ARGUMENT,"invalid MQTT property config output"); return false;
	}
	*Config = (xmqttpropertyconfig){sizeof(*Config),1024u*1024u,256,0}; return true;
}

static const xmqttpropertyspec* xmqttPropertiesSpec(xmqttpropertyid Id)
{
	return Id > 0 && Id < 43 && xmqttPropertySpecs[Id].Type != 0 ? &xmqttPropertySpecs[Id] : NULL;
}

static xmqttpropertystatus xmqttPropertiesRule(const xmqttproperty* Property, const xmqttpropertyspec* Spec,
	xmqttpropertyscope Scope, xmqttdirection Direction, uint64* Seen)
{
	if (!(Spec->Scopes & (UINT32_C(1) << Scope)))
		return xmqttPropertiesFail(XMQTT_PROPERTIES_MALFORMED,XMQTT_PROPERTY_MALFORMED,"MQTT property is not valid for its packet or Will scope");
	if ((Property->Id == XMQTT_PROPERTY_SUBSCRIPTION_ID && Scope == XMQTT_PROPERTIES_PUBLISH && Direction == XMQTT_CLIENT_TO_SERVER) ||
		(Property->Id == XMQTT_PROPERTY_SESSION_EXPIRY && Scope == XMQTT_PROPERTIES_DISCONNECT && Direction == XMQTT_SERVER_TO_CLIENT))
		return xmqttPropertiesFail(XMQTT_PROPERTIES_PROTOCOL_ERROR,XMQTT_PROPERTY_PROTOCOL,"MQTT property is forbidden in this direction");
	uint64 Bit = UINT64_C(1) << Property->Id;
	bool Repeated = Property->Id == XMQTT_PROPERTY_USER || (Property->Id == XMQTT_PROPERTY_SUBSCRIPTION_ID && Scope == XMQTT_PROPERTIES_PUBLISH);
	if ((*Seen & Bit) && !Repeated)
		return xmqttPropertiesFail(XMQTT_PROPERTIES_PROTOCOL_ERROR,XMQTT_PROPERTY_PROTOCOL,"duplicate singleton MQTT property");
	*Seen |= Bit; return XMQTT_PROPERTIES_DONE;
}

static xmqttpropertystatus xmqttPropertiesValue(const xmqttproperty* Property)
{
	uint32 Number = Property->Type <= XMQTT_PROPERTY_VARIABLE ? Property->Value.Number : 0;
	if ((Property->Type == XMQTT_PROPERTY_BYTE && Number > 255u) ||
		(Property->Type == XMQTT_PROPERTY_UINT16 && Number > 65535u) ||
		(Property->Type == XMQTT_PROPERTY_VARIABLE && Number > XMQTT_VARIABLE_MAX) ||
		((Property->Id == XMQTT_PROPERTY_PAYLOAD_FORMAT || Property->Id == XMQTT_PROPERTY_REQUEST_PROBLEM_INFO ||
		Property->Id == XMQTT_PROPERTY_REQUEST_RESPONSE_INFO || Property->Id == XMQTT_PROPERTY_MAXIMUM_QOS ||
		Property->Id == XMQTT_PROPERTY_RETAIN_AVAILABLE || Property->Id == XMQTT_PROPERTY_WILDCARD_AVAILABLE ||
		Property->Id == XMQTT_PROPERTY_SUBSCRIPTION_ID_AVAILABLE || Property->Id == XMQTT_PROPERTY_SHARED_AVAILABLE) && Number > 1u) ||
		((Property->Id == XMQTT_PROPERTY_SUBSCRIPTION_ID || Property->Id == XMQTT_PROPERTY_RECEIVE_MAXIMUM ||
		Property->Id == XMQTT_PROPERTY_TOPIC_ALIAS || Property->Id == XMQTT_PROPERTY_MAXIMUM_PACKET_SIZE) && Number == 0))
		return xmqttPropertiesFail(XMQTT_PROPERTIES_PROTOCOL_ERROR,XMQTT_PROPERTY_PROTOCOL,"MQTT property numeric value is out of range");
	if (Property->Id == XMQTT_PROPERTY_RESPONSE_TOPIC) {
		xstrview Text = Property->Value.Text;
		if (!Text.Size) return xmqttPropertiesFail(XMQTT_PROPERTIES_PROTOCOL_ERROR,XMQTT_PROPERTY_PROTOCOL,"MQTT response topic is empty");
		for (size_t Index = 0; Index < Text.Size; ++Index)
			if (Text.Data[Index] == '+' || Text.Data[Index] == '#')
				return xmqttPropertiesFail(XMQTT_PROPERTIES_PROTOCOL_ERROR,XMQTT_PROPERTY_PROTOCOL,"MQTT response topic contains a wildcard");
	}
	return XMQTT_PROPERTIES_DONE;
}

static xmqttpropertystatus xmqttPropertiesFinal(xmqttpropertyscope Scope, uint64 Seen)
{
	uint64 Method = UINT64_C(1) << XMQTT_PROPERTY_AUTH_METHOD, Data = UINT64_C(1) << XMQTT_PROPERTY_AUTH_DATA;
	if ((Scope == XMQTT_PROPERTIES_CONNECT && (Seen & Data) && !(Seen & Method)) || (Scope == XMQTT_PROPERTIES_AUTH && !(Seen & Method)))
		return xmqttPropertiesFail(XMQTT_PROPERTIES_PROTOCOL_ERROR,XMQTT_PROPERTY_PROTOCOL,"MQTT authentication method is required in this property section");
	return XMQTT_PROPERTIES_DONE;
}

static xmqttpropertystatus xmqttPropertiesField(xbytesview Input, uint32 Flags, xmqttproperty* Property, size_t* Consumed)
{
	xmqttwirestatus Status; size_t First;
	if (Property->Type == XMQTT_PROPERTY_STRING) Status = xmqttUtf8StringRead(Input,Flags,&Property->Value.Text,Consumed);
	else if (Property->Type == XMQTT_PROPERTY_BINARY) Status = xmqttBinaryRead(Input,&Property->Value.Binary,Consumed);
	else {
		Status = xmqttUtf8StringRead(Input,Flags,&Property->Value.Pair.Name,&First);
		if (Status == XMQTT_WIRE_DONE) {
			Status = xmqttUtf8StringRead((xbytesview){Input.Data+First,Input.Size-First},Flags,&Property->Value.Pair.Value,Consumed);
			if (Status == XMQTT_WIRE_DONE) *Consumed += First;
		}
	}
	if (Status == XMQTT_WIRE_DONE) return XMQTT_PROPERTIES_DONE;
	if (Status == XMQTT_WIRE_ERROR) return XMQTT_PROPERTIES_MALFORMED; /* Confirmed UTF-8 error, even if diagnostic OOM. */
	return xmqttPropertiesFail(XMQTT_PROPERTIES_MALFORMED,XMQTT_PROPERTY_MALFORMED,"MQTT property field crosses its declared section boundary");
}

static bool xmqttPropertiesReadArgs(xbytesview Input, const xmqttpropertyconfig* Config,
	xmqttproperty* Properties, size_t Capacity, xmqttpropertydocument* Document)
{
	if (!xmqttPropertiesSpan(Input.Data,Input.Size) || Capacity > SIZE_MAX/sizeof(*Properties) ||
		!xmqttPropertiesSpan(Properties,Capacity*sizeof(*Properties)) || Document == NULL || !xrtMemRangeValid(Document,sizeof(*Document))) goto Invalid;
	size_t Storage = Capacity*sizeof(*Properties);
	if (xrtMemRangesOverlap(Properties,Storage,Input.Data,Input.Size) || xrtMemRangesOverlap(Document,sizeof(*Document),Input.Data,Input.Size) ||
		xrtMemRangesOverlap(Properties,Storage,Document,sizeof(*Document)) ||
		(Config != NULL && (xrtMemRangesOverlap(Properties,Storage,Config,sizeof(*Config)) || xrtMemRangesOverlap(Document,sizeof(*Document),Config,sizeof(*Config))))) goto Invalid;
	return true;
Invalid:
	xmqttPropertiesFail(XMQTT_PROPERTIES_LOCAL_ERROR,XMQTT_PROPERTY_ARGUMENT,"invalid MQTT property input/output spans or overlap"); return false;
}

XMQTT_API xmqttpropertystatus xmqttPropertiesRead(xbytesview Input, xmqttpropertyscope Scope,
	xmqttdirection Direction, const xmqttpropertyconfig* Config, xmqttproperty* Properties, size_t Capacity, xmqttpropertydocument* Document)
{
	xmqttpropertyconfig Limits;
	if (!xmqttPropertiesSettings(Scope,Direction,Config,&Limits) || !xmqttPropertiesReadArgs(Input,Config,Properties,Capacity,Document)) return XMQTT_PROPERTIES_LOCAL_ERROR;
	uint32 Length; size_t Prefix;
	xmqttwirestatus WireStatus = xmqttVbiRead(Input,XMQTT_VERSION_5,&Length,&Prefix);
	if (WireStatus != XMQTT_WIRE_DONE) return WireStatus == XMQTT_WIRE_MORE ? XMQTT_PROPERTIES_MORE : XMQTT_PROPERTIES_MALFORMED;
	size_t Total = Prefix+Length;
	if (Total > Limits.MaxBytes) return xmqttPropertiesFail(XMQTT_PROPERTIES_LOCAL_ERROR,XMQTT_PROPERTY_LIMIT,"MQTT property section exceeds configured bytes");
	if (Input.Size < Total) return XMQTT_PROPERTIES_MORE;
	size_t Position = Prefix, Count = 0; uint64 Seen = 0;
	while (Position < Total) {
		if (Count >= Limits.MaxProperties) return xmqttPropertiesFail(XMQTT_PROPERTIES_LOCAL_ERROR,XMQTT_PROPERTY_LIMIT,"MQTT property count exceeds configured limit");
		if (Count >= Capacity) return xmqttPropertiesFail(XMQTT_PROPERTIES_LOCAL_ERROR,XMQTT_PROPERTY_CAPACITY,"MQTT property scratch capacity exhausted");
		uint32 Id; size_t Read;
		WireStatus = xmqttVbiRead((xbytesview){Input.Data+Position,Total-Position},XMQTT_VERSION_5,&Id,&Read);
		if (WireStatus != XMQTT_WIRE_DONE) {
			if (WireStatus == XMQTT_WIRE_MORE) return xmqttPropertiesFail(XMQTT_PROPERTIES_MALFORMED,XMQTT_PROPERTY_MALFORMED,"truncated MQTT property identifier");
			return XMQTT_PROPERTIES_MALFORMED;
		}
		Position += Read;
		xmqttproperty Property = {0}; Property.Id = (xmqttpropertyid)Id;
		const xmqttpropertyspec* Spec = xmqttPropertiesSpec(Property.Id);
		if (Spec == NULL) return xmqttPropertiesFail(XMQTT_PROPERTIES_MALFORMED,XMQTT_PROPERTY_MALFORMED,"unknown MQTT property identifier");
		Property.Type = Spec->Type;
		xmqttpropertystatus Status = xmqttPropertiesRule(&Property,Spec,Scope,Direction,&Seen);
		if (Status != XMQTT_PROPERTIES_DONE) return Status;
		if (Property.Type >= XMQTT_PROPERTY_STRING) {
			Status = xmqttPropertiesField((xbytesview){Input.Data+Position,Total-Position},Limits.Utf8Flags,&Property,&Read);
			if (Status != XMQTT_PROPERTIES_DONE) return Status;
		} else if (Property.Type == XMQTT_PROPERTY_VARIABLE) {
			WireStatus = xmqttVbiRead((xbytesview){Input.Data+Position,Total-Position},XMQTT_VERSION_5,&Property.Value.Number,&Read);
			if (WireStatus == XMQTT_WIRE_MORE) return xmqttPropertiesFail(XMQTT_PROPERTIES_MALFORMED,XMQTT_PROPERTY_MALFORMED,"truncated MQTT variable property value");
			if (WireStatus != XMQTT_WIRE_DONE) return XMQTT_PROPERTIES_MALFORMED;
		} else {
			Read = Property.Type == XMQTT_PROPERTY_BYTE ? 1u : Property.Type == XMQTT_PROPERTY_UINT16 ? 2u : 4u;
			if (Total-Position < Read) return xmqttPropertiesFail(XMQTT_PROPERTIES_MALFORMED,XMQTT_PROPERTY_MALFORMED,"truncated MQTT numeric property value");
			for (size_t Index = 0; Index < Read; ++Index) Property.Value.Number = (Property.Value.Number << 8) | Input.Data[Position+Index];
		}
		Position += Read;
		Status = xmqttPropertiesValue(&Property); if (Status != XMQTT_PROPERTIES_DONE) return Status;
		Properties[Count++] = Property;
	}
	xmqttpropertystatus Status = xmqttPropertiesFinal(Scope,Seen); if (Status != XMQTT_PROPERTIES_DONE) return Status;
	*Document = (xmqttpropertydocument){Count,Total,{Input.Data,Total}}; return XMQTT_PROPERTIES_DONE;
}

static size_t xmqttPropertiesVbiSize(uint32 Number)
{
	return 1u + (Number >= 128u) + (Number >= 16384u) + (Number >= 2097152u);
}

static size_t xmqttPropertiesViews(const xmqttproperty* Property, xbytesview* Views)
{
	if (Property->Type == XMQTT_PROPERTY_STRING) { Views[0] = (xbytesview){(cbytes)Property->Value.Text.Data,Property->Value.Text.Size}; return 1; }
	if (Property->Type == XMQTT_PROPERTY_BINARY) { Views[0] = Property->Value.Binary; return 1; }
	if (Property->Type == XMQTT_PROPERTY_STRING_PAIR) {
		Views[0] = (xbytesview){(cbytes)Property->Value.Pair.Name.Data,Property->Value.Pair.Name.Size};
		Views[1] = (xbytesview){(cbytes)Property->Value.Pair.Value.Data,Property->Value.Pair.Value.Size}; return 2;
	}
	return 0;
}

XMQTT_INTERNAL bool xmqttInternalPropertiesPrepare(const xmqttproperty* Properties, size_t Count, xmqttpropertyscope Scope,
	xmqttdirection Direction, const xmqttpropertyconfig* Config, xmqttpropertyconfig* Limits, size_t* Size, uint32* BodySize)
{
	if (!xmqttPropertiesSettings(Scope,Direction,Config,Limits)) return false;
	if (Count > SIZE_MAX/sizeof(*Properties) || !xmqttPropertiesSpan(Properties,Count*sizeof(*Properties))) {
		xmqttPropertiesFail(XMQTT_PROPERTIES_LOCAL_ERROR,XMQTT_PROPERTY_ARGUMENT,"invalid MQTT property vector span"); return false;
	}
	if (Count > Limits->MaxProperties) {
		xmqttPropertiesFail(XMQTT_PROPERTIES_LOCAL_ERROR,XMQTT_PROPERTY_LIMIT,"MQTT property count exceeds configured limit"); return false;
	}
	size_t Body = 0; uint64 Seen = 0;
	for (size_t Index = 0; Index < Count; ++Index) {
		const xmqttproperty* Property = &Properties[Index]; const xmqttpropertyspec* Spec = xmqttPropertiesSpec(Property->Id);
		if (Spec == NULL || Property->Type != Spec->Type) {
			xmqttPropertiesFail(XMQTT_PROPERTIES_LOCAL_ERROR,XMQTT_PROPERTY_ARGUMENT,"unknown MQTT property id or mismatched descriptor type"); return false;
		}
		if (xmqttPropertiesRule(Property,Spec,Scope,Direction,&Seen) != XMQTT_PROPERTIES_DONE) return false;
		xbytesview Views[2]; size_t ViewCount = xmqttPropertiesViews(Property,Views), Bytes = 1;
		for (size_t View = 0; View < ViewCount; ++View) {
			if (!xmqttPropertiesSpan(Views[View].Data,Views[View].Size)) {
				xmqttPropertiesFail(XMQTT_PROPERTIES_LOCAL_ERROR,XMQTT_PROPERTY_ARGUMENT,"invalid MQTT property data span"); return false;
			}
			if (Views[View].Size > XMQTT_STRING_MAX_BYTES) {
				xmqttPropertiesFail(XMQTT_PROPERTIES_LOCAL_ERROR,XMQTT_PROPERTY_LIMIT,"MQTT property field exceeds uint16 length"); return false;
			}
			Bytes += 2u+Views[View].Size;
		}
		if (!ViewCount && xmqttPropertiesValue(Property) != XMQTT_PROPERTIES_DONE) return false;
		if (!ViewCount) Bytes += Property->Type == XMQTT_PROPERTY_BYTE ? 1u : Property->Type == XMQTT_PROPERTY_UINT16 ? 2u :
			Property->Type == XMQTT_PROPERTY_UINT32 ? 4u : xmqttPropertiesVbiSize(Property->Value.Number);
		/* Keep at least one byte for Property Length. Reject a known byte-budget
		 * failure before walking UTF-8 or scanning a response topic. */
		if (Bytes > XMQTT_VARIABLE_MAX-Body || Bytes >= Limits->MaxBytes || Body >= Limits->MaxBytes-Bytes) {
			xmqttPropertiesFail(XMQTT_PROPERTIES_LOCAL_ERROR,XMQTT_PROPERTY_LIMIT,"MQTT property section exceeds configured or protocol size"); return false;
		}
		for (size_t View = 0; View < ViewCount; ++View)
			if (Property->Type != XMQTT_PROPERTY_BINARY && !xmqttUtf8StringCheck((xstrview){(cstr)Views[View].Data,Views[View].Size},Limits->Utf8Flags,NULL)) return false;
		if (ViewCount && xmqttPropertiesValue(Property) != XMQTT_PROPERTIES_DONE) return false;
		Body += Bytes;
	}
	if (xmqttPropertiesFinal(Scope,Seen) != XMQTT_PROPERTIES_DONE) return false;
	size_t Total = Body+xmqttPropertiesVbiSize((uint32)Body);
	if (Total > Limits->MaxBytes) {
		xmqttPropertiesFail(XMQTT_PROPERTIES_LOCAL_ERROR,XMQTT_PROPERTY_LIMIT,"MQTT property length prefix exceeds configured section bytes"); return false;
	}
	*Size = Total; *BodySize = (uint32)Body; return true;
}

XMQTT_INTERNAL bool xmqttInternalPropertiesOutput(const void* Output, size_t Size, const xmqttproperty* Properties,
	size_t Count, const xmqttpropertyconfig* Config)
{
	if (Output == NULL || !xrtMemRangeValid(Output,Size) || xrtMemRangesOverlap(Output,Size,Properties,Count*sizeof(*Properties)) ||
		(Config != NULL && xrtMemRangesOverlap(Output,Size,Config,sizeof(*Config)))) goto Invalid;
	for (size_t Index = 0; Index < Count; ++Index) {
		xbytesview Views[2]; size_t ViewCount = xmqttPropertiesViews(&Properties[Index],Views);
		for (size_t View = 0; View < ViewCount; ++View)
			if (xrtMemRangesOverlap(Output,Size,Views[View].Data,Views[View].Size)) goto Invalid;
	}
	return true;
Invalid:
	xmqttPropertiesFail(XMQTT_PROPERTIES_LOCAL_ERROR,XMQTT_PROPERTY_ARGUMENT,"MQTT property output overlaps inputs/config or has invalid span"); return false;
}

XMQTT_API bool xmqttPropertiesSize(const xmqttproperty* Properties, size_t Count, xmqttpropertyscope Scope,
	xmqttdirection Direction, const xmqttpropertyconfig* Config, size_t* Required)
{
	xmqttpropertyconfig Limits; size_t Size; uint32 Body;
	if (!xmqttInternalPropertiesPrepare(Properties,Count,Scope,Direction,Config,&Limits,&Size,&Body) || !xmqttInternalPropertiesOutput(Required,sizeof(*Required),Properties,Count,Config)) return false;
	*Required = Size; return true;
}

XMQTT_API bool xmqttPropertiesWrite(ptr Output, size_t Capacity, const xmqttproperty* Properties, size_t Count,
	xmqttpropertyscope Scope, xmqttdirection Direction, const xmqttpropertyconfig* Config, size_t* Written)
{
	xmqttpropertyconfig Limits; size_t Size; uint32 Body;
	if (Output == NULL || !xrtMemRangeValid(Output,Capacity)) {
		xmqttPropertiesFail(XMQTT_PROPERTIES_LOCAL_ERROR,XMQTT_PROPERTY_ARGUMENT,"invalid MQTT property wire buffer"); return false;
	}
	if (!xmqttInternalPropertiesPrepare(Properties,Count,Scope,Direction,Config,&Limits,&Size,&Body)) return false;
	if (Capacity < Size) {
		xmqttPropertiesFail(XMQTT_PROPERTIES_LOCAL_ERROR,XMQTT_PROPERTY_CAPACITY,"MQTT property wire capacity is too small"); return false;
	}
	if (!xmqttInternalPropertiesOutput(Output,Size,Properties,Count,Config)) return false;
	if (Written != NULL && (!xmqttInternalPropertiesOutput(Written,sizeof(*Written),Properties,Count,Config) || xrtMemRangesOverlap(Written,sizeof(*Written),Output,Size))) {
		xmqttPropertiesFail(XMQTT_PROPERTIES_LOCAL_ERROR,XMQTT_PROPERTY_ARGUMENT,"MQTT property Written overlaps wire or inputs"); return false;
	}
	(void)xmqttInternalPropertiesEmit(Output,Properties,Count,Body,Size);
	if (Written != NULL) *Written = Size;
	return true;
}

XMQTT_INTERNAL size_t xmqttInternalPropertiesEmit(ptr Output, const xmqttproperty* Properties, size_t Count, uint32 Body, size_t Size)
{
	unsigned char* Cursor = (unsigned char*)Output; size_t Bytes;
	/* Infallible emission for preflighted immutable, nonoverlapping inputs.
	 * Text is not rescanned; only the active views' bytes are copied once. */
	(void)xmqttVbiWrite(Cursor,Size,Body,&Bytes); Cursor += Bytes;
	for (size_t Index = 0; Index < Count; ++Index) {
		const xmqttproperty* Property = &Properties[Index]; *Cursor++ = (unsigned char)Property->Id;
		xbytesview Views[2]; size_t ViewCount = xmqttPropertiesViews(Property,Views);
		for (size_t View = 0; View < ViewCount; ++View) {
			*Cursor++ = (unsigned char)(Views[View].Size >> 8); *Cursor++ = (unsigned char)Views[View].Size;
			if (Views[View].Size) { memcpy(Cursor,Views[View].Data,Views[View].Size); Cursor += Views[View].Size; }
		}
		if (!ViewCount) {
			if (Property->Type == XMQTT_PROPERTY_VARIABLE) {
				(void)xmqttVbiWrite(Cursor,Size-(size_t)(Cursor-(unsigned char*)Output),Property->Value.Number,&Bytes); Cursor += Bytes;
			}
			else {
				unsigned Width = Property->Type == XMQTT_PROPERTY_BYTE ? 1u : Property->Type == XMQTT_PROPERTY_UINT16 ? 2u : 4u;
				for (unsigned Byte = 0; Byte < Width; ++Byte) *Cursor++ = (unsigned char)(Property->Value.Number >> (8u*(Width-Byte-1u)));
			}
		}
	}
	return (size_t)(Cursor-(unsigned char*)Output);
}

#endif
