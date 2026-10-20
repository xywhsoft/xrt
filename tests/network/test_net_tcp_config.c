/* Offline configuration checks only: no Engine, Socket, DNS or packets. */
#include "../test.h"

static void streamConfig(void)
{
	xnetstreamconfig Value, Saved;
	xrtNetStreamConfigInit(&Value);
	Saved = Value;
	testRequire(xrtNetStreamConfigValid(&Value), "stream defaults invalid");
	testRequire(memcmp(&Value, &Saved, sizeof(Value)) == 0, "stream validation mutated input");
	Value.ReadSize = 0;
	testRequire(!xrtNetStreamConfigValid(&Value), "zero read size accepted");
	xrtClearError();
	Value = Saved;
	Value.ReadLimit = Value.ReadSize - 1;
	testRequire(!xrtNetStreamConfigValid(&Value), "small read limit accepted");
	xrtClearError();
	Value = Saved;
	Value.WriteLowWater = Value.WriteHighWater + 1;
	testRequire(!xrtNetStreamConfigValid(&Value), "inverted watermarks accepted");
	xrtClearError();
	Value = Saved;
	Value.WriteHighWater = Value.WriteLimit + 1;
	testRequire(!xrtNetStreamConfigValid(&Value), "watermark beyond limit accepted");
	xrtClearError();
	Value = Saved;
	Value.ConnectTimeout = -2;
	testRequire(!xrtNetStreamConfigValid(&Value), "invalid timeout accepted");
	xrtClearError();
	Value = Saved;
	Value.ReadMode = (xnetstreamreadmode)-1;
	testRequire(!xrtNetStreamConfigValid(&Value), "invalid read mode accepted");
	xrtClearError();
	Value = Saved;
	Value.ConnectTimeout = -1;
	testRequire(xrtNetStreamConfigValid(&Value), "infinite timeout rejected");
	Value.ConnectTimeout = 0;
	testRequire(xrtNetStreamConfigValid(&Value), "zero timeout rejected");
	testRequire(!xrtNetStreamConfigValid(NULL), "null stream accepted");
	xrtClearError();
}

static void listenConfig(void)
{
	xnetlistenconfig Value, Saved;
	xrtNetListenConfigInit(&Value);
	Saved = Value;
	testRequire(xrtNetListenConfigValid(&Value), "listener defaults invalid");
	testRequire(memcmp(&Value, &Saved, sizeof(Value)) == 0, "listener validation mutated input");
	Value.AcceptConcurrency = 0;
	testRequire(!xrtNetListenConfigValid(&Value), "zero accept concurrency accepted");
	xrtClearError();
	Value = Saved;
	Value.AcceptConcurrency = 1025;
	testRequire(!xrtNetListenConfigValid(&Value), "excess accept concurrency accepted");
	xrtClearError();
	Value = Saved;
	Value.AcceptQueueLimit = 0;
	testRequire(!xrtNetListenConfigValid(&Value), "zero accept queue accepted");
	xrtClearError();
	Value = Saved;
	Value.Backlog = 0;
	testRequire(!xrtNetListenConfigValid(&Value), "zero backlog accepted");
	xrtClearError();
	Value = Saved;
	Value.ExclusiveAddress = true;
	Value.ReuseAddress = true;
	testRequire(!xrtNetListenConfigValid(&Value), "exclusive reuse accepted");
	xrtClearError();
	Value = Saved;
	Value.Stream.ReadSize = 0;
	testRequire(!xrtNetListenConfigValid(&Value), "invalid nested stream accepted");
	xrtClearError();
	testRequire(!xrtNetListenConfigValid(NULL), "null listener accepted");
	xrtClearError();
}

int main(void)
{
	streamConfig();
	listenConfig();
	puts("[PASS] offline TCP configuration validation without resource creation");
	return 0;
}
