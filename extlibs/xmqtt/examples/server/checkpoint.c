#include <xmqtt/features.h>
#if defined(XMQTT_FEATURE_XMQTT_SERVER_AUTH) && defined(XRT_FEATURE_CRYPTO_HMAC_SHA256)
#define XMQTT_EXAMPLE_SERVER_AUTH
#include "app_auth.h"
#endif
#if defined(XMQTT_FEATURE_XMQTT_SERVER_WS_TLS) && defined(XMQTT_FEATURE_XMQTT_SERVER_TLS) && \
	defined(XRT_FEATURE_HTTP1_TLS) && defined(XRT_FEATURE_WEBSOCKET_UPGRADE_STREAM) && defined(XRT_FEATURE_TLS_IDENTITY_RSA)
#define XMQTT_EXAMPLE_SERVER_WS
#include "app_tls.h"
#include "app_ws.h"
#else
#include "app_listener.h"
#endif
#include <xmqtt/server_retained_commit.h>
#include <xrt/buffer.h>
#include <xrt/crc32.h>
#include <xrt/file.h>
#include <xrt/memory.h>
#include <xrt/sync.h>
#include <xrt/task.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

/* Application-owned binary fixture for a single loopback broker. */
enum { CheckpointHeader = 20, CheckpointRecord = 15, SessionRecord = 22, CheckpointChecksum = 4, RetainedTopics = 4096, SessionConnected = 1 };
static const unsigned char CheckpointMagic[8] = {'X','M','Q','R','E','T','0','5'};
static const size_t RetainedBytes = 16u * 1024u * 1024u;
static const size_t CheckpointLimit = 192u * 1024u * 1024u; /* Aggregate default protocol quotas. */

static void checkpointWriteNumber(bytes Output, size_t Width, uint64 Number)
{
	while (Width) {
		Output[--Width] = (unsigned char)Number;
		Number >>= 8;
	}
}

static uint64 checkpointReadNumber(cbytes Input, size_t Width)
{
	uint64 Value = 0;
	for (size_t Index = 0; Index < Width; ++Index) Value = Value * 256 + Input[Index];
	return Value;
}

typedef struct checkpointoutput {
	xbuffer Image;
	size_t Count;
	uint64 Offset; /* Record time relative to this file's capture time. */
	bool TimedWills;
} checkpointoutput;

static bool checkpointRecord(const xmqttserverretainedrecord* Record, ptr Data)
{
	checkpointoutput* Output = Data;
	unsigned char Header[CheckpointRecord + 8];
	Header[0] = (unsigned char)Record->Version;
	checkpointWriteNumber(Header + 1, 2, Record->PublisherId.Size);
	checkpointWriteNumber(Header + 3, 4, Record->Publish.Size);
	checkpointWriteNumber(Header + 7, 8, Record->RemainingMilliseconds);
	checkpointWriteNumber(Header + CheckpointRecord, 8, Output->Offset);
	if (!xrtBufferAppend(&Output->Image, (xbytesview){Header, sizeof(Header)}) ||
		!xrtBufferAppend(&Output->Image, (xbytesview){(cbytes)Record->PublisherId.Data, Record->PublisherId.Size}) ||
		!xrtBufferAppend(&Output->Image, Record->Publish)) return false;
	++Output->Count;
	return true;
}

static bool checkpointDelivery(xmqttserverrecordkind Kind)
{
	return Kind == XMQTT_SERVER_RECORD_QUEUED || Kind == XMQTT_SERVER_RECORD_SENT;
}

static bool checkpointSession(const xmqttserversessionrecord* Record, ptr Data)
{
	checkpointoutput* Output = Data;
	unsigned char Header[SessionRecord + 24];
	size_t HeaderBytes = SessionRecord;
	if (Record->Kind == XMQTT_SERVER_RECORD_SESSION) {
		checkpointWriteNumber(Header + SessionRecord, 8, Record->Key.High);
		checkpointWriteNumber(Header + SessionRecord + 8, 8, Record->Key.Low);
		HeaderBytes += 16;
	}
	if (Record->Kind == XMQTT_SERVER_RECORD_SESSION || Record->Kind == XMQTT_SERVER_RECORD_QUEUED ||
		(Output->TimedWills && Record->Kind == XMQTT_SERVER_RECORD_WILL)) {
		checkpointWriteNumber(Header + HeaderBytes, 8, Output->Offset);
		HeaderBytes += 8;
	}
	if (checkpointDelivery(Record->Kind)) {
		checkpointWriteNumber(Header + HeaderBytes, 8, Record->DeliveryKey.High);
		checkpointWriteNumber(Header + HeaderBytes + 8, 8, Record->DeliveryKey.Low);
		HeaderBytes += 16;
	}
	Header[0] = (uint8)Record->Kind;
	Header[1] = Record->Code;
	checkpointWriteNumber(Header + 2, 2, Record->Identifier);
	checkpointWriteNumber(Header + 4, 4, Record->Value);
	checkpointWriteNumber(Header + 8, 8, Record->Milliseconds);
	checkpointWriteNumber(Header + 16, 4, Record->Data.Size);
	checkpointWriteNumber(Header + 20, 2, Record->Group.Size);
	if (!xrtBufferAppend(&Output->Image, (xbytesview){Header, HeaderBytes}) ||
		!xrtBufferAppend(&Output->Image, Record->Data) || !xrtBufferAppend(&Output->Image, Record->Group)) return false;
	++Output->Count;
	return true;
}

static bool checkpointSave(xmqttserver* Server, cstr Path)
{
	int64 SavedAt;
	if (!xrtTimeToUnixMs(xrtNow(), &SavedAt) || SavedAt < 0) return false;
	checkpointoutput Output = {0};
	bytes Header = xrtBufferAdd(&Output.Image, CheckpointHeader);
	if (!Header) return false;
	memcpy(Header, CheckpointMagic, sizeof(CheckpointMagic));
	checkpointWriteNumber(Header + 8, 8, (uint64)SavedAt);
	checkpointWriteNumber(Header + 16, 4, 0);
	bool Written = xmqttServerVisitRetained(Server, checkpointRecord, &Output);
	if (Written) {
		checkpointWriteNumber(Output.Image.Data + 16, 4, Output.Count);
		size_t Offset = Output.Image.Size;
		Output.Count = 0;
		Written = xrtBufferAdd(&Output.Image, 4) && xmqttServerVisitSessions(Server, checkpointSession, &Output);
		if (Written) checkpointWriteNumber(Output.Image.Data + Offset, 4, Output.Count);
	}
	if (Written) {
		unsigned char Checksum[CheckpointChecksum];
		checkpointWriteNumber(Checksum, sizeof(Checksum), xrtCrc32(Output.Image.Data, Output.Image.Size));
		Written = xrtBufferAppend(&Output.Image, (xbytesview){Checksum, sizeof(Checksum)}) &&
			xrtFileWriteAtomicRename(Path, xrtBufferView(&Output.Image));
	}
	xrtBufferUnit(&Output.Image);
	return Written;
}

static bool checkpointReadRetained(xbytesview* Cursor, bool Timed, xmqttserverretainedrecord* Record, uint64* Offset)
{
	size_t HeaderBytes = CheckpointRecord + (Timed ? 8u : 0u);
	if (Cursor->Size < HeaderBytes) return false;
	cbytes Header = Cursor->Data;
	size_t Id = (size_t)checkpointReadNumber(Header + 1, 2);
	size_t Wire = (size_t)checkpointReadNumber(Header + 3, 4);
	Cursor->Data += HeaderBytes;
	Cursor->Size -= HeaderBytes;
	if (Id > Cursor->Size || Wire > Cursor->Size - Id) return false;
	*Record = (xmqttserverretainedrecord){
		(xmqttversion)Header[0], {(cstr)Cursor->Data, Id}, {Cursor->Data + Id, Wire},
		checkpointReadNumber(Header + 7, 8)
	};
	*Offset = Timed ? checkpointReadNumber(Header + CheckpointRecord, 8) : 0;
	Cursor->Data += Id + Wire;
	Cursor->Size -= Id + Wire;
	return true;
}

static bool checkpointParseRetained(xbytesview* Cursor, bool Timed, xmqttserverretainedrecord* Records,
	size_t* Count, uint64 Elapsed)
{
	size_t Live = 0;
	for (size_t At = 0; At < *Count; ++At) {
		xmqttserverretainedrecord Record;
		uint64 Offset;
		if (!checkpointReadRetained(Cursor, Timed, &Record, &Offset) || Offset > Elapsed) return false;
		if (Timed && Record.RemainingMilliseconds) {
			uint64 Age = Elapsed - Offset;
			if (Record.RemainingMilliseconds <= Age) continue;
			Record.RemainingMilliseconds -= Age;
		}
		if (Records) Records[Live++] = Record;
	}
	*Count = Live;
	return true;
}

static bool checkpointReadSession(xbytesview* Input, uint8 Format, xmqttserversessionrecord* Record, uint64* Offset)
{
	if (Input->Size < SessionRecord) return false;
	cbytes Header = Input->Data;
	size_t Data = (size_t)checkpointReadNumber(Header + 16, 4);
	size_t Group = (size_t)checkpointReadNumber(Header + 20, 2);
	bool Session = Header[0] == XMQTT_SERVER_RECORD_SESSION;
	bool Timed = (Format >= '3' && (Session || Header[0] == XMQTT_SERVER_RECORD_QUEUED)) ||
		(Format >= '7' && Header[0] == XMQTT_SERVER_RECORD_WILL);
	bool KeyedDelivery = Format >= '5' && checkpointDelivery((xmqttserverrecordkind)Header[0]);
	size_t TimeAt = SessionRecord + (Format >= '3' && Session ? 16u : 0u);
	size_t DeliveryAt = TimeAt + (Timed ? 8u : 0u);
	size_t HeaderBytes = DeliveryAt + (KeyedDelivery ? 16u : 0u);
	if (HeaderBytes > Input->Size) return false;
	if (Offset) *Offset = Timed ? checkpointReadNumber(Header + TimeAt, 8) : 0;
	Input->Data += HeaderBytes;
	Input->Size -= HeaderBytes;
	if (Data > Input->Size || Group > Input->Size - Data) return false;
	*Record = (xmqttserversessionrecord){
		.Kind = (xmqttserverrecordkind)Header[0], .Code = Header[1],
		.Identifier = (uint16)checkpointReadNumber(Header + 2, 2),
		.Value = (uint32)checkpointReadNumber(Header + 4, 4),
		.Milliseconds = checkpointReadNumber(Header + 8, 8),
		.Data = {Input->Data, Data}, .Group = {Input->Data + Data, Group}
	};
	if (Format >= '3' && Session)
		Record->Key = (xmqttserversessionkey){checkpointReadNumber(Header + SessionRecord, 8),
			checkpointReadNumber(Header + SessionRecord + 8, 8)};
	if (KeyedDelivery)
		Record->DeliveryKey = (xmqttserversessionkey){checkpointReadNumber(Header + DeliveryAt, 8),
			checkpointReadNumber(Header + DeliveryAt + 8, 8)};
	Input->Data += Data + Group;
	Input->Size -= Data + Group;
	return true;
}

static bool checkpointParseSessions(xbytesview Input, uint8 Format, xmqttserversessionrecord* Records,
	size_t Count, uint64 Elapsed, bool* Rebased)
{
	bool Connected = false;
	for (size_t Index = 0; Index < Count; ++Index) {
		xmqttserversessionrecord* Record = Records + Index;
		uint64 Offset;
		cbytes Header = Input.Data;
		if (!checkpointReadSession(&Input, Format, Record, &Offset) || Offset > Elapsed) return false;
		if (Record->Kind == XMQTT_SERVER_RECORD_SESSION)
			Connected = Format >= '6' && Record->Value == SessionConnected;
		if (Connected && Record->Kind == XMQTT_SERVER_RECORD_SESSION) {
			/* A live record has no observed network-close time. Start the
			 * outage clock once at recovery, before accepting connections. */
			Record->Value = 0;
			Offset = Elapsed;
			checkpointWriteNumber((bytes)Header + 4, 4, 0);
			checkpointWriteNumber((bytes)Header + SessionRecord + 16, 8, Offset);
			*Rebased = true;
		} else if (Connected && Format >= '7' && Record->Kind == XMQTT_SERVER_RECORD_WILL) {
			Offset = Elapsed;
			checkpointWriteNumber((bytes)Header + SessionRecord, 8, Offset);
		}
		if (Format >= '3' && Record->Milliseconds != UINT64_MAX &&
			(Record->Kind == XMQTT_SERVER_RECORD_SESSION || Record->Kind == XMQTT_SERVER_RECORD_QUEUED ||
			 Record->Kind == XMQTT_SERVER_RECORD_WILL)) {
			if (Record->Milliseconds > UINT64_C(1000) * UINT32_MAX) return false;
			uint64 Age = Elapsed - Offset;
			Record->Milliseconds = Record->Milliseconds > Age ? Record->Milliseconds - Age : 0;
		}
	}
	return Input.Size == 0;
}

static bool checkpointRestoreSessions(xmqttserver* Server, xbytesview Input, uint8 Format, uint64 Elapsed,
	bool* Rebased)
{
	if (Input.Size < 4) return false;
	size_t Count = (size_t)checkpointReadNumber(Input.Data, 4);
	Input.Data += 4;
	Input.Size -= 4;
	/* Bound the temporary descriptor table by the sample broker's default quotas. */
	if (Count > Input.Size / SessionRecord || Count > 2u * 1024u + 65536u + 65536u + 16384u) return false;
	xmqttserversessionrecord* Records = Count ? xrtCalloc(Count, sizeof(*Records)) : NULL;
	bool Restored = (!Count || Records) && checkpointParseSessions(Input, Format, Records, Count, Elapsed, Rebased) &&
		xmqttServerRestoreSessions(Server, Records, Count, Format >= '3' ? 0 : Elapsed);
	xrtFree(Records);
	return Restored;
}

static bytes checkpointOpen(cstr Path, xbytesview* Cursor, size_t* Count)
{
	size_t Bytes;
	bytes Image = xrtFileReadAllLimit(Path, CheckpointLimit, &Bytes);
	if (!Image) return NULL;
	if (Bytes >= CheckpointHeader + CheckpointChecksum && !memcmp(Image, CheckpointMagic, 7) &&
		(Image[7] >= '1' && Image[7] <= '7')) {
		size_t Content = Bytes - CheckpointChecksum;
		*Count = (size_t)checkpointReadNumber(Image + 16, 4);
		if (*Count <= RetainedTopics && xrtCrc32(Image, Content) == checkpointReadNumber(Image + Content, CheckpointChecksum)) {
			*Cursor = (xbytesview){Image + CheckpointHeader, Content - CheckpointHeader};
			return Image;
		}
	}
	xrtFree(Image);
	return NULL;
}

static bool checkpointLoad(xmqttserver* Server, cstr Path, bool Online)
{
	xbytesview Cursor;
	size_t Count;
	bytes Image = checkpointOpen(Path, &Cursor, &Count);
	if (!Image) return false;
	if (Online && Image[7] < '3') {
		xrtFree(Image);
		return false;
	}
	size_t Content = CheckpointHeader + Cursor.Size;
	bool Rebased = false;
	uint64 SavedAt = checkpointReadNumber(Image + 8, 8);
	xmqttserverretainedrecord* Records = Count ? xrtCalloc(Count, sizeof(*Records)) : NULL;
	int64 Now;
	bool Clock = xrtTimeToUnixMs(xrtNow(), &Now) && Now >= 0 && (uint64)Now >= SavedAt;
	uint64 Elapsed = Clock ? (uint64)Now - SavedAt : 0;
	bool Timed = Image[7] >= '4';
	bool Restored = Clock && (!Count || Records) &&
		checkpointParseRetained(&Cursor, Timed, Records, &Count, Elapsed) &&
		xmqttServerRestoreRetained(Server, Records, Count, Timed ? 0 : Elapsed);
	if (Restored)
		Restored = Image[7] == '1' ? Cursor.Size == 0 :
			checkpointRestoreSessions(Server, Cursor, Image[7], Elapsed, &Rebased);
	if (Restored && Rebased) {
		checkpointWriteNumber(Image + Content, CheckpointChecksum, xrtCrc32(Image, Content));
		Restored = xrtFileWriteAtomicRename(Path, (xbytesview){Image, Content + CheckpointChecksum});
	}
	xrtFree(Records);
	xrtFree(Image);
	return Restored;
}

typedef struct checkpointstore {
	cstr Path;
	xtaskpool* Writer;
	xmutex* WriterGate;
	int64 Delay;
	bool Online, Expiry, Wills, WriterReady;
} checkpointstore;

typedef struct checkpointchange {
	checkpointstore* Store;
	xmqttserversessionticket* Ticket;
	xmqttservertransaction Change;
	xmqttserverretainedticket* RetainedTicket;
} checkpointchange;

static xmqttserversessionresult checkpointApply(const checkpointchange* Change, bytes Image,
	xbytesview Cursor, size_t Count)
{
	if (!checkpointParseRetained(&Cursor, Image[7] >= '4', NULL, &Count, UINT64_MAX))
		return XMQTT_SERVER_SESSION_REJECTED;
	if (Image[7] == '1')
		return Cursor.Size ? XMQTT_SERVER_SESSION_REJECTED : XMQTT_SERVER_SESSION_COMMITTED;
	if (Cursor.Size < 4) return XMQTT_SERVER_SESSION_REJECTED;
	bytes CountField = Image + (Cursor.Data - Image);
	Count = (size_t)checkpointReadNumber(CountField, 4);
	Cursor.Data += 4;
	Cursor.Size -= 4;
	if (Count > Cursor.Size / SessionRecord) return XMQTT_SERVER_SESSION_REJECTED;
	bytes Output = CountField + 4;
	size_t Kept = 0;
	bool Selected = false;
	for (size_t Index = 0; Index < Count; ++Index) {
		cbytes Start = Cursor.Data;
		xmqttserversessionrecord Record;
		if (!checkpointReadSession(&Cursor, Image[7], &Record, NULL) ||
			(!Index && Record.Kind != XMQTT_SERVER_RECORD_SESSION))
			return XMQTT_SERVER_SESSION_REJECTED;
		if (Record.Kind == XMQTT_SERVER_RECORD_SESSION)
			Selected = Record.Data.Size == Change->Change.ClientId.Size &&
				!memcmp(Record.Data.Data, Change->Change.ClientId.Data, Record.Data.Size);
		if (Selected && (Change->Change.Operation == XMQTT_SERVER_SESSION_DISCARD ||
			(Record.Kind == XMQTT_SERVER_RECORD_INCOMING_QOS2 && Record.Identifier == Change->Change.Identifier)))
			continue;
		size_t Size = (size_t)(Cursor.Data - Start);
		memmove(Output, Start, Size);
		Output += Size;
		++Kept;
	}
	if (Cursor.Size) return XMQTT_SERVER_SESSION_REJECTED;
	if (Kept == Count) return XMQTT_SERVER_SESSION_COMMITTED;
	checkpointWriteNumber(CountField, 4, Kept);
	size_t Size = (size_t)(Output - Image);
	checkpointWriteNumber(Output, CheckpointChecksum, xrtCrc32(Image, Size));
	return xrtFileWriteAtomicRename(Change->Store->Path, (xbytesview){Image, Size + CheckpointChecksum}) ?
		XMQTT_SERVER_SESSION_COMMITTED : XMQTT_SERVER_SESSION_UNKNOWN;
}

static bool checkpointIdEqual(xstrview Id, xbytesview Key)
{
	return Id.Size == Key.Size && !memcmp(Id.Data, Key.Data, Key.Size);
}

static bool checkpointHasKey(xmqttserversessionkey Key)
{
	return Key.High || Key.Low;
}

static bool checkpointKeyEqual(xmqttserversessionkey First, xmqttserversessionkey Second)
{
	return First.High == Second.High && First.Low == Second.Low;
}

static bool checkpointCollectRecipient(const xmqttservertransactionrecipient* Recipient, ptr User)
{
	return xrtBufferAppend(User, (xbytesview){(cbytes)Recipient, sizeof(*Recipient)});
}

static bool checkpointAcceptsPublication(xmqttserversessionoperation Operation)
{
	return Operation == XMQTT_SERVER_SESSION_ACCEPT_INCOMING_QOS1 ||
		Operation == XMQTT_SERVER_SESSION_ACCEPT_INCOMING_QOS2 ||
		Operation == XMQTT_SERVER_SESSION_PUBLISH_WILL;
}

static bool checkpointEditsSubscriptions(xmqttserversessionoperation Operation)
{
	return Operation == XMQTT_SERVER_SESSION_UPSERT_SUBSCRIPTIONS ||
		Operation == XMQTT_SERVER_SESSION_REMOVE_SUBSCRIPTIONS;
}

static bool checkpointCollectSubscription(const xmqttserversessionrecord* Record, ptr User)
{
	return xrtBufferAppend(User, (xbytesview){(cbytes)Record, sizeof(*Record)});
}

static bool checkpointSameBytes(xbytesview A, xbytesview B)
{
	return A.Size == B.Size && (!A.Size || !memcmp(A.Data, B.Data, A.Size));
}

static bool checkpointHasSubscription(const xbuffer* Targets, const xmqttserversessionrecord* Record)
{
	const xmqttserversessionrecord* Items = (const void*)Targets->Data;
	for (size_t At = 0; At < Targets->Size / sizeof(*Items); ++At)
		if (checkpointSameBytes(Record->Data, Items[At].Data) &&
			checkpointSameBytes(Record->Group, Items[At].Group)) return true;
	return false;
}

static bool checkpointArmWill(checkpointoutput* Output, const xmqttservertransaction* Change)
{
	if (!Change->WillPublish.Size) return true;
	const xmqttserversessionrecord Record = {.Kind = XMQTT_SERVER_RECORD_WILL,
		.Data = Change->WillPublish, .Milliseconds = UINT64_C(1000) * Change->WillDelay};
	return checkpointSession(&Record, Output);
}

static bool checkpointAppendGroup(checkpointoutput* Output, const xmqttserversessionrecord* Group,
	const xmqttservertransaction* Change, const xbuffer* Targets, bool* Found)
{
	if (!Group->Data.Size) return true;
	if (Change->Operation == XMQTT_SERVER_SESSION_RESUME && checkpointIdEqual(Change->ClientId, Group->Data))
		return checkpointArmWill(Output, Change);
	if (checkpointEditsSubscriptions(Change->Operation)) {
		if (!checkpointIdEqual(Change->ClientId, Group->Data)) return true;
		if (!checkpointKeyEqual(Change->Key, Group->Key)) return false;
		if (Change->Operation == XMQTT_SERVER_SESSION_UPSERT_SUBSCRIPTIONS) {
			const xmqttserversessionrecord* Items = (const void*)Targets->Data;
			for (size_t At = 0; At < Targets->Size / sizeof(*Items); ++At)
				if (!checkpointSession(Items + At, Output)) return false;
		}
		*Found = true;
		return true;
	}
	if (!checkpointAcceptsPublication(Change->Operation)) return true;
	if (checkpointIdEqual(Change->ClientId, Group->Data) && checkpointHasKey(Change->Key)) {
		if (!checkpointKeyEqual(Change->Key, Group->Key)) return false;
		if (Change->Operation == XMQTT_SERVER_SESSION_ACCEPT_INCOMING_QOS2) {
			const xmqttserversessionrecord Record = {.Kind = XMQTT_SERVER_RECORD_INCOMING_QOS2,
				.Identifier = Change->Identifier, .Code = Change->Reason};
			if (!checkpointSession(&Record, Output)) return false;
		}
		*Found = true;
	}
	const xmqttservertransactionrecipient* Items = (const void*)Targets->Data;
	for (size_t At = 0; At < Targets->Size / sizeof(*Items); ++At) {
		const xmqttservertransactionrecipient* Recipient = Items + At;
		if (!checkpointIdEqual(Recipient->ClientId, Group->Data) || !checkpointKeyEqual(Recipient->Key, Group->Key))
			continue;
		const xmqttserversessionrecord Record = {.Kind = XMQTT_SERVER_RECORD_QUEUED, .Data = Recipient->Publish,
			.Milliseconds = Recipient->RemainingMilliseconds, .DeliveryKey = Recipient->DeliveryKey};
		if (!checkpointSession(&Record, Output)) return false;
	}
	return true;
}

static uint64 checkpointExpiryMilliseconds(uint32 Interval)
{
	return Interval == UINT32_MAX ? UINT64_MAX : UINT64_C(1000) * Interval;
}

static bool checkpointWriteTransaction(const checkpointchange* Request, xbytesview Cursor,
	size_t Count, uint8 Format, const xbuffer* Targets, uint64 OfflineMs, uint64 WillMs, checkpointoutput* Output)
{
	const xmqttservertransaction* Change = &Request->Change;
	uint64 Elapsed = Output->Offset;
	xmqttserversessionrecord Group = {0}, FirstSend = {0};
	size_t GroupPosition = 0;
	bool Matching = false, MatchedGroup = false, Found = !checkpointHasKey(Change->Key);
	bool Closing = Change->Operation == XMQTT_SERVER_SESSION_CLOSE;
	bool Resuming = Change->Operation == XMQTT_SERVER_SESSION_RESUME;
	bool PublishingWill = Change->Operation == XMQTT_SERVER_SESSION_PUBLISH_WILL;
	bool CancellingWill = Change->Operation == XMQTT_SERVER_SESSION_CANCEL_WILL;
	bool KeepWill = Closing && Change->WillPublish.Size != 0;
	bool FoundWill = false;
	bool Replacing = Change->Operation == XMQTT_SERVER_SESSION_DISCARD ||
		Change->Operation == XMQTT_SERVER_SESSION_START;
	for (size_t At = 0; At < Count; ++At) {
		xmqttserversessionrecord Record;
		uint64 Offset;
		if (!checkpointReadSession(&Cursor, Format, &Record, &Offset) || Offset > Elapsed ||
			(!At && Record.Kind != XMQTT_SERVER_RECORD_SESSION)) return false;
		if (Record.Kind == XMQTT_SERVER_RECORD_SESSION) {
			if (FirstSend.Kind && !checkpointSession(&FirstSend, Output)) return false;
			FirstSend.Kind = 0;
			if (!checkpointAppendGroup(Output, &Group, Change, Targets, &Found)) return false;
			Group = Record;
			GroupPosition = Output->Image.Size;
			Matching = checkpointIdEqual(Change->ClientId, Group.Data);
			if (Matching && checkpointKeyEqual(Change->Key, Group.Key)) MatchedGroup = true;
			if (Matching && (Closing || Resuming || CancellingWill)) {
				if (!checkpointKeyEqual(Change->Key, Group.Key)) return false;
				Record.Value = Closing ? 0 : SessionConnected;
				Record.Milliseconds = Closing ? OfflineMs : checkpointExpiryMilliseconds(Change->ExpiryInterval);
				Offset = Elapsed;
			}
		}
		if (Matching && PublishingWill) {
			if (!checkpointKeyEqual(Change->Key, Group.Key)) return false;
			if (Record.Kind == XMQTT_SERVER_RECORD_WILL) {
				if (FoundWill) return false;
				FoundWill = true;
				continue;
			}
			if (Change->EndSession) continue;
		}
		if (Matching && Record.Kind == XMQTT_SERVER_RECORD_WILL) {
			if (Resuming && Request->Store->Wills) continue;
			if (CancellingWill || (Closing && Request->Store->Wills)) {
				if (FoundWill) return false;
				FoundWill = true;
				if (!KeepWill) continue;
				Record.Milliseconds = WillMs;
				Offset = Elapsed;
			}
		}
		/* A resume reserved before expiry may be queued behind this write.
		 * Preserve its key if time elapsed in the writer. A zero lifetime
		 * at the close event ends the session and removes all state. */
		if (Matching && (Replacing || (Closing && !Change->OfflineLifetimeMs &&
			(!KeepWill || (Record.Kind != XMQTT_SERVER_RECORD_SESSION && Record.Kind != XMQTT_SERVER_RECORD_WILL)))))
			continue;
		if (Matching && Record.Kind == XMQTT_SERVER_RECORD_SUBSCRIPTION &&
			checkpointEditsSubscriptions(Change->Operation) && checkpointHasSubscription(Targets, &Record)) continue;
		if (Matching && checkpointKeyEqual(Change->Key, Group.Key) &&
			Record.Kind == XMQTT_SERVER_RECORD_INCOMING_QOS2 && Record.Identifier == Change->Identifier) {
			if (Change->Operation == XMQTT_SERVER_SESSION_FINISH_INCOMING_QOS2) continue;
			if (Change->Operation == XMQTT_SERVER_SESSION_ACCEPT_INCOMING_QOS2) return false;
		}
		if (Matching && checkpointKeyEqual(Change->Key, Group.Key) &&
			checkpointDelivery(Record.Kind) && checkpointHasKey(Change->DeliveryKey) &&
			checkpointKeyEqual(Change->DeliveryKey, Record.DeliveryKey)) {
			if (Found) return false;
			Found = true;
			switch (Change->Operation) {
			case XMQTT_SERVER_SESSION_SEND_OUTGOING:
				if (Record.Kind != XMQTT_SERVER_RECORD_QUEUED) return false;
				checkpointWriteNumber(Output->Image.Data + GroupPosition + 2, 2, Change->Identifier);
				/* APP writers may see recipient acceptance in a different order
				 * than the recipient worker processes posts. Append in send order. */
				FirstSend = Record;
				FirstSend.Kind = XMQTT_SERVER_RECORD_SENT;
				FirstSend.Identifier = Change->Identifier;
				FirstSend.Code = Change->OutgoingPhase;
				FirstSend.Value = Change->OutgoingExpiry;
				FirstSend.Milliseconds = 0;
				continue;
			case XMQTT_SERVER_SESSION_RELEASE_OUTGOING:
				if (Record.Kind != XMQTT_SERVER_RECORD_SENT || Record.Identifier != Change->Identifier ||
					Record.Code != XMQTT_PACKET_PUBREC) return false;
				Record.Code = Change->OutgoingPhase;
				break;
			case XMQTT_SERVER_SESSION_FINISH_OUTGOING:
				if (Record.Identifier != Change->Identifier || Record.Code != Change->OutgoingPhase) return false;
				continue;
			default:
				return false;
			}
		}
		Output->Offset = Offset;
		if (!checkpointSession(&Record, Output)) return false;
		Output->Offset = Elapsed;
	}
	if (Cursor.Size || (FirstSend.Kind && !checkpointSession(&FirstSend, Output)) ||
		!checkpointAppendGroup(Output, &Group, Change, Targets, &Found)) return false;
	switch (Change->Operation) {
	case XMQTT_SERVER_SESSION_ACCEPT_INCOMING_QOS2:
	case XMQTT_SERVER_SESSION_ACCEPT_INCOMING_QOS1:
	case XMQTT_SERVER_SESSION_UPSERT_SUBSCRIPTIONS:
	case XMQTT_SERVER_SESSION_REMOVE_SUBSCRIPTIONS:
	case XMQTT_SERVER_SESSION_SEND_OUTGOING:
	case XMQTT_SERVER_SESSION_RELEASE_OUTGOING:
		return Found;
	case XMQTT_SERVER_SESSION_PUBLISH_WILL:
	case XMQTT_SERVER_SESSION_CANCEL_WILL:
		return MatchedGroup && FoundWill;
	case XMQTT_SERVER_SESSION_CLOSE:
		return MatchedGroup && (!KeepWill || FoundWill);
	case XMQTT_SERVER_SESSION_FINISH_OUTGOING:
	case XMQTT_SERVER_SESSION_RESUME:
		return MatchedGroup;
	case XMQTT_SERVER_SESSION_START:
	case XMQTT_SERVER_SESSION_DISCARD:
		if (checkpointHasKey(Change->Key)) {
			const xmqttserversessionrecord Record = {.Kind = XMQTT_SERVER_RECORD_SESSION,
				.Data = {(cbytes)Change->ClientId.Data, Change->ClientId.Size},
				.Milliseconds = checkpointExpiryMilliseconds(Change->ExpiryInterval),
				.Value = Change->Operation == XMQTT_SERVER_SESSION_START ? SessionConnected : 0,
				.Code = (uint8)Change->Version, .Key = Change->Key};
			return checkpointSession(&Record, Output) && checkpointArmWill(Output, Change);
		}
		break;
	default:
		break;
	}
	return true;
}

static bool checkpointRetainedTopic(const xmqttserverretainedrecord* Record, xbytesview* Topic)
{
	xmqttwireconfig Config;
	xmqttWireConfigInit(&Config);
	Config.Version = Record->Version;
	xmqttframe Frame;
	if (xmqttFrameRead(Record->Publish, &Config, &Frame) != XMQTT_WIRE_DONE ||
		Frame.Header.Type != XMQTT_PACKET_PUBLISH || Frame.Wire.Size != Record->Publish.Size || Frame.Body.Size < 2)
		return false;
	size_t Length = (size_t)checkpointReadNumber(Frame.Body.Data, 2);
	if (!Length || Length > Frame.Body.Size - 2) return false;
	*Topic = (xbytesview){Frame.Body.Data + 2, Length};
	return true;
}

static bool checkpointWriteRetained(xbytesview* Cursor, bool Timed, size_t Count,
	const xmqttservertransaction* Change, double Captured, checkpointoutput* Output)
{
	uint64 AtCapture = Output->Offset;
	for (size_t At = 0; At < Count; ++At) {
		xmqttserverretainedrecord Record;
		uint64 Offset;
		if (!checkpointReadRetained(Cursor, Timed, &Record, &Offset) || Offset > AtCapture) return false;
		if (Record.RemainingMilliseconds && Record.RemainingMilliseconds <= AtCapture - Offset) continue;
		if (Change->Retained) {
			xbytesview Topic;
			if (!checkpointRetainedTopic(&Record, &Topic)) return false;
			if (checkpointIdEqual(Change->RetainedTopic, Topic)) continue;
		}
		Output->Offset = Offset;
		if (!checkpointRecord(&Record, Output)) return false;
	}
	Output->Offset = AtCapture;
	if (Change->Retained && !Change->RemoveRetained) {
		xmqttserverretainedrecord Record = *Change->Retained;
		if (Record.RemainingMilliseconds) {
			double Age = ceil((Captured - Change->RetainedReceived) * 1000.0);
			if (!isfinite(Age) || Age < 0) return false;
			if (Age >= (double)Record.RemainingMilliseconds) return true;
			Record.RemainingMilliseconds -= (uint64)Age;
		}
		if (!checkpointRecord(&Record, Output)) return false;
	}
	return true;
}

static xmqttserversessionresult checkpointTransact(const checkpointchange* Request,
	bytes Image, xbytesview Cursor, size_t Count)
{
	int64 Now;
	double Captured = xrtTimer();
	uint64 SavedAt = checkpointReadNumber(Image + 8, 8);
	if (Image[7] < '3' || !isfinite(Captured) ||
		!xrtTimeToUnixMs(xrtNow(), &Now) || Now < 0 || (uint64)Now < SavedAt)
		return XMQTT_SERVER_SESSION_REJECTED;
	checkpointoutput Output = {.Offset = (uint64)Now - SavedAt,
		.TimedWills = Request->Store->Wills || Image[7] >= '7'};
	uint64 OfflineMs = Request->Change.OfflineLifetimeMs;
	uint64 WillMs = UINT64_C(1000) * Request->Change.WillDelay;
	if (Request->Change.Operation == XMQTT_SERVER_SESSION_CLOSE) {
		double ClosedAt = Request->Change.ClosedAt;
		if (!isfinite(ClosedAt) || ClosedAt > Captured) return XMQTT_SERVER_SESSION_REJECTED;
		double Age = (Captured - ClosedAt) * 1000.0;
		if (OfflineMs < WillMs) WillMs = OfflineMs;
		WillMs = Age >= (double)WillMs ? 0 : WillMs - (uint64)Age;
		if (OfflineMs != UINT64_MAX) OfflineMs = Age >= (double)OfflineMs ? 0 : OfflineMs - (uint64)Age;
	}
	xbuffer Targets = {0};
	bool Collected = true;
	if (checkpointAcceptsPublication(Request->Change.Operation))
		Collected = xmqttServerVisitTransactionRecipients(Request->Ticket, checkpointCollectRecipient, &Targets);
	else if (checkpointEditsSubscriptions(Request->Change.Operation))
		Collected = xmqttServerVisitTransactionSubscriptions(Request->Ticket, checkpointCollectSubscription, &Targets);
	bool Written = Collected && xrtBufferAppend(&Output.Image, (xbytesview){Image, CheckpointHeader}) &&
		checkpointWriteRetained(&Cursor, Image[7] >= '4', Count, &Request->Change, Captured, &Output) &&
		Output.Count <= RetainedTopics && Cursor.Size >= 4;
	size_t Section = Output.Image.Size;
	if (Written) {
		Output.Image.Data[7] = Output.TimedWills ? '7' : (Request->Store->Expiry || Image[7] == '6' ? '6' : '5');
		checkpointWriteNumber(Output.Image.Data + 16, 4, Output.Count);
		Output.Count = 0;
		Count = (size_t)checkpointReadNumber(Cursor.Data, 4);
		Cursor.Data += 4;
		Cursor.Size -= 4;
		Written = Count <= Cursor.Size / SessionRecord && xrtBufferAdd(&Output.Image, 4) &&
			checkpointWriteTransaction(Request, Cursor, Count, Image[7], &Targets, OfflineMs, WillMs, &Output) &&
			Output.Count <= UINT32_MAX && Output.Image.Size <= CheckpointLimit - CheckpointChecksum;
	}
	xrtBufferUnit(&Targets);
	xmqttserversessionresult Result = XMQTT_SERVER_SESSION_REJECTED;
	if (Written) {
		checkpointWriteNumber(Output.Image.Data + Section, 4, Output.Count);
		unsigned char Sum[CheckpointChecksum];
		checkpointWriteNumber(Sum, sizeof(Sum), xrtCrc32(Output.Image.Data, Output.Image.Size));
		if (xrtBufferAppend(&Output.Image, (xbytesview){Sum, sizeof(Sum)}))
			Result = xrtFileWriteAtomicRename(Request->Store->Path, xrtBufferView(&Output.Image)) ?
				XMQTT_SERVER_SESSION_COMMITTED : XMQTT_SERVER_SESSION_UNKNOWN;
	}
	xrtBufferUnit(&Output.Image);
	return Result;
}

static xtaskoutcome checkpointCommitTask(xcancel* Cancel, ptr Data, xtaskvalue* Value)
{
	(void)Cancel;
	(void)Value;
	checkpointchange* Change = Data;
	checkpointstore* Store = Change->Store;
	/* The APP can hold its first running write before touching the file. */
	if (Store->WriterGate && !Store->WriterReady) {
		Store->WriterReady = true;
		if (puts("writer-ready") == EOF || fflush(stdout) ||
			!xrtMutexLock(Store->WriterGate) || !xrtMutexUnlock(Store->WriterGate)) abort();
	}
	if (Store->Delay) xrtSleep(Store->Delay);
	xbytesview Cursor;
	size_t Count;
	bytes Image = checkpointOpen(Change->Store->Path, &Cursor, &Count);
	xmqttserversessionresult Result = XMQTT_SERVER_SESSION_REJECTED;
	if (Image) {
		Result = Change->Store->Online ? checkpointTransact(Change, Image, Cursor, Count) :
			checkpointApply(Change, Image, Cursor, Count);
	}
	xrtFree(Image);
	if (Change->RetainedTicket) {
		xmqttserverretainedresult Retained = Result == XMQTT_SERVER_SESSION_COMMITTED ?
			XMQTT_SERVER_RETAINED_COMMITTED : (Result == XMQTT_SERVER_SESSION_UNKNOWN ?
			XMQTT_SERVER_RETAINED_UNKNOWN : XMQTT_SERVER_RETAINED_REJECTED);
		if (!xmqttServerCompleteRetained(Change->RetainedTicket, Retained)) abort();
	} else if (!xmqttServerCompleteSessionCommit(Change->Ticket, Result)) abort();
	xrtFree(Change);
	return XTASK_SUCCESS;
}

static bool checkpointEnqueue(const checkpointchange* Request)
{
	checkpointchange* Change = xrtMalloc(sizeof(*Change));
	if (!Change) return false;
	*Change = *Request;
	xfuture* Write = xrtTaskSubmit(Request->Store->Writer, checkpointCommitTask, Change, NULL);
	if (!Write) {
		xrtFree(Change);
		return false;
	}
	xrtFutureDestroy(Write);
	return true;
}

static bool checkpointSubmitTransaction(xmqttserversessionticket* Ticket, const xmqttservertransaction* Record, ptr User)
{
	const checkpointchange Request = {.Store = User, .Ticket = Ticket, .Change = *Record};
	return checkpointEnqueue(&Request);
}

static bool checkpointSubmitRetained(xmqttserverretainedticket* Ticket, xstrview Topic,
	const xmqttserverretainedrecord* Record, bool Remove, double Received, ptr User)
{
	const checkpointchange Request = {.Store = User, .RetainedTicket = Ticket,
		.Change = {.Retained = Record, .RetainedTopic = Topic, .RemoveRetained = Remove, .RetainedReceived = Received}};
	return checkpointEnqueue(&Request);
}

static bool checkpointSubmit(xmqttserversessionticket* Ticket, xmqttserversessionoperation Operation,
	xstrview ClientId, uint16 Identifier, ptr User)
{
	const xmqttservertransaction Change = {.Operation = Operation, .ClientId = ClientId, .Identifier = Identifier};
	return checkpointSubmitTransaction(Ticket, &Change, User);
}

static bool checkpointPrepare(xmqttserver* Server, ptr User)
{
	checkpointstore* Store = User;
	bool Ready = Store->Online ?
		(xmqttServerSetSessionTransactions(Server, checkpointSubmitTransaction, Store) &&
		 xmqttServerSetRetainedCommit(Server, 4u * 1024u * 1024u, checkpointSubmitRetained, Store) &&
		 xmqttServerEnableRetainedTransactions(Server) && xmqttServerEnableOutboundTransactions(Server) &&
		 xmqttServerEnableQos1Transactions(Server) && xmqttServerEnableSubscriptionTransactions(Server)) :
		(!Store->Writer || xmqttServerSetSessionCommit(Server, checkpointSubmit, Store));
	return Ready && (!Store->Expiry || xmqttServerEnableSessionExpiryTransactions(Server)) &&
		(!Store->Wills || xmqttServerEnableWillTransactions(Server)) && checkpointLoad(Server, Store->Path, Store->Online);
}

int main(int Argc, char** Argv)
{
	bool RequireMethod = Argc > 1 && !strcmp(Argv[1], "--require-method");
	if (RequireMethod) {
		--Argc;
		++Argv;
	}
	bool Compare = Argc >= 2 && !strcmp(Argv[1], "--compare");
	if (Compare) {
		--Argc;
		++Argv;
	}
	bool WebSocket = Argc > 1 && (!strcmp(Argv[1], "--ws") || !strcmp(Argv[1], "--wss"));
	bool Secure = Argc > 1 && (!strcmp(Argv[1], "--tls") || !strcmp(Argv[1], "--wss"));
#if defined(XMQTT_EXAMPLE_SERVER_WS)
	char** Transport = Argv;
	if (Secure && Argc < 4) {
		fputs("Supply certificate.der and rsa-private-key.der after --tls or --wss.\n", stderr);
		return 1;
	}
	int Prefix = Secure ? 3 : WebSocket ? 1 : 0;
	Argc -= Prefix;
	Argv += Prefix;
#else
	if (WebSocket || Secure) {
		fputs("Build the checkpoint example with the WS and TLS example dependencies.\n", stderr);
		return 1;
	}
#endif
	bool Wills = Argc >= 3 && !strcmp(Argv[1], "restore-online-will");
	bool Expiry = Wills || (Argc >= 3 && !strcmp(Argv[1], "restore-online-expiry"));
	bool Online = Expiry || (Argc >= 3 && !strcmp(Argv[1], "restore-online"));
	bool Fenced = Online || (Argc >= 3 && !strcmp(Argv[1], "restore-fenced"));
	bool HoldWriter = Argc == 4 && !strcmp(Argv[3], "--hold-writer");
	if ((HoldWriter && strcmp(Argv[1], "restore-online")) || (Fenced && Argc > 4) ||
		(!Fenced && (Argc != 3 || (strcmp(Argv[1], "capture") && strcmp(Argv[1], "restore"))))) {
		puts("usage: examples_server_checkpoint [--require-method] [--compare] capture|restore checkpoint.bin");
		puts("       examples_server_checkpoint restore-fenced|restore-online checkpoint.bin [delay-ms]");
		puts("       examples_server_checkpoint restore-online-expiry|restore-online-will checkpoint.bin [delay-ms]");
		puts("       examples_server_checkpoint restore-online checkpoint.bin --hold-writer");
#if defined(XMQTT_EXAMPLE_SERVER_WS)
		puts("       optional transport prefix: --ws or --tls|--wss certificate.der rsa-private-key.der");
#endif
		return Argc == 1 ? 0 : 1;
	}
	serverexample Example = {0};
#if defined(XMQTT_EXAMPLE_SERVER_AUTH)
	exampleauth AuthState;
	xmqttserverauthconfig Auth;
	if (RequireMethod) {
		const char* Secret = getenv("XMQTT_EXAMPLE_AUTH_KEY");
		if (!Secret || strlen(Secret) < 32) {
			fputs("Set XMQTT_EXAMPLE_AUTH_KEY to a test secret with at least 32 bytes.\n", stderr);
			return 1;
		}
		exampleAuthInit(&AuthState, &Auth, Secret);
		Example.RequireMethod = true;
		Example.Auth = &Auth;
	}
#else
	if (RequireMethod) {
		fputs("Build the checkpoint example with MQTT server auth and XRT HMAC-SHA256.\n", stderr);
		return 1;
	}
#endif
	xmqttserverconfig Config;
	xmqttServerConfigInit(&Config);
	Config.MaxRetainedTopics = RetainedTopics;
	Config.MaxRetainedBytes = RetainedBytes;
#if defined(XMQTT_FEATURE_XMQTT_CODEC311)
	Config.Allow311 = true;
#endif
	checkpointstore Store = {.Path = Argv[2], .Delay = Argc == 4 && !HoldWriter ? strtol(Argv[3], NULL, 10) : 0,
		.Online = Online, .Expiry = Expiry, .Wills = Wills};
	if (Store.Delay < 0 || Store.Delay > 2000) return 1;
	if (HoldWriter) {
		Store.WriterGate = xrtMutexCreate();
		if (!Store.WriterGate) return 1;
		if (!xrtMutexLock(Store.WriterGate)) {
			xrtMutexDestroy(Store.WriterGate);
			return 1;
		}
	}
	if (Fenced) {
		const xtaskpoolconfig PoolConfig = {.Threads = 1, .QueueLimit = 16};
		Store.Writer = xrtTaskPoolCreate(&PoolConfig);
		if (!Store.Writer) {
			if (Store.WriterGate) {
				xrtMutexUnlock(Store.WriterGate);
				xrtMutexDestroy(Store.WriterGate);
			}
			return 1;
		}
	}
	if (strcmp(Argv[1], "capture")) {
		Example.Prepare = checkpointPrepare;
		Example.PrepareData = &Store;
	}
	bool TransportReady = true;
#if defined(XMQTT_EXAMPLE_SERVER_WS)
	xtlsserverconfig Tls;
	Example.WebSocket = WebSocket;
	if (Secure) {
		TransportReady = exampleServerLoadTls(&Tls, Transport[2], Transport[3]);
		if (TransportReady) Example.Tls = &Tls;
		else fputs("Could not load checkpoint listener TLS credentials.\n", stderr);
	}
#endif
	char Endpoint[80];
	bool Ready = TransportReady && exampleServerStart(&Example, &Config, Compare ? 1 : 2) &&
		xrtNetAddrEndpointText(&Example.Address, Endpoint, sizeof(Endpoint));
	if (Ready) {
		puts(Endpoint);
		fflush(stdout);
		(void)getchar();
	}
	bool Capture = Ready && xmqttServerCloseForCheckpoint(Example.Server);
	if (Store.WriterGate && !xrtMutexUnlock(Store.WriterGate)) abort();
	bool Drained = exampleServerDrain(&Example);
	bool Joined = Drained && (!Store.Writer ||
		(xrtTaskPoolClose(Store.Writer) && xrtTaskPoolWaitFor(Store.Writer, 5000) == XWAIT_OK &&
		xrtTaskPoolDestroy(Store.Writer)));
	bool Saved = Capture && Joined && checkpointSave(Example.Server, Store.Path);
	bool Closed = Joined && exampleServerStop(&Example);
	if (Joined && Store.WriterGate) Closed = xrtMutexDestroy(Store.WriterGate) && Closed;
#if defined(XMQTT_EXAMPLE_SERVER_WS)
	if (Closed && Example.Tls) exampleServerReleaseTls(Example.Tls);
#endif
#if defined(XMQTT_EXAMPLE_SERVER_AUTH)
	Closed = Closed && (!RequireMethod || !xrtAtomic32Load(&AuthState.Active, XMEMORY_RELAXED));
#endif
	if (!Saved) fputs("broker checkpoint could not be restored or saved\n", stderr);
	return Saved && Closed ? 0 : 1;
}
