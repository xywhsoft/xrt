#define XMQTT_EXAMPLE_SERVER_WS
#include "app_listener.h"
#include "app_tls.h"
#include "app_ws.h"
#include <xrt/crc32.h>
#include <xrt/file.h>
#include <xrt/memory.h>
#include <xrt/task.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

/* APP policy for one loopback topic. An atomic file replacement stores its
 * latest publication or tombstone. The pool owns all file operations. */
enum { SlotHeader = 32, SlotTrailer = 4, SlotLimit = 32768 };
static const unsigned char SlotMagic[8] = {'X','M','Q','R','L','I','V','1'};
static const char SlotTopic[] = "durable/retained";

typedef struct retainedslot {
	cstr Path;
	xtaskpool* Pool;
	int64 Delay;
	bool Restore;
} retainedslot;

typedef struct retainedwrite {
	const retainedslot* Slot;
	xmqttserverretainedticket* Ticket;
	const xmqttserverretainedrecord* Record;
	double Received;
	bool Remove;
} retainedwrite;

static void slotPut(bytes Target, size_t Width, uint64 Value)
{
	while (Width) {
		Target[--Width] = (uint8)Value;
		Value >>= 8;
	}
}

static uint64 slotGet(cbytes Source, size_t Width)
{
	uint64 Value = 0;
	for (size_t Index = 0; Index < Width; ++Index) Value = Value * 256 + Source[Index];
	return Value;
}

static xmqttserverretainedresult slotSave(const retainedwrite* Write)
{
	const xmqttserverretainedrecord* Record = Write->Record;
	size_t Size = SlotHeader + SlotTrailer + Record->PublisherId.Size + Record->Publish.Size;
	if (Size > SlotLimit) return XMQTT_SERVER_RETAINED_REJECTED;
	int64 Wall;
	double Age = xrtTimer() - Write->Received;
	if (!isfinite(Age) || Age < 0 || !xrtTimeToUnixMs(xrtNow(), &Wall) || Wall < 0)
		return XMQTT_SERVER_RETAINED_REJECTED;
	uint64 Remaining = Record->RemainingMilliseconds;
	bool Remove = Write->Remove;
	if (Remaining) {
		double Milliseconds = ceil(Age * 1000.0);
		if (Milliseconds >= (double)Remaining) Remove = true;
		else Remaining -= (uint64)Milliseconds;
	}
	bytes Image = xrtMalloc(Size);
	if (!Image) return XMQTT_SERVER_RETAINED_REJECTED;
	memcpy(Image, SlotMagic, sizeof(SlotMagic));
	slotPut(Image + 8, 8, (uint64)Wall);
	slotPut(Image + 16, 8, Remaining);
	Image[24] = Remove;
	Image[25] = (uint8)Record->Version;
	slotPut(Image + 26, 2, Record->PublisherId.Size);
	slotPut(Image + 28, 4, Record->Publish.Size);
	memcpy(Image + SlotHeader, Record->PublisherId.Data, Record->PublisherId.Size);
	memcpy(Image + SlotHeader + Record->PublisherId.Size, Record->Publish.Data, Record->Publish.Size);
	slotPut(Image + Size - SlotTrailer, SlotTrailer, xrtCrc32(Image, Size - SlotTrailer));
	bool Saved = xrtFileWriteAtomic(Write->Slot->Path, (xbytesview){Image, Size});
	xrtFree(Image);
	return Saved ? XMQTT_SERVER_RETAINED_COMMITTED : XMQTT_SERVER_RETAINED_UNKNOWN;
}

static xtaskoutcome slotTask(xcancel* Cancel, ptr Data, xtaskvalue* Value)
{
	(void)Cancel;
	(void)Value;
	retainedwrite* Write = Data;
	if (Write->Slot->Delay) xrtSleep(Write->Slot->Delay);
	xmqttserverretainedresult Result = slotSave(Write);
	if (!xmqttServerCompleteRetained(Write->Ticket, Result)) abort();
	xrtFree(Write);
	return XTASK_SUCCESS;
}

static bool slotSubmit(xmqttserverretainedticket* Ticket, xstrview Topic,
	const xmqttserverretainedrecord* Record, bool Remove, double Received, ptr User)
{
	retainedslot* Slot = User;
	if (Topic.Size != sizeof(SlotTopic) - 1 || memcmp(Topic.Data, SlotTopic, Topic.Size)) return false;
	retainedwrite* Write = xrtMalloc(sizeof(*Write));
	if (!Write) return false;
	*Write = (retainedwrite){Slot, Ticket, Record, Received, Remove};
	xfuture* Future = xrtTaskSubmit(Slot->Pool, slotTask, Write, NULL);
	if (!Future) {
		xrtFree(Write);
		return false;
	}
	xrtFutureDestroy(Future);
	return true;
}

static bool slotLoad(xmqttserver* Server, cstr Path)
{
	size_t Size;
	bytes Image = xrtFileReadAllLimit(Path, SlotLimit, &Size);
	if (!Image) return false;
	bool Loaded = false;
	if (Size >= SlotHeader + SlotTrailer && !memcmp(Image, SlotMagic, sizeof(SlotMagic)) &&
		xrtCrc32(Image, Size - SlotTrailer) == slotGet(Image + Size - SlotTrailer, SlotTrailer)) {
		size_t Id = (size_t)slotGet(Image + 26, 2);
		size_t Wire = (size_t)slotGet(Image + 28, 4);
		size_t Available = Size - SlotHeader - SlotTrailer;
		uint64 Saved = slotGet(Image + 8, 8);
		int64 Now;
		if (Id <= Available && Wire == Available - Id && Image[24] <= 1 &&
			xrtTimeToUnixMs(xrtNow(), &Now) && Now >= 0 && (uint64)Now >= Saved) {
			const xmqttserverretainedrecord Record = {
				(xmqttversion)Image[25], {(cstr)Image + SlotHeader, Id},
				{Image + SlotHeader + Id, Wire}, slotGet(Image + 16, 8)
			};
			Loaded = Image[24] || xmqttServerRestoreRetained(Server, &Record, 1, (uint64)Now - Saved);
		}
	}
	xrtFree(Image);
	return Loaded;
}

static bool slotPrepare(xmqttserver* Server, ptr User)
{
	retainedslot* Slot = User;
	return xmqttServerSetRetainedCommit(Server, 256u * 1024u, slotSubmit, Slot) &&
		(!Slot->Restore || slotLoad(Server, Slot->Path));
}

int main(int Argc, char** Argv)
{
	const char* Mode = Argc >= 5 ? Argv[4] : "tcp";
	bool WebSocket = !strcmp(Mode, "ws") || !strcmp(Mode, "wss");
	bool Secure = !strcmp(Mode, "tls") || !strcmp(Mode, "wss");
	if (Argc < 3 || (Secure ? Argc != 7 : Argc > 5) ||
		(!WebSocket && !Secure && strcmp(Mode, "tcp")) ||
		(strcmp(Argv[1], "capture") && strcmp(Argv[1], "restore"))) {
		puts("usage: examples_server_retained_commit capture|restore slot.bin [delay-ms [tcp|ws]]");
		puts("       examples_server_retained_commit capture|restore slot.bin delay-ms tls|wss certificate.der key.der");
		return Argc == 1 ? 0 : 1;
	}
	int64 Delay = Argc >= 4 ? strtol(Argv[3], NULL, 10) : 0;
	if (Delay < 0 || Delay > 2000) return 1;
	xtlsserverconfig Tls;
	if (Secure && !exampleServerLoadTls(&Tls, Argv[5], Argv[6])) return 1;
	const xtaskpoolconfig PoolConfig = {.Threads = 1, .QueueLimit = 16};
	retainedslot Slot = {Argv[2], xrtTaskPoolCreate(&PoolConfig), Delay, !strcmp(Argv[1], "restore")};
	if (!Slot.Pool) {
		if (Secure) exampleServerReleaseTls(&Tls);
		return 1;
	}
	xmqttserverconfig Config;
	xmqttServerConfigInit(&Config);
#if defined(XMQTT_FEATURE_XMQTT_CODEC311)
	Config.Allow311 = true;
#endif
	serverexample Example = {
		.Prepare = slotPrepare, .PrepareData = &Slot, .WebSocket = WebSocket, .Tls = Secure ? &Tls : NULL
	};
	char Endpoint[80];
	bool Ready = exampleServerStart(&Example, &Config, 2) &&
		xrtNetAddrEndpointText(&Example.Address, Endpoint, sizeof(Endpoint));
	if (Ready) {
		puts(Endpoint);
		fflush(stdout);
		(void)getchar();
	}
	bool Drained = exampleServerDrain(&Example);
	bool Joined = Drained && xrtTaskPoolClose(Slot.Pool) && xrtTaskPoolWaitFor(Slot.Pool, 5000) == XWAIT_OK &&
		xrtTaskPoolDestroy(Slot.Pool);
	bool Closed = Joined && exampleServerStop(&Example);
	if (Secure && Closed) exampleServerReleaseTls(&Tls);
	return Ready && Closed && Joined ? 0 : 1;
}
