#include "app_listener.h"
#include <xrt/buffer.h>
#include <xrt/crc32.h>
#include <xrt/file.h>
#include <xrt/memory.h>
#include <string.h>

/* Application-owned binary fixture for a single loopback broker. */
enum { CheckpointHeader = 20, CheckpointRecord = 15, SessionRecord = 22, CheckpointChecksum = 4, RetainedTopics = 4096 };
static const unsigned char CheckpointMagic[8] = {'X','M','Q','R','E','T','0','2'};
static const size_t RetainedBytes = 16u * 1024u * 1024u;

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
		Records[Index] = (xmqttserverretainedrecord){
			(xmqttversion)Header[0], {(cstr)Input.Data, Id}, {Input.Data + Id, Wire},
			checkpointReadNumber(Header + 7, 8)
		};
		Input.Data += Id + Wire;
		Input.Size -= Id + Wire;
	}
	*Cursor = Input;
	return true;
}

static bool checkpointParseSessions(xbytesview Input, xmqttserversessionrecord* Records, size_t Count)
{
	for (size_t Index = 0; Index < Count; ++Index) {
		if (Input.Size < SessionRecord) return false;
		cbytes Header = Input.Data;
		size_t Data = (size_t)checkpointReadNumber(Header + 16, 4);
		size_t Group = (size_t)checkpointReadNumber(Header + 20, 2);
		Input.Data += SessionRecord;
		Input.Size -= SessionRecord;
		if (Data > Input.Size || Group > Input.Size - Data) return false;
		Records[Index] = (xmqttserversessionrecord){
			.Kind = (xmqttserverrecordkind)Header[0], .Code = Header[1],
			.Identifier = (uint16)checkpointReadNumber(Header + 2, 2),
			.Value = (uint32)checkpointReadNumber(Header + 4, 4),
			.Milliseconds = checkpointReadNumber(Header + 8, 8),
			.Data = {Input.Data, Data}, .Group = {Input.Data + Data, Group}
		};
		Input.Data += Data + Group;
		Input.Size -= Data + Group;
	}
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

static bool checkpointLoad(xmqttserver* Server, ptr Data)
{
	const size_t Limit = 192u * 1024u * 1024u; /* Aggregate default protocol quotas and record headers. */
	size_t Bytes;
	bytes Image = xrtFileReadAllLimit(Data, Limit, &Bytes);
	if (!Image) return false;
	bool Restored = false;
	if (Bytes >= CheckpointHeader + CheckpointChecksum && !memcmp(Image, CheckpointMagic, 7) &&
		(Image[7] == '1' || Image[7] == '2')) {
		size_t Content = Bytes - CheckpointChecksum;
		uint64 SavedAt = checkpointReadNumber(Image + 8, 8);
		size_t Count = (size_t)checkpointReadNumber(Image + 16, 4);
		if (Count <= RetainedTopics && xrtCrc32(Image, Content) == checkpointReadNumber(Image + Content, CheckpointChecksum)) {
			xmqttserverretainedrecord* Records = Count ? xrtCalloc(Count, sizeof(*Records)) : NULL;
			xbytesview Cursor = {Image + CheckpointHeader, Content - CheckpointHeader};
			int64 Now;
			Restored = (!Count || Records) &&
				checkpointParseRetained(&Cursor, Records, Count) &&
				xrtTimeToUnixMs(xrtNow(), &Now) && Now >= 0 && (uint64)Now >= SavedAt &&
				xmqttServerRestoreRetained(Server, Records, Count, (uint64)Now - SavedAt);
			if (Restored)
				Restored = Image[7] == '1' ? Cursor.Size == 0 :
					checkpointRestoreSessions(Server, Cursor, (uint64)Now - SavedAt);
			xrtFree(Records);
		}
	}
	xrtFree(Image);
	return Restored;
}

int main(int Argc, char** Argv)
{
	if (Argc != 3 || (strcmp(Argv[1], "capture") && strcmp(Argv[1], "restore"))) {
		puts("usage: examples_server_checkpoint capture|restore checkpoint.bin");
		return Argc == 1 ? 0 : 1;
	}
	xmqttserverconfig Config;
	xmqttServerConfigInit(&Config);
	Config.MaxRetainedTopics = RetainedTopics;
	Config.MaxRetainedBytes = RetainedBytes;
#if defined(XMQTT_FEATURE_XMQTT_CODEC311)
	Config.Allow311 = true;
#endif
	serverexample Example = {0};
	if (!strcmp(Argv[1], "restore")) {
		Example.Prepare = checkpointLoad;
		Example.PrepareData = Argv[2];
	}
	char Endpoint[80];
	bool Ready = exampleServerStart(&Example, &Config, 2) &&
		xrtNetAddrEndpointText(&Example.Address, Endpoint, sizeof(Endpoint));
	if (Ready) {
		puts(Endpoint);
		fflush(stdout);
		(void)getchar();
	}
	bool Saved = Ready && xmqttServerCloseForCheckpoint(Example.Server) &&
		exampleServerDrain(&Example) && checkpointSave(Example.Server, Argv[2]);
	bool Closed = exampleServerStop(&Example);
	if (!Saved) fputs("broker checkpoint could not be restored or saved\n", stderr);
	return Saved && Closed ? 0 : 1;
}
