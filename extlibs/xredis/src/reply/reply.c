#include <xredis/reply.h>
#include <xredis/tracking.h>
#include <xredis/pubsub.h>
#include <xrt/atomic.h>
#include <xrt/memory.h>
#include <xrt/number.h>
#include <string.h>

#if defined(XREDIS_FEATURE_XREDIS_REPLY)

typedef struct xredisreplynode {
	xredisreplytype Type;
	uint32 Flags;
	uint64 Length;
	int64 Integer;
	bool Boolean;
	size_t Offset, Bytes, ChildCount, FirstChild, Next, Attributes;
} xredisreplynode;

struct xredisreply {
	xatomic32 References;
	xredisreplynode* Nodes;
	unsigned char* Data;
	size_t Count, NodeCapacity, DataBytes, DataCapacity, Root, Reserved, Peak;
	xredisreplynode InlineNode[1];
	unsigned char InlineData[64];
};

typedef struct xredisreplylevel {
	size_t Open, LastChild, PendingFirst, PendingLast;
} xredisreplylevel;

struct xredisreplyreader {
	xredisreplyreaderconfig Config;
	xredisreader* Parser;
	xredisreply* Partial;
	size_t StorageBytes, Peak;
	xredisreplyerror Error;
	xerrkind ErrorKind;
	cstr ErrorMessage;
	xredisreplylevel Levels[];
};

static bool xredisReplyReport(xerrkind Kind, xredisreplyerror Code, cstr Message)
{
	xrtSetErrorInfo(Kind, "xrt.redis", (int32)Code, Message);
	return false;
}

static bool xredisReplyFail(xredisreplyreader* Reader, xerrkind Kind,
	xredisreplyerror Code, cstr Message)
{
	Reader->Error = Code; Reader->ErrorKind = Kind; Reader->ErrorMessage = Message;
	return false;
}

static bool xredisReplyOverlap(const xredisreply* Reply, const void* Data, size_t Bytes)
{
	return Reply != NULL && (xrtMemRangesOverlap(Reply, sizeof(*Reply), Data, Bytes) ||
		(Reply->Nodes != Reply->InlineNode && xrtMemRangesOverlap(Reply->Nodes,
			Reply->NodeCapacity * sizeof(*Reply->Nodes), Data, Bytes)) ||
		(Reply->Data != Reply->InlineData && xrtMemRangesOverlap(Reply->Data, Reply->DataCapacity, Data, Bytes)));
}

static bool xredisReplyReaderOverlap(const xredisreplyreader* Reader, const void* Data, size_t Bytes)
{
	return xrtMemRangesOverlap(Reader, Reader->StorageBytes, Data, Bytes) ||
		xredisReplyOverlap(Reader->Partial, Data, Bytes);
}

XREDIS_API bool xredisReplyRetain(xredisreply* Reply)
{
	if (Reply == NULL) return xredisReplyReport(XERR_ARGUMENT, XREDIS_REPLY_ARGUMENT, "reply is NULL");
	uint32 Count = xrtAtomic32Load(&Reply->References, XMEMORY_RELAXED);
	for (;;) {
		if (Count == UINT32_MAX)
			return xredisReplyReport(XERR_RANGE, XREDIS_REPLY_LIMIT, "reply reference count overflows");
		if (xrtAtomic32CompareExchange(&Reply->References, &Count, Count + 1u, XMEMORY_RELAXED, XMEMORY_RELAXED)) return true;
	}
}

XREDIS_API void xredisReplyRelease(xredisreply* Reply)
{
	if (Reply == NULL) return;
	if (xrtAtomic32FetchSub(&Reply->References, 1, XMEMORY_ACQ_REL) != 1) return;
	if (Reply->Nodes != Reply->InlineNode) xrtFree(Reply->Nodes);
	if (Reply->Data != Reply->InlineData) xrtFree(Reply->Data);
	xrtFree(Reply);
}

XREDIS_API bool xredisReplyInfo(const xredisreply* Reply, xredisreplyinfo* Info)
{
	if (Reply == NULL || Info == NULL || !xrtMemRangeValid(Info, sizeof(*Info)) ||
		xredisReplyOverlap(Reply, Info, sizeof(*Info)) || Info->Size != sizeof(*Info))
		return xredisReplyReport(XERR_ARGUMENT, XREDIS_REPLY_ARGUMENT, "invalid reply info output or size");
	*Info = (xredisreplyinfo){sizeof(*Info), Reply->Root, Reply->Count, Reply->DataBytes, Reply->Reserved, Reply->Peak};
	return true;
}

XREDIS_API bool xredisReplyGet(const xredisreply* Reply, size_t Index, xredisreplyvalue* Value)
{
	if (Reply == NULL || Index >= Reply->Count || Value == NULL || !xrtMemRangeValid(Value, sizeof(*Value)) ||
		xredisReplyOverlap(Reply, Value, sizeof(*Value)))
		return xredisReplyReport(XERR_ARGUMENT, XREDIS_REPLY_ARGUMENT, "invalid reply node index or output");
	const xredisreplynode* Node = &Reply->Nodes[Index];
	*Value = (xredisreplyvalue){Node->Type, Node->Flags, Node->Length, Node->Integer, Node->Boolean,
		{Node->Bytes == 0 ? NULL : Reply->Data + Node->Offset, Node->Bytes},
		Node->ChildCount, Node->FirstChild, Node->Next, Node->Attributes};
	return true;
}

XREDIS_API bool xredisReplyDouble(const xredisreply* Reply, size_t Index, double* Value)
{
	if (Reply == NULL || Index >= Reply->Count || Reply->Nodes[Index].Type != XREDIS_REPLY_DOUBLE ||
		Value == NULL || !xrtMemRangeValid(Value, sizeof(*Value)) || xredisReplyOverlap(Reply, Value, sizeof(*Value)))
		return xredisReplyReport(XERR_ARGUMENT, XREDIS_REPLY_ARGUMENT, "invalid double node or output");
	const xredisreplynode* Node = &Reply->Nodes[Index];
	xstrview Text = {(cstr)Reply->Data + Node->Offset, Node->Bytes};
	size_t Start = Text.Data[0] == '-' || Text.Data[0] == '+' ? 1u : 0;
	if (Text.Data[Start] == 'n' || Text.Data[Start] == 'N')
		Text = Text.Data[0] == '-' ? (xstrview){"-nan", 4} : (xstrview){"nan", 3};
	return xrtNumParse(Text, XNUMBER_PARSE_SPECIAL, Value);
}

XREDIS_API bool xredisReplyReaderConfigInit(xredisreplyreaderconfig* Config)
{
	if (Config == NULL || !xrtMemRangeValid(Config, sizeof(*Config)))
		return xredisReplyReport(XERR_ARGUMENT, XREDIS_REPLY_ARGUMENT, "invalid reply reader config output");
	*Config = (xredisreplyreaderconfig){0}; Config->Size = sizeof(*Config);
	xredisReaderConfigInit(&Config->Reader); Config->MaxOwnedBytes = 64u * 1024u * 1024u;
	return true;
}

/* Budget includes the complete destination before the old buffer is freed.
 * Explicit copy/free counts only used bytes and keeps failure ownership clear. */
static size_t xredisReplyGrowth(xredisreplyreader* Reader, size_t Old, size_t Need, size_t Unit)
{
	xredisreply* Reply = Reader->Partial;
	size_t Available = (Reader->Config.MaxOwnedBytes - Reply->Reserved) / Unit;
	if (Need > Available) {
		xredisReplyFail(Reader, XERR_RANGE, XREDIS_REPLY_LIMIT, "owned reply peak byte limit exceeded"); return 0;
	}
	size_t Capacity = Old <= SIZE_MAX / 2u ? Old * 2u : SIZE_MAX;
	if (Capacity < Need) Capacity = Need;
	if (Capacity > Available) Capacity = Available;
	return Capacity;
}

static void xredisReplyPeak(xredisreplyreader* Reader, size_t Extra)
{
	xredisreply* Reply = Reader->Partial;
	if (Reply->Reserved + Extra > Reply->Peak) Reply->Peak = Reply->Reserved + Extra;
	Reader->Peak = Reply->Peak;
}

static bool xredisReplyNodes(xredisreplyreader* Reader)
{
	xredisreply* Reply = Reader->Partial;
	if (Reply->Count < Reply->NodeCapacity) return true;
	if (Reply->Count == SIZE_MAX)
		return xredisReplyFail(Reader, XERR_RANGE, XREDIS_REPLY_LIMIT, "owned node count overflows");
	size_t Capacity = xredisReplyGrowth(Reader, Reply->NodeCapacity, Reply->Count + 1u, sizeof(*Reply->Nodes));
	if (Capacity == 0) return false;
	size_t Bytes = Capacity * sizeof(*Reply->Nodes);
	xredisreplynode* Nodes = (xredisreplynode*)xrtMalloc(Bytes);
	if (Nodes == NULL) return xredisReplyFail(Reader, XERR_MEMORY, XREDIS_REPLY_MEMORY, "owned node allocation failed");
	xredisReplyPeak(Reader, Bytes);
	memcpy(Nodes, Reply->Nodes, Reply->Count * sizeof(*Nodes));
	if (Reply->Nodes != Reply->InlineNode) {
		xrtFree(Reply->Nodes); Reply->Reserved -= Reply->NodeCapacity * sizeof(*Nodes);
	}
	Reply->Nodes = Nodes; Reply->NodeCapacity = Capacity; Reply->Reserved += Bytes;
	return true;
}

static bool xredisReplyPayload(xredisreplyreader* Reader, size_t Additional)
{
	xredisreply* Reply = Reader->Partial;
	if (Additional > SIZE_MAX - Reply->DataBytes)
		return xredisReplyFail(Reader, XERR_RANGE, XREDIS_REPLY_LIMIT, "owned payload length overflows");
	size_t Need = Reply->DataBytes + Additional;
	if (Need <= Reply->DataCapacity) return true;
	size_t Capacity = xredisReplyGrowth(Reader, Reply->DataCapacity, Need, 1);
	if (Capacity == 0) return false;
	unsigned char* Data = (unsigned char*)xrtMalloc(Capacity);
	if (Data == NULL) return xredisReplyFail(Reader, XERR_MEMORY, XREDIS_REPLY_MEMORY, "owned payload allocation failed");
	xredisReplyPeak(Reader, Capacity);
	memcpy(Data, Reply->Data, Reply->DataBytes);
	if (Reply->Data != Reply->InlineData) { xrtFree(Reply->Data); Reply->Reserved -= Reply->DataCapacity; }
	Reply->Data = Data; Reply->DataCapacity = Capacity; Reply->Reserved += Capacity;
	return true;
}

static void xredisReplyLevels(xredisreplyreader* Reader)
{
	Reader->Levels[0] = (xredisreplylevel){XREDIS_REPLY_NONE, XREDIS_REPLY_NONE, XREDIS_REPLY_NONE, XREDIS_REPLY_NONE};
}

static xredisreadaction xredisReplyCollect(const xredisreadevent* Event, ptr User)
{
	xredisreplyreader* Reader = (xredisreplyreader*)User;
	xredisreplylevel* Level = &Reader->Levels[Event->Depth];
	if (Reader->Partial == NULL) {
		Reader->Peak = 0;
		if (sizeof(xredisreply) > Reader->Config.MaxOwnedBytes) {
			xredisReplyFail(Reader, XERR_RANGE, XREDIS_REPLY_LIMIT, "owned reply header exceeds byte limit"); return XREDIS_READ_FAIL;
		}
		xredisreply* Reply = (xredisreply*)xrtCalloc(1, sizeof(*Reply));
		if (Reply == NULL) {
			xredisReplyFail(Reader, XERR_MEMORY, XREDIS_REPLY_MEMORY, "owned reply allocation failed"); return XREDIS_READ_FAIL;
		}
		xrtAtomic32Init(&Reply->References, 1); Reply->Nodes = Reply->InlineNode; Reply->NodeCapacity = 1;
		Reply->Data = Reply->InlineData; Reply->DataCapacity = sizeof(Reply->InlineData); Reply->Root = XREDIS_REPLY_NONE;
		Reply->Reserved = sizeof(*Reply); Reply->Peak = sizeof(*Reply); Reader->Peak = Reply->Peak;
		Reader->Partial = Reply;
	}
	xredisreply* Reply = Reader->Partial;
	if (Event->Kind == XREDIS_READ_BEGIN) {
		/* A new parent's child level has no pending metadata. Initialize lazily
		 * so a scalar frame never clears the entire configured depth stack. */
		if (Event->Depth < Reader->Config.Reader.MaxDepth)
			Reader->Levels[Event->Depth + 1u] = (xredisreplylevel){XREDIS_REPLY_NONE,
				XREDIS_REPLY_NONE, XREDIS_REPLY_NONE, XREDIS_REPLY_NONE};
		if (!xredisReplyNodes(Reader)) return XREDIS_READ_FAIL;
		size_t Index = Reply->Count++;
		xredisreplynode* Node = &Reply->Nodes[Index];
		*Node = (xredisreplynode){0}; Node->Type = Event->Type; Node->Flags = Event->Flags;
		Node->Offset = Reply->DataBytes; Node->FirstChild = XREDIS_REPLY_NONE;
		Node->Next = XREDIS_REPLY_NONE; Node->Attributes = XREDIS_REPLY_NONE;
		Level->Open = Index; Level->LastChild = XREDIS_REPLY_NONE;
		if (Event->Type != XREDIS_REPLY_ATTRIBUTE) {
			Node->Attributes = Level->PendingFirst;
			Level->PendingFirst = XREDIS_REPLY_NONE; Level->PendingLast = XREDIS_REPLY_NONE;
			if (Event->Depth == 0) Reply->Root = Index;
			else {
				xredisreplylevel* ParentLevel = &Reader->Levels[Event->Depth - 1u];
				xredisreplynode* Parent = &Reply->Nodes[ParentLevel->Open];
				if (ParentLevel->LastChild == XREDIS_REPLY_NONE) Parent->FirstChild = Index;
				else Reply->Nodes[ParentLevel->LastChild].Next = Index;
				ParentLevel->LastChild = Index; ++Parent->ChildCount;
			}
		}
		if (!(Event->Flags & XREDIS_REPLY_NULL_VALUE) && Event->Length != XREDIS_LENGTH_UNKNOWN &&
			(Event->Type == XREDIS_REPLY_BULK_STRING || Event->Type == XREDIS_REPLY_BULK_ERROR || Event->Type == XREDIS_REPLY_VERBATIM)) {
			if (Event->Length > (uint64)SIZE_MAX) {
				xredisReplyFail(Reader, XERR_RANGE, XREDIS_REPLY_LIMIT, "owned bulk length overflows size_t"); return XREDIS_READ_FAIL;
			}
			if (!xredisReplyPayload(Reader, (size_t)Event->Length)) return XREDIS_READ_FAIL;
		}
	} else if (Event->Kind == XREDIS_READ_DATA) {
		if (!xredisReplyPayload(Reader, Event->Data.Size)) return XREDIS_READ_FAIL;
		xredisreplynode* Node = &Reply->Nodes[Level->Open];
		memcpy(Reply->Data + Reply->DataBytes, Event->Data.Data, Event->Data.Size);
		Reply->DataBytes += Event->Data.Size; Node->Bytes += Event->Data.Size;
	} else {
		xredisreplynode* Node = &Reply->Nodes[Level->Open];
		Node->Length = Event->Length; Node->Integer = Event->Integer; Node->Boolean = Event->Boolean;
		if (Event->Type == XREDIS_REPLY_ATTRIBUTE) {
			if (Level->PendingLast == XREDIS_REPLY_NONE) Level->PendingFirst = Level->Open;
			else Reply->Nodes[Level->PendingLast].Next = Level->Open;
			Level->PendingLast = Level->Open;
		}
		Level->Open = XREDIS_REPLY_NONE;
	}
	return XREDIS_READ_NEXT;
}

XREDIS_API bool xredisReplyReaderCreate(const xredisreplyreaderconfig* Config, xredisreplyreader** Result)
{
	xredisreplyreaderconfig Copy;
	if (Result == NULL || !xrtMemRangeValid(Result, sizeof(*Result)) ||
		(Config != NULL && (!xrtMemRangeValid(Config, sizeof(*Config)) ||
		 xrtMemRangesOverlap(Config, sizeof(*Config), Result, sizeof(*Result)))))
		return xredisReplyReport(XERR_ARGUMENT, XREDIS_REPLY_ARGUMENT, "invalid reply reader creation arguments");
	if (Config == NULL) xredisReplyReaderConfigInit(&Copy); else Copy = *Config;
	if (Copy.Size != sizeof(Copy) || Copy.MaxOwnedBytes == 0 ||
		Copy.Reader.MaxDepth >= (SIZE_MAX - sizeof(xredisreplyreader)) / sizeof(xredisreplylevel))
		return xredisReplyReport(XERR_ARGUMENT, XREDIS_REPLY_ARGUMENT, "invalid reply reader size or limits");
	size_t Bytes = sizeof(xredisreplyreader) + (Copy.Reader.MaxDepth + 1u) * sizeof(xredisreplylevel);
	xredisreplyreader* Reader = (xredisreplyreader*)xrtCalloc(1, Bytes);
	if (Reader == NULL) return false;
	Reader->Config = Copy; Reader->StorageBytes = Bytes;
	if (!xredisReaderCreate(&Copy.Reader, xredisReplyCollect, Reader, &Reader->Parser)) { xrtFree(Reader); return false; }
	xredisReplyLevels(Reader); *Result = Reader;
	return true;
}

XREDIS_API void xredisReplyReaderDestroy(xredisreplyreader* Reader)
{
	if (Reader == NULL) return;
	xredisReplyRelease(Reader->Partial); xredisReaderDestroy(Reader->Parser); xrtFree(Reader);
}

XREDIS_API bool xredisReplyReaderReset(xredisreplyreader* Reader)
{
	if (Reader == NULL) return xredisReplyReport(XERR_ARGUMENT, XREDIS_REPLY_ARGUMENT, "reply reader is NULL");
	xredisReplyRelease(Reader->Partial); Reader->Partial = NULL;
	Reader->Error = XREDIS_REPLY_OK; Reader->ErrorKind = XERR_NONE; Reader->ErrorMessage = NULL; Reader->Peak = 0;
	xredisReplyLevels(Reader);
	return xredisReaderReset(Reader->Parser);
}

XREDIS_API bool xredisReplyReaderInfo(const xredisreplyreader* Reader, xredisreplyreaderinfo* Info)
{
	if (Reader == NULL || Info == NULL || !xrtMemRangeValid(Info, sizeof(*Info)) ||
		xredisReplyReaderOverlap(Reader, Info, sizeof(*Info)) || Info->Size != sizeof(*Info))
		return xredisReplyReport(XERR_ARGUMENT, XREDIS_REPLY_ARGUMENT, "invalid reply reader info output or size");
	xredisreplyreaderinfo Copy = {0}; Copy.Size = sizeof(Copy); Copy.Reader.Size = sizeof(Copy.Reader);
	if (!xredisReaderInfo(Reader->Parser, &Copy.Reader)) return false;
	Copy.Error = Reader->Error; Copy.PeakOwnedBytes = Reader->Peak;
	if (Reader->Partial != NULL) {
		Copy.PartialNodes = Reader->Partial->Count; Copy.PartialPayloadBytes = Reader->Partial->DataBytes;
		Copy.PartialReservedBytes = Reader->Partial->Reserved;
	}
	*Info = Copy; return true;
}

static bool xredisReplyParserFailure(xredisreplyreader* Reader)
{
	if (Reader->Error == XREDIS_REPLY_OK) {
		xredisreaderinfo Info = {0}; Info.Size = sizeof(Info); xredisReaderInfo(Reader->Parser, &Info);
		switch (Info.Error) {
			case XREDIS_READER_LIMIT:
				xredisReplyFail(Reader, XERR_RANGE, XREDIS_REPLY_LIMIT, "RESP reader limit exceeded"); break;
			case XREDIS_READER_TRUNCATED:
				xredisReplyFail(Reader, XERR_PROTOCOL, XREDIS_REPLY_TRUNCATED, "incomplete RESP reply at EOF"); break;
			case XREDIS_READER_PROTOCOL:
				xredisReplyFail(Reader, XERR_PROTOCOL, XREDIS_REPLY_PROTOCOL, "invalid RESP reply"); break;
			default:
				xredisReplyFail(Reader, XERR_STATE, XREDIS_REPLY_STATE, "unexpected RESP reader state"); break;
		}
	}
	xredisReplyRelease(Reader->Partial); Reader->Partial = NULL;
	return xredisReplyReport(Reader->ErrorKind, Reader->Error, Reader->ErrorMessage);
}

XREDIS_API xredisreplystatus xredisReplyReaderFeed(xredisreplyreader* Reader,
	xbytesview Input, size_t* Consumed, xredisreply** Result)
{
	if (Reader == NULL || Consumed == NULL || Result == NULL ||
		!xrtMemRangeValid(Consumed, sizeof(*Consumed)) || !xrtMemRangeValid(Result, sizeof(*Result)) ||
		(Input.Data == NULL && Input.Size != 0) || !xrtMemRangeValid(Input.Data, Input.Size) ||
		xredisReplyReaderOverlap(Reader, Input.Data, Input.Size) ||
		xredisReplyReaderOverlap(Reader, Consumed, sizeof(*Consumed)) ||
		xredisReplyReaderOverlap(Reader, Result, sizeof(*Result)) ||
		xrtMemRangesOverlap(Input.Data, Input.Size, Consumed, sizeof(*Consumed)) ||
		xrtMemRangesOverlap(Input.Data, Input.Size, Result, sizeof(*Result)) ||
		xrtMemRangesOverlap(Consumed, sizeof(*Consumed), Result, sizeof(*Result))) {
		xredisReplyReport(XERR_ARGUMENT, XREDIS_REPLY_ARGUMENT, "invalid reply input, outputs or overlap"); return XREDIS_REPLY_ERROR;
	}
	if (Reader->Error != XREDIS_REPLY_OK) {
		*Consumed = 0; xredisReplyReport(Reader->ErrorKind, Reader->Error, Reader->ErrorMessage); return XREDIS_REPLY_ERROR;
	}
	xredisreadstatus Status = xredisReaderFeed(Reader->Parser, Input, Consumed);
	if (Status == XREDIS_READER_ERROR) { xredisReplyParserFailure(Reader); return XREDIS_REPLY_ERROR; }
	if (Status == XREDIS_READER_READY) {
		*Result = Reader->Partial; Reader->Partial = NULL; xredisReplyLevels(Reader); return XREDIS_REPLY_READY;
	}
	return Status == XREDIS_READER_YIELD ? XREDIS_REPLY_YIELD : XREDIS_REPLY_MORE;
}

XREDIS_API bool xredisReplyReaderFinish(xredisreplyreader* Reader)
{
	if (Reader == NULL) return xredisReplyReport(XERR_ARGUMENT, XREDIS_REPLY_ARGUMENT, "reply reader is NULL");
	if (Reader->Error != XREDIS_REPLY_OK)
		return xredisReplyReport(Reader->ErrorKind, Reader->Error, Reader->ErrorMessage);
	if (!xredisReaderFinish(Reader->Parser)) return xredisReplyParserFailure(Reader);
	return true;
}

#if defined(XREDIS_FEATURE_XREDIS_PUBSUB)
static bool xredisPubSubString(const xredisreplynode* N)
{return (N->Type==XREDIS_REPLY_BULK_STRING || N->Type==XREDIS_REPLY_SIMPLE_STRING) && !(N->Flags&XREDIS_REPLY_NULL_VALUE);}
static bool xredisPubSubWord(const xredisreplynode* N,const xredisreply* R,cstr Word,size_t Length)
{return xredisPubSubString(N) && N->Bytes==Length && !memcmp(R->Data+N->Offset,Word,Length);}
static xbytesview xredisPubSubView(const xredisreplynode* N,const xredisreply* R)
{return (xbytesview){R->Data+N->Offset,N->Bytes};}
static bool xredisPubSubNullName(const xredisreplynode* N)
{return N->Type==XREDIS_REPLY_NULL || (N->Type==XREDIS_REPLY_BULK_STRING && (N->Flags&XREDIS_REPLY_NULL_VALUE));}
static bool xredisPubSubOutput(const xredisreply* R,xredispubsubevent* Output)
{
    if(!R || !Output || !xrtMemRangeValid(Output,sizeof(*Output)) || xredisReplyOverlap(R,Output,sizeof(*Output))){
        xrtSetErrorInfo(XERR_ARGUMENT,"xrt.redis",XREDIS_PUBSUB_ARGUMENT,"invalid Pub/Sub event output");return false;
    }
    return true;
}
static xredispubsubstatus xredisPubSubRead(const xredisreply* R,xredispubsubevent* Output)
{
    const xredisreplynode* Root=R->Nodes+R->Root;
    if((Root->Type!=XREDIS_REPLY_ARRAY && Root->Type!=XREDIS_REPLY_PUSH) || !Root->ChildCount)return XREDIS_PUBSUB_OTHER;
    const xredisreplynode* Tag=R->Nodes+Root->FirstChild;
    static const xbytesview Tags[]={XRT_BYTES_LITERAL("message"),XRT_BYTES_LITERAL("pmessage"),XRT_BYTES_LITERAL("smessage"),
        XRT_BYTES_LITERAL("subscribe"),XRT_BYTES_LITERAL("psubscribe"),XRT_BYTES_LITERAL("ssubscribe"),
        XRT_BYTES_LITERAL("unsubscribe"),XRT_BYTES_LITERAL("punsubscribe"),XRT_BYTES_LITERAL("sunsubscribe"),XRT_BYTES_LITERAL("pong")};
    _Static_assert(sizeof(Tags)/sizeof(*Tags)==XREDIS_PUBSUB_PONG+1,"tag table follows public event kinds");
    size_t Kind;
    for(Kind=0;Kind<sizeof(Tags)/sizeof(*Tags);++Kind)
        if(xredisPubSubWord(Tag,R,(cstr)Tags[Kind].Data,Tags[Kind].Size))break;
    if(Kind==sizeof(Tags)/sizeof(*Tags))return XREDIS_PUBSUB_OTHER;
    size_t Arity=Kind==XREDIS_PUBSUB_PMESSAGE?4:Kind==XREDIS_PUBSUB_PONG?2:3;
    if(Root->ChildCount!=Arity)return XREDIS_PUBSUB_MALFORMED;
    xredispubsubevent E={0};E.Reply=R;E.Kind=(xredispubsubkind)Kind;E.Envelope=Root->Type;E.PayloadNode=XREDIS_REPLY_NONE;
    const xredisreplynode* Name=R->Nodes+Tag->Next;
    if(Kind>=XREDIS_PUBSUB_SUBSCRIBE && Kind<=XREDIS_PUBSUB_SUNSUBSCRIBE){
        const xredisreplynode* Count=R->Nodes+Name->Next;
        if(Count->Type!=XREDIS_REPLY_INTEGER || Count->Integer<0 || (Kind<=XREDIS_PUBSUB_SSUBSCRIBE && !Count->Integer))return XREDIS_PUBSUB_MALFORMED;
        if(xredisPubSubNullName(Name)){
            if(Kind<XREDIS_PUBSUB_UNSUBSCRIBE)return XREDIS_PUBSUB_MALFORMED;
            E.Flags=XREDIS_PUBSUB_NAME_ABSENT;
        }else {
            if(!xredisPubSubString(Name))return XREDIS_PUBSUB_MALFORMED;
            if(Kind==XREDIS_PUBSUB_PSUBSCRIBE || Kind==XREDIS_PUBSUB_PUNSUBSCRIBE)E.Pattern=xredisPubSubView(Name,R);
            else E.Channel=xredisPubSubView(Name,R);
        }
        E.Subscriptions=(uint64)Count->Integer;
    }else if(Kind==XREDIS_PUBSUB_PONG){
        if(!xredisPubSubString(Name))return XREDIS_PUBSUB_MALFORMED;
        E.Payload=xredisPubSubView(Name,R);E.PayloadNode=Tag->Next;
    }else{
        if(!xredisPubSubString(Name))return XREDIS_PUBSUB_MALFORMED;
        const xredisreplynode* Channel=Name;
        if(Kind==XREDIS_PUBSUB_PMESSAGE){E.Pattern=xredisPubSubView(Name,R);Channel=R->Nodes+Name->Next;if(!xredisPubSubString(Channel))return XREDIS_PUBSUB_MALFORMED;}
        E.Channel=xredisPubSubView(Channel,R);E.PayloadNode=Channel->Next;const xredisreplynode* Payload=R->Nodes+E.PayloadNode;
        if(xredisPubSubString(Payload))E.Payload=xredisPubSubView(Payload,R);
        else {
            if(Kind!=XREDIS_PUBSUB_MESSAGE || Root->Type!=XREDIS_REPLY_ARRAY || !xredisPubSubWord(Channel,R,"__redis__:invalidate",20))return XREDIS_PUBSUB_MALFORMED;
            bool Null=Payload->Type==XREDIS_REPLY_NULL || ((Payload->Type==XREDIS_REPLY_ARRAY || Payload->Type==XREDIS_REPLY_BULK_STRING) && (Payload->Flags&XREDIS_REPLY_NULL_VALUE));
            if(!Null){
                if(Payload->Type!=XREDIS_REPLY_ARRAY)return XREDIS_PUBSUB_MALFORMED;
                for(size_t N=Payload->FirstChild;N!=XREDIS_REPLY_NONE;N=R->Nodes[N].Next)
                    if(R->Nodes[N].Type!=XREDIS_REPLY_BULK_STRING || (R->Nodes[N].Flags&XREDIS_REPLY_NULL_VALUE))return XREDIS_PUBSUB_MALFORMED;
            }
            E.Flags=XREDIS_PUBSUB_STRUCTURED;
        }
    }
    *Output=E;return XREDIS_PUBSUB_EVENT;
}
XREDIS_API xredispubsubstatus xredisPubSubEventRead(const xredisreply* R,xredispubsubevent* Output)
{
    if(!xredisPubSubOutput(R,Output))return XREDIS_PUBSUB_ERROR;
    return xredisPubSubRead(R,Output);
}
XREDIS_API xredispubsubstatus xredisPubSubPongRead(const xredisreply* R,const xbytesview* Expected,xredispubsubevent* Output)
{
    if(!xredisPubSubOutput(R,Output))return XREDIS_PUBSUB_ERROR;
    if(Expected && (!xrtMemRangeValid(Expected,sizeof(*Expected)) || xrtMemRangesOverlap(Output,sizeof(*Output),Expected,sizeof(*Expected)) ||
        !xrtMemRangeValid(Expected->Data,Expected->Size) || xrtMemRangesOverlap(Output,sizeof(*Output),Expected->Data,Expected->Size))){
        xrtSetErrorInfo(XERR_ARGUMENT,"xrt.redis",XREDIS_PUBSUB_ARGUMENT,"invalid Pub/Sub expected PING payload");return XREDIS_PUBSUB_ERROR;
    }
    const xredisreplynode* Root=R->Nodes+R->Root;xredispubsubevent E;
    if(Root->Type==XREDIS_REPLY_ARRAY || Root->Type==XREDIS_REPLY_PUSH){
        xredispubsubstatus Status=xredisPubSubRead(R,&E);
        if(Status!=XREDIS_PUBSUB_EVENT)return Status;
        if(E.Kind!=XREDIS_PUBSUB_PONG)return XREDIS_PUBSUB_OTHER;
    }else{
        if(Root->Type==XREDIS_REPLY_SIMPLE_ERROR || Root->Type==XREDIS_REPLY_BULK_ERROR)return XREDIS_PUBSUB_OTHER;
        E=(xredispubsubevent){0};E.Reply=R;E.Kind=XREDIS_PUBSUB_PONG;E.Envelope=Root->Type;E.PayloadNode=R->Root;
        if(!Expected){if(!xredisPubSubWord(Root,R,"PONG",4) || Root->Type!=XREDIS_REPLY_SIMPLE_STRING)return XREDIS_PUBSUB_MALFORMED;}
        else {if(Root->Type!=XREDIS_REPLY_BULK_STRING || !xredisPubSubString(Root))return XREDIS_PUBSUB_MALFORMED;E.Payload=xredisPubSubView(Root,R);}
    }
    size_t Size=Expected?Expected->Size:0;
    if(E.Payload.Size!=Size || (Size && memcmp(E.Payload.Data,Expected->Data,Size)))return XREDIS_PUBSUB_MALFORMED;
    *Output=E;return XREDIS_PUBSUB_EVENT;
}
#endif

#if defined(XREDIS_FEATURE_XREDIS_TRACKING)
static bool xredisTrackingWord(const xredisreplynode* N,const xredisreply* R,cstr Word,size_t Length)
{
    return (N->Type==XREDIS_REPLY_BULK_STRING || N->Type==XREDIS_REPLY_SIMPLE_STRING) && !(N->Flags&XREDIS_REPLY_NULL_VALUE) &&
        N->Bytes==Length && !memcmp(R->Data+N->Offset,Word,Length);
}
XREDIS_API xredistrackingstatus xredisTrackingEventRead(const xredisreply* R,xredistrackingevent* Output)
{
    if(!R || !Output || !xrtMemRangeValid(Output,sizeof(*Output)) || xredisReplyOverlap(R,Output,sizeof(*Output))){
        xredisReplyReport(XERR_ARGUMENT,XREDIS_REPLY_ARGUMENT,"invalid tracking event output");return XREDIS_TRACKING_ERROR;
    }
    const xredisreplynode* Root=R->Nodes+R->Root;
    if((Root->Type!=XREDIS_REPLY_PUSH && Root->Type!=XREDIS_REPLY_ARRAY) || !Root->ChildCount)return XREDIS_TRACKING_OTHER;
    size_t First=Root->FirstChild;const xredisreplynode* Tag=R->Nodes+First;
    bool Keys=Root->Type==XREDIS_REPLY_PUSH && xredisTrackingWord(Tag,R,"invalidate",10);
    bool Broken=Root->Type==XREDIS_REPLY_PUSH && xredisTrackingWord(Tag,R,"tracking-redir-broken",21);
    size_t Payload=Tag->Next;
    if(Root->Type==XREDIS_REPLY_ARRAY && xredisTrackingWord(Tag,R,"message",7) && Root->ChildCount>=2){
        const xredisreplynode* Channel=R->Nodes+Payload;
        if(xredisTrackingWord(Channel,R,"__redis__:invalidate",20)){
            if(Root->ChildCount!=3)return XREDIS_TRACKING_MALFORMED;
            Keys=true;Payload=Channel->Next;
        }
    }
    if(!Keys && !Broken)return XREDIS_TRACKING_OTHER;
    if(Root->Type==XREDIS_REPLY_PUSH && Root->ChildCount!=2)return XREDIS_TRACKING_MALFORMED;
    const xredisreplynode* Value=R->Nodes+Payload;
    xredistrackingevent E={R,XREDIS_TRACKING_KEYS,XREDIS_REPLY_NONE,0,0};
    if(Broken){
        if(Value->Type!=XREDIS_REPLY_INTEGER || Value->Integer<0)return XREDIS_TRACKING_MALFORMED;
        E.Kind=XREDIS_TRACKING_REDIRECT_BROKEN;E.Redirect=(uint64)Value->Integer;
    }else if(Value->Type==XREDIS_REPLY_NULL ||
        ((Value->Type==XREDIS_REPLY_ARRAY || Value->Type==XREDIS_REPLY_BULK_STRING) && (Value->Flags&XREDIS_REPLY_NULL_VALUE)))E.Kind=XREDIS_TRACKING_FLUSH;
    else {
        if(Value->Type!=XREDIS_REPLY_ARRAY)return XREDIS_TRACKING_MALFORMED;
        E.FirstKey=Value->FirstChild;E.Keys=Value->ChildCount;
        for(size_t N=E.FirstKey;N!=XREDIS_REPLY_NONE;N=R->Nodes[N].Next)
            if(R->Nodes[N].Type!=XREDIS_REPLY_BULK_STRING || (R->Nodes[N].Flags&XREDIS_REPLY_NULL_VALUE))return XREDIS_TRACKING_MALFORMED;
    }
    *Output=E;return XREDIS_TRACKING_EVENT;
}
#endif

#endif
