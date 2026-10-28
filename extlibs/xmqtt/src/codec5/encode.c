#include <xmqtt/codec5.h>
#include "../internal/xmqtt_internal.h"
#include <xrt/memory.h>
#include <string.h>

#if defined(XMQTT_FEATURE_XMQTT_CODEC5)

typedef struct xmqtt5plan {
	const xmqttpacket* Packet;
	xmqttdirection Direction;
	xmqttcodecconfig Config;
	size_t Body, Total, PropertyBytes, PropertyCount;
	size_t RootSize, WillSize;
	uint32 RootBody, WillBody;
	uint8 ConnectFlags;
} xmqtt5plan;

static bool xmqtt5WriteFail(xerrkind Kind, xmqttcodecerror Code, cstr Message)
{
	xrtSetErrorInfo(Kind,"xrt.mqtt",Code,Message); return false;
}

static bool xmqtt5WriteSpan(const void* Data, size_t Size)
{
	return ((Data!=NULL || !Size) && xrtMemRangeValid(Data,Size)) ||
		xmqtt5WriteFail(XERR_ARGUMENT,XMQTT_CODEC_ARGUMENT,"invalid MQTT descriptor data span");
}

static size_t xmqtt5WriteVbiSize(size_t Value)
{
	return 1u+(Value>=128u)+(Value>=16384u)+(Value>=2097152u);
}

static bool xmqtt5WriteAdd(xmqtt5plan* Plan, size_t Size)
{
	if (Size>XMQTT_VARIABLE_MAX-Plan->Body) return xmqtt5WriteFail(XERR_RANGE,XMQTT_CODEC_LIMIT,"MQTT body exceeds protocol length");
	Plan->Body+=Size;
	if (Plan->Body+1u+xmqtt5WriteVbiSize(Plan->Body)>Plan->Config.MaxPacketBytes)
		return xmqtt5WriteFail(XERR_RANGE,XMQTT_CODEC_LIMIT,"MQTT packet exceeds configured limit");
	return true;
}

static bool xmqtt5WriteField(xmqtt5plan* Plan, xbytesview Value, bool Text)
{
	if (!xmqtt5WriteSpan(Value.Data,Value.Size)) return false;
	if (Value.Size>XMQTT_STRING_MAX_BYTES) return xmqtt5WriteFail(XERR_RANGE,XMQTT_CODEC_LIMIT,"MQTT field exceeds two-byte length");
	if (!xmqtt5WriteAdd(Plan,2u+Value.Size)) return false;
	return !Text || xmqttUtf8StringCheck((xstrview){(cstr)Value.Data,Value.Size},Plan->Config.Utf8Flags,NULL);
}

static bool xmqtt5WriteText(xmqtt5plan* Plan, xstrview Value)
{
	return xmqtt5WriteField(Plan,(xbytesview){(cbytes)Value.Data,Value.Size},true);
}

static bool xmqtt5WriteProperties(xmqtt5plan* Plan, xmqttpropertylist Values, xmqttpropertyscope Scope, size_t* Size, uint32* Body)
{
	if (Plan->PropertyBytes==Plan->Config.MaxPropertyBytes)
		return xmqtt5WriteFail(XERR_RANGE,XMQTT_CODEC_LIMIT,"MQTT aggregate property byte limit exhausted");
	xmqttpropertyconfig Config={sizeof(Config),Plan->Config.MaxPropertyBytes-Plan->PropertyBytes,
		Plan->Config.MaxProperties-Plan->PropertyCount,Plan->Config.Utf8Flags}, Limits;
	/* At least two fixed-header bytes are needed. Bound the property preflight
	 * by the remaining packet budget before it scans any property text. The
	 * final Add also accounts for a longer Remaining Length integer. */
	size_t Available=Plan->Config.MaxPacketBytes-2u-Plan->Body;
	if (Available>XMQTT_VARIABLE_MAX-Plan->Body) Available=XMQTT_VARIABLE_MAX-Plan->Body;
	if (Config.MaxBytes>Available) Config.MaxBytes=Available;
	if (!Config.MaxBytes) return xmqtt5WriteFail(XERR_RANGE,XMQTT_CODEC_LIMIT,"no MQTT packet budget remains for a property section");
	if (!xmqttInternalPropertiesPrepare(Values.Data,Values.Count,Scope,Plan->Direction,&Config,&Limits,Size,Body)) return false;
	Plan->PropertyBytes+=*Size; Plan->PropertyCount+=Values.Count; return xmqtt5WriteAdd(Plan,*Size);
}

static bool xmqtt5WritePayload(xbytesview Value, xmqttpropertylist Properties)
{
	for (size_t Index=0; Index<Properties.Count; ++Index)
		if (Properties.Data[Index].Id==XMQTT_PROPERTY_PAYLOAD_FORMAT && Properties.Data[Index].Value.Number==1) {
			if (!xrtUtf8Valid((xstrview){(cstr)Value.Data,Value.Size},NULL))
				return xmqtt5WriteFail(XERR_PROTOCOL,XMQTT_CODEC_PAYLOAD_FORMAT,"MQTT indicated payload format is not well-formed UTF-8");
			break;
		}
	return true;
}

static bool xmqtt5WriteConnect(xmqtt5plan* Plan)
{
	const xmqttconnectpacket* Value=&Plan->Packet->Body.Connect;
	if (Value->WillQos>2 || (!Value->HasWill && (Value->WillRetain || Value->WillQos || Value->WillProperties.Count || Value->WillTopic.Size || Value->WillPayload.Size)) ||
		(!Value->HasUsername && Value->Username.Size) || (!Value->HasPassword && Value->Password.Size))
		return xmqtt5WriteFail(XERR_ARGUMENT,XMQTT_CODEC_ARGUMENT,"inconsistent MQTT CONNECT descriptor flags/fields");
	Plan->ConnectFlags=(uint8)((Value->CleanStart ? 2u : 0u)|(Value->HasWill ? 4u : 0u)|(Value->WillQos<<3)|
		(Value->WillRetain ? 32u : 0u)|(Value->HasUsername ? 128u : 0u)|(Value->HasPassword ? 64u : 0u));
	if (!xmqtt5WriteAdd(Plan,10) || !xmqtt5WriteProperties(Plan,Plan->Packet->Properties,XMQTT_PROPERTIES_CONNECT,&Plan->RootSize,&Plan->RootBody) ||
		!xmqtt5WriteText(Plan,Value->ClientId)) return false;
	if (Value->HasWill) {
		if (Value->WillPayload.Size>Plan->Config.MaxPayloadBytes)
			return xmqtt5WriteFail(XERR_RANGE,XMQTT_CODEC_LIMIT,"MQTT Will payload exceeds configured limit");
		if (!xmqtt5WriteProperties(Plan,Value->WillProperties,XMQTT_PROPERTIES_WILL,&Plan->WillSize,&Plan->WillBody) ||
			!xmqtt5WriteText(Plan,Value->WillTopic)) return false;
		if (!xmqttInternalTopic(Value->WillTopic,false,false,NULL)) return xmqtt5WriteFail(XERR_PROTOCOL,XMQTT_CODEC_TOPIC,"invalid MQTT Will Topic");
		if (!xmqtt5WriteField(Plan,Value->WillPayload,false)) return false;
	}
	if (Value->HasUsername && !xmqtt5WriteText(Plan,Value->Username)) return false;
	if (Value->HasPassword && !xmqtt5WriteField(Plan,Value->Password,false)) return false;
	return !Value->HasWill || xmqtt5WritePayload(Value->WillPayload,Value->WillProperties);
}

static bool xmqtt5WritePublish(xmqtt5plan* Plan)
{
	const xmqttpublishpacket* Value=&Plan->Packet->Body.Publish;
	if (!xmqtt5WriteSpan(Value->Payload.Data,Value->Payload.Size)) return false;
	if (Value->Payload.Size>Plan->Config.MaxPayloadBytes)
		return xmqtt5WriteFail(XERR_RANGE,XMQTT_CODEC_LIMIT,"MQTT PUBLISH payload exceeds configured limit");
	if (!xmqtt5WriteText(Plan,Value->Topic)) return false;
	if (Value->Topic.Size && !xmqttInternalTopic(Value->Topic,false,false,NULL))
		return xmqtt5WriteFail(XERR_PROTOCOL,XMQTT_CODEC_TOPIC,"PUBLISH Topic Name contains wildcards");
	if (Plan->Packet->Identifier && !xmqtt5WriteAdd(Plan,2)) return false;
	if (!xmqtt5WriteProperties(Plan,Plan->Packet->Properties,XMQTT_PROPERTIES_PUBLISH,&Plan->RootSize,&Plan->RootBody)) return false;
	if (!Value->Topic.Size) {
		bool Alias=false;
		for (size_t Index=0; Index<Plan->Packet->Properties.Count; ++Index) if (Plan->Packet->Properties.Data[Index].Id==XMQTT_PROPERTY_TOPIC_ALIAS) Alias=true;
		if (!Alias) return xmqtt5WriteFail(XERR_PROTOCOL,XMQTT_CODEC_TOPIC,"empty PUBLISH topic has no Topic Alias");
	}
	return xmqtt5WriteAdd(Plan,Value->Payload.Size) && xmqtt5WritePayload(Value->Payload,Plan->Packet->Properties);
}

static bool xmqtt5WriteTopics(xmqtt5plan* Plan)
{
	xmqttsubscriptionlist Values=Plan->Packet->Body.Topics;
	if (!Values.Count) return xmqtt5WriteFail(XERR_PROTOCOL,XMQTT_CODEC_PROTOCOL,"MQTT topic request has an empty payload");
	if (Values.Count>Plan->Config.MaxTopics) return xmqtt5WriteFail(XERR_RANGE,XMQTT_CODEC_LIMIT,"MQTT topic count exceeds configured limit");
	if (Values.Count>SIZE_MAX/sizeof(*Values.Data)) return xmqtt5WriteFail(XERR_ARGUMENT,XMQTT_CODEC_ARGUMENT,"MQTT topic vector size overflows");
	if (!xmqtt5WriteSpan(Values.Data,Values.Count*sizeof(*Values.Data))) return false;
	if (!xmqtt5WriteAdd(Plan,2) || !xmqtt5WriteProperties(Plan,Plan->Packet->Properties,(xmqttpropertyscope)Plan->Packet->Type,&Plan->RootSize,&Plan->RootBody)) return false;
	for (size_t Index=0; Index<Values.Count; ++Index) {
		const xmqttsubscription* Value=&Values.Data[Index]; bool Shared;
		if (!xmqtt5WriteText(Plan,Value->Filter)) return false;
		if (!xmqttInternalTopic(Value->Filter,true,true,&Shared)) return xmqtt5WriteFail(XERR_PROTOCOL,XMQTT_CODEC_TOPIC,"invalid MQTT Topic Filter");
		if (Plan->Packet->Type==XMQTT_PACKET_UNSUBSCRIBE) {
			if (Value->Options) return xmqtt5WriteFail(XERR_ARGUMENT,XMQTT_CODEC_ARGUMENT,"UNSUBSCRIBE descriptor has subscription options");
		} else {
			if ((Value->Options&0xc0u) || (Value->Options&3u)==3u || (Value->Options&0x30u)==0x30u || (Shared && (Value->Options&XMQTT_SUBSCRIBE_NO_LOCAL)))
				return xmqtt5WriteFail(XERR_PROTOCOL,XMQTT_CODEC_PROTOCOL,"invalid MQTT subscription options");
			if (!xmqtt5WriteAdd(Plan,1)) return false;
		}
	}
	return true;
}

static bool xmqtt5WritePrepare(const xmqttpacket* Packet, xmqttdirection Direction, const xmqttcodecconfig* Config, xmqtt5plan* Plan)
{
	if (Packet==NULL || !xmqtt5WriteSpan(Packet,sizeof(*Packet)) || !xmqttInternalCodecSettings(Direction,Config,&Plan->Config))
		return xmqtt5WriteFail(XERR_ARGUMENT,XMQTT_CODEC_ARGUMENT,"invalid MQTT packet/config descriptor");
	Plan->Packet=Packet; Plan->Direction=Direction;
	xmqttpackettype Type=Packet->Type;
	if (!xmqttInternalRole(Type,Direction) || !xmqttInternalFlags(Type,Packet->Flags,XMQTT_VERSION_5))
		return xmqtt5WriteFail(XERR_ARGUMENT,XMQTT_CODEC_ARGUMENT,"invalid MQTT descriptor type/direction/fixed flags");
	bool Identifier=(Type>=XMQTT_PACKET_PUBACK && Type<=XMQTT_PACKET_UNSUBACK) || (Type==XMQTT_PACKET_PUBLISH && (Packet->Flags&XMQTT_PUBLISH_QOS_MASK));
	bool Reason=Type==XMQTT_PACKET_CONNACK || (Type>=XMQTT_PACKET_PUBACK && Type<=XMQTT_PACKET_PUBCOMP) || Type>=XMQTT_PACKET_DISCONNECT;
	if (Identifier!=(Packet->Identifier!=0) || (Reason ? !xmqttInternalReason(XMQTT_VERSION_5,Type,Packet->Reason,Direction) : Packet->Reason!=0) ||
		(Packet->SessionPresent && (Type!=XMQTT_PACKET_CONNACK || Packet->Reason)))
		return xmqtt5WriteFail(XERR_PROTOCOL,XMQTT_CODEC_PROTOCOL,"inconsistent MQTT identifier/reason/session descriptor");
	if (Type==XMQTT_PACKET_CONNECT) return xmqtt5WriteConnect(Plan);
	if (Type==XMQTT_PACKET_PUBLISH) return xmqtt5WritePublish(Plan);
	if (Type==XMQTT_PACKET_SUBSCRIBE || Type==XMQTT_PACKET_UNSUBSCRIBE) return xmqtt5WriteTopics(Plan);
	if (Type==XMQTT_PACKET_CONNACK) return xmqtt5WriteAdd(Plan,2) &&
		xmqtt5WriteProperties(Plan,Packet->Properties,XMQTT_PROPERTIES_CONNACK,&Plan->RootSize,&Plan->RootBody);
	if (Type==XMQTT_PACKET_SUBACK || Type==XMQTT_PACKET_UNSUBACK) {
		xbytesview Reasons=Packet->Body.Reasons;
		if (!Reasons.Size) return xmqtt5WriteFail(XERR_PROTOCOL,XMQTT_CODEC_PROTOCOL,"MQTT acknowledgement reason list is empty");
		if (Reasons.Size>Plan->Config.MaxTopics) return xmqtt5WriteFail(XERR_RANGE,XMQTT_CODEC_LIMIT,"MQTT acknowledgement reason count exceeds limit");
		if (!xmqtt5WriteSpan(Reasons.Data,Reasons.Size) || !xmqtt5WriteAdd(Plan,2) ||
			!xmqtt5WriteProperties(Plan,Packet->Properties,(xmqttpropertyscope)Type,&Plan->RootSize,&Plan->RootBody) || !xmqtt5WriteAdd(Plan,Reasons.Size)) return false;
		for (size_t Index=0; Index<Reasons.Size; ++Index)
			if (!xmqttInternalReason(XMQTT_VERSION_5,Type,Reasons.Data[Index],Direction)) return xmqtt5WriteFail(XERR_PROTOCOL,XMQTT_CODEC_PROTOCOL,"invalid MQTT acknowledgement reason");
		return true;
	}
	if (Type==XMQTT_PACKET_PINGREQ || Type==XMQTT_PACKET_PINGRESP) {
		if (Packet->Properties.Count) return xmqtt5WriteFail(XERR_ARGUMENT,XMQTT_CODEC_ARGUMENT,"PING descriptor has properties");
		return true;
	}
	if (Type>=XMQTT_PACKET_PUBACK && Type<=XMQTT_PACKET_PUBCOMP) {
		if (!xmqtt5WriteAdd(Plan,2)) return false;
		if (!Packet->Reason && !Packet->Properties.Count) return true;
	} else if (!Packet->Reason && !Packet->Properties.Count) return true;
	if (!xmqtt5WriteAdd(Plan,1)) return false;
	if (!Packet->Properties.Count && Type!=XMQTT_PACKET_AUTH) return true;
	return xmqtt5WriteProperties(Plan,Packet->Properties,(xmqttpropertyscope)Type,&Plan->RootSize,&Plan->RootBody);
}

static bool xmqtt5WriteOverlap(const void* Output, size_t Size, const void* Data, size_t Bytes)
{
	return !xrtMemRangesOverlap(Output,Size,Data,Bytes) || xmqtt5WriteFail(XERR_ARGUMENT,XMQTT_CODEC_ARGUMENT,"MQTT output overlaps descriptor inputs");
}

static bool xmqtt5WriteOutput(const xmqtt5plan* Plan, const void* Output, size_t Size, const xmqttcodecconfig* Config)
{
	const xmqttpacket* Packet=Plan->Packet;
	if (Output==NULL) return xmqtt5WriteFail(XERR_ARGUMENT,XMQTT_CODEC_ARGUMENT,"NULL MQTT codec output");
	if (!xmqtt5WriteSpan(Output,Size) || !xmqtt5WriteOverlap(Output,Size,Packet,sizeof(*Packet)) ||
		(Config!=NULL && !xmqtt5WriteOverlap(Output,Size,Config,sizeof(*Config)))) return false;
	if (Plan->RootSize && !xmqttInternalPropertiesOutput(Output,Size,Packet->Properties.Data,Packet->Properties.Count,NULL)) return false;
	if (Packet->Type==XMQTT_PACKET_CONNECT) {
		const xmqttconnectpacket* Value=&Packet->Body.Connect;
		if (!xmqtt5WriteOverlap(Output,Size,Value->ClientId.Data,Value->ClientId.Size)) return false;
		if (Value->HasWill && (!xmqttInternalPropertiesOutput(Output,Size,Value->WillProperties.Data,Value->WillProperties.Count,NULL) ||
			!xmqtt5WriteOverlap(Output,Size,Value->WillTopic.Data,Value->WillTopic.Size) || !xmqtt5WriteOverlap(Output,Size,Value->WillPayload.Data,Value->WillPayload.Size))) return false;
		if (Value->HasUsername && !xmqtt5WriteOverlap(Output,Size,Value->Username.Data,Value->Username.Size)) return false;
		if (Value->HasPassword && !xmqtt5WriteOverlap(Output,Size,Value->Password.Data,Value->Password.Size)) return false;
	} else if (Packet->Type==XMQTT_PACKET_PUBLISH) {
		if (!xmqtt5WriteOverlap(Output,Size,Packet->Body.Publish.Topic.Data,Packet->Body.Publish.Topic.Size) ||
			!xmqtt5WriteOverlap(Output,Size,Packet->Body.Publish.Payload.Data,Packet->Body.Publish.Payload.Size)) return false;
	} else if (Packet->Type==XMQTT_PACKET_SUBSCRIBE || Packet->Type==XMQTT_PACKET_UNSUBSCRIBE) {
		xmqttsubscriptionlist Values=Packet->Body.Topics;
		if (!xmqtt5WriteOverlap(Output,Size,Values.Data,Values.Count*sizeof(*Values.Data))) return false;
		for (size_t Index=0; Index<Values.Count; ++Index)
			if (!xmqtt5WriteOverlap(Output,Size,Values.Data[Index].Filter.Data,Values.Data[Index].Filter.Size)) return false;
	} else if (Packet->Type==XMQTT_PACKET_SUBACK || Packet->Type==XMQTT_PACKET_UNSUBACK) {
		if (!xmqtt5WriteOverlap(Output,Size,Packet->Body.Reasons.Data,Packet->Body.Reasons.Size)) return false;
	}
	return true;
}

static unsigned char* xmqtt5EmitWord(unsigned char* Output, uint16 Value)
{
	*Output++=(unsigned char)(Value>>8); *Output++=(unsigned char)Value; return Output;
}

static unsigned char* xmqtt5EmitData(unsigned char* Output, const void* Data, size_t Size)
{
	if (Size) memcpy(Output,Data,Size);
	return Output+Size;
}

static unsigned char* xmqtt5EmitField(unsigned char* Output, const void* Data, size_t Size)
{
	return xmqtt5EmitData(xmqtt5EmitWord(Output,(uint16)Size),Data,Size);
}

static unsigned char* xmqtt5EmitProperties(unsigned char* Output, xmqttpropertylist Values, uint32 Body, size_t Size)
{
	return Output+xmqttInternalPropertiesEmit(Output,Values.Data,Values.Count,Body,Size);
}

static void xmqtt5Emit(const xmqtt5plan* Plan, ptr Output)
{
	const xmqttpacket* Packet=Plan->Packet; xmqttpackettype Type=Packet->Type; unsigned char* Cursor=(unsigned char*)Output; size_t Bytes;
	*Cursor++=(uint8)((Type<<4)|Packet->Flags); (void)xmqttVbiWrite(Cursor,Plan->Total-1u,(uint32)Plan->Body,&Bytes); Cursor+=Bytes;
	if (Type==XMQTT_PACKET_CONNECT) {
		const xmqttconnectpacket* Value=&Packet->Body.Connect;
		Cursor=xmqtt5EmitData(Cursor,"\x00\x04MQTT\x05",7); *Cursor++=Plan->ConnectFlags; Cursor=xmqtt5EmitWord(Cursor,Value->KeepAlive);
		Cursor=xmqtt5EmitProperties(Cursor,Packet->Properties,Plan->RootBody,Plan->RootSize);
		Cursor=xmqtt5EmitField(Cursor,Value->ClientId.Data,Value->ClientId.Size);
		if (Value->HasWill) {
			Cursor=xmqtt5EmitProperties(Cursor,Value->WillProperties,Plan->WillBody,Plan->WillSize);
			Cursor=xmqtt5EmitField(Cursor,Value->WillTopic.Data,Value->WillTopic.Size); Cursor=xmqtt5EmitField(Cursor,Value->WillPayload.Data,Value->WillPayload.Size);
		}
		if (Value->HasUsername) Cursor=xmqtt5EmitField(Cursor,Value->Username.Data,Value->Username.Size);
		if (Value->HasPassword) Cursor=xmqtt5EmitField(Cursor,Value->Password.Data,Value->Password.Size);
	} else if (Type==XMQTT_PACKET_PUBLISH) {
		Cursor=xmqtt5EmitField(Cursor,Packet->Body.Publish.Topic.Data,Packet->Body.Publish.Topic.Size);
		if (Packet->Identifier) Cursor=xmqtt5EmitWord(Cursor,Packet->Identifier);
		Cursor=xmqtt5EmitProperties(Cursor,Packet->Properties,Plan->RootBody,Plan->RootSize);
		(void)xmqtt5EmitData(Cursor,Packet->Body.Publish.Payload.Data,Packet->Body.Publish.Payload.Size);
	} else if (Type==XMQTT_PACKET_SUBSCRIBE || Type==XMQTT_PACKET_UNSUBSCRIBE) {
		Cursor=xmqtt5EmitWord(Cursor,Packet->Identifier); Cursor=xmqtt5EmitProperties(Cursor,Packet->Properties,Plan->RootBody,Plan->RootSize);
		for (size_t Index=0; Index<Packet->Body.Topics.Count; ++Index) {
			const xmqttsubscription* Value=&Packet->Body.Topics.Data[Index]; Cursor=xmqtt5EmitField(Cursor,Value->Filter.Data,Value->Filter.Size);
			if (Type==XMQTT_PACKET_SUBSCRIBE) *Cursor++=Value->Options;
		}
	} else if (Type==XMQTT_PACKET_CONNACK) {
		*Cursor++=Packet->SessionPresent ? 1u : 0u; *Cursor++=Packet->Reason;
		(void)xmqtt5EmitProperties(Cursor,Packet->Properties,Plan->RootBody,Plan->RootSize);
	} else if (Type==XMQTT_PACKET_SUBACK || Type==XMQTT_PACKET_UNSUBACK) {
		Cursor=xmqtt5EmitWord(Cursor,Packet->Identifier); Cursor=xmqtt5EmitProperties(Cursor,Packet->Properties,Plan->RootBody,Plan->RootSize);
		(void)xmqtt5EmitData(Cursor,Packet->Body.Reasons.Data,Packet->Body.Reasons.Size);
	} else if (Type!=XMQTT_PACKET_PINGREQ && Type!=XMQTT_PACKET_PINGRESP) {
		if (Packet->Identifier) Cursor=xmqtt5EmitWord(Cursor,Packet->Identifier);
		if (Plan->RootSize || Packet->Reason) *Cursor++=Packet->Reason;
		if (Plan->RootSize) (void)xmqtt5EmitProperties(Cursor,Packet->Properties,Plan->RootBody,Plan->RootSize);
	}
}

XMQTT_API bool xmqttPacketSize5(const xmqttpacket* Packet, xmqttdirection Direction, const xmqttcodecconfig* Config, size_t* Required)
{
	xmqtt5plan Plan={0};
	if (!xmqtt5WritePrepare(Packet,Direction,Config,&Plan) || !xmqtt5WriteOutput(&Plan,Required,sizeof(*Required),Config)) return false;
	*Required=Plan.Body+1u+xmqtt5WriteVbiSize(Plan.Body); return true;
}

XMQTT_API bool xmqttPacketWrite5(ptr Output, size_t Capacity, const xmqttpacket* Packet, xmqttdirection Direction, const xmqttcodecconfig* Config, size_t* Written)
{
	xmqtt5plan Plan={0};
	if (Output==NULL || !xmqtt5WriteSpan(Output,Capacity)) return xmqtt5WriteFail(XERR_ARGUMENT,XMQTT_CODEC_ARGUMENT,"invalid MQTT packet wire buffer");
	if (!xmqtt5WritePrepare(Packet,Direction,Config,&Plan)) return false;
	Plan.Total=Plan.Body+1u+xmqtt5WriteVbiSize(Plan.Body);
	if (Capacity<Plan.Total) return xmqtt5WriteFail(XERR_RANGE,XMQTT_CODEC_CAPACITY,"MQTT packet wire capacity too small");
	if (!xmqtt5WriteOutput(&Plan,Output,Plan.Total,Config) || (Written!=NULL &&
		(!xmqtt5WriteOutput(&Plan,Written,sizeof(*Written),Config) || !xmqtt5WriteOverlap(Written,sizeof(*Written),Output,Plan.Total)))) return false;
	xmqtt5Emit(&Plan,Output); if (Written!=NULL) *Written=Plan.Total; return true;
}
#endif
