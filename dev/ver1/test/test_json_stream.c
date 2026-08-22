#include "../xrt.h"

#include <stdio.h>
#include <string.h>


typedef struct {
	int count;
	int saw_name;
	int saw_array;
	int stop_after;
} JsonVisitState;


static bool JsonVisitProc(const xrt_json_event* event, void* userdata)
{
	JsonVisitState* state = (JsonVisitState*)userdata;

	if ( event == NULL || state == NULL ) {
		return FALSE;
	}
	++state->count;
	if ( event->type == XRT_JSON_EVENT_STRING && event->key_size == 4 &&
		memcmp(event->key, "name", 4) == 0 && event->text_size == 5 &&
		memcmp(event->text, "xlang", 5) == 0 ) {
		state->saw_name = 1;
	}
	if ( event->type == XRT_JSON_EVENT_ARRAY_BEGIN && event->key_size == 5 &&
		memcmp(event->key, "items", 5) == 0 && event->depth == 1 ) {
		state->saw_array = 1;
	}
	return state->stop_after <= 0 || state->count < state->stop_after;
}


static int Require(int condition, const char* message)
{
	if ( condition ) {
		return 1;
	}
	fprintf(stderr, "[json-stream] %s\n", message);
	return 0;
}


int main(void)
{
	const char* source = "{\"name\":\"xlang\",\"items\":[1,true,null]}";
	JsonVisitState state = { 0 };
	JsonVisitState stopped = { 0 };
	xjsonwriter writer;
	xjsonwriter bad;
	xson_options options;
	xvalue parsed;
	char* text;
	size_t size = 0;
	int ok = 1;

	if ( xrtInit() == NULL ) {
		return 1;
	}

	ok &= Require(xrtJsonVisit(source, 0, JsonVisitProc, &state) == 0,
		"complete visit failed");
	ok &= Require(state.count == 8 && state.saw_name && state.saw_array,
		"event sequence mismatch");
	stopped.stop_after = 3;
	ok &= Require(xrtJsonVisit(source, 0, JsonVisitProc, &stopped) == 1 && stopped.count == 3,
		"early stop mismatch");
	ok &= Require(xrtJsonVisit("{bad}", 0, JsonVisitProc, &state) == -1,
		"invalid input should fail");

	writer = xrtJsonWriterCreate(FALSE);
	ok &= Require(writer != NULL, "writer create failed");
	ok &= Require(xrtJsonWriterBeginObject(writer), "begin object failed");
	ok &= Require(xrtJsonWriterStringKey(writer, "name", "xlang"), "write string failed");
	ok &= Require(xrtJsonWriterBeginArrayKey(writer, "items"), "begin array failed");
	ok &= Require(xrtJsonWriterInt(writer, 1), "write int failed");
	ok &= Require(xrtJsonWriterBool(writer, TRUE), "write bool failed");
	ok &= Require(xrtJsonWriterNull(writer), "write null failed");
	ok &= Require(xrtJsonWriterEndArray(writer), "end array failed");
	ok &= Require(xrtJsonWriterBeginObjectKey(writer, "nested"), "begin nested object failed");
	ok &= Require(xrtJsonWriterFloatKey(writer, "value", 2.5), "write float failed");
	ok &= Require(xrtJsonWriterEndObject(writer), "end nested object failed");
	ok &= Require(xrtJsonWriterEndObject(writer), "end object failed");
	text = xrtJsonWriterFinish(writer, &size);
	ok &= Require(text != NULL && size == strlen(text) &&
		strcmp(text, "{\"name\":\"xlang\",\"items\":[1,true,null],\"nested\":{\"value\":2.5}}") == 0,
		"writer output mismatch");
	xrtFree(text);
	xrtJsonWriterDestroy(writer);

	bad = xrtJsonWriterCreate(FALSE);
	ok &= Require(xrtJsonWriterBeginArray(bad), "bad writer begin failed");
	ok &= Require(!xrtJsonWriterIntKey(bad, "forbidden", 1), "array key should fail");
	ok &= Require(xrtJsonWriterFinish(bad, NULL) == NULL, "failed writer should not finish");
	xrtJsonWriterDestroy(bad);

	options = xrtXsonOptionsDefault();
	options.ignore_unsupported_decode = TRUE;
	parsed = xrtParseXSONWithOptions("{\"keep\":1,\"skip\":ptr(123),\"after\":2}", 0, &options);
	ok &= Require(xvoType(parsed) == XVO_DT_TABLE &&
		xvoTableGetInt(parsed, "keep", 0) == 1 &&
		xvoTableGetInt(parsed, "after", 0) == 2 &&
		!xvoTableExists(parsed, (str)"skip", 0), "xson options decode mismatch");
	options.pretty = TRUE;
	text = xrtStringifyXSONWithOptions(parsed, &options, &size);
	ok &= Require(text != NULL && size > 0 && strchr(text, '\n') != NULL,
		"xson options stringify mismatch");
	xrtFree(text);
	xvoUnref(parsed);

	xrtUnit();
	return ok ? 0 : 1;
}
