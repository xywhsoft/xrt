#include <xrpc/id.h>
#include "../internal/xrpc_string.h"
#include <xrt/memory.h>

#if defined(XRPC_FEATURE_XRPC_ID)
typedef struct xrpcidexponent {
	xstrview Digits;
	/* The bounded native offset is stored as little-endian decimal digits.
	 * sizeof(size_t)*3 is larger than its decimal representation on all targets. */
	unsigned char Small[sizeof(size_t)*3u];
	size_t SmallCount, Length;
	bool Subtract, Swap, Negative;
} xrpcidexponent;
typedef struct xrpcidplan {
	xrpcidconfig Config;
	xjsonscantype Type;
	xstrview Raw;
	size_t Bytes, First, Last, Digits;
	bool Negative;
	xrpcidexponent Exponent;
} xrpcidplan;

static bool xrpcIdReport(xerrkind Kind, xrpciderror Code, cstr Message)
{ xrtSetErrorInfo(Kind, "xrt.rpc", (int32)Code, Message); return false; }
XRPC_API bool xrpcIdConfigInit(xrpcidconfig* Config)
{
	if (!Config || !xrtMemRangeValid(Config, sizeof(*Config)))
		return xrpcIdReport(XERR_ARGUMENT, XRPC_ID_ARGUMENT, "invalid RPC ID config output");
	*Config = (xrpcidconfig){sizeof(*Config),4096,4096,8192}; return true;
}
static xjsonvisitaction xrpcIdToken(const xjsonscanevent* Event, ptr User)
{
	xrpcidplan* Plan = User;
	if (Event->Type != XJSON_SCAN_STRING && Event->Type != XJSON_SCAN_NUMBER && Event->Type != XJSON_SCAN_NULL) {
		xrpcIdReport(XERR_VALUE, XRPC_ID_VALUE, "RPC ID requires a string, number or null"); return XJSON_VISIT_FAIL;
	}
	Plan->Type = Event->Type; Plan->Raw = Event->Raw; return XJSON_VISIT_NEXT;
}

/* Read one digit from the right. No exponent is converted to a native value. */
static unsigned xrpcIdBigDigit(const xrpcidexponent* Exponent, size_t Position)
{ return Position < Exponent->Digits.Size ? (unsigned)(Exponent->Digits.Data[Exponent->Digits.Size-1u-Position]-'0') : 0; }
static unsigned xrpcIdSmallDigit(const xrpcidexponent* Exponent, size_t Position)
{ return Position < Exponent->SmallCount ? Exponent->Small[Position] : 0; }
static size_t xrpcIdExponentRun(const xrpcidexponent* Exponent, unsigned char* Output)
{
	size_t Count = Exponent->Digits.Size > Exponent->SmallCount ? Exponent->Digits.Size : Exponent->SmallCount;
	size_t Length = 1; unsigned Carry = 0;
	for (size_t Index = 0; Index < Count; ++Index) {
		unsigned A = xrpcIdBigDigit(Exponent, Index), B = xrpcIdSmallDigit(Exponent, Index), Digit;
		if (Exponent->Swap) { unsigned Temp = A; A = B; B = Temp; }
		if (Exponent->Subtract) {
			int Difference = (int)A - (int)B - (int)Carry;
			Carry = Difference < 0; Digit = (unsigned)(Difference + (Carry ? 10 : 0));
		} else { unsigned Sum = A + B + Carry; Digit = Sum % 10u; Carry = Sum / 10u; }
		if (Digit) Length = Index+1u;
		if (Output && Index < Exponent->Length) Output[Exponent->Length-1u-Index] = (unsigned char)('0'+Digit);
	}
	if (!Exponent->Subtract && Carry) {
		Length = Count+1u;
		if (Output) Output[0] = (unsigned char)('0'+Carry);
	}
	return Length;
}
static void xrpcIdExponentInit(xrpcidexponent* Exponent, xstrview Digits, bool Negative,
	size_t Offset, bool OffsetNegative)
{
	while (Digits.Size > 1 && Digits.Data[0] == '0') { ++Digits.Data; --Digits.Size; }
	if (Digits.Size == 1 && Digits.Data[0] == '0') Negative = false;
	Exponent->Digits = Digits;
	do { Exponent->Small[Exponent->SmallCount++] = (unsigned char)(Offset % 10u); Offset /= 10u; } while (Offset);
	if (Exponent->SmallCount == 1 && !Exponent->Small[0]) OffsetNegative = false;
	int Order = Digits.Size > Exponent->SmallCount ? 1 : Digits.Size < Exponent->SmallCount ? -1 : 0;
	if (!Order) for (size_t Index = 0; Index < Digits.Size; ++Index) {
		unsigned A = (unsigned)(Digits.Data[Index]-'0'), B = Exponent->Small[Digits.Size-1u-Index];
		if (A != B) { Order = A > B ? 1 : -1; break; }
	}
	Exponent->Subtract = Negative != OffsetNegative;
	Exponent->Swap = Exponent->Subtract && Order < 0;
	Exponent->Negative = Exponent->Subtract ? (Order < 0 ? OffsetNegative : Negative) : Negative;
	if (Exponent->Subtract && !Order) Exponent->Negative = false;
	Exponent->Length = xrpcIdExponentRun(Exponent, NULL);
}
static bool xrpcIdNumber(xrpcidplan* Plan)
{
	xstrview Raw = Plan->Raw; size_t Fraction = 0, Zeros = 0, Position = 0;
	bool Dot = false, Nonzero = false; Plan->Negative = Raw.Data[0] == '-';
	if (Plan->Negative) ++Position;
	for (; Position < Raw.Size && Raw.Data[Position] != 'e' && Raw.Data[Position] != 'E'; ++Position) {
		char Byte = Raw.Data[Position];
		if (Byte == '.') { Dot = true; continue; }
		if (Dot) ++Fraction;
		if (Byte != '0') {
			if (!Nonzero) Plan->First = Position;
			Nonzero = true; Plan->Last = Position; Zeros = 0;
		} else ++Zeros;
	}
	if (!Nonzero) { Plan->Bytes = 2; return true; }
	for (size_t Index = Plan->First; Index <= Plan->Last; ++Index) if (Raw.Data[Index] != '.') ++Plan->Digits;
	xstrview Digits = XRT_STR_LITERAL("0"); bool Negative = false;
	if (Position < Raw.Size) {
		++Position;
		if (Raw.Data[Position] == '+' || Raw.Data[Position] == '-') Negative = Raw.Data[Position++] == '-';
		Digits = (xstrview){Raw.Data+Position,Raw.Size-Position};
	}
	xrpcIdExponentInit(&Plan->Exponent, Digits, Negative, Zeros > Fraction ? Zeros-Fraction : Fraction-Zeros, Zeros < Fraction);
	/* Domain/sign, significant digits, delimiter and signed decimal exponent. */
	size_t Extra = 3u + (Plan->Exponent.Negative ? 1u : 0u);
	if (Plan->Digits > Plan->Config.MaxKeyBytes || Extra > Plan->Config.MaxKeyBytes-Plan->Digits ||
		Plan->Exponent.Length > Plan->Config.MaxKeyBytes-Plan->Digits-Extra)
		return xrpcIdReport(XERR_RANGE, XRPC_ID_LIMIT, "RPC numeric ID key limit exceeded");
	Plan->Bytes = Plan->Digits + Extra + Plan->Exponent.Length; return true;
}
static bool xrpcIdPrepare(xrpcidplan* Plan, xstrview Input, const xrpcidconfig* Config)
{
	if ((!Input.Data && Input.Size) || !xrtMemRangeValid(Input.Data, Input.Size) ||
		(Config && !xrtMemRangeValid(Config, sizeof(*Config))))
		return xrpcIdReport(XERR_ARGUMENT, XRPC_ID_ARGUMENT, "invalid RPC ID input/config span");
	if (Config) Plan->Config = *Config; else xrpcIdConfigInit(&Plan->Config);
	const xrpcidconfig* Limits = &Plan->Config;
	if (Limits->Size != sizeof(*Limits) || !Limits->MaxInputBytes || !Limits->MaxStringBytes || !Limits->MaxKeyBytes)
		return xrpcIdReport(XERR_ARGUMENT, XRPC_ID_ARGUMENT, "invalid RPC ID configuration");
	if (Input.Size > Limits->MaxInputBytes)
		return xrpcIdReport(XERR_RANGE, XRPC_ID_LIMIT, "RPC ID input byte limit exceeded");
	xjsonreadconfig Json; xrtJsonReadConfigInit(&Json);
	Json.MaxInputBytes = Limits->MaxInputBytes; Json.MaxStringBytes = Limits->MaxStringBytes;
	Json.MaxDepth = 1; Json.MaxValues = Json.MaxContainerItems = 1;
	if (xrtJsonScan(Input, &Json, xrpcIdToken, Plan) != XJSON_VISIT_DONE) return false;
	if (Plan->Type == XJSON_SCAN_NUMBER) {
		if (!xrpcIdNumber(Plan)) return false;
	} else if (Plan->Type == XJSON_SCAN_NULL) Plan->Bytes = 1;
	else {
		Plan->Bytes = 1; xrpcstringiterator It = {Plan->Raw,1,{0},0,0}; unsigned char Byte;
		while (xrpcStringNext(&It, &Byte)) {
			if (Plan->Bytes == Limits->MaxKeyBytes)
				return xrpcIdReport(XERR_RANGE, XRPC_ID_LIMIT, "RPC string ID key limit exceeded");
			++Plan->Bytes;
		}
	}
	if (Plan->Bytes > Limits->MaxKeyBytes)
		return xrpcIdReport(XERR_RANGE, XRPC_ID_LIMIT, "RPC ID key limit exceeded");
	return true;
}
static bool xrpcIdOutputSafe(const void* Output, size_t Size, xstrview Input, const xrpcidconfig* Config)
{
	if (!xrtMemRangeValid(Output, Size) || xrtMemRangesOverlap(Output, Size, Input.Data, Input.Size) ||
		(Config && xrtMemRangesOverlap(Output, Size, Config, sizeof(*Config))))
		return xrpcIdReport(XERR_ARGUMENT, XRPC_ID_ARGUMENT, "RPC ID output overlaps input/config or wraps");
	return true;
}
XRPC_API bool xrpcIdKeySize(xstrview Input, const xrpcidconfig* Config, size_t* Required)
{
	xrpcidplan Plan = {0};
	if (!Required) return xrpcIdReport(XERR_ARGUMENT, XRPC_ID_ARGUMENT, "RPC ID key size requires output");
	if (!xrpcIdPrepare(&Plan, Input, Config) || !xrpcIdOutputSafe(Required, sizeof(*Required), Input, Config)) return false;
	*Required = Plan.Bytes; return true;
}
static void xrpcIdEmit(const xrpcidplan* Plan, unsigned char* Output)
{
	if (Plan->Type == XJSON_SCAN_NULL) { Output[0] = 0x10; return; }
	if (Plan->Type == XJSON_SCAN_STRING) {
		*Output++ = 0x11; xrpcstringiterator It = {Plan->Raw,1,{0},0,0}; unsigned char Byte;
		while (xrpcStringNext(&It, &Byte)) *Output++ = Byte;
		return;
	}
	*Output++ = 0x12;
	if (!Plan->Digits) { *Output = '0'; return; }
	*Output++ = Plan->Negative ? '-' : '+';
	for (size_t Index = Plan->First; Index <= Plan->Last; ++Index)
		if (Plan->Raw.Data[Index] != '.') *Output++ = (unsigned char)Plan->Raw.Data[Index];
	*Output++ = 0;
	if (Plan->Exponent.Negative) *Output++ = '-';
	(void)xrpcIdExponentRun(&Plan->Exponent, Output);
}
XRPC_API bool xrpcIdKeyWrite(ptr Output, size_t Capacity, xstrview Input,
	const xrpcidconfig* Config, size_t* Written)
{
	xrpcidplan Plan = {0};
	if (!Output || !xrtMemRangeValid(Output, Capacity))
		return xrpcIdReport(XERR_ARGUMENT, XRPC_ID_ARGUMENT, "invalid RPC ID key buffer");
	if (!xrpcIdPrepare(&Plan, Input, Config)) return false;
	if (Capacity < Plan.Bytes) return xrpcIdReport(XERR_RANGE, XRPC_ID_CAPACITY, "RPC ID key capacity is too small");
	if (!xrpcIdOutputSafe(Output, Plan.Bytes, Input, Config) || (Written &&
		(!xrpcIdOutputSafe(Written, sizeof(*Written), Input, Config) || xrtMemRangesOverlap(Output, Plan.Bytes, Written, sizeof(*Written))))) return false;
	xrpcIdEmit(&Plan, Output);
	if (Written) *Written = Plan.Bytes;
	return true;
}
#endif
