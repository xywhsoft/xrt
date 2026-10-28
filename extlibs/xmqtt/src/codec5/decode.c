#include <xmqtt/codec5.h>
#include "../internal/xmqtt_internal.h"
#include <xrt/memory.h>
#include <string.h>

#if defined(XMQTT_FEATURE_XMQTT_CODEC5)

typedef struct xmqtt5reader {
	xbytesview Input;
	size_t Position, End, PropertiesUsed, PropertyBytes;
	xmqttdirection Direction;
	xmqttcodecconfig Config;
	xmqttcodecbuffers Buffers;
	xmqttpacket Value;
	xmqttcodecissue Failure;
} xmqtt5reader;

static bool xmqtt5ReadMark(xmqtt5reader* Reader, xmqttcodecstatus Status, xmqttcodecerror Error, size_t Offset)
{
	Reader->Failure=(xmqttcodecissue){Status,Error,Offset,Reader->Value.Type,Reader->Value.Flags,Reader->Value.Identifier}; return false;
}

static bool xmqtt5ReadFail(xmqtt5reader* Reader, xmqttcodecstatus Status, xmqttcodecerror Error, size_t Offset, cstr Message)
{
	xerrkind Kind=Status==XMQTT_CODEC_LOCAL_ERROR ? (Error==XMQTT_CODEC_ARGUMENT ? XERR_ARGUMENT : XERR_RANGE) : XERR_PROTOCOL;
	xmqtt5ReadMark(Reader,Status,Error,Offset); xrtSetErrorInfo(Kind,"xrt.mqtt",Error,Message); return false;
}

static bool xmqtt5ReadArgs(xbytesview Input, const xmqttcodecconfig* Config, const xmqttcodecbuffers* Buffers,
	xmqttpacket* Packet, xmqttcodecissue* Issue, xmqttcodecbuffers* Copy)
{
	if ((Input.Data==NULL && Input.Size) || !xrtMemRangeValid(Input.Data,Input.Size) || Packet==NULL || !xrtMemRangeValid(Packet,sizeof(*Packet)) ||
		(Issue!=NULL && !xrtMemRangeValid(Issue,sizeof(*Issue))) || (Buffers!=NULL && !xrtMemRangeValid(Buffers,sizeof(*Buffers)))) goto Invalid;
	*Copy=Buffers==NULL ? (xmqttcodecbuffers){0} : *Buffers;
	if (Copy->PropertyCapacity>SIZE_MAX/sizeof(xmqttproperty) || Copy->TopicCapacity>SIZE_MAX/sizeof(xmqttsubscription) ||
		(Copy->Properties==NULL && Copy->PropertyCapacity) || (Copy->Topics==NULL && Copy->TopicCapacity) ||
		!xrtMemRangeValid(Copy->Properties,Copy->PropertyCapacity*sizeof(xmqttproperty)) || !xrtMemRangeValid(Copy->Topics,Copy->TopicCapacity*sizeof(xmqttsubscription))) goto Invalid;
	const void* Outputs[]={Packet,Issue,Copy->Properties,Copy->Topics};
	size_t Sizes[]={sizeof(*Packet),Issue==NULL ? 0 : sizeof(*Issue),Copy->PropertyCapacity*sizeof(xmqttproperty),Copy->TopicCapacity*sizeof(xmqttsubscription)};
	for (size_t Index=0; Index<4; ++Index) {
		if (xrtMemRangesOverlap(Outputs[Index],Sizes[Index],Input.Data,Input.Size) ||
			(Config!=NULL && xrtMemRangesOverlap(Outputs[Index],Sizes[Index],Config,sizeof(*Config))) ||
			(Buffers!=NULL && xrtMemRangesOverlap(Outputs[Index],Sizes[Index],Buffers,sizeof(*Buffers)))) goto Invalid;
		for (size_t Other=0; Other<Index; ++Other)
			if (xrtMemRangesOverlap(Outputs[Index],Sizes[Index],Outputs[Other],Sizes[Other])) goto Invalid;
	}
	return true;
Invalid:
	xrtSetErrorInfo(XERR_ARGUMENT,"xrt.mqtt",XMQTT_CODEC_ARGUMENT,"invalid MQTT packet read spans or output overlap"); return false;
}

static bool xmqtt5ReadByte(xmqtt5reader* Reader, uint8* Value)
{
	if (Reader->Position==Reader->End) return xmqtt5ReadFail(Reader,XMQTT_CODEC_MALFORMED,XMQTT_CODEC_FORMAT,Reader->Position,"truncated MQTT byte field");
	*Value=Reader->Input.Data[Reader->Position++]; return true;
}

static bool xmqtt5ReadWord(xmqtt5reader* Reader, uint16* Value)
{
	if (Reader->End-Reader->Position<2) return xmqtt5ReadFail(Reader,XMQTT_CODEC_MALFORMED,XMQTT_CODEC_FORMAT,Reader->Position,"truncated MQTT two-byte integer");
	*Value=(uint16)(((uint16)Reader->Input.Data[Reader->Position]<<8)|Reader->Input.Data[Reader->Position+1u]); Reader->Position+=2; return true;
}

static bool xmqtt5ReadIdentifier(xmqtt5reader* Reader)
{
	uint16 Identifier; size_t Start=Reader->Position;
	if (!xmqtt5ReadWord(Reader,&Identifier)) return false;
	if (!Identifier) return xmqtt5ReadFail(Reader,XMQTT_CODEC_PROTOCOL_ERROR,XMQTT_CODEC_PROTOCOL,Start,"MQTT packet identifier is zero");
	Reader->Value.Identifier=Identifier; return true;
}

static bool xmqtt5ReadField(xmqtt5reader* Reader, bool Text, ptr Value)
{
	xbytesview Input={Reader->Input.Data+Reader->Position,Reader->End-Reader->Position}; size_t Read;
	xmqttwirestatus Status=Text ? xmqttUtf8StringRead(Input,Reader->Config.Utf8Flags,(xstrview*)Value,&Read) : xmqttBinaryRead(Input,(xbytesview*)Value,&Read);
	if (Status==XMQTT_WIRE_MORE) return xmqtt5ReadFail(Reader,XMQTT_CODEC_MALFORMED,XMQTT_CODEC_FORMAT,Reader->Position,"MQTT field crosses packet boundary");
	if (Status==XMQTT_WIRE_ERROR) return xmqtt5ReadMark(Reader,XMQTT_CODEC_MALFORMED,XMQTT_CODEC_FORMAT,Reader->Position);
	Reader->Position+=Read; return true;
}

static bool xmqtt5ReadProperties(xmqtt5reader* Reader, xmqttpropertyscope Scope, xmqttpropertylist* Values)
{
	if (Reader->PropertyBytes==Reader->Config.MaxPropertyBytes)
		return xmqtt5ReadFail(Reader,XMQTT_CODEC_LOCAL_ERROR,XMQTT_CODEC_LIMIT,Reader->Position,"MQTT aggregate property byte limit exhausted");
	xmqttpropertyconfig Config={sizeof(Config),Reader->Config.MaxPropertyBytes-Reader->PropertyBytes,
		Reader->Config.MaxProperties-Reader->PropertiesUsed,Reader->Config.Utf8Flags};
	xmqttproperty* Output=Reader->Buffers.Properties==NULL ? NULL : Reader->Buffers.Properties+Reader->PropertiesUsed;
	xmqttpropertydocument Doc;
	xmqttpropertystatus Status=xmqttPropertiesRead((xbytesview){Reader->Input.Data+Reader->Position,Reader->End-Reader->Position},Scope,Reader->Direction,
		&Config,Output,Reader->Buffers.PropertyCapacity-Reader->PropertiesUsed,&Doc);
	if (Status!=XMQTT_PROPERTIES_DONE) {
		if (Status==XMQTT_PROPERTIES_MORE) return xmqtt5ReadFail(Reader,XMQTT_CODEC_MALFORMED,XMQTT_CODEC_FORMAT,Reader->Position,"MQTT property section crosses packet boundary");
		return xmqtt5ReadMark(Reader,Status==XMQTT_PROPERTIES_LOCAL_ERROR ? XMQTT_CODEC_LOCAL_ERROR :
			Status==XMQTT_PROPERTIES_PROTOCOL_ERROR ? XMQTT_CODEC_PROTOCOL_ERROR : XMQTT_CODEC_MALFORMED,
			Status==XMQTT_PROPERTIES_LOCAL_ERROR ? XMQTT_CODEC_LIMIT : Status==XMQTT_PROPERTIES_PROTOCOL_ERROR ? XMQTT_CODEC_PROTOCOL : XMQTT_CODEC_FORMAT,Reader->Position);
	}
	*Values=(xmqttpropertylist){Doc.Count ? Output : NULL,Doc.Count};
	Reader->PropertiesUsed+=Doc.Count; Reader->PropertyBytes+=Doc.Consumed; Reader->Position+=Doc.Consumed; return true;
}

static bool xmqtt5ReadPayload(xmqtt5reader* Reader, xbytesview Data, xmqttpropertylist Properties, size_t Offset)
{
	if (Data.Size>Reader->Config.MaxPayloadBytes) return xmqtt5ReadFail(Reader,XMQTT_CODEC_LOCAL_ERROR,XMQTT_CODEC_LIMIT,Offset,"MQTT payload exceeds configured limit");
	for (size_t Index=0; Index<Properties.Count; ++Index) {
		if (Properties.Data[Index].Id==XMQTT_PROPERTY_PAYLOAD_FORMAT && Properties.Data[Index].Value.Number==1) {
			size_t Error;
			if (!xrtUtf8Valid((xstrview){(cstr)Data.Data,Data.Size},&Error))
				return xmqtt5ReadFail(Reader,XMQTT_CODEC_PAYLOAD_ERROR,XMQTT_CODEC_PAYLOAD_FORMAT,Offset+Error,"MQTT indicated payload format is not well-formed UTF-8");
			break;
		}
	}
	return true;
}

static bool xmqtt5ReadConnect(xmqtt5reader* Reader)
{
	xmqttconnectpacket* Value=&Reader->Value.Body.Connect; xstrview Name; uint8 Version, Flags; size_t Start=Reader->Position;
	if (!xmqtt5ReadField(Reader,true,&Name)) return false;
	if (Name.Size!=4 || memcmp(Name.Data,"MQTT",4)) return xmqtt5ReadFail(Reader,XMQTT_CODEC_UNSUPPORTED,XMQTT_CODEC_VERSION,Start,"unsupported MQTT protocol name");
	Start=Reader->Position; if (!xmqtt5ReadByte(Reader,&Version)) return false;
	if (Version!=5) return xmqtt5ReadFail(Reader,XMQTT_CODEC_UNSUPPORTED,XMQTT_CODEC_VERSION,Start,"CONNECT is not MQTT version 5");
	Start=Reader->Position; if (!xmqtt5ReadByte(Reader,&Flags)) return false;
	if ((Flags&1u) || ((Flags&0x18u)==0x18u) || (!(Flags&4u) && (Flags&0x38u)))
		return xmqtt5ReadFail(Reader,XMQTT_CODEC_MALFORMED,XMQTT_CODEC_FORMAT,Start,"invalid MQTT CONNECT flags");
	Value->CleanStart=(Flags&2u)!=0; Value->HasWill=(Flags&4u)!=0; Value->WillQos=(Flags>>3)&3u; Value->WillRetain=(Flags&0x20u)!=0;
	Value->HasUsername=(Flags&0x80u)!=0; Value->HasPassword=(Flags&0x40u)!=0;
	if (!xmqtt5ReadWord(Reader,&Value->KeepAlive) || !xmqtt5ReadProperties(Reader,XMQTT_PROPERTIES_CONNECT,&Reader->Value.Properties) ||
		!xmqtt5ReadField(Reader,true,&Value->ClientId)) return false;
	if (Value->HasWill) {
		if (!xmqtt5ReadProperties(Reader,XMQTT_PROPERTIES_WILL,&Value->WillProperties)) return false;
		Start=Reader->Position; if (!xmqtt5ReadField(Reader,true,&Value->WillTopic)) return false;
		if (!xmqttInternalTopic(Value->WillTopic,false,false,NULL)) return xmqtt5ReadFail(Reader,XMQTT_CODEC_PROTOCOL_ERROR,XMQTT_CODEC_TOPIC,Start,"invalid MQTT Will Topic");
		Start=Reader->Position;
		if (!xmqtt5ReadField(Reader,false,&Value->WillPayload) || !xmqtt5ReadPayload(Reader,Value->WillPayload,Value->WillProperties,Start+2u)) return false;
	}
	if (Value->HasUsername && !xmqtt5ReadField(Reader,true,&Value->Username)) return false;
	if (Value->HasPassword && !xmqtt5ReadField(Reader,false,&Value->Password)) return false;
	return true;
}

static bool xmqtt5ReadPublish(xmqtt5reader* Reader)
{
	xmqttpublishpacket* Value=&Reader->Value.Body.Publish; size_t Start=Reader->Position;
	if (!xmqtt5ReadField(Reader,true,&Value->Topic)) return false;
	if (Value->Topic.Size && !xmqttInternalTopic(Value->Topic,false,false,NULL)) return xmqtt5ReadFail(Reader,XMQTT_CODEC_PROTOCOL_ERROR,XMQTT_CODEC_TOPIC,Start,"PUBLISH Topic Name contains wildcards");
	if ((Reader->Value.Flags&XMQTT_PUBLISH_QOS_MASK) && !xmqtt5ReadIdentifier(Reader)) return false;
	if (!xmqtt5ReadProperties(Reader,XMQTT_PROPERTIES_PUBLISH,&Reader->Value.Properties)) return false;
	if (!Value->Topic.Size) {
		bool Alias=false;
		for (size_t Index=0; Index<Reader->Value.Properties.Count; ++Index) if (Reader->Value.Properties.Data[Index].Id==XMQTT_PROPERTY_TOPIC_ALIAS) Alias=true;
		if (!Alias) return xmqtt5ReadFail(Reader,XMQTT_CODEC_PROTOCOL_ERROR,XMQTT_CODEC_TOPIC,Start,"empty PUBLISH topic has no Topic Alias");
	}
	Value->Payload=(xbytesview){Reader->Input.Data+Reader->Position,Reader->End-Reader->Position};
	if (!xmqtt5ReadPayload(Reader,Value->Payload,Reader->Value.Properties,Reader->Position)) return false;
	Reader->Position=Reader->End; return true;
}

static bool xmqtt5ReadTopics(xmqtt5reader* Reader)
{
	if (!xmqtt5ReadIdentifier(Reader) || !xmqtt5ReadProperties(Reader,(xmqttpropertyscope)Reader->Value.Type,&Reader->Value.Properties)) return false;
	size_t Count=0;
	while (Reader->Position<Reader->End) {
		if (Count>=Reader->Config.MaxTopics) return xmqtt5ReadFail(Reader,XMQTT_CODEC_LOCAL_ERROR,XMQTT_CODEC_LIMIT,Reader->Position,"MQTT topic count exceeds configured limit");
		if (Count>=Reader->Buffers.TopicCapacity) return xmqtt5ReadFail(Reader,XMQTT_CODEC_LOCAL_ERROR,XMQTT_CODEC_CAPACITY,Reader->Position,"MQTT topic scratch capacity exhausted");
		xmqttsubscription Value={0}; bool Shared; size_t Start=Reader->Position;
		if (!xmqtt5ReadField(Reader,true,&Value.Filter)) return false;
		if (!xmqttInternalTopic(Value.Filter,true,true,&Shared)) return xmqtt5ReadFail(Reader,XMQTT_CODEC_PROTOCOL_ERROR,XMQTT_CODEC_TOPIC,Start,"invalid MQTT Topic Filter");
		if (Reader->Value.Type==XMQTT_PACKET_SUBSCRIBE) {
			Start=Reader->Position; if (!xmqtt5ReadByte(Reader,&Value.Options)) return false;
			if ((Value.Options&0xc0u) || (Value.Options&3u)==3u || (Value.Options&0x30u)==0x30u)
				return xmqtt5ReadFail(Reader,XMQTT_CODEC_MALFORMED,XMQTT_CODEC_FORMAT,Start,"invalid MQTT subscription option bits");
			if (Shared && (Value.Options&XMQTT_SUBSCRIBE_NO_LOCAL)) return xmqtt5ReadFail(Reader,XMQTT_CODEC_PROTOCOL_ERROR,XMQTT_CODEC_PROTOCOL,Start,"No Local is forbidden on shared subscriptions");
		}
		Reader->Buffers.Topics[Count++]=Value;
	}
	if (!Count) return xmqtt5ReadFail(Reader,XMQTT_CODEC_PROTOCOL_ERROR,XMQTT_CODEC_PROTOCOL,Reader->Position,"MQTT topic request has an empty payload");
	Reader->Value.Body.Topics=(xmqttsubscriptionlist){Reader->Buffers.Topics,Count}; return true;
}

static bool xmqtt5ReadReason(xmqtt5reader* Reader)
{
	size_t Start=Reader->Position;
	if (!xmqtt5ReadByte(Reader,&Reader->Value.Reason)) return false;
	if (!xmqttInternalReason(XMQTT_VERSION_5,Reader->Value.Type,Reader->Value.Reason,Reader->Direction))
		return xmqtt5ReadFail(Reader,XMQTT_CODEC_PROTOCOL_ERROR,XMQTT_CODEC_PROTOCOL,Start,"reason code is not valid for MQTT packet/direction");
	return true;
}

static bool xmqtt5ReadBody(xmqtt5reader* Reader)
{
	xmqttpackettype Type=Reader->Value.Type;
	if (Type==XMQTT_PACKET_CONNECT) return xmqtt5ReadConnect(Reader);
	if (Type==XMQTT_PACKET_PUBLISH) return xmqtt5ReadPublish(Reader);
	if (Type==XMQTT_PACKET_SUBSCRIBE || Type==XMQTT_PACKET_UNSUBSCRIBE) return xmqtt5ReadTopics(Reader);
	if (Type==XMQTT_PACKET_CONNACK) {
		uint8 Flags; size_t Start=Reader->Position;
		if (!xmqtt5ReadByte(Reader,&Flags)) return false;
		if (Flags&0xfeu) return xmqtt5ReadFail(Reader,XMQTT_CODEC_MALFORMED,XMQTT_CODEC_FORMAT,Start,"reserved CONNACK bits are nonzero");
		Reader->Value.SessionPresent=Flags!=0;
		if (!xmqtt5ReadReason(Reader)) return false;
		if (Reader->Value.Reason && Flags) return xmqtt5ReadFail(Reader,XMQTT_CODEC_PROTOCOL_ERROR,XMQTT_CODEC_PROTOCOL,Start,"failed CONNACK has Session Present");
		return xmqtt5ReadProperties(Reader,XMQTT_PROPERTIES_CONNACK,&Reader->Value.Properties);
	}
	if (Type==XMQTT_PACKET_PINGREQ || Type==XMQTT_PACKET_PINGRESP) return true;
	if (Type==XMQTT_PACKET_SUBACK || Type==XMQTT_PACKET_UNSUBACK) {
		if (!xmqtt5ReadIdentifier(Reader) || !xmqtt5ReadProperties(Reader,(xmqttpropertyscope)Type,&Reader->Value.Properties)) return false;
		size_t Count=Reader->End-Reader->Position;
		if (!Count) return xmqtt5ReadFail(Reader,XMQTT_CODEC_PROTOCOL_ERROR,XMQTT_CODEC_PROTOCOL,Reader->Position,"MQTT acknowledgement has no reason list");
		if (Count>Reader->Config.MaxTopics) return xmqtt5ReadFail(Reader,XMQTT_CODEC_LOCAL_ERROR,XMQTT_CODEC_LIMIT,Reader->Position,"MQTT acknowledgement reason count exceeds limit");
		for (size_t Index=0; Index<Count; ++Index)
			if (!xmqttInternalReason(XMQTT_VERSION_5,Type,Reader->Input.Data[Reader->Position+Index],Reader->Direction))
				return xmqtt5ReadFail(Reader,XMQTT_CODEC_PROTOCOL_ERROR,XMQTT_CODEC_PROTOCOL,Reader->Position+Index,"invalid MQTT acknowledgement reason");
		Reader->Value.Body.Reasons=(xbytesview){Reader->Input.Data+Reader->Position,Count}; Reader->Position=Reader->End; return true;
	}
	if (Type>=XMQTT_PACKET_PUBACK && Type<=XMQTT_PACKET_PUBCOMP) {
		if (!xmqtt5ReadIdentifier(Reader)) return false;
		if (Reader->Position<Reader->End && !xmqtt5ReadReason(Reader)) return false;
		return Reader->Position==Reader->End || xmqtt5ReadProperties(Reader,(xmqttpropertyscope)Type,&Reader->Value.Properties);
	}
	/* DISCONNECT and AUTH have no identifier. AUTH reason-only is not a defined
	 * abbreviation; success with no body is explicitly permitted by the spec. */
	if (Reader->Position==Reader->End) {
		if (!xmqttInternalReason(XMQTT_VERSION_5,Type,0,Reader->Direction))
			return xmqtt5ReadFail(Reader,XMQTT_CODEC_PROTOCOL_ERROR,XMQTT_CODEC_PROTOCOL,Reader->Position,"implicit MQTT reason is invalid for sender");
		return true;
	}
	if (!xmqtt5ReadReason(Reader)) return false;
	if (Type==XMQTT_PACKET_DISCONNECT && Reader->Position==Reader->End) return true;
	return xmqtt5ReadProperties(Reader,(xmqttpropertyscope)Type,&Reader->Value.Properties);
}

XMQTT_API xmqttcodecstatus xmqttPacketRead5(xbytesview Input, xmqttdirection Direction,
	const xmqttcodecconfig* Config, const xmqttcodecbuffers* Buffers, xmqttpacket* Packet, xmqttcodecissue* Issue)
{
	xmqtt5reader Reader={0}; Reader.Input=Input; Reader.Direction=Direction;
	if (!xmqttInternalCodecSettings(Direction,Config,&Reader.Config) || !xmqtt5ReadArgs(Input,Config,Buffers,Packet,Issue,&Reader.Buffers)) return XMQTT_CODEC_LOCAL_ERROR;
	xmqttwireconfig Wire={sizeof(Wire),XMQTT_VERSION_5,Reader.Config.MaxPacketBytes}; xmqttfixedheader Header; xmqttwireerror Error=0;
	xmqttwirestatus Status=xmqttInternalHeader(Input,&Wire,&Header,&Error);
	if (Status==XMQTT_WIRE_MORE) return XMQTT_CODEC_MORE;
	if (Status==XMQTT_WIRE_ERROR) { xmqtt5ReadMark(&Reader,Error==XMQTT_WIRE_LIMIT ? XMQTT_CODEC_LOCAL_ERROR : XMQTT_CODEC_MALFORMED,
		Error==XMQTT_WIRE_LIMIT ? XMQTT_CODEC_LIMIT : XMQTT_CODEC_FORMAT,0); goto Failed; }
	Reader.Value.Type=Header.Type; Reader.Value.Flags=Header.Flags;
	if (!xmqttInternalRole(Header.Type,Direction)) { xmqtt5ReadFail(&Reader,XMQTT_CODEC_PROTOCOL_ERROR,XMQTT_CODEC_PROTOCOL,0,"MQTT packet travels in forbidden direction"); goto Failed; }
	if ((Header.Type==XMQTT_PACKET_PINGREQ || Header.Type==XMQTT_PACKET_PINGRESP) && Header.RemainingLength) {
		xmqtt5ReadFail(&Reader,XMQTT_CODEC_MALFORMED,XMQTT_CODEC_FORMAT,0,"MQTT PING packet has a body"); goto Failed;
	}
	if (Header.Type==XMQTT_PACKET_AUTH && Header.RemainingLength==1) {
		xmqtt5ReadFail(&Reader,XMQTT_CODEC_MALFORMED,XMQTT_CODEC_FORMAT,0,"AUTH lacks its property section"); goto Failed;
	}
	if (Input.Size<Header.PacketBytes) return XMQTT_CODEC_MORE;
	Reader.Position=Header.HeaderBytes; Reader.End=Header.PacketBytes;
	if (!xmqtt5ReadBody(&Reader)) goto Failed;
	if (Reader.Position!=Reader.End) { xmqtt5ReadFail(&Reader,XMQTT_CODEC_MALFORMED,XMQTT_CODEC_FORMAT,Reader.Position,"unexpected trailing MQTT packet body bytes"); goto Failed; }
	Reader.Value.Wire=(xbytesview){Input.Data,Header.PacketBytes}; *Packet=Reader.Value;
	if (Issue!=NULL) *Issue=(xmqttcodecissue){XMQTT_CODEC_DONE,XMQTT_CODEC_OK,XRT_NPOS,0,0,0};
	return XMQTT_CODEC_DONE;
Failed:
	if (Issue!=NULL) *Issue=Reader.Failure;
	return Reader.Failure.Status;
}
#endif
