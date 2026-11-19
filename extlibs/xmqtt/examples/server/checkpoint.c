#include "app_listener.h"
#include <xrt/buffer.h>
#include <xrt/crc32.h>
#include <xrt/file.h>
#include <xrt/memory.h>
#include <xrt/task.h>
#include <stdlib.h>
#include <string.h>

/* Application-owned binary fixture for a single loopback broker. */
enum { CheckpointHeader = 20, CheckpointRecord = 15, SessionRecord = 22, CheckpointChecksum = 4, RetainedTopics = 4096 };
static const unsigned char CheckpointMagic[8] = {'X','M','Q','R','E','T','0','2'};
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
} checkpointoutput;

static bool checkpointRecord(const xmqttserverretainedrecord* Record, ptr Data)
{
	checkpointoutput* Output = Data;
	unsigned char Header[CheckpointRecord];
	Header[0] = (unsigned char)Record->Version;
	checkpointWriteNumber(Header + 1, 2, Record->PublisherId.Size);
	checkpointWriteNumber(Header + 3, 4, Record->Publish.Size);
	checkpointWriteNumber(Header + 7, 8, Record->RemainingMilliseconds);
	if (!xrtBufferAppend(&Output->Image, (xbytesview){Header, sizeof(Header)}) ||
		!xrtBufferAppend(&Output->Image, (xbytesview){(cbytes)Record->PublisherId.Data, Record->PublisherId.Size}) ||
		!xrtBufferAppend(&Output->Image, Record->Publish)) return false;
	++Output->Count;
	return true;
}

static bool checkpointSession(const xmqttserversessionrecord* Record, ptr Data)
{
	checkpointoutput* Output = Data;
	unsigned char Header[SessionRecord];
	Header[0] = (uint8)Record->Kind;
	Header[1] = Record->Code;
	checkpointWriteNumber(Header + 2, 2, Record->Identifier);
	checkpointWriteNumber(Header + 4, 4, Record->Value);
	checkpointWriteNumber(Header + 8, 8, Record->Milliseconds);
	checkpointWriteNumber(Header + 16, 4, Record->Data.Size);
	checkpointWriteNumber(Header + 20, 2, Record->Group.Size);
	if (!xrtBufferAppend(&Output->Image, (xbytesview){Header, sizeof(Header)}) ||
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
			xrtFileWriteAtomic(Path, xrtBufferView(&Output.Image));
	}
	xrtBufferUnit(&Output.Image);
	return Written;
}

static bool checkpointParseRetained(xbytesview* Cursor, xmqttserverretainedrecord* Records, size_t Count)
{
	xbytesview Input = *Cursor;
	for (size_t Index = 0; Index < Count; ++Index) {
		if (Input.Size < CheckpointRecord) return false;
		cbytes Header = Input.Data;
		size_t Id = (size_t)checkpointReadNumber(Header + 1, 2);
		size_t Wire = (size_t)checkpointReadNumber(Header + 3, 4);
		Input.Data += CheckpointRecord;
		Input.Size -= CheckpointRecord;
		if (Id > Input.Size || Wire > Input.Size - Id) return false;
		if (Records) Records[Index] = (xmqttserverretainedrecord){
			(xmqttversion)Header[0], {(cstr)Input.Data, Id}, {Input.Data + Id, Wire},
			checkpointReadNumber(Header + 7, 8)
		};
		Input.Data += Id + Wire;
		Input.Size -= Id + Wire;
	}
	*Cursor = Input;
	return true;
}

static bool checkpointReadSession(xbytesview* Input, xmqttserversessionrecord* Record)
{
	if (Input->Size < SessionRecord) return false;
	cbytes Header = Input->Data;
	size_t Data = (size_t)checkpointReadNumber(Header + 16, 4);
	size_t Group = (size_t)checkpointReadNumber(Header + 20, 2);
	Input->Data += SessionRecord;
	Input->Size -= SessionRecord;
	if (Data > Input->Size || Group > Input->Size - Data) return false;
	*Record = (xmqttserversessionrecord){
		.Kind = (xmqttserverrecordkind)Header[0], .Code = Header[1],
		.Identifier = (uint16)checkpointReadNumber(Header + 2, 2),
		.Value = (uint32)checkpointReadNumber(Header + 4, 4),
		.Milliseconds = checkpointReadNumber(Header + 8, 8),
		.Data = {Input->Data, Data}, .Group = {Input->Data + Data, Group}
	};
	Input->Data += Data + Group;
	Input->Size -= Data + Group;
	return true;
}

static bool checkpointParseSessions(xbytesview Input, xmqttserversessionrecord* Records, size_t Count)
{
	for (size_t Index = 0; Index < Count; ++Index)
		if (!checkpointReadSession(&Input, &Records[Index])) return false;
	return Input.Size == 0;
}

static bool checkpointRestoreSessions(xmqttserver* Server, xbytesview Input, uint64 Elapsed)
{
	if (Input.Size < 4) return false;
	size_t Count = (size_t)checkpointReadNumber(Input.Data, 4);
	Input.Data += 4;
	Input.Size -= 4;
	/* Bound the temporary descriptor table by the sample broker's default quotas. */
	if (Count > Input.Size / SessionRecord || Count > 2u * 1024u + 65536u + 65536u + 16384u) return false;
	xmqttserversessionrecord* Records = Count ? xrtCalloc(Count, sizeof(*Records)) : NULL;
	bool Restored = (!Count || Records) && checkpointParseSessions(Input, Records, Count) &&
		xmqttServerRestoreSessions(Server, Records, Count, Elapsed);
	xrtFree(Records);
	return Restored;
}

static bytes checkpointOpen(cstr Path, xbytesview* Cursor, size_t* Count)
{
	size_t Bytes;
	bytes Image = xrtFileReadAllLimit(Path, CheckpointLimit, &Bytes);
	if (!Image) return NULL;
	if (Bytes >= CheckpointHeader + CheckpointChecksum && !memcmp(Image, CheckpointMagic, 7) &&
		(Image[7] == '1' || Image[7] == '2')) {
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

static bool checkpointLoad(xmqttserver* Server, ptr Data)
{
	xbytesview Cursor;
	size_t Count;
	bytes Image = checkpointOpen(Data, &Cursor, &Count);
	if (!Image) return false;
	uint64 SavedAt = checkpointReadNumber(Image + 8, 8);
	xmqttserverretainedrecord* Records = Count ? xrtCalloc(Count, sizeof(*Records)) : NULL;
	int64 Now;
	bool Restored = (!Count || Records) && checkpointParseRetained(&Cursor, Records, Count) &&
		xrtTimeToUnixMs(xrtNow(), &Now) && Now >= 0 && (uint64)Now >= SavedAt &&
		xmqttServerRestoreRetained(Server, Records, Count, (uint64)Now - SavedAt);
	if (Restored)
		Restored = Image[7] == '1' ? Cursor.Size == 0 :
			checkpointRestoreSessions(Server, Cursor, (uint64)Now - SavedAt);
	xrtFree(Records);
	xrtFree(Image);
	return Restored;
}

typedef struct checkpointstore {
	cstr Path;
	xtaskpool* Writer;
	int64 Delay;
} checkpointstore;

typedef struct checkpointchange {
	checkpointstore* Store;
	xmqttserversessionticket* Ticket;
	xstrview ClientId;
	xmqttserversessionoperation Operation;
	uint16 Identifier;
} checkpointchange;

static xmqttserversessionresult checkpointApply(const checkpointchange* Change, bytes Image,
	xbytesview Cursor, size_t Count)
{
	if (!checkpointParseRetained(&Cursor, NULL, Count))
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
		if (!checkpointReadSession(&Cursor, &Record) ||
			(!Index && Record.Kind != XMQTT_SERVER_RECORD_SESSION))
			return XMQTT_SERVER_SESSION_REJECTED;
		if (Record.Kind == XMQTT_SERVER_RECORD_SESSION)
			Selected = Record.Data.Size == Change->ClientId.Size &&
				!memcmp(Record.Data.Data, Change->ClientId.Data, Record.Data.Size);
		if (Selected && (Change->Operation == XMQTT_SERVER_SESSION_DISCARD ||
			(Record.Kind == XMQTT_SERVER_RECORD_INCOMING_QOS2 && Record.Identifier == Change->Identifier)))
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
	return xrtFileWriteAtomic(Change->Store->Path, (xbytesview){Image, Size + CheckpointChecksum}) ?
		XMQTT_SERVER_SESSION_COMMITTED : XMQTT_SERVER_SESSION_UNKNOWN;
}

static xtaskoutcome checkpointCommitTask(xcancel* Cancel, ptr Data, xtaskvalue* Value)
{
	(void)Cancel;
	(void)Value;
	checkpointchange* Change = Data;
	if (Change->Store->Delay) xrtSleep(Change->Store->Delay);
	xbytesview Cursor;
	size_t Count;
	bytes Image = checkpointOpen(Change->Store->Path, &Cursor, &Count);
	xmqttserversessionresult Result = Image ? checkpointApply(Change, Image, Cursor, Count) :
		XMQTT_SERVER_SESSION_REJECTED;
	xrtFree(Image);
	if (!xmqttServerCompleteSessionCommit(Change->Ticket, Result)) abort();
	xrtFree(Change);
	return XTASK_SUCCESS;
}

static bool checkpointSubmit(xmqttserversessionticket* Ticket, xmqttserversessionoperation Operation,
	xstrview ClientId, uint16 Identifier, ptr User)
{
	checkpointstore* Store = User;
	checkpointchange* Change = xrtMalloc(sizeof(*Change));
	if (!Change) return false;
	*Change = (checkpointchange){Store, Ticket, ClientId, Operation, Identifier};
	xfuture* Write = xrtTaskSubmit(Store->Writer, checkpointCommitTask, Change, NULL);
	if (!Write) {
		xrtFree(Change);
		return false;
	}
	xrtFutureDestroy(Write);
	return true;
}

static bool checkpointPrepare(xmqttserver* Server, ptr User)
{
	checkpointstore* Store = User;
	return (!Store->Writer || xmqttServerSetSessionCommit(Server, checkpointSubmit, Store)) &&
		checkpointLoad(Server, (ptr)Store->Path);
}

int main(int Argc, char** Argv)
{
	bool Fenced = Argc >= 3 && !strcmp(Argv[1], "restore-fenced");
	if ((Fenced && Argc > 4) ||
		(!Fenced && (Argc != 3 || (strcmp(Argv[1], "capture") && strcmp(Argv[1], "restore"))))) {
		puts("usage: examples_server_checkpoint capture|restore checkpoint.bin");
		puts("       examples_server_checkpoint restore-fenced checkpoint.bin [delay-ms]");
		return Argc == 1 ? 0 : 1;
	}
	xmqttserverconfig Config;
	xmqttServerConfigInit(&Config);
	Config.MaxRetainedTopics = RetainedTopics;
	Config.MaxRetainedBytes = RetainedBytes;
#if defined(XMQTT_FEATURE_XMQTT_CODEC311)
	Config.Allow311 = true;
#endif
	checkpointstore Store = {.Path = Argv[2], .Delay = Argc == 4 ? strtol(Argv[3], NULL, 10) : 0};
	if (Store.Delay < 0 || Store.Delay > 2000) return 1;
	if (Fenced) {
		const xtaskpoolconfig PoolConfig = {.Threads = 1, .QueueLimit = 16};
		Store.Writer = xrtTaskPoolCreate(&PoolConfig);
		if (!Store.Writer) return 1;
	}
	serverexample Example = {0};
	if (strcmp(Argv[1], "capture")) {
		Example.Prepare = checkpointPrepare;
		Example.PrepareData = &Store;
	}
	char Endpoint[80];
	bool Ready = exampleServerStart(&Example, &Config, 2) &&
		xrtNetAddrEndpointText(&Example.Address, Endpoint, sizeof(Endpoint));
	if (Ready) {
		puts(Endpoint);
		fflush(stdout);
		(void)getchar();
	}
	bool Capture = Ready && xmqttServerCloseForCheckpoint(Example.Server);
	bool Drained = exampleServerDrain(&Example);
	bool Joined = Drained && (!Store.Writer ||
		(xrtTaskPoolClose(Store.Writer) && xrtTaskPoolWaitFor(Store.Writer, 5000) == XWAIT_OK &&
		xrtTaskPoolDestroy(Store.Writer)));
	bool Saved = Capture && Joined && checkpointSave(Example.Server, Store.Path);
	bool Closed = Joined && exampleServerStop(&Example);
	if (!Saved) fputs("broker checkpoint could not be restored or saved\n", stderr);
	return Saved && Closed ? 0 : 1;
}
