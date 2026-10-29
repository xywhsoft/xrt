#include <xrpc/methods.h>
#include <xrt/atomic.h>
#include <xrt/charset.h>
#include <xrt/memory.h>
#include "../internal/xrpc_string.h"
#include <string.h>

#if defined(XRPC_FEATURE_XRPC_METHODS)
typedef struct xrpcmethodsentry { uint32 Hash; xrpcmethod Binding; } xrpcmethodsentry;
struct xrpcmethods {
	xatomic32 References;
	xrpcmethodsconfig Config;
	size_t Count, NameBytes, OwnedBytes;
	xrpcmethodsentry Entries[];
};

static bool xrpcMethodsReport(xerrkind Kind, xrpcmethodserror Code, cstr Message)
{ xrtSetErrorInfo(Kind, "xrt.rpc", (int32)Code, Message); return false; }

XRPC_API bool xrpcMethodsConfigInit(xrpcmethodsconfig* Config)
{
	if (Config == NULL || !xrtMemRangeValid(Config, sizeof(*Config)))
		return xrpcMethodsReport(XERR_ARGUMENT, XRPC_METHODS_ARGUMENT, "invalid RPC method config output");
	*Config = (xrpcmethodsconfig){sizeof(*Config),4096,4096,24578,1024u*1024u,2u*1024u*1024u}; return true;
}

static uint32 xrpcMethodsHash(xstrview Name)
{
	uint32 Hash = 2166136261u;
	for (size_t Index = 0; Index < Name.Size; ++Index) Hash = (Hash ^ (unsigned char)Name.Data[Index]) * 16777619u;
	return Hash;
}
static int xrpcMethodsCompareNames(xstrview A, xstrview B)
{
	size_t Common = A.Size < B.Size ? A.Size : B.Size;
	int Order = Common ? memcmp(A.Data, B.Data, Common) : 0;
	return Order ? Order : (A.Size > B.Size) - (A.Size < B.Size);
}
static int xrpcMethodsCompareEntries(const xrpcmethodsentry* A, const xrpcmethodsentry* B)
{
	return A->Hash != B->Hash ? (A->Hash > B->Hash ? 1 : -1) : xrpcMethodsCompareNames(A->Binding.Name, B->Binding.Name);
}
static void xrpcMethodsSift(xrpcmethodsentry* Entries, size_t Count, size_t Root)
{
	while (Root < Count / 2u) {
		size_t Child = Root * 2u + 1u;
		if (Child + 1u < Count && xrpcMethodsCompareEntries(&Entries[Child], &Entries[Child+1u]) < 0) ++Child;
		if (xrpcMethodsCompareEntries(&Entries[Root], &Entries[Child]) >= 0) break;
		xrpcmethodsentry Temp = Entries[Root]; Entries[Root] = Entries[Child]; Entries[Child] = Temp; Root = Child;
	}
}
static void xrpcMethodsSort(xrpcmethodsentry* Entries, size_t Count)
{
	for (size_t Root = Count / 2u; Root; --Root) xrpcMethodsSift(Entries, Count, Root-1u);
	for (size_t End = Count; End > 1; --End) {
		xrpcmethodsentry Temp = Entries[0]; Entries[0] = Entries[End-1u]; Entries[End-1u] = Temp;
		xrpcMethodsSift(Entries, End-1u, 0);
	}
}

XRPC_API bool xrpcMethodsCreate(const xrpcmethod* Source, size_t Count,
	const xrpcmethodsconfig* Config, xrpcmethods** Result)
{
	if (Result == NULL || !xrtMemRangeValid(Result, sizeof(*Result)) ||
		(Count && Source == NULL) || Count > SIZE_MAX / sizeof(*Source) ||
		!xrtMemRangeValid(Source, Count * sizeof(*Source)) ||
		(Config && !xrtMemRangeValid(Config, sizeof(*Config))))
		return xrpcMethodsReport(XERR_ARGUMENT, XRPC_METHODS_ARGUMENT, "invalid RPC method constructor span");
	if (xrtMemRangesOverlap(Result, sizeof(*Result), Source, Count * sizeof(*Source)) ||
		(Config && xrtMemRangesOverlap(Result, sizeof(*Result), Config, sizeof(*Config))))
		return xrpcMethodsReport(XERR_ARGUMENT, XRPC_METHODS_ARGUMENT, "RPC method output overlaps input");
	xrpcmethodsconfig Limits;
	if (Config) Limits = *Config; else xrpcMethodsConfigInit(&Limits);
	if (Limits.Size != sizeof(Limits) || !Limits.MaxMethods || !Limits.MaxNameBytes || !Limits.MaxRawNameBytes ||
		!Limits.MaxTotalNameBytes || !Limits.MaxOwnedBytes)
		return xrpcMethodsReport(XERR_ARGUMENT, XRPC_METHODS_ARGUMENT, "invalid RPC method configuration");
	if (Count > Limits.MaxMethods || Count > (SIZE_MAX-sizeof(xrpcmethods))/sizeof(xrpcmethodsentry))
		return xrpcMethodsReport(XERR_RANGE, XRPC_METHODS_LIMIT, "RPC method count limit exceeded");
	size_t Names = 0, Bytes = sizeof(xrpcmethods) + Count * sizeof(xrpcmethodsentry);
	for (size_t Index = 0; Index < Count; ++Index) {
		xstrview Name = Source[Index].Name;
		if (!Source[Index].Call || (!Name.Data && Name.Size) || !xrtMemRangeValid(Name.Data, Name.Size) ||
			xrtMemRangesOverlap(Result, sizeof(*Result), Name.Data, Name.Size))
			return xrpcMethodsReport(XERR_ARGUMENT, XRPC_METHODS_ARGUMENT, "invalid RPC method binding/name/output overlap");
		if (Name.Size > Limits.MaxNameBytes || Name.Size > Limits.MaxTotalNameBytes-Names || Name.Size > SIZE_MAX-Bytes)
			return xrpcMethodsReport(XERR_RANGE, XRPC_METHODS_LIMIT, "RPC method name/storage limit exceeded");
		if (!xrtUtf8Valid(Name, NULL)) return false;
		if (Name.Size >= 4 && memcmp(Name.Data, "rpc.", 4) == 0)
			return xrpcMethodsReport(XERR_VALUE, XRPC_METHODS_VALUE, "reserved RPC method cannot be registered");
		Names += Name.Size; Bytes += Name.Size;
	}
	if (Bytes > Limits.MaxOwnedBytes)
		return xrpcMethodsReport(XERR_RANGE, XRPC_METHODS_LIMIT, "RPC method owned-byte limit exceeded");
	xrpcmethods* Methods = xrtMalloc(Bytes);
	if (!Methods) return xrpcMethodsReport(XERR_MEMORY, XRPC_METHODS_MEMORY, "RPC method allocation failed");
	xrtAtomic32Init(&Methods->References, 1); Methods->Config = Limits;
	Methods->Count = Count; Methods->NameBytes = Names; Methods->OwnedBytes = Bytes;
	char* Next = (char*)(Methods->Entries + Count);
	for (size_t Index = 0; Index < Count; ++Index) {
		xrpcmethod Binding = Source[Index];
		if (Binding.Name.Size) memcpy(Next, Binding.Name.Data, Binding.Name.Size);
		Binding.Name.Data = Next; Next += Binding.Name.Size;
		Methods->Entries[Index].Hash = xrpcMethodsHash(Binding.Name); Methods->Entries[Index].Binding = Binding;
	}
	xrpcMethodsSort(Methods->Entries, Count);
	for (size_t Index = 1; Index < Count; ++Index) if (!xrpcMethodsCompareEntries(&Methods->Entries[Index-1u], &Methods->Entries[Index])) {
		xrtFree(Methods); return xrpcMethodsReport(XERR_EXISTS, XRPC_METHODS_DUPLICATE, "duplicate RPC method name");
	}
	*Result = Methods; return true;
}

XRPC_API bool xrpcMethodsRetain(xrpcmethods* Methods)
{
	if (!Methods) return xrpcMethodsReport(XERR_ARGUMENT, XRPC_METHODS_ARGUMENT, "RPC method index is NULL");
	uint32 Count = xrtAtomic32Load(&Methods->References, XMEMORY_RELAXED);
	for (;;) {
		if (Count == UINT32_MAX) return xrpcMethodsReport(XERR_RANGE, XRPC_METHODS_LIMIT, "RPC method references overflow");
		if (xrtAtomic32CompareExchange(&Methods->References, &Count, Count+1u, XMEMORY_RELAXED, XMEMORY_RELAXED)) return true;
	}
}
XRPC_API void xrpcMethodsRelease(xrpcmethods* Methods)
{
	if (Methods && xrtAtomic32FetchSub(&Methods->References, 1, XMEMORY_ACQ_REL) == 1) xrtFree(Methods);
}
XRPC_API bool xrpcMethodsInfo(const xrpcmethods* Methods, xrpcmethodsinfo* Info)
{
	if (!Methods || !Info || !xrtMemRangeValid(Info, sizeof(*Info)) || Info->Size != sizeof(*Info) ||
		xrtMemRangesOverlap(Methods, Methods->OwnedBytes, Info, sizeof(*Info)))
		return xrpcMethodsReport(XERR_ARGUMENT, XRPC_METHODS_ARGUMENT, "invalid RPC method info output");
	*Info = (xrpcmethodsinfo){sizeof(*Info),Methods->Count,Methods->NameBytes,Methods->OwnedBytes}; return true;
}

static xjsonvisitaction xrpcMethodsToken(const xjsonscanevent* Event, ptr User)
{
	if (Event->Type != XJSON_SCAN_STRING) {
		xrpcMethodsReport(XERR_VALUE, XRPC_METHODS_VALUE, "RPC method lookup requires one JSON string"); return XJSON_VISIT_FAIL;
	}
	*(xstrview*)User = Event->Raw; return XJSON_VISIT_NEXT;
}
static int xrpcMethodsCompareRaw(xstrview Raw, xstrview Name)
{
	xrpcstringiterator It = {Raw,1,{0},0,0}; unsigned char Byte; size_t Index = 0;
	while (xrpcStringNext(&It, &Byte)) {
		if (Index == Name.Size) return 1;
		unsigned char Other = (unsigned char)Name.Data[Index++];
		if (Byte != Other) return Byte > Other ? 1 : -1;
	}
	return Index == Name.Size ? 0 : -1;
}
XRPC_API xrpcmethodsstatus xrpcMethodsFind(const xrpcmethods* Methods, xstrview Input, xrpcmethod* Binding)
{
	if (!Methods || !Binding || !xrtMemRangeValid(Binding, sizeof(*Binding)) ||
		(!Input.Data && Input.Size) || !xrtMemRangeValid(Input.Data, Input.Size) ||
		xrtMemRangesOverlap(Binding, sizeof(*Binding), Input.Data, Input.Size) ||
		xrtMemRangesOverlap(Binding, sizeof(*Binding), Methods, Methods->OwnedBytes)) {
		xrpcMethodsReport(XERR_ARGUMENT, XRPC_METHODS_ARGUMENT, "invalid RPC method lookup input/output"); return XRPC_METHODS_ERROR;
	}
	if (Input.Size > Methods->Config.MaxRawNameBytes) {
		xrpcMethodsReport(XERR_RANGE, XRPC_METHODS_LIMIT, "RPC raw method byte limit exceeded"); return XRPC_METHODS_ERROR;
	}
	xjsonreadconfig Json; xrtJsonReadConfigInit(&Json); Json.MaxInputBytes = Methods->Config.MaxRawNameBytes;
	Json.MaxStringBytes = Methods->Config.MaxNameBytes; Json.MaxDepth = 1; Json.MaxValues = Json.MaxContainerItems = 1;
	xstrview Raw = {0};
	if (xrtJsonScan(Input, &Json, xrpcMethodsToken, &Raw) != XJSON_VISIT_DONE) return XRPC_METHODS_ERROR;
	if (!Methods->Count) return XRPC_METHODS_NOT_FOUND;
	xstrview Plain = {Raw.Data+1,Raw.Size-2u}; bool Escaped = memchr(Plain.Data, '\\', Plain.Size) != NULL;
	uint32 Hash;
	if (!Escaped) Hash = xrpcMethodsHash(Plain);
	else {
		xrpcstringiterator It = {Raw,1,{0},0,0}; unsigned char Byte; Hash = 2166136261u;
		while (xrpcStringNext(&It, &Byte)) Hash = (Hash ^ Byte) * 16777619u;
	}
	size_t Begin = 0, End = Methods->Count;
	while (Begin < End) {
		size_t Middle = Begin + (End-Begin)/2u; const xrpcmethodsentry* Entry = &Methods->Entries[Middle];
		int Order = Hash != Entry->Hash ? (Hash > Entry->Hash ? 1 : -1) :
			Escaped ? xrpcMethodsCompareRaw(Raw, Entry->Binding.Name) : xrpcMethodsCompareNames(Plain, Entry->Binding.Name);
		if (!Order) { *Binding = Entry->Binding; return XRPC_METHODS_FOUND; }
		if (Order < 0) End = Middle; else Begin = Middle+1u;
	}
	return XRPC_METHODS_NOT_FOUND;
}
XRPC_API bool xrpcMethodsCall(const xrpcmessage* Request, xrpcreply* Reply, ptr User)
{
	if (!Reply) return xrpcMethodsReport(XERR_ARGUMENT, XRPC_METHODS_ARGUMENT, "RPC method adapter reply is NULL");
	if (!Request || !xrtMemRangeValid(Request, sizeof(*Request)) ||
		(Request->Kind != XRPC_MESSAGE_CALL && Request->Kind != XRPC_MESSAGE_NOTIFICATION) || Request->Method.Kind != XRPC_VALUE_STRING) {
		xrpcMethodsReport(XERR_ARGUMENT, XRPC_METHODS_ARGUMENT, "RPC method adapter requires a valid request");
		return xrpcReplyAbort(Reply, xrtGetError());
	}
	xrpcmethod Binding; xrpcmethodsstatus Status = xrpcMethodsFind((const xrpcmethods*)User, Request->Method.Raw, &Binding);
	if (Status == XRPC_METHODS_ERROR) return xrpcReplyAbort(Reply, xrtGetError());
	return Status == XRPC_METHODS_FOUND ? Binding.Call(Request, Reply, Binding.User) : false;
}
#endif
