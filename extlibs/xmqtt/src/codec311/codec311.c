#include <xmqtt/codec311.h>
#include "../internal/xmqtt_internal.h"
#include <xrt/memory.h>
#include <string.h>

#if defined(XMQTT_FEATURE_XMQTT_CODEC311)

/* Separate static protocol paths keep the 3.1.1 leaf independent of properties
 * and avoid a version switch in each field. Wire/Unicode and packet grammar
 * come from the same shared leaves as the 5.0 codec. */
static bool xmqtt311Fail(xerrkind Kind, xmqttcodecerror Error, cstr Message)
{
	xrtSetErrorInfo(Kind,"xrt.mqtt",Error,Message); return false;
}

static bool xmqtt311Span(const void* Data, size_t Size)
{
	return ((Data!=NULL || !Size) && xrtMemRangeValid(Data,Size)) || xmqtt311Fail(XERR_ARGUMENT,XMQTT_CODEC_ARGUMENT,"invalid MQTT 3.1.1 data span");
}

static bool xmqtt311Role(xmqttpackettype Type, xmqttdirection Direction)
{
	return Type!=XMQTT_PACKET_AUTH && xmqttInternalRole(Type,Direction) && (Type!=XMQTT_PACKET_DISCONNECT || Direction==XMQTT_CLIENT_TO_SERVER);
}

typedef struct xmqtt311reader {
	xbytesview Input;
	size_t Position, End;
	xmqttdirection Direction;
	xmqttcodecconfig Config;
	xmqttcodecbuffers Buffers;
	xmqttpacket Value;
	xmqttcodecissue Failure;
} xmqtt311reader;

static bool xmqtt311Mark(xmqtt311reader* Reader, xmqttcodecstatus Status, xmqttcodecerror Error, size_t Offset)
{
	Reader->Failure=(xmqttcodecissue){Status,Error,Offset,Reader->Value.Type,Reader->Value.Flags,Reader->Value.Identifier}; return false;
}

static bool xmqtt311ReadFail(xmqtt311reader* Reader, xmqttcodecstatus Status, xmqttcodecerror Error, size_t Offset, cstr Message)
{
	xmqtt311Mark(Reader,Status,Error,Offset); return xmqtt311Fail(Status==XMQTT_CODEC_LOCAL_ERROR ? XERR_RANGE : XERR_PROTOCOL,Error,Message);
}

static bool xmqtt311ReadArgs(xbytesview Input, const xmqttcodecconfig* Config, const xmqttcodecbuffers* Buffers,
	xmqttpacket* Packet, xmqttcodecissue* Issue, xmqttcodecbuffers* Copy)
{
	if (!xmqtt311Span(Input.Data,Input.Size) || Packet==NULL || !xrtMemRangeValid(Packet,sizeof(*Packet)) ||
		(Issue!=NULL && !xrtMemRangeValid(Issue,sizeof(*Issue))) || (Buffers!=NULL && !xrtMemRangeValid(Buffers,sizeof(*Buffers)))) goto Invalid;
	*Copy=Buffers==NULL ? (xmqttcodecbuffers){0} : *Buffers;
	if (Copy->TopicCapacity>SIZE_MAX/sizeof(xmqttsubscription) || (Copy->Topics==NULL && Copy->TopicCapacity) ||
		!xrtMemRangeValid(Copy->Topics,Copy->TopicCapacity*sizeof(xmqttsubscription))) goto Invalid;
	const void* Outputs[]={Packet,Issue,Copy->Topics};
	size_t Sizes[]={sizeof(*Packet),Issue==NULL ? 0 : sizeof(*Issue),Copy->TopicCapacity*sizeof(xmqttsubscription)};
	for (size_t Index=0; Index<3; ++Index) {
		if (xrtMemRangesOverlap(Outputs[Index],Sizes[Index],Input.Data,Input.Size) ||
			(Config!=NULL && xrtMemRangesOverlap(Outputs[Index],Sizes[Index],Config,sizeof(*Config))) ||
			(Buffers!=NULL && xrtMemRangesOverlap(Outputs[Index],Sizes[Index],Buffers,sizeof(*Buffers)))) goto Invalid;
		for (size_t Other=0; Other<Index; ++Other) if (xrtMemRangesOverlap(Outputs[Index],Sizes[Index],Outputs[Other],Sizes[Other])) goto Invalid;
	}
	return true;
Invalid:
	return xmqtt311Fail(XERR_ARGUMENT,XMQTT_CODEC_ARGUMENT,"invalid MQTT 3.1.1 read outputs or aliases");
}

static bool xmqtt311ReadByte(xmqtt311reader* Reader, uint8* Value)
{
	if (Reader->Position==Reader->End) return xmqtt311ReadFail(Reader,XMQTT_CODEC_MALFORMED,XMQTT_CODEC_FORMAT,Reader->Position,"truncated MQTT byte");
	*Value=Reader->Input.Data[Reader->Position++]; return true;
}

static bool xmqtt311ReadWord(xmqtt311reader* Reader, uint16* Value)
{
	if (Reader->End-Reader->Position<2) return xmqtt311ReadFail(Reader,XMQTT_CODEC_MALFORMED,XMQTT_CODEC_FORMAT,Reader->Position,"truncated MQTT two-byte integer");
	*Value=(uint16)(((uint16)Reader->Input.Data[Reader->Position]<<8)|Reader->Input.Data[Reader->Position+1u]); Reader->Position+=2; return true;
}

static bool xmqtt311ReadIdentifier(xmqtt311reader* Reader)
{
	uint16 Value; size_t Start=Reader->Position;
	if (!xmqtt311ReadWord(Reader,&Value)) return false;
	if (!Value) return xmqtt311ReadFail(Reader,XMQTT_CODEC_PROTOCOL_ERROR,XMQTT_CODEC_PROTOCOL,Start,"MQTT packet identifier is zero");
	Reader->Value.Identifier=Value; return true;
}

static bool xmqtt311ReadField(xmqtt311reader* Reader, bool Text, ptr Value)
{
	xbytesview Input={Reader->Input.Data+Reader->Position,Reader->End-Reader->Position}; size_t Bytes;
	xmqttwirestatus Status=Text ? xmqttUtf8StringRead(Input,Reader->Config.Utf8Flags,(xstrview*)Value,&Bytes) : xmqttBinaryRead(Input,(xbytesview*)Value,&Bytes);
	if (Status==XMQTT_WIRE_MORE) return xmqtt311ReadFail(Reader,XMQTT_CODEC_MALFORMED,XMQTT_CODEC_FORMAT,Reader->Position,"MQTT field crosses packet boundary");
	if (Status==XMQTT_WIRE_ERROR) return xmqtt311Mark(Reader,XMQTT_CODEC_MALFORMED,XMQTT_CODEC_FORMAT,Reader->Position);
	Reader->Position+=Bytes; return true;
}

static bool xmqtt311ReadConnect(xmqtt311reader* Reader)
{
	xmqttconnectpacket* Value=&Reader->Value.Body.Connect; xstrview Name; uint8 Version, Flags; size_t Start=Reader->Position;
	if (!xmqtt311ReadField(Reader,true,&Name)) return false;
	if (Name.Size!=4 || memcmp(Name.Data,"MQTT",4)) return xmqtt311ReadFail(Reader,XMQTT_CODEC_UNSUPPORTED,XMQTT_CODEC_VERSION,Start,"unsupported MQTT protocol name");
	Start=Reader->Position; if (!xmqtt311ReadByte(Reader,&Version)) return false;
	if (Version!=4) return xmqtt311ReadFail(Reader,XMQTT_CODEC_UNSUPPORTED,XMQTT_CODEC_VERSION,Start,"CONNECT is not MQTT 3.1.1");
	Start=Reader->Position; if (!xmqtt311ReadByte(Reader,&Flags)) return false;
	if ((Flags&1u) || (Flags&0x18u)==0x18u || (!(Flags&4u) && (Flags&0x38u)) || ((Flags&0xc0u)==0x40u))
		return xmqtt311ReadFail(Reader,XMQTT_CODEC_MALFORMED,XMQTT_CODEC_FORMAT,Start,"invalid MQTT 3.1.1 CONNECT flags");
	Value->CleanStart=(Flags&2u)!=0; Value->HasWill=(Flags&4u)!=0; Value->WillQos=(Flags>>3)&3u; Value->WillRetain=(Flags&32u)!=0;
	Value->HasUsername=(Flags&128u)!=0; Value->HasPassword=(Flags&64u)!=0;
	if (!xmqtt311ReadWord(Reader,&Value->KeepAlive)) return false;
	Start=Reader->Position; if (!xmqtt311ReadField(Reader,true,&Value->ClientId)) return false;
	if (!Value->ClientId.Size && !Value->CleanStart) return xmqtt311ReadFail(Reader,XMQTT_CODEC_PROTOCOL_ERROR,XMQTT_CODEC_CLIENT_ID,Start,"empty MQTT 3.1.1 ClientId requires Clean Session");
	if (Value->HasWill) {
		Start=Reader->Position; if (!xmqtt311ReadField(Reader,true,&Value->WillTopic)) return false;
		if (!xmqttInternalTopic(Value->WillTopic,false,false,NULL)) return xmqtt311ReadFail(Reader,XMQTT_CODEC_PROTOCOL_ERROR,XMQTT_CODEC_TOPIC,Start,"invalid MQTT Will Topic");
		Start=Reader->Position; if (!xmqtt311ReadField(Reader,false,&Value->WillPayload)) return false;
		if (Value->WillPayload.Size>Reader->Config.MaxPayloadBytes) return xmqtt311ReadFail(Reader,XMQTT_CODEC_LOCAL_ERROR,XMQTT_CODEC_LIMIT,Start,"MQTT Will payload exceeds limit");
	}
	if (Value->HasUsername && !xmqtt311ReadField(Reader,true,&Value->Username)) return false;
	if (Value->HasPassword && !xmqtt311ReadField(Reader,false,&Value->Password)) return false;
	return true;
}

static bool xmqtt311ReadBody(xmqtt311reader* Reader)
{
	xmqttpackettype Type=Reader->Value.Type;
	if (Type==XMQTT_PACKET_CONNECT) return xmqtt311ReadConnect(Reader);
	if (Type==XMQTT_PACKET_PUBLISH) {
		xmqttpublishpacket* Value=&Reader->Value.Body.Publish; size_t Start=Reader->Position;
		if (!xmqtt311ReadField(Reader,true,&Value->Topic)) return false;
		if (!xmqttInternalTopic(Value->Topic,false,false,NULL)) return xmqtt311ReadFail(Reader,XMQTT_CODEC_PROTOCOL_ERROR,XMQTT_CODEC_TOPIC,Start,"invalid MQTT PUBLISH Topic Name");
		if ((Reader->Value.Flags&XMQTT_PUBLISH_QOS_MASK) && !xmqtt311ReadIdentifier(Reader)) return false;
		Value->Payload=(xbytesview){Reader->Input.Data+Reader->Position,Reader->End-Reader->Position};
		if (Value->Payload.Size>Reader->Config.MaxPayloadBytes) return xmqtt311ReadFail(Reader,XMQTT_CODEC_LOCAL_ERROR,XMQTT_CODEC_LIMIT,Reader->Position,"MQTT PUBLISH payload exceeds limit");
		Reader->Position=Reader->End; return true;
	}
	if (Type==XMQTT_PACKET_CONNACK) {
		uint8 Flags; size_t Start=Reader->Position;
		if (!xmqtt311ReadByte(Reader,&Flags) || !xmqtt311ReadByte(Reader,&Reader->Value.Reason)) return false;
		if (Flags&0xfeu) return xmqtt311ReadFail(Reader,XMQTT_CODEC_MALFORMED,XMQTT_CODEC_FORMAT,Start,"reserved CONNACK flags are nonzero");
		if (!xmqttInternalReason(XMQTT_VERSION_311,Type,Reader->Value.Reason,Reader->Direction) || (Flags && Reader->Value.Reason))
			return xmqtt311ReadFail(Reader,XMQTT_CODEC_PROTOCOL_ERROR,XMQTT_CODEC_PROTOCOL,Start,"invalid MQTT 3.1.1 CONNACK code/session flag");
		Reader->Value.SessionPresent=Flags!=0; return true;
	}
	if (Type==XMQTT_PACKET_PINGREQ || Type==XMQTT_PACKET_PINGRESP || Type==XMQTT_PACKET_DISCONNECT) return true;
	if (!xmqtt311ReadIdentifier(Reader)) return false;
	if (Type==XMQTT_PACKET_SUBACK) {
		size_t Count=Reader->End-Reader->Position;
		if (!Count) return xmqtt311ReadFail(Reader,XMQTT_CODEC_PROTOCOL_ERROR,XMQTT_CODEC_PROTOCOL,Reader->Position,"SUBACK return-code list is empty");
		if (Count>Reader->Config.MaxTopics) return xmqtt311ReadFail(Reader,XMQTT_CODEC_LOCAL_ERROR,XMQTT_CODEC_LIMIT,Reader->Position,"SUBACK return-code count exceeds limit");
		for (size_t Index=0; Index<Count; ++Index)
			if (!xmqttInternalReason(XMQTT_VERSION_311,Type,Reader->Input.Data[Reader->Position+Index],Reader->Direction))
				return xmqtt311ReadFail(Reader,XMQTT_CODEC_PROTOCOL_ERROR,XMQTT_CODEC_PROTOCOL,Reader->Position+Index,"invalid SUBACK return code");
		Reader->Value.Body.Reasons=(xbytesview){Reader->Input.Data+Reader->Position,Count}; Reader->Position=Reader->End; return true;
	}
	if (Type==XMQTT_PACKET_SUBSCRIBE || Type==XMQTT_PACKET_UNSUBSCRIBE) {
		size_t Count=0;
		while (Reader->Position<Reader->End) {
			if (Count>=Reader->Config.MaxTopics) return xmqtt311ReadFail(Reader,XMQTT_CODEC_LOCAL_ERROR,XMQTT_CODEC_LIMIT,Reader->Position,"MQTT topic count exceeds limit");
			if (Count>=Reader->Buffers.TopicCapacity) return xmqtt311ReadFail(Reader,XMQTT_CODEC_LOCAL_ERROR,XMQTT_CODEC_CAPACITY,Reader->Position,"MQTT topic scratch exhausted");
			xmqttsubscription Value={0}; size_t Start=Reader->Position;
			if (!xmqtt311ReadField(Reader,true,&Value.Filter)) return false;
			if (!xmqttInternalTopic(Value.Filter,true,false,NULL)) return xmqtt311ReadFail(Reader,XMQTT_CODEC_PROTOCOL_ERROR,XMQTT_CODEC_TOPIC,Start,"invalid MQTT Topic Filter");
			if (Type==XMQTT_PACKET_SUBSCRIBE) {
				Start=Reader->Position; if (!xmqtt311ReadByte(Reader,&Value.Options)) return false;
				if (Value.Options>2) return xmqtt311ReadFail(Reader,XMQTT_CODEC_MALFORMED,XMQTT_CODEC_FORMAT,Start,"invalid MQTT 3.1.1 subscription QoS byte");
			}
			Reader->Buffers.Topics[Count++]=Value;
		}
		if (!Count) return xmqtt311ReadFail(Reader,XMQTT_CODEC_PROTOCOL_ERROR,XMQTT_CODEC_PROTOCOL,Reader->Position,"MQTT topic request is empty");
		Reader->Value.Body.Topics=(xmqttsubscriptionlist){Reader->Buffers.Topics,Count};
	}
	return true;
}

XMQTT_API xmqttcodecstatus xmqttPacketRead311(xbytesview Input, xmqttdirection Direction,
	const xmqttcodecconfig* Config, const xmqttcodecbuffers* Buffers, xmqttpacket* Packet, xmqttcodecissue* Issue)
{
	xmqtt311reader Reader={0}; Reader.Input=Input; Reader.Direction=Direction;
	if (!xmqttInternalCodecSettings(Direction,Config,&Reader.Config) || !xmqtt311ReadArgs(Input,Config,Buffers,Packet,Issue,&Reader.Buffers)) return XMQTT_CODEC_LOCAL_ERROR;
	xmqttwireconfig Wire={sizeof(Wire),XMQTT_VERSION_311,Reader.Config.MaxPacketBytes}; xmqttfixedheader Header; xmqttwireerror Error=0;
	xmqttwirestatus Status=xmqttInternalHeader(Input,&Wire,&Header,&Error);
	if (Status==XMQTT_WIRE_MORE) return XMQTT_CODEC_MORE;
	if (Status==XMQTT_WIRE_ERROR) {
		xmqtt311Mark(&Reader,Error==XMQTT_WIRE_LIMIT ? XMQTT_CODEC_LOCAL_ERROR : XMQTT_CODEC_MALFORMED,Error==XMQTT_WIRE_LIMIT ? XMQTT_CODEC_LIMIT : XMQTT_CODEC_FORMAT,0); goto Failed;
	}
	Reader.Value.Type=Header.Type; Reader.Value.Flags=Header.Flags;
	if (!xmqtt311Role(Header.Type,Direction)) { xmqtt311ReadFail(&Reader,XMQTT_CODEC_PROTOCOL_ERROR,XMQTT_CODEC_PROTOCOL,0,"forbidden MQTT 3.1.1 sender direction"); goto Failed; }
	bool TwoBytes=Header.Type==XMQTT_PACKET_CONNACK || (Header.Type>=XMQTT_PACKET_PUBACK && Header.Type<=XMQTT_PACKET_PUBCOMP) || Header.Type==XMQTT_PACKET_UNSUBACK;
	bool Empty=Header.Type>=XMQTT_PACKET_PINGREQ;
	if ((TwoBytes && Header.RemainingLength!=2) || (Empty && Header.RemainingLength)) {
		xmqtt311ReadFail(&Reader,XMQTT_CODEC_MALFORMED,XMQTT_CODEC_FORMAT,0,"incorrect fixed MQTT 3.1.1 body length"); goto Failed;
	}
	if (Input.Size<Header.PacketBytes) return XMQTT_CODEC_MORE;
	Reader.Position=Header.HeaderBytes; Reader.End=Header.PacketBytes;
	if (!xmqtt311ReadBody(&Reader)) goto Failed;
	if (Reader.Position!=Reader.End) { xmqtt311ReadFail(&Reader,XMQTT_CODEC_MALFORMED,XMQTT_CODEC_FORMAT,Reader.Position,"unexpected trailing MQTT body bytes"); goto Failed; }
	Reader.Value.Wire=(xbytesview){Input.Data,Header.PacketBytes}; *Packet=Reader.Value;
	if (Issue!=NULL) *Issue=(xmqttcodecissue){XMQTT_CODEC_DONE,XMQTT_CODEC_OK,XRT_NPOS,0,0,0};
	return XMQTT_CODEC_DONE;
Failed:
	if (Issue!=NULL) *Issue=Reader.Failure;
	return Reader.Failure.Status;
}

typedef struct xmqtt311plan {
	const xmqttpacket* Packet;
	xmqttcodecconfig Config;
	size_t Body, Total;
	uint8 ConnectFlags;
} xmqtt311plan;

static size_t xmqtt311VbiSize(size_t Value) { return 1u+(Value>=128u)+(Value>=16384u)+(Value>=2097152u); }

static bool xmqtt311Add(xmqtt311plan* Plan, size_t Size)
{
	if (Size>XMQTT_VARIABLE_MAX-Plan->Body) return xmqtt311Fail(XERR_RANGE,XMQTT_CODEC_LIMIT,"MQTT body exceeds protocol length");
	Plan->Body+=Size;
	return Plan->Body+1u+xmqtt311VbiSize(Plan->Body)<=Plan->Config.MaxPacketBytes || xmqtt311Fail(XERR_RANGE,XMQTT_CODEC_LIMIT,"MQTT packet exceeds configured limit");
}

static bool xmqtt311Field(xmqtt311plan* Plan, const void* Data, size_t Size, bool Text)
{
	if (!xmqtt311Span(Data,Size)) return false;
	if (Size>XMQTT_STRING_MAX_BYTES) return xmqtt311Fail(XERR_RANGE,XMQTT_CODEC_LIMIT,"MQTT field exceeds two-byte length");
	return xmqtt311Add(Plan,Size+2u) && (!Text || xmqttUtf8StringCheck((xstrview){(cstr)Data,Size},Plan->Config.Utf8Flags,NULL));
}

static bool xmqtt311Prepare(const xmqttpacket* Packet, xmqttdirection Direction, const xmqttcodecconfig* Config, xmqtt311plan* Plan)
{
	if (Packet==NULL) return xmqtt311Fail(XERR_ARGUMENT,XMQTT_CODEC_ARGUMENT,"NULL MQTT packet descriptor");
	if (!xmqtt311Span(Packet,sizeof(*Packet)) || !xmqttInternalCodecSettings(Direction,Config,&Plan->Config)) return false;
	Plan->Packet=Packet; xmqttpackettype Type=Packet->Type;
	if (!xmqtt311Role(Type,Direction) || !xmqttInternalFlags(Type,Packet->Flags,XMQTT_VERSION_311) || Packet->Properties.Count)
		return xmqtt311Fail(XERR_ARGUMENT,XMQTT_CODEC_ARGUMENT,"invalid MQTT 3.1.1 type/direction/flags/properties");
	bool Identifier=(Type>=XMQTT_PACKET_PUBACK && Type<=XMQTT_PACKET_UNSUBACK) || (Type==XMQTT_PACKET_PUBLISH && (Packet->Flags&XMQTT_PUBLISH_QOS_MASK));
	if (Identifier!=(Packet->Identifier!=0) || !xmqttInternalReason(XMQTT_VERSION_311,Type,Packet->Reason,Direction) ||
		(Type==XMQTT_PACKET_SUBACK && Packet->Reason) || (Packet->SessionPresent && (Type!=XMQTT_PACKET_CONNACK || Packet->Reason)))
		return xmqtt311Fail(XERR_PROTOCOL,XMQTT_CODEC_PROTOCOL,"invalid MQTT 3.1.1 identifier/return code/session flag");
	if (Type==XMQTT_PACKET_CONNECT) {
		const xmqttconnectpacket* Value=&Packet->Body.Connect;
		if (Value->WillQos>2 || Value->WillProperties.Count || (!Value->HasWill && (Value->WillQos || Value->WillRetain || Value->WillTopic.Size || Value->WillPayload.Size)) ||
			(Value->HasPassword && !Value->HasUsername) || (!Value->HasUsername && Value->Username.Size) || (!Value->HasPassword && Value->Password.Size))
			return xmqtt311Fail(XERR_ARGUMENT,XMQTT_CODEC_ARGUMENT,"inconsistent MQTT 3.1.1 CONNECT flags/fields");
		if (!Value->ClientId.Size && !Value->CleanStart) return xmqtt311Fail(XERR_PROTOCOL,XMQTT_CODEC_CLIENT_ID,"empty ClientId requires Clean Session");
		Plan->ConnectFlags=(uint8)((Value->CleanStart ? 2u : 0u)|(Value->HasWill ? 4u : 0u)|(Value->WillQos<<3)|
			(Value->WillRetain ? 32u : 0u)|(Value->HasUsername ? 128u : 0u)|(Value->HasPassword ? 64u : 0u));
		if (!xmqtt311Add(Plan,10) || !xmqtt311Field(Plan,Value->ClientId.Data,Value->ClientId.Size,true)) return false;
		if (Value->HasWill) {
			if (Value->WillPayload.Size>Plan->Config.MaxPayloadBytes) return xmqtt311Fail(XERR_RANGE,XMQTT_CODEC_LIMIT,"Will payload exceeds configured limit");
			if (!xmqtt311Field(Plan,Value->WillTopic.Data,Value->WillTopic.Size,true)) return false;
			if (!xmqttInternalTopic(Value->WillTopic,false,false,NULL)) return xmqtt311Fail(XERR_PROTOCOL,XMQTT_CODEC_TOPIC,"invalid MQTT Will Topic");
			if (!xmqtt311Field(Plan,Value->WillPayload.Data,Value->WillPayload.Size,false)) return false;
		}
		if (Value->HasUsername && !xmqtt311Field(Plan,Value->Username.Data,Value->Username.Size,true)) return false;
		return !Value->HasPassword || xmqtt311Field(Plan,Value->Password.Data,Value->Password.Size,false);
	}
	if (Type==XMQTT_PACKET_PUBLISH) {
		const xmqttpublishpacket* Value=&Packet->Body.Publish;
		if (!xmqtt311Span(Value->Payload.Data,Value->Payload.Size)) return false;
		if (Value->Payload.Size>Plan->Config.MaxPayloadBytes) return xmqtt311Fail(XERR_RANGE,XMQTT_CODEC_LIMIT,"PUBLISH payload exceeds configured limit");
		if (!xmqtt311Field(Plan,Value->Topic.Data,Value->Topic.Size,true)) return false;
		if (!xmqttInternalTopic(Value->Topic,false,false,NULL)) return xmqtt311Fail(XERR_PROTOCOL,XMQTT_CODEC_TOPIC,"invalid MQTT PUBLISH Topic Name");
		return (!Packet->Identifier || xmqtt311Add(Plan,2)) && xmqtt311Add(Plan,Value->Payload.Size);
	}
	if (Type==XMQTT_PACKET_SUBSCRIBE || Type==XMQTT_PACKET_UNSUBSCRIBE) {
		xmqttsubscriptionlist Values=Packet->Body.Topics;
		if (!Values.Count) return xmqtt311Fail(XERR_PROTOCOL,XMQTT_CODEC_PROTOCOL,"MQTT topic request is empty");
		if (Values.Count>Plan->Config.MaxTopics) return xmqtt311Fail(XERR_RANGE,XMQTT_CODEC_LIMIT,"MQTT topic count exceeds limit");
		if (Values.Count>SIZE_MAX/sizeof(*Values.Data)) return xmqtt311Fail(XERR_ARGUMENT,XMQTT_CODEC_ARGUMENT,"MQTT topic vector size overflows");
		if (!xmqtt311Span(Values.Data,Values.Count*sizeof(*Values.Data)) || !xmqtt311Add(Plan,2)) return false;
		for (size_t Index=0; Index<Values.Count; ++Index) {
			const xmqttsubscription* Value=&Values.Data[Index];
			if (!xmqtt311Field(Plan,Value->Filter.Data,Value->Filter.Size,true)) return false;
			if (!xmqttInternalTopic(Value->Filter,true,false,NULL)) return xmqtt311Fail(XERR_PROTOCOL,XMQTT_CODEC_TOPIC,"invalid MQTT Topic Filter");
			if (Value->Options>(Type==XMQTT_PACKET_SUBSCRIBE ? 2u : 0u)) return xmqtt311Fail(XERR_PROTOCOL,XMQTT_CODEC_PROTOCOL,"invalid MQTT 3.1.1 subscription options");
			if (Type==XMQTT_PACKET_SUBSCRIBE && !xmqtt311Add(Plan,1)) return false;
		}
		return true;
	}
	if (Type==XMQTT_PACKET_SUBACK) {
		xbytesview Values=Packet->Body.Reasons;
		if (!Values.Size) return xmqtt311Fail(XERR_PROTOCOL,XMQTT_CODEC_PROTOCOL,"SUBACK return-code list is empty");
		if (Values.Size>Plan->Config.MaxTopics) return xmqtt311Fail(XERR_RANGE,XMQTT_CODEC_LIMIT,"SUBACK return-code count exceeds limit");
		if (!xmqtt311Span(Values.Data,Values.Size) || !xmqtt311Add(Plan,2) || !xmqtt311Add(Plan,Values.Size)) return false;
		for (size_t Index=0; Index<Values.Size; ++Index)
			if (!xmqttInternalReason(XMQTT_VERSION_311,Type,Values.Data[Index],Direction)) return xmqtt311Fail(XERR_PROTOCOL,XMQTT_CODEC_PROTOCOL,"invalid MQTT 3.1.1 SUBACK return code");
		return true;
	}
	if (Type==XMQTT_PACKET_UNSUBACK && Packet->Body.Reasons.Size) return xmqtt311Fail(XERR_ARGUMENT,XMQTT_CODEC_ARGUMENT,"MQTT 3.1.1 UNSUBACK has no reason list");
	return Type>=XMQTT_PACKET_PINGREQ || xmqtt311Add(Plan,2);
}

static bool xmqtt311Disjoint(const void* Output, size_t Size, const void* Data, size_t Bytes)
{
	return !xrtMemRangesOverlap(Output,Size,Data,Bytes) || xmqtt311Fail(XERR_ARGUMENT,XMQTT_CODEC_ARGUMENT,"MQTT output overlaps active source data");
}

static bool xmqtt311Output(const xmqtt311plan* Plan, const void* Output, size_t Size, const xmqttcodecconfig* Config)
{
	if (Output==NULL) return xmqtt311Fail(XERR_ARGUMENT,XMQTT_CODEC_ARGUMENT,"NULL MQTT output");
	const xmqttpacket* Packet=Plan->Packet;
	if (!xmqtt311Span(Output,Size) || !xmqtt311Disjoint(Output,Size,Packet,sizeof(*Packet)) || (Config!=NULL && !xmqtt311Disjoint(Output,Size,Config,sizeof(*Config)))) return false;
	if (Packet->Type==XMQTT_PACKET_CONNECT) {
		const xmqttconnectpacket* Value=&Packet->Body.Connect;
		if (!xmqtt311Disjoint(Output,Size,Value->ClientId.Data,Value->ClientId.Size) || (Value->HasWill &&
			(!xmqtt311Disjoint(Output,Size,Value->WillTopic.Data,Value->WillTopic.Size) || !xmqtt311Disjoint(Output,Size,Value->WillPayload.Data,Value->WillPayload.Size))) ||
			(Value->HasUsername && !xmqtt311Disjoint(Output,Size,Value->Username.Data,Value->Username.Size)) ||
			(Value->HasPassword && !xmqtt311Disjoint(Output,Size,Value->Password.Data,Value->Password.Size))) return false;
	} else if (Packet->Type==XMQTT_PACKET_PUBLISH) {
		if (!xmqtt311Disjoint(Output,Size,Packet->Body.Publish.Topic.Data,Packet->Body.Publish.Topic.Size) ||
			!xmqtt311Disjoint(Output,Size,Packet->Body.Publish.Payload.Data,Packet->Body.Publish.Payload.Size)) return false;
	} else if (Packet->Type==XMQTT_PACKET_SUBSCRIBE || Packet->Type==XMQTT_PACKET_UNSUBSCRIBE) {
		xmqttsubscriptionlist Values=Packet->Body.Topics;
		if (!xmqtt311Disjoint(Output,Size,Values.Data,Values.Count*sizeof(*Values.Data))) return false;
		for (size_t Index=0; Index<Values.Count; ++Index) if (!xmqtt311Disjoint(Output,Size,Values.Data[Index].Filter.Data,Values.Data[Index].Filter.Size)) return false;
	} else if (Packet->Type==XMQTT_PACKET_SUBACK && !xmqtt311Disjoint(Output,Size,Packet->Body.Reasons.Data,Packet->Body.Reasons.Size)) return false;
	return true;
}

static unsigned char* xmqtt311EmitWord(unsigned char* Output, uint16 Value)
{
	*Output++=(unsigned char)(Value>>8); *Output++=(unsigned char)Value; return Output;
}

static unsigned char* xmqtt311EmitData(unsigned char* Output, const void* Data, size_t Size)
{
	if (Size) memcpy(Output,Data,Size);
	return Output+Size;
}

static unsigned char* xmqtt311EmitField(unsigned char* Output, const void* Data, size_t Size)
{
	return xmqtt311EmitData(xmqtt311EmitWord(Output,(uint16)Size),Data,Size);
}

static void xmqtt311Emit(const xmqtt311plan* Plan, ptr Output)
{
	const xmqttpacket* Packet=Plan->Packet; xmqttpackettype Type=Packet->Type; unsigned char* Cursor=(unsigned char*)Output; size_t Bytes;
	*Cursor++=(uint8)((Type<<4)|Packet->Flags); (void)xmqttVbiWrite(Cursor,Plan->Total-1u,(uint32)Plan->Body,&Bytes); Cursor+=Bytes;
	if (Type==XMQTT_PACKET_CONNECT) {
		const xmqttconnectpacket* Value=&Packet->Body.Connect;
		Cursor=xmqtt311EmitData(Cursor,"\x00\x04MQTT\x04",7); *Cursor++=Plan->ConnectFlags; Cursor=xmqtt311EmitWord(Cursor,Value->KeepAlive);
		Cursor=xmqtt311EmitField(Cursor,Value->ClientId.Data,Value->ClientId.Size);
		if (Value->HasWill) { Cursor=xmqtt311EmitField(Cursor,Value->WillTopic.Data,Value->WillTopic.Size); Cursor=xmqtt311EmitField(Cursor,Value->WillPayload.Data,Value->WillPayload.Size); }
		if (Value->HasUsername) Cursor=xmqtt311EmitField(Cursor,Value->Username.Data,Value->Username.Size);
		if (Value->HasPassword) Cursor=xmqtt311EmitField(Cursor,Value->Password.Data,Value->Password.Size);
	} else if (Type==XMQTT_PACKET_PUBLISH) {
		Cursor=xmqtt311EmitField(Cursor,Packet->Body.Publish.Topic.Data,Packet->Body.Publish.Topic.Size);
		if (Packet->Identifier) Cursor=xmqtt311EmitWord(Cursor,Packet->Identifier);
		(void)xmqtt311EmitData(Cursor,Packet->Body.Publish.Payload.Data,Packet->Body.Publish.Payload.Size);
	} else if (Type==XMQTT_PACKET_SUBSCRIBE || Type==XMQTT_PACKET_UNSUBSCRIBE) {
		Cursor=xmqtt311EmitWord(Cursor,Packet->Identifier);
		for (size_t Index=0; Index<Packet->Body.Topics.Count; ++Index) {
			const xmqttsubscription* Value=&Packet->Body.Topics.Data[Index]; Cursor=xmqtt311EmitField(Cursor,Value->Filter.Data,Value->Filter.Size);
			if (Type==XMQTT_PACKET_SUBSCRIBE) *Cursor++=Value->Options;
		}
	} else if (Type==XMQTT_PACKET_CONNACK) { *Cursor++=Packet->SessionPresent ? 1u : 0u; *Cursor=Packet->Reason; }
	else if (Type<XMQTT_PACKET_PINGREQ) {
		Cursor=xmqtt311EmitWord(Cursor,Packet->Identifier);
		if (Type==XMQTT_PACKET_SUBACK) (void)xmqtt311EmitData(Cursor,Packet->Body.Reasons.Data,Packet->Body.Reasons.Size);
	}
}

XMQTT_API bool xmqttPacketSize311(const xmqttpacket* Packet, xmqttdirection Direction, const xmqttcodecconfig* Config, size_t* Required)
{
	xmqtt311plan Plan={0};
	if (!xmqtt311Prepare(Packet,Direction,Config,&Plan) || !xmqtt311Output(&Plan,Required,sizeof(*Required),Config)) return false;
	*Required=Plan.Body+1u+xmqtt311VbiSize(Plan.Body); return true;
}

XMQTT_API bool xmqttPacketWrite311(ptr Output, size_t Capacity, const xmqttpacket* Packet, xmqttdirection Direction, const xmqttcodecconfig* Config, size_t* Written)
{
	xmqtt311plan Plan={0};
	if (Output==NULL) return xmqtt311Fail(XERR_ARGUMENT,XMQTT_CODEC_ARGUMENT,"NULL MQTT wire output");
	if (!xmqtt311Span(Output,Capacity) || !xmqtt311Prepare(Packet,Direction,Config,&Plan)) return false;
	Plan.Total=Plan.Body+1u+xmqtt311VbiSize(Plan.Body);
	if (Capacity<Plan.Total) return xmqtt311Fail(XERR_RANGE,XMQTT_CODEC_CAPACITY,"MQTT wire capacity is too small");
	if (!xmqtt311Output(&Plan,Output,Plan.Total,Config) || (Written!=NULL &&
		(!xmqtt311Output(&Plan,Written,sizeof(*Written),Config) || !xmqtt311Disjoint(Written,sizeof(*Written),Output,Plan.Total)))) return false;
	xmqtt311Emit(&Plan,Output); if (Written!=NULL) *Written=Plan.Total; return true;
}
#endif
