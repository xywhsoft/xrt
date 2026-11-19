#include "app_listener.h"
#include <xrt/buffer.h>
#include <xrt/crc32.h>
#include <xrt/file.h>
#include <xrt/memory.h>
#include <string.h>

/* Application-owned binary fixture for a single loopback broker. */
enum { CheckpointHeader = 20, CheckpointRecord = 15, CheckpointChecksum = 4, RetainedTopics = 4096 };
static const unsigned char CheckpointMagic[8] = {'X','M','Q','R','E','T','0','1'};
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
		unsigned char Checksum[CheckpointChecksum];
		checkpointWriteNumber(Checksum, sizeof(Checksum), xrtCrc32(Output.Image.Data, Output.Image.Size));
		Written = xrtBufferAppend(&Output.Image, (xbytesview){Checksum, sizeof(Checksum)}) &&
			xrtFileWriteAtomic(Path, xrtBufferView(&Output.Image));
	}
	xrtBufferUnit(&Output.Image);
	return Written;
}

static bool checkpointParse(xbytesview Input, xmqttserverretainedrecord* Records, size_t Count)
{
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
	return Input.Size == 0;
}

static bool checkpointLoad(xmqttserver* Server, ptr Data)
{
	const size_t Limit = RetainedBytes + RetainedTopics * CheckpointRecord + CheckpointHeader + CheckpointChecksum;
	size_t Bytes;
	bytes Image = xrtFileReadAllLimit(Data, Limit, &Bytes);
	if (!Image) return false;
	bool Restored = false;
	if (Bytes >= CheckpointHeader + CheckpointChecksum && !memcmp(Image, CheckpointMagic, sizeof(CheckpointMagic))) {
		size_t Content = Bytes - CheckpointChecksum;
		uint64 SavedAt = checkpointReadNumber(Image + 8, 8);
		size_t Count = (size_t)checkpointReadNumber(Image + 16, 4);
		if (Count <= RetainedTopics && xrtCrc32(Image, Content) == checkpointReadNumber(Image + Content, CheckpointChecksum)) {
			xmqttserverretainedrecord* Records = Count ? xrtCalloc(Count, sizeof(*Records)) : NULL;
			int64 Now;
			Restored = (!Count || Records) &&
				checkpointParse((xbytesview){Image + CheckpointHeader, Content - CheckpointHeader}, Records, Count) &&
				xrtTimeToUnixMs(xrtNow(), &Now) && Now >= 0 && (uint64)Now >= SavedAt &&
				xmqttServerRestoreRetained(Server, Records, Count, (uint64)Now - SavedAt);
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
	bool Saved = Ready && exampleServerDrain(&Example) && checkpointSave(Example.Server, Argv[2]);
	bool Closed = exampleServerStop(&Example);
	if (!Saved) fputs("retained checkpoint could not be restored or saved\n", stderr);
	return Saved && Closed ? 0 : 1;
}
