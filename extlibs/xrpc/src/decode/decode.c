#include <xrpc/decode.h>
#include <xrt/memory.h>
#include <string.h>

#if defined(XRPC_FEATURE_XRPC_DECODE)

enum { XRPC_F_VERSION = 1u, XRPC_F_ID = 2u, XRPC_F_METHOD = 4u,
	XRPC_F_PARAMS = 8u, XRPC_F_RESULT = 16u, XRPC_F_ERROR = 32u };

typedef struct xrpcdecodecontext {
	xrpcdecodeconfig Config;
	xrpcmessage* Messages;
	size_t Capacity, Count, InvalidCount, BaseDepth;
	bool RootSeen, Batch, CurrentObject, ErrorActive;
	uint32 Seen, Duplicate, ErrorSeen, ErrorDuplicate;
	bool VersionValid;
	xrpccodecerror Failure;
} xrpcdecodecontext;

static bool xrpcDecodeReport(xerrkind Kind, xrpccodecerror Code, cstr Message)
{
	xrtSetErrorInfo(Kind, "xrt.rpc", (int32)Code, Message); return false;
}

XRPC_API bool xrpcDecodeConfigInit(xrpcdecodeconfig* Config)
{
	if (Config == NULL || !xrtMemRangeValid(Config, sizeof(*Config)))
		return xrpcDecodeReport(XERR_ARGUMENT, XRPC_CODEC_ARGUMENT, "invalid RPC decode config output");
	*Config = (xrpcdecodeconfig){sizeof(*Config), XRPC_DECODE_DUPLEX, 1024u*1024u, 64,
		1024u*1024u, 65536, 65536, 256, 4096, 4096};
	return true;
}

/* Interpret an already-validated JSON string only to compare ASCII protocol
 * names/version/reserved prefix. This is not a second JSON grammar validator. */
static bool xrpcDecodeAscii(xstrview Raw, cstr Text, bool Prefix)
{
	size_t Position = 1, Index = 0;
	while (Text[Index] != 0) {
		if (Position >= Raw.Size - 1u) return false;
		unsigned Value = (unsigned char)Raw.Data[Position++];
		if (Value == '\\') {
			Value = (unsigned char)Raw.Data[Position++];
			if (Value == 'u') {
				Value = 0;
				for (unsigned Digit = 0; Digit < 4; ++Digit) {
					unsigned Byte = (unsigned char)Raw.Data[Position++];
					Value = Value * 16u + (Byte <= '9' ? Byte - '0' : (Byte <= 'F' ? Byte - 'A' + 10u : Byte - 'a' + 10u));
				}
			} else if (Value == 'b') Value = '\b';
			else if (Value == 'f') Value = '\f';
			else if (Value == 'n') Value = '\n';
			else if (Value == 'r') Value = '\r';
			else if (Value == 't') Value = '\t';
		}
		if (Value != (unsigned char)Text[Index++]) return false;
	}
	return Prefix || Position == Raw.Size - 1u;
}

static xrpcvaluekind xrpcDecodeValueKind(xjsonscantype Type)
{
	switch (Type) {
		case XJSON_SCAN_NULL: return XRPC_VALUE_NULL;
		case XJSON_SCAN_BOOL: return XRPC_VALUE_BOOLEAN;
		case XJSON_SCAN_NUMBER: return XRPC_VALUE_NUMBER;
		case XJSON_SCAN_STRING: return XRPC_VALUE_STRING;
		case XJSON_SCAN_ARRAY_BEGIN: case XJSON_SCAN_ARRAY_END: return XRPC_VALUE_ARRAY;
		case XJSON_SCAN_OBJECT_BEGIN: case XJSON_SCAN_OBJECT_END: return XRPC_VALUE_OBJECT;
		default: return XRPC_VALUE_MISSING;
	}
}

static bool xrpcDecodeEnd(xjsonscantype Type)
{
	return Type == XJSON_SCAN_ARRAY_END || Type == XJSON_SCAN_OBJECT_END;
}

static bool xrpcDecodeBegin(xjsonscantype Type)
{
	return Type == XJSON_SCAN_ARRAY_BEGIN || Type == XJSON_SCAN_OBJECT_BEGIN;
}

static uint32 xrpcDecodeField(xstrview Name)
{
	if (xrpcDecodeAscii(Name, "jsonrpc", false)) return XRPC_F_VERSION;
	if (xrpcDecodeAscii(Name, "id", false)) return XRPC_F_ID;
	if (xrpcDecodeAscii(Name, "method", false)) return XRPC_F_METHOD;
	if (xrpcDecodeAscii(Name, "params", false)) return XRPC_F_PARAMS;
	if (xrpcDecodeAscii(Name, "result", false)) return XRPC_F_RESULT;
	if (xrpcDecodeAscii(Name, "error", false)) return XRPC_F_ERROR;
	return 0;
}

static bool xrpcDecodeIntegral(xstrview Raw)
{
	size_t Position = 0, Fraction = 0, Zeros = 0, Exponent = 0;
	bool Dot = false, Nonzero = false, NegativeExponent = false;
	while (Position < Raw.Size && Raw.Data[Position] != 'e' && Raw.Data[Position] != 'E') {
		char Byte = Raw.Data[Position++];
		if (Byte == '.') Dot = true;
		else if (Byte != '-') {
			if (Dot) ++Fraction;
			if (Byte == '0') ++Zeros; else { Zeros = 0; Nonzero = true; }
		}
	}
	if (!Nonzero) return true;
	if (Position < Raw.Size) {
		++Position;
		if (Raw.Data[Position] == '+' || Raw.Data[Position] == '-') NegativeExponent = Raw.Data[Position++] == '-';
		while (Position < Raw.Size) {
			size_t Digit = (size_t)(Raw.Data[Position++] - '0');
			if (Digit > Raw.Size || Exponent > (Raw.Size - Digit) / 10u) Exponent = Raw.Size;
			else Exponent = Exponent * 10u + Digit;
		}
	}
	if (NegativeExponent) return Zeros >= Fraction && Zeros - Fraction >= Exponent;
	return Fraction <= Zeros || Exponent >= Fraction - Zeros;
}

static bool xrpcDecodeId(xrpcvaluekind Kind)
{
	return Kind == XRPC_VALUE_STRING || Kind == XRPC_VALUE_NUMBER || Kind == XRPC_VALUE_NULL;
}

static void xrpcDecodeClassify(xrpcdecodecontext* Context, xrpcmessage* Message)
{
	uint32 Relevant = XRPC_F_VERSION | XRPC_F_ID;
	bool Request = Context->Config.Mode == XRPC_DECODE_REQUESTS ||
		(Context->Config.Mode == XRPC_DECODE_DUPLEX && (Context->Seen & XRPC_F_METHOD));
	if (!Context->CurrentObject) { Message->Reason = XRPC_INVALID_OBJECT; return; }
	Relevant |= Request ? (XRPC_F_METHOD | XRPC_F_PARAMS) : (XRPC_F_RESULT | XRPC_F_ERROR);
	if (Context->Config.Mode == XRPC_DECODE_DUPLEX) Relevant = 63u;
	if (Context->Duplicate & Relevant) Message->Reason = XRPC_INVALID_DUPLICATE;
	else if (!Context->VersionValid) Message->Reason = XRPC_INVALID_VERSION;
	else if (Context->Config.Mode == XRPC_DECODE_DUPLEX && (Context->Seen & XRPC_F_METHOD) &&
		(Context->Seen & (XRPC_F_RESULT | XRPC_F_ERROR))) Message->Reason = XRPC_INVALID_AMBIGUOUS;
	else if (Message->Id.Kind != XRPC_VALUE_MISSING && !xrpcDecodeId(Message->Id.Kind)) Message->Reason = XRPC_INVALID_ID;
	else if (Request) {
		if (Message->Method.Kind != XRPC_VALUE_STRING) Message->Reason = XRPC_INVALID_METHOD;
		else if (Message->Params.Kind != XRPC_VALUE_MISSING && Message->Params.Kind != XRPC_VALUE_OBJECT &&
			Message->Params.Kind != XRPC_VALUE_ARRAY) Message->Reason = XRPC_INVALID_PARAMS;
		else {
			Message->Kind = Message->Id.Kind == XRPC_VALUE_MISSING ? XRPC_MESSAGE_NOTIFICATION : XRPC_MESSAGE_CALL;
			if (xrpcDecodeAscii(Message->Method.Raw, "rpc.", true)) Message->Flags |= XRPC_METHOD_RESERVED;
		}
	} else if (Message->Id.Kind == XRPC_VALUE_MISSING ||
		((Context->Seen & XRPC_F_RESULT) != 0) == ((Context->Seen & XRPC_F_ERROR) != 0)) Message->Reason = XRPC_INVALID_RESPONSE;
	else if (Context->Seen & XRPC_F_RESULT) Message->Kind = XRPC_MESSAGE_RESULT;
	else if (Message->Error.Kind != XRPC_VALUE_OBJECT || Context->ErrorDuplicate ||
		Message->ErrorCode.Kind != XRPC_VALUE_NUMBER || Message->ErrorMessage.Kind != XRPC_VALUE_STRING ||
		!xrpcDecodeIntegral(Message->ErrorCode.Raw)) Message->Reason = XRPC_INVALID_ERROR;
	else Message->Kind = XRPC_MESSAGE_ERROR;
}

static xjsonvisitaction xrpcDecodeFail(xrpcdecodecontext* Context, xrpccodecerror Code, cstr Message)
{
	Context->Failure = Code;
	xrpcDecodeReport(XERR_RANGE, Code, Message);
	return XJSON_VISIT_FAIL;
}

static xjsonvisitaction xrpcDecodeEvent(const xjsonscanevent* Event, ptr User)
{
	xrpcdecodecontext* Context = (xrpcdecodecontext*)User;
	bool End = xrpcDecodeEnd(Event->Type), Begin = xrpcDecodeBegin(Event->Type);
	if (!Context->RootSeen) {
		Context->RootSeen = true; Context->Batch = Event->Type == XJSON_SCAN_ARRAY_BEGIN;
		Context->BaseDepth = Context->Batch ? 1u : 0u;
	}
	if (Context->Batch && Event->Depth == 0) return XJSON_VISIT_NEXT;
	if (Event->Depth == Context->BaseDepth && !End) {
		if (Context->Count >= Context->Config.MaxMessages) return xrpcDecodeFail(Context, XRPC_CODEC_LIMIT, "RPC batch message limit exceeded");
		if (Context->Count >= Context->Capacity) return xrpcDecodeFail(Context, XRPC_CODEC_CAPACITY, "RPC message scratch capacity exceeded");
		Context->Messages[Context->Count] = (xrpcmessage){0};
		Context->CurrentObject = Event->Type == XJSON_SCAN_OBJECT_BEGIN;
		Context->Seen = Context->Duplicate = Context->ErrorSeen = Context->ErrorDuplicate = 0;
		Context->VersionValid = Context->ErrorActive = false;
	}
	xrpcmessage* Message = &Context->Messages[Context->Count];
	if (Event->Depth == Context->BaseDepth) {
		if (Begin) return XJSON_VISIT_NEXT;
		Message->Wire = Event->Raw; xrpcDecodeClassify(Context, Message);
		if (Message->Kind == XRPC_MESSAGE_INVALID) ++Context->InvalidCount;
		++Context->Count; return XJSON_VISIT_NEXT;
	}
	if (!Context->CurrentObject || !Event->HasName) return XJSON_VISIT_NEXT;
	if (Event->Depth == Context->BaseDepth + 1u) {
		uint32 Field = xrpcDecodeField(Event->Name);
		if (Field == 0) return XJSON_VISIT_NEXT;
		if (!End) { Context->Duplicate |= Context->Seen & Field; Context->Seen |= Field; }
		if (Field == XRPC_F_VERSION) {
			if (!End) Context->VersionValid = Event->Type == XJSON_SCAN_STRING && xrpcDecodeAscii(Event->Raw, "2.0", false);
			return XJSON_VISIT_NEXT;
		}
		xrpcvalue* Value = Field == XRPC_F_ID ? &Message->Id : Field == XRPC_F_METHOD ? &Message->Method :
			Field == XRPC_F_PARAMS ? &Message->Params : Field == XRPC_F_RESULT ? &Message->Result : &Message->Error;
		*Value = (xrpcvalue){xrpcDecodeValueKind(Event->Type), Event->Raw};
		if (Field == XRPC_F_ERROR) {
			Context->ErrorActive = Event->Type == XJSON_SCAN_OBJECT_BEGIN;
			if (!End) { Context->ErrorSeen = Context->ErrorDuplicate = 0; Message->ErrorCode = Message->ErrorMessage = Message->ErrorData = (xrpcvalue){0}; }
		}
		if (!Begin && ((Field == XRPC_F_ID && Value->Raw.Size > Context->Config.MaxIdBytes) ||
			(Field == XRPC_F_METHOD && Context->Config.Mode != XRPC_DECODE_RESPONSES && Value->Raw.Size > Context->Config.MaxMethodBytes)))
			return xrpcDecodeFail(Context, XRPC_CODEC_LIMIT, "RPC id or method token limit exceeded");
	} else if (Context->ErrorActive && Event->Depth == Context->BaseDepth + 2u) {
		uint32 Field = xrpcDecodeAscii(Event->Name, "code", false) ? 1u : xrpcDecodeAscii(Event->Name, "message", false) ? 2u :
			xrpcDecodeAscii(Event->Name, "data", false) ? 4u : 0u;
		if (!Field) return XJSON_VISIT_NEXT;
		if (!End) { Context->ErrorDuplicate |= Context->ErrorSeen & Field; Context->ErrorSeen |= Field; }
		xrpcvalue* Value = Field == 1u ? &Message->ErrorCode : Field == 2u ? &Message->ErrorMessage : &Message->ErrorData;
		*Value = (xrpcvalue){xrpcDecodeValueKind(Event->Type), Event->Raw};
	}
	return XJSON_VISIT_NEXT;
}

static bool xrpcDecodeConfigValid(const xrpcdecodeconfig* Config)
{
	return Config->Size == sizeof(*Config) && Config->Mode >= XRPC_DECODE_REQUESTS && Config->Mode <= XRPC_DECODE_DUPLEX &&
		Config->MaxInputBytes && Config->MaxDepth && Config->MaxDepth <= XJSON_DEPTH_DEFAULT && Config->MaxStringBytes &&
		Config->MaxValues && Config->MaxContainerItems && Config->MaxMessages && Config->MaxIdBytes && Config->MaxMethodBytes;
}

XRPC_API xrpcdecodestatus xrpcDecode(xstrview Input, const xrpcdecodeconfig* Config,
	xrpcmessage* Messages, size_t Capacity, xrpcdocument* Document)
{
	xrpcdecodecontext Context = {0};
	size_t Storage;
	if (Messages == NULL || Document == NULL || Capacity == 0 || Capacity > SIZE_MAX / sizeof(*Messages) ||
		!xrtMemRangeValid(Messages, Capacity * sizeof(*Messages)) || !xrtMemRangeValid(Document, sizeof(*Document)) ||
		(Input.Data == NULL && Input.Size != 0) || !xrtMemRangeValid(Input.Data, Input.Size) ||
		(Config != NULL && !xrtMemRangeValid(Config, sizeof(*Config)))) {
		xrpcDecodeReport(XERR_ARGUMENT, XRPC_CODEC_ARGUMENT, "invalid RPC decode input or output span"); return XRPC_DECODE_LOCAL_ERROR;
	}
	Storage = Capacity * sizeof(*Messages);
	if (xrtMemRangesOverlap(Messages, Storage, Document, sizeof(*Document)) || xrtMemRangesOverlap(Messages, Storage, Input.Data, Input.Size) ||
		xrtMemRangesOverlap(Document, sizeof(*Document), Input.Data, Input.Size) ||
		(Config != NULL && (xrtMemRangesOverlap(Config, sizeof(*Config), Messages, Storage) ||
		 xrtMemRangesOverlap(Config, sizeof(*Config), Document, sizeof(*Document))))) {
		xrpcDecodeReport(XERR_ARGUMENT, XRPC_CODEC_ARGUMENT, "RPC decode inputs and outputs overlap"); return XRPC_DECODE_LOCAL_ERROR;
	}
	if (Config == NULL) xrpcDecodeConfigInit(&Context.Config); else Context.Config = *Config;
	if (!xrpcDecodeConfigValid(&Context.Config)) {
		xrpcDecodeReport(XERR_ARGUMENT, XRPC_CODEC_ARGUMENT, "invalid RPC decode configuration"); return XRPC_DECODE_LOCAL_ERROR;
	}
	if (Input.Size > Context.Config.MaxInputBytes) {
		xrpcDecodeReport(XERR_RANGE, XRPC_CODEC_LIMIT, "RPC input exceeds configured limit"); return XRPC_DECODE_LOCAL_ERROR;
	}
	Context.Messages = Messages; Context.Capacity = Capacity;
	xjsonreadconfig Json; xrtJsonReadConfigInit(&Json);
	Json.MaxInputBytes = Context.Config.MaxInputBytes; Json.MaxDepth = Context.Config.MaxDepth;
	Json.MaxStringBytes = Context.Config.MaxStringBytes; Json.MaxValues = Context.Config.MaxValues;
	Json.MaxContainerItems = Context.Config.MaxContainerItems;
	if (xrtJsonScan(Input, &Json, xrpcDecodeEvent, &Context) != XJSON_VISIT_DONE) {
		if (Context.Failure) return XRPC_DECODE_LOCAL_ERROR;
		const xerror* Error = xrtGetError();
		if (xrtErrorKind(Error) == XERR_MEMORY) return XRPC_DECODE_LOCAL_ERROR;
		if (xrtErrorDomain(Error) != NULL && strcmp(xrtErrorDomain(Error), "xrt.json") == 0 && xrtErrorCode(Error) == XJSON_ERROR_LIMIT) {
			xrpcDecodeReport(XERR_RANGE, XRPC_CODEC_LIMIT, "RPC JSON resource limit exceeded"); return XRPC_DECODE_LOCAL_ERROR;
		}
		if (xrtErrorDomain(Error) != NULL && strcmp(xrtErrorDomain(Error), "xrt.json") == 0 && xrtErrorCode(Error) == XJSON_ERROR_SYNTAX) {
			*Document = (xrpcdocument){0}; return XRPC_DECODE_PARSE_ERROR;
		}
		return XRPC_DECODE_LOCAL_ERROR;
	}
	if (Context.Batch && Context.Count == 0) {
		Messages[0] = (xrpcmessage){0}; Messages[0].Reason = XRPC_INVALID_EMPTY_BATCH;
		/* Raw [] may contain whitespace. No business data is copied. */
		Messages[0].Wire = Input; Context.Count = Context.InvalidCount = 1; Context.Batch = false;
	}
	*Document = (xrpcdocument){Context.Batch, Context.Count, Context.InvalidCount};
	return XRPC_DECODE_DONE;
}
#endif
