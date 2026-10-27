#include <xredis/reader.h>
#include <xrt/memory.h>
#include <string.h>

#if defined(XREDIS_FEATURE_XREDIS_READER)

typedef enum xredisreaderstate {
	XREDIS_RS_TYPE, XREDIS_RS_BEGIN_LINE, XREDIS_RS_LINE, XREDIS_RS_LINE_LF,
	XREDIS_RS_HEADER, XREDIS_RS_HEADER_LF, XREDIS_RS_BEGIN_VALUE,
	XREDIS_RS_BULK, XREDIS_RS_BULK_CR, XREDIS_RS_BULK_LF, XREDIS_RS_CHUNK,
	XREDIS_RS_END, XREDIS_RS_FINISH_VALUE, XREDIS_RS_END_CONTAINER,
	XREDIS_RS_TERM_CR, XREDIS_RS_TERM_LF, XREDIS_RS_COMPLETE, XREDIS_RS_FAILED
} xredisreaderstate;

typedef enum xredisreaderfloatstate {
	XREDIS_RF_START, XREDIS_RF_SIGN, XREDIS_RF_INTEGER, XREDIS_RF_DOT,
	XREDIS_RF_FRACTION, XREDIS_RF_EXP, XREDIS_RF_EXP_SIGN, XREDIS_RF_EXP_DIGITS,
	XREDIS_RF_I, XREDIS_RF_IN, XREDIS_RF_INF,
	XREDIS_RF_N, XREDIS_RF_NA, XREDIS_RF_NAN, XREDIS_RF_NAN_PAYLOAD,
	XREDIS_RF_NAN_DONE
} xredisreaderfloatstate;

typedef struct xredisreaderlevel {
	uint64 Remaining;
	uint64 Seen;
	xredisreplytype Type;
	uint32 Flags;
	bool Attributes;
} xredisreaderlevel;

typedef struct xredisreaderparse {
	xredisreaderstate State;
	xredisreadevent Current;
	size_t Depth;
	uint64 StreamBytes, FrameBytes, FrameValues;
	uint64 LastFrameBytes, LastFrameValues, CompletedFrames;
	uint64 HeaderValue, HeaderBytes, HeaderDigits;
	uint64 LineBytes, Digits, Number, Remaining, PayloadBytes;
	xredisreaderfloatstate FloatState;
	bool Negative, Plus, HeaderNegative, HeaderUnknown, ChunkHeader;
	bool FrameActive, RootAttributes, Paused;
	xredisreadererror Error;
	xerrkind ErrorKind;
	cstr ErrorMessage;
	uint64 ErrorOffset;
} xredisreaderparse;

struct xredisreader {
	xredisreaderconfig Config;
	xredisreadproc Callback;
	ptr User;
	size_t StorageBytes;
	bool Active;
	xredisreaderparse Parse;
	xredisreaderlevel Stack[];
};

static bool xredisReaderReport(xerrkind Kind, xredisreadererror Code, cstr Message)
{
	xrtSetErrorInfo(Kind, "xrt.redis", (int32)Code, Message);
	return false;
}

static bool xredisReaderFail(xredisreader* Reader, xerrkind Kind,
	xredisreadererror Code, cstr Message)
{
	xredisreaderparse* P = &Reader->Parse;
	P->State = XREDIS_RS_FAILED; P->Error = Code; P->ErrorKind = Kind;
	P->ErrorMessage = Message; P->ErrorOffset = P->StreamBytes;
	return xredisReaderReport(Kind, Code, Message);
}

static bool xredisReaderIdle(xredisreader* Reader)
{
	if (Reader == NULL)
		return xredisReaderReport(XERR_ARGUMENT, XREDIS_READER_ARGUMENT, "reader is NULL");
	if (Reader->Active)
		return xredisReaderReport(XERR_STATE, XREDIS_READER_STATE, "reader callback cannot reenter mutation");
	return true;
}

XREDIS_API bool xredisReaderConfigInit(xredisreaderconfig* Config)
{
	if (Config == NULL || !xrtMemRangeValid(Config, sizeof(*Config)))
		return xredisReaderReport(XERR_ARGUMENT, XREDIS_READER_ARGUMENT, "invalid reader config output");
	*Config = (xredisreaderconfig){sizeof(*Config), XREDIS_RESP3, 0, 64,
		64u * 1024u * 1024u, 16u * 1024u * 1024u, 64u * 1024u,
		1000000u, 1000000u, 64u * 1024u};
	return true;
}

XREDIS_API bool xredisReaderCreate(const xredisreaderconfig* Config,
	xredisreadproc Callback, ptr User, xredisreader** Result)
{
	xredisreaderconfig Copy;
	xredisreader* Reader;
	size_t StorageBytes;
	if (Callback == NULL || Result == NULL || !xrtMemRangeValid(Result, sizeof(*Result)) ||
		(Config != NULL && (!xrtMemRangeValid(Config, sizeof(*Config)) ||
		 xrtMemRangesOverlap(Config, sizeof(*Config), Result, sizeof(*Result)))))
		return xredisReaderReport(XERR_ARGUMENT, XREDIS_READER_ARGUMENT, "invalid reader creation arguments");
	if (Config == NULL) xredisReaderConfigInit(&Copy);
	else Copy = *Config;
	if (Copy.Size != sizeof(Copy) || (Copy.Protocol != XREDIS_RESP2 && Copy.Protocol != XREDIS_RESP3) ||
		(Copy.Flags & ~XREDIS_READER_LEGACY_NAN) != 0 || Copy.MaxFrameBytes == 0 ||
		Copy.MaxValues == 0 || Copy.MaxFeedBytes == 0 || Copy.MaxFrameBytes > (uint64)INT64_MAX ||
		Copy.MaxBulkBytes > (uint64)INT64_MAX || Copy.MaxLineBytes > (uint64)INT64_MAX ||
		Copy.MaxContainerItems > (uint64)INT64_MAX || Copy.MaxValues > (uint64)INT64_MAX ||
		Copy.MaxDepth > (SIZE_MAX - sizeof(*Reader)) / sizeof(xredisreaderlevel))
		return xredisReaderReport(XERR_ARGUMENT, XREDIS_READER_ARGUMENT, "invalid reader config size, protocol or limits");
	StorageBytes = sizeof(*Reader) + Copy.MaxDepth * sizeof(xredisreaderlevel);
	Reader = (xredisreader*)xrtCalloc(1, StorageBytes);
	if (Reader == NULL) return false;
	Reader->Config = Copy; Reader->Callback = Callback; Reader->User = User;
	Reader->StorageBytes = StorageBytes; Reader->Parse.State = XREDIS_RS_TYPE;
	*Result = Reader;
	return true;
}

XREDIS_API bool xredisReaderDestroy(xredisreader* Reader)
{
	if (Reader == NULL) return true;
	if (!xredisReaderIdle(Reader)) return false;
	xrtFree(Reader);
	return true;
}

XREDIS_API bool xredisReaderReset(xredisreader* Reader)
{
	if (!xredisReaderIdle(Reader)) return false;
	memset(&Reader->Parse, 0, sizeof(Reader->Parse));
	Reader->Parse.State = XREDIS_RS_TYPE;
	return true;
}

XREDIS_API bool xredisReaderInfo(const xredisreader* Reader, xredisreaderinfo* Info)
{
	if (Reader == NULL || Info == NULL || !xrtMemRangeValid(Info, sizeof(*Info)) ||
		xrtMemRangesOverlap(Reader, Reader->StorageBytes, Info, sizeof(*Info)) || Info->Size != sizeof(*Info))
		return xredisReaderReport(XERR_ARGUMENT, XREDIS_READER_ARGUMENT, "invalid reader info output or size");
	const xredisreaderparse* P = &Reader->Parse;
	*Info = (xredisreaderinfo){sizeof(*Info), P->StreamBytes, P->FrameBytes, P->FrameValues,
		P->LastFrameBytes, P->LastFrameValues, P->CompletedFrames, P->Depth,
		P->Paused, P->Error, P->ErrorOffset};
	return true;
}

static bool xredisReaderAggregate(xredisreplytype Type)
{
	return Type == XREDIS_REPLY_ARRAY || Type == XREDIS_REPLY_MAP ||
		Type == XREDIS_REPLY_ATTRIBUTE || Type == XREDIS_REPLY_SET || Type == XREDIS_REPLY_PUSH;
}

static bool xredisReaderPairs(xredisreplytype Type)
{
	return Type == XREDIS_REPLY_MAP || Type == XREDIS_REPLY_ATTRIBUTE;
}

static bool xredisReaderRoom(xredisreader* Reader)
{
	if (Reader->Parse.FrameBytes >= Reader->Config.MaxFrameBytes || Reader->Parse.StreamBytes == UINT64_MAX)
		return xredisReaderFail(Reader, XERR_RANGE, XREDIS_READER_LIMIT, "reader wire byte limit exceeded");
	return true;
}

static void xredisReaderAdvance(xredisreader* Reader, size_t* Index, size_t Count)
{
	Reader->Parse.StreamBytes += Count; Reader->Parse.FrameBytes += Count;
	*Index += Count;
}

static bool xredisReaderEmit(xredisreader* Reader, xredisreadkind Kind, xbytesview Data)
{
	xredisreadevent Event = Reader->Parse.Current;
	Event.Kind = Kind; Event.Data = Data;
	xredisreadaction Action = Reader->Callback(&Event, Reader->User);
	if (Action == XREDIS_READ_NEXT) return true;
	if (Action == XREDIS_READ_PAUSE) { Reader->Parse.Paused = true; return false; }
	return xredisReaderFail(Reader, XERR_CANCELLED, XREDIS_READER_CALLBACK,
		Action == XREDIS_READ_FAIL ? "reader callback rejected the frame" : "invalid reader callback action");
}

static void xredisReaderHeaderStart(xredisreaderparse* P, bool Chunk)
{
	P->HeaderValue = 0; P->HeaderBytes = 0; P->HeaderDigits = 0;
	P->HeaderNegative = false; P->HeaderUnknown = false;
	P->ChunkHeader = Chunk; P->State = XREDIS_RS_HEADER;
}

static bool xredisReaderType(xredisreader* Reader, unsigned char Byte)
{
	xredisreaderparse* P = &Reader->Parse;
	xredisreplytype Type;
	switch (Byte) {
		case '+': Type = XREDIS_REPLY_SIMPLE_STRING; break;
		case '-': Type = XREDIS_REPLY_SIMPLE_ERROR; break;
		case ':': Type = XREDIS_REPLY_INTEGER; break;
		case '$': Type = XREDIS_REPLY_BULK_STRING; break;
		case '*': Type = XREDIS_REPLY_ARRAY; break;
		case '_': Type = XREDIS_REPLY_NULL; break;
		case '#': Type = XREDIS_REPLY_BOOLEAN; break;
		case ',': Type = XREDIS_REPLY_DOUBLE; break;
		case '(': Type = XREDIS_REPLY_BIG_NUMBER; break;
		case '!': Type = XREDIS_REPLY_BULK_ERROR; break;
		case '=': Type = XREDIS_REPLY_VERBATIM; break;
		case '%': Type = XREDIS_REPLY_MAP; break;
		case '|': Type = XREDIS_REPLY_ATTRIBUTE; break;
		case '~': Type = XREDIS_REPLY_SET; break;
		case '>': Type = XREDIS_REPLY_PUSH; break;
		default: return xredisReaderFail(Reader, XERR_PROTOCOL, XREDIS_READER_PROTOCOL, "unknown RESP type prefix");
	}
	if (Reader->Config.Protocol == XREDIS_RESP2 && Type > XREDIS_REPLY_ARRAY)
		return xredisReaderFail(Reader, XERR_PROTOCOL, XREDIS_READER_PROTOCOL, "RESP3 type in RESP2 mode");
	if (Type == XREDIS_REPLY_PUSH && P->Depth != 0)
		return xredisReaderFail(Reader, XERR_PROTOCOL, XREDIS_READER_PROTOCOL, "push must be top-level");
	if (P->Depth != 0 && Type != XREDIS_REPLY_ATTRIBUTE) {
		xredisreaderlevel* Parent = &Reader->Stack[P->Depth - 1u];
		if (Parent->Type == XREDIS_REPLY_PUSH && Parent->Seen == 0 &&
			Type != XREDIS_REPLY_SIMPLE_STRING && Type != XREDIS_REPLY_BULK_STRING)
			return xredisReaderFail(Reader, XERR_PROTOCOL, XREDIS_READER_PROTOCOL, "push kind must be a string");
		if ((Parent->Flags & XREDIS_REPLY_STREAMED) &&
			(xredisReaderPairs(Parent->Type) ? Parent->Seen / 2u : Parent->Seen) >= Reader->Config.MaxContainerItems)
			return xredisReaderFail(Reader, XERR_RANGE, XREDIS_READER_LIMIT, "streamed aggregate item limit exceeded");
	}
	if (P->FrameValues >= Reader->Config.MaxValues)
		return xredisReaderFail(Reader, XERR_RANGE, XREDIS_READER_LIMIT, "frame value limit exceeded");
	++P->FrameValues; P->FrameActive = true;
	P->Current = (xredisreadevent){0}; P->Current.Type = Type;
	P->Current.Depth = P->Depth; P->Current.Length = XREDIS_LENGTH_UNKNOWN;
	P->LineBytes = 0; P->Digits = 0; P->Number = 0; P->PayloadBytes = 0;
	P->Negative = false; P->Plus = false; P->FloatState = XREDIS_RF_START;
	if (Type == XREDIS_REPLY_NULL) { P->Current.Flags = XREDIS_REPLY_NULL_VALUE; P->Current.Length = 0; }
	if (Type == XREDIS_REPLY_BULK_STRING || Type == XREDIS_REPLY_BULK_ERROR ||
		Type == XREDIS_REPLY_VERBATIM || xredisReaderAggregate(Type)) xredisReaderHeaderStart(P, false);
	else P->State = XREDIS_RS_BEGIN_LINE;
	return true;
}

static bool xredisReaderHeaderByte(xredisreader* Reader, unsigned char Byte)
{
	xredisreaderparse* P = &Reader->Parse;
	if (P->HeaderBytes >= Reader->Config.MaxLineBytes)
		return xredisReaderFail(Reader, XERR_RANGE, XREDIS_READER_LIMIT, "RESP header line limit exceeded");
	if (P->HeaderBytes == 0 && Byte == '-') P->HeaderNegative = true;
	else if (P->HeaderBytes == 0 && Byte == '?') P->HeaderUnknown = true;
	else if (Byte >= '0' && Byte <= '9' && !P->HeaderUnknown) {
		uint64 Digit = (uint64)(Byte - '0');
		if (P->HeaderValue > ((uint64)INT64_MAX - Digit) / 10u)
			return xredisReaderFail(Reader, XERR_PROTOCOL, XREDIS_READER_PROTOCOL, "RESP length exceeds int64");
		P->HeaderValue = P->HeaderValue * 10u + Digit; ++P->HeaderDigits;
	} else return xredisReaderFail(Reader, XERR_PROTOCOL, XREDIS_READER_PROTOCOL, "invalid RESP length token");
	++P->HeaderBytes;
	return true;
}

static bool xredisReaderBulkRoom(xredisreader* Reader, uint64 Bytes)
{
	xredisreaderparse* P = &Reader->Parse;
	uint64 Room = Reader->Config.MaxFrameBytes - P->FrameBytes;
	if (Bytes > Reader->Config.MaxBulkBytes - P->PayloadBytes || Room < 2u || Bytes > Room - 2u)
		return xredisReaderFail(Reader, XERR_RANGE, XREDIS_READER_LIMIT, "bulk payload or frame limit exceeded");
	return true;
}

static bool xredisReaderHeaderEnd(xredisreader* Reader)
{
	xredisreaderparse* P = &Reader->Parse;
	xredisreplytype Type = P->Current.Type;
	uint64 Length = P->HeaderValue;
	if (P->HeaderUnknown) {
		if (P->HeaderBytes != 1u || P->ChunkHeader || Reader->Config.Protocol != XREDIS_RESP3 ||
			(Type != XREDIS_REPLY_BULK_STRING && Type != XREDIS_REPLY_ARRAY &&
			 Type != XREDIS_REPLY_MAP && Type != XREDIS_REPLY_SET))
			return xredisReaderFail(Reader, XERR_PROTOCOL, XREDIS_READER_PROTOCOL, "streaming is invalid for this RESP type");
		P->Current.Flags = XREDIS_REPLY_STREAMED; P->Current.Length = XREDIS_LENGTH_UNKNOWN;
	} else {
		if (P->HeaderDigits == 0)
			return xredisReaderFail(Reader, XERR_PROTOCOL, XREDIS_READER_PROTOCOL, "missing RESP length digits");
		if (P->HeaderNegative) {
			if (P->HeaderBytes != 2u || Length != 1u || P->ChunkHeader ||
				(Type != XREDIS_REPLY_BULK_STRING && Type != XREDIS_REPLY_ARRAY))
				return xredisReaderFail(Reader, XERR_PROTOCOL, XREDIS_READER_PROTOCOL, "invalid negative RESP length");
			if (P->Depth != 0 && Reader->Stack[P->Depth - 1u].Type == XREDIS_REPLY_PUSH &&
				Reader->Stack[P->Depth - 1u].Seen == 0)
				return xredisReaderFail(Reader, XERR_PROTOCOL, XREDIS_READER_PROTOCOL, "push kind cannot be null");
			P->Current.Flags = XREDIS_REPLY_NULL_VALUE; P->Current.Length = 0;
		} else if (P->ChunkHeader) {
			if (Length != 0 && !xredisReaderBulkRoom(Reader, Length)) return false;
			P->Remaining = Length; P->State = Length == 0 ? XREDIS_RS_END : XREDIS_RS_BULK;
			return true;
		} else {
			P->Current.Length = Length;
			if (xredisReaderAggregate(Type)) {
				uint64 Slots = xredisReaderPairs(Type) ? Length * 2u : Length;
				if (Length > Reader->Config.MaxContainerItems || Slots > Reader->Config.MaxValues - P->FrameValues)
					return xredisReaderFail(Reader, XERR_RANGE, XREDIS_READER_LIMIT, "declared aggregate exceeds item or value limit");
				if (Type == XREDIS_REPLY_PUSH && Length == 0)
					return xredisReaderFail(Reader, XERR_PROTOCOL, XREDIS_READER_PROTOCOL, "push requires a kind string");
			} else if (!xredisReaderBulkRoom(Reader, Length)) return false;
			if (Type == XREDIS_REPLY_VERBATIM && Length < 4u)
				return xredisReaderFail(Reader, XERR_PROTOCOL, XREDIS_READER_PROTOCOL, "verbatim payload requires a format prefix");
		}
	}
	if (xredisReaderAggregate(Type) && !(P->Current.Flags & XREDIS_REPLY_NULL_VALUE) && P->Depth >= Reader->Config.MaxDepth)
		return xredisReaderFail(Reader, XERR_RANGE, XREDIS_READER_LIMIT, "aggregate nesting limit exceeded");
	P->State = XREDIS_RS_BEGIN_VALUE;
	return true;
}

/* An incremental grammar, independent of locale/libc strtod and without a
 * token buffer. Conversion/rounding is intentionally left to a reply builder. */
static bool xredisReaderFloatByte(xredisreader* Reader, unsigned char Byte)
{
	xredisreaderparse* P = &Reader->Parse;
	bool Digit = Byte >= '0' && Byte <= '9';
	bool Legacy = (Reader->Config.Flags & XREDIS_READER_LEGACY_NAN) != 0;
	unsigned char Special = Legacy && Byte >= 'A' && Byte <= 'Z' ? (unsigned char)(Byte + ('a' - 'A')) : Byte;
	switch (P->FloatState) {
		case XREDIS_RF_START:
			if (Byte == '+' || Byte == '-') {
				P->Negative = Byte == '-'; P->Plus = Byte == '+'; P->FloatState = XREDIS_RF_SIGN; return true;
			}
			/* fall through */
		case XREDIS_RF_SIGN:
			if (Digit) P->FloatState = XREDIS_RF_INTEGER;
			else if (Byte == 'i' && !P->Plus) P->FloatState = XREDIS_RF_I;
			else if (Special == 'n' && (Legacy || (!P->Negative && !P->Plus))) P->FloatState = XREDIS_RF_N;
			else return false;
			return true;
		case XREDIS_RF_INTEGER:
			if (Digit) return true;
			if (Byte == '.') { P->FloatState = XREDIS_RF_DOT; return true; }
			if (Byte == 'e' || Byte == 'E') { P->FloatState = XREDIS_RF_EXP; return true; }
			return false;
		case XREDIS_RF_DOT:
			if (!Digit) return false;
			P->FloatState = XREDIS_RF_FRACTION; return true;
		case XREDIS_RF_FRACTION:
			if (Digit) return true;
			if (Byte == 'e' || Byte == 'E') { P->FloatState = XREDIS_RF_EXP; return true; }
			return false;
		case XREDIS_RF_EXP:
			if (Byte == '+' || Byte == '-') { P->FloatState = XREDIS_RF_EXP_SIGN; return true; }
			/* fall through */
		case XREDIS_RF_EXP_SIGN:
			if (!Digit) return false;
			P->FloatState = XREDIS_RF_EXP_DIGITS; return true;
		case XREDIS_RF_EXP_DIGITS: return Digit;
		case XREDIS_RF_I: if (Byte != 'n') return false; P->FloatState = XREDIS_RF_IN; return true;
		case XREDIS_RF_IN: if (Byte != 'f') return false; P->FloatState = XREDIS_RF_INF; return true;
		case XREDIS_RF_N: if (Special != 'a') return false; P->FloatState = XREDIS_RF_NA; return true;
		case XREDIS_RF_NA: if (Special != 'n') return false; P->FloatState = XREDIS_RF_NAN; return true;
		case XREDIS_RF_NAN:
			if (Byte != '(' || !Legacy) return false;
			P->FloatState = XREDIS_RF_NAN_PAYLOAD; return true;
		case XREDIS_RF_NAN_PAYLOAD:
			if (Byte == ')') { P->FloatState = XREDIS_RF_NAN_DONE; return true; }
			return Digit || (Special >= 'a' && Special <= 'z') || Byte == '_';
		default: return false;
	}
}

static bool xredisReaderScalarByte(xredisreader* Reader, unsigned char Byte)
{
	xredisreaderparse* P = &Reader->Parse;
	switch (P->Current.Type) {
		case XREDIS_REPLY_NULL: return false;
		case XREDIS_REPLY_BOOLEAN:
			if (P->LineBytes != 0 || (Byte != 't' && Byte != 'f')) return false;
			P->Current.Boolean = Byte == 't'; break;
		case XREDIS_REPLY_INTEGER:
		case XREDIS_REPLY_BIG_NUMBER:
			if (P->LineBytes == 0 && (Byte == '+' || Byte == '-')) P->Negative = Byte == '-';
			else {
				if (Byte < '0' || Byte > '9') return false;
				if (P->Current.Type == XREDIS_REPLY_INTEGER) {
					uint64 Limit = (uint64)INT64_MAX + (P->Negative ? 1u : 0u);
					uint64 Digit = (uint64)(Byte - '0');
					if (P->Number > (Limit - Digit) / 10u) return false;
					P->Number = P->Number * 10u + Digit;
				}
				++P->Digits;
			} break;
		case XREDIS_REPLY_DOUBLE: if (!xredisReaderFloatByte(Reader, Byte)) return false; break;
		default: break;
	}
	++P->LineBytes;
	return true;
}

static bool xredisReaderScalarEnd(xredisreader* Reader)
{
	xredisreaderparse* P = &Reader->Parse;
	xredisreplytype Type = P->Current.Type;
	bool Valid = true;
	if (Type == XREDIS_REPLY_INTEGER || Type == XREDIS_REPLY_BIG_NUMBER) Valid = P->Digits != 0;
	else if (Type == XREDIS_REPLY_BOOLEAN) Valid = P->LineBytes == 1u;
	else if (Type == XREDIS_REPLY_DOUBLE) Valid = P->FloatState == XREDIS_RF_INTEGER ||
		P->FloatState == XREDIS_RF_FRACTION || P->FloatState == XREDIS_RF_EXP_DIGITS ||
		P->FloatState == XREDIS_RF_INF || P->FloatState == XREDIS_RF_NAN || P->FloatState == XREDIS_RF_NAN_DONE;
	if (!Valid) return xredisReaderFail(Reader, XERR_PROTOCOL, XREDIS_READER_PROTOCOL, "incomplete RESP scalar token");
	if (Type == XREDIS_REPLY_INTEGER)
		P->Current.Integer = P->Negative ? (P->Number == (uint64)INT64_MAX + 1u ? INT64_MIN : -(int64)P->Number) : (int64)P->Number;
	P->Current.Length = P->LineBytes; P->State = XREDIS_RS_END;
	return true;
}

static size_t xredisReaderAvailable(const xredisreader* Reader, size_t Available)
{
	uint64 Room = Reader->Config.MaxFrameBytes - Reader->Parse.FrameBytes;
	uint64 StreamRoom = UINT64_MAX - Reader->Parse.StreamBytes;
	if ((uint64)Available > Room) Available = (size_t)Room;
	if ((uint64)Available > StreamRoom) Available = (size_t)StreamRoom;
	return Available;
}

XREDIS_API xredisreadstatus xredisReaderFeed(xredisreader* Reader,
	xbytesview Input, size_t* Consumed)
{
	xredisreadstatus Result = XREDIS_READER_MORE;
	size_t Index = 0, Budget;
	xredisreaderparse* P;
	const xbytesview Empty = {NULL, 0};
	if (!xredisReaderIdle(Reader)) return XREDIS_READER_ERROR;
	if (Consumed == NULL || !xrtMemRangeValid(Consumed, sizeof(*Consumed)) ||
		(Input.Data == NULL && Input.Size != 0) || !xrtMemRangeValid(Input.Data, Input.Size) ||
		xrtMemRangesOverlap(Reader, Reader->StorageBytes, Input.Data, Input.Size) ||
		xrtMemRangesOverlap(Reader, Reader->StorageBytes, Consumed, sizeof(*Consumed)) ||
		xrtMemRangesOverlap(Input.Data, Input.Size, Consumed, sizeof(*Consumed))) {
		xredisReaderReport(XERR_ARGUMENT, XREDIS_READER_ARGUMENT, "invalid reader input, consumed output or overlap");
		return XREDIS_READER_ERROR;
	}
	P = &Reader->Parse;
	if (P->Error != XREDIS_READER_OK) {
		*Consumed = 0; xredisReaderReport(P->ErrorKind, P->Error, P->ErrorMessage); return XREDIS_READER_ERROR;
	}
	Budget = Input.Size < Reader->Config.MaxFeedBytes ? Input.Size : Reader->Config.MaxFeedBytes;
	Reader->Active = true; P->Paused = false;
	for (;;) {
		/* Pending events/transitions are committed before callbacks. Draining
		 * them costs O(depth), even for empty input or a one-byte work budget. */
		if (P->State == XREDIS_RS_BEGIN_LINE) {
			P->State = XREDIS_RS_LINE;
			if (!xredisReaderEmit(Reader, XREDIS_READ_BEGIN, Empty)) goto callback_stop;
			continue;
		}
		if (P->State == XREDIS_RS_BEGIN_VALUE) {
			if (P->Current.Flags & XREDIS_REPLY_NULL_VALUE) P->State = XREDIS_RS_END;
			else if (xredisReaderAggregate(P->Current.Type)) {
				xredisreaderlevel* Level = &Reader->Stack[P->Depth++];
				*Level = (xredisreaderlevel){0}; Level->Type = P->Current.Type; Level->Flags = P->Current.Flags;
				Level->Remaining = (Level->Flags & XREDIS_REPLY_STREAMED) ? 0 :
					(xredisReaderPairs(Level->Type) ? P->Current.Length * 2u : P->Current.Length);
				P->State = !(Level->Flags & XREDIS_REPLY_STREAMED) && Level->Remaining == 0 ? XREDIS_RS_END_CONTAINER : XREDIS_RS_TYPE;
			} else {
				P->Remaining = P->Current.Length;
				P->State = (P->Current.Flags & XREDIS_REPLY_STREAMED) ? XREDIS_RS_CHUNK :
					(P->Remaining == 0 ? XREDIS_RS_BULK_CR : XREDIS_RS_BULK);
			}
			if (!xredisReaderEmit(Reader, XREDIS_READ_BEGIN, Empty)) goto callback_stop;
			continue;
		}
		if (P->State == XREDIS_RS_END_CONTAINER) {
			xredisreaderlevel* Level = &Reader->Stack[--P->Depth];
			P->Current = (xredisreadevent){0}; P->Current.Type = Level->Type;
			P->Current.Flags = Level->Flags; P->Current.Depth = P->Depth;
			P->Current.Length = xredisReaderPairs(Level->Type) ? Level->Seen / 2u : Level->Seen;
			P->State = XREDIS_RS_END; continue;
		}
		if (P->State == XREDIS_RS_END) {
			if (P->Current.Type == XREDIS_REPLY_BULK_STRING || P->Current.Type == XREDIS_REPLY_BULK_ERROR ||
				P->Current.Type == XREDIS_REPLY_VERBATIM) P->Current.Length = P->PayloadBytes;
			P->State = XREDIS_RS_FINISH_VALUE;
			if (!xredisReaderEmit(Reader, XREDIS_READ_END, Empty)) goto callback_stop;
			continue;
		}
		if (P->State == XREDIS_RS_FINISH_VALUE) {
			if (P->Current.Type == XREDIS_REPLY_ATTRIBUTE) {
				if (P->Depth == 0) P->RootAttributes = true;
				else Reader->Stack[P->Depth - 1u].Attributes = true;
				P->State = XREDIS_RS_TYPE;
			} else if (P->Depth == 0) P->State = XREDIS_RS_COMPLETE;
			else {
				xredisreaderlevel* Parent = &Reader->Stack[P->Depth - 1u];
				++Parent->Seen; Parent->Attributes = false;
				if (!(Parent->Flags & XREDIS_REPLY_STREAMED) && --Parent->Remaining == 0) P->State = XREDIS_RS_END_CONTAINER;
				else P->State = XREDIS_RS_TYPE;
			}
			continue;
		}
		if (P->State == XREDIS_RS_COMPLETE) {
			if (P->CompletedFrames == UINT64_MAX) {
				xredisReaderFail(Reader, XERR_RANGE, XREDIS_READER_LIMIT, "completed frame counter overflows"); goto failed;
			}
			++P->CompletedFrames; P->LastFrameBytes = P->FrameBytes; P->LastFrameValues = P->FrameValues;
			P->FrameBytes = 0; P->FrameValues = 0; P->FrameActive = false; P->RootAttributes = false;
			P->State = XREDIS_RS_TYPE; Result = XREDIS_READER_READY; break;
		}
		if (Index == Budget) { Result = Index < Input.Size ? XREDIS_READER_YIELD : XREDIS_READER_MORE; break; }
		if (!xredisReaderRoom(Reader)) goto failed;
		unsigned char Byte = Input.Data[Index];
		switch (P->State) {
			case XREDIS_RS_TYPE:
				if (Byte == '.') {
					if (P->Depth == 0 || !(Reader->Stack[P->Depth - 1u].Flags & XREDIS_REPLY_STREAMED) ||
						Reader->Stack[P->Depth - 1u].Attributes ||
						(xredisReaderPairs(Reader->Stack[P->Depth - 1u].Type) && (Reader->Stack[P->Depth - 1u].Seen & 1u))) {
						xredisReaderFail(Reader, XERR_PROTOCOL, XREDIS_READER_PROTOCOL, "invalid streamed aggregate terminator or orphan attributes"); goto failed;
					}
					P->State = XREDIS_RS_TERM_CR;
				} else if (!xredisReaderType(Reader, Byte)) goto failed;
				xredisReaderAdvance(Reader, &Index, 1); break;
			case XREDIS_RS_HEADER:
				if (Byte == '\r') P->State = XREDIS_RS_HEADER_LF;
				else if (!xredisReaderHeaderByte(Reader, Byte)) goto failed;
				xredisReaderAdvance(Reader, &Index, 1); break;
			case XREDIS_RS_HEADER_LF:
				if (Byte != '\n') goto bad_terminator;
				xredisReaderAdvance(Reader, &Index, 1);
				if (!xredisReaderHeaderEnd(Reader)) goto failed;
				break;
			case XREDIS_RS_LINE: {
				size_t Count = xredisReaderAvailable(Reader, Budget - Index);
				const unsigned char* CR = (const unsigned char*)memchr(Input.Data + Index, '\r', Count);
				if (CR != NULL) Count = (size_t)(CR - Input.Data - Index);
				const unsigned char* LF = (const unsigned char*)memchr(Input.Data + Index, '\n', Count);
				if (LF != NULL) Count = (size_t)(LF - Input.Data - Index);
				if (Count == 0) {
					if (Byte != '\r') goto bad_terminator;
					xredisReaderAdvance(Reader, &Index, 1); P->State = XREDIS_RS_LINE_LF; break;
				}
				uint64 LineRoom = Reader->Config.MaxLineBytes - P->LineBytes;
				if (LineRoom == 0) { xredisReaderFail(Reader, XERR_RANGE, XREDIS_READER_LIMIT, "RESP scalar line limit exceeded"); goto failed; }
				if ((uint64)Count > LineRoom) Count = (size_t)LineRoom;
				if (P->Current.Type == XREDIS_REPLY_SIMPLE_STRING || P->Current.Type == XREDIS_REPLY_SIMPLE_ERROR) P->LineBytes += Count;
				else for (size_t Part = 0; Part < Count; ++Part) {
					if (!xredisReaderScalarByte(Reader, Input.Data[Index + Part])) {
						xredisReaderAdvance(Reader, &Index, Part);
						xredisReaderFail(Reader, XERR_PROTOCOL, XREDIS_READER_PROTOCOL, "invalid RESP scalar or integer overflow"); goto failed;
					}
				}
				xbytesview Data = {Input.Data + Index, Count}; xredisReaderAdvance(Reader, &Index, Count);
				if (!xredisReaderEmit(Reader, XREDIS_READ_DATA, Data)) goto callback_stop;
				break;
			}
			case XREDIS_RS_LINE_LF:
				if (Byte != '\n') goto bad_terminator;
				xredisReaderAdvance(Reader, &Index, 1);
				if (!xredisReaderScalarEnd(Reader)) goto failed;
				break;
			case XREDIS_RS_CHUNK:
				if (Byte != ';') { xredisReaderFail(Reader, XERR_PROTOCOL, XREDIS_READER_PROTOCOL, "missing streamed bulk chunk prefix"); goto failed; }
				xredisReaderAdvance(Reader, &Index, 1); xredisReaderHeaderStart(P, true); break;
			case XREDIS_RS_BULK: {
				size_t Count = xredisReaderAvailable(Reader, Budget - Index);
				if ((uint64)Count > P->Remaining) Count = (size_t)P->Remaining;
				if (P->Current.Type == XREDIS_REPLY_VERBATIM && P->PayloadBytes <= 3u &&
					(uint64)Count > 3u - P->PayloadBytes && Input.Data[Index + (size_t)(3u - P->PayloadBytes)] != ':') {
					xredisReaderAdvance(Reader, &Index, (size_t)(3u - P->PayloadBytes));
					xredisReaderFail(Reader, XERR_PROTOCOL, XREDIS_READER_PROTOCOL, "verbatim format requires a colon"); goto failed;
				}
				xbytesview Data = {Input.Data + Index, Count};
				P->Remaining -= Count; P->PayloadBytes += Count;
				xredisReaderAdvance(Reader, &Index, Count);
				if (P->Remaining == 0) P->State = XREDIS_RS_BULK_CR;
				if (!xredisReaderEmit(Reader, XREDIS_READ_DATA, Data)) goto callback_stop;
				break;
			}
			case XREDIS_RS_BULK_CR:
			case XREDIS_RS_TERM_CR:
				if (Byte != '\r') goto bad_terminator;
				xredisReaderAdvance(Reader, &Index, 1);
				P->State = P->State == XREDIS_RS_BULK_CR ? XREDIS_RS_BULK_LF : XREDIS_RS_TERM_LF; break;
			case XREDIS_RS_BULK_LF:
				if (Byte != '\n') goto bad_terminator;
				xredisReaderAdvance(Reader, &Index, 1);
				P->State = (P->Current.Flags & XREDIS_REPLY_STREAMED) ? XREDIS_RS_CHUNK : XREDIS_RS_END; break;
			case XREDIS_RS_TERM_LF:
				if (Byte != '\n') goto bad_terminator;
				xredisReaderAdvance(Reader, &Index, 1); P->State = XREDIS_RS_END_CONTAINER; break;
			default:
				xredisReaderFail(Reader, XERR_INTERNAL, XREDIS_READER_STATE, "invalid reader state"); goto failed;
		}
	}
	goto done;
bad_terminator:
	xredisReaderFail(Reader, XERR_PROTOCOL, XREDIS_READER_PROTOCOL, "missing RESP CRLF terminator");
failed:
	Result = XREDIS_READER_ERROR; goto done;
callback_stop:
	Result = P->Error != XREDIS_READER_OK ? XREDIS_READER_ERROR : XREDIS_READER_PAUSED;
done:
	Reader->Active = false; *Consumed = Index;
	return Result;
}

XREDIS_API bool xredisReaderFinish(xredisreader* Reader)
{
	if (!xredisReaderIdle(Reader)) return false;
	xredisreaderparse* P = &Reader->Parse;
	if (P->Error != XREDIS_READER_OK) return xredisReaderReport(P->ErrorKind, P->Error, P->ErrorMessage);
	if (P->Paused) return xredisReaderReport(XERR_STATE, XREDIS_READER_STATE, "resume paused reader before end-of-stream");
	if (P->State == XREDIS_RS_TYPE && !P->FrameActive && !P->RootAttributes && P->Depth == 0) return true;
	return xredisReaderFail(Reader, XERR_PROTOCOL, XREDIS_READER_TRUNCATED, "truncated RESP frame or orphan attributes at end-of-stream");
}

#endif
