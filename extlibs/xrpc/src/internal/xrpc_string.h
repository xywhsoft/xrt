#ifndef XRPC_INTERNAL_STRING_H
#define XRPC_INTERNAL_STRING_H
#include <xrpc/features.h>
#include <xrt/charset.h>

#if defined(XRPC_FEATURE_XRPC_METHODS) || defined(XRPC_FEATURE_XRPC_ID)
/* Interpret ONLY an already fully validated strict JSON string token. The
 * public raw scanner must reach DONE before any use. No grammar validation,
 * allocation or retained view is added here; scalar escapes use public Unicode. */
typedef struct xrpcstringiterator {
	xstrview Raw;
	size_t Position;
	char Pending[4];
	unsigned Read, Count;
} xrpcstringiterator;
static uint32 xrpcStringHex(xrpcstringiterator* It)
{
	uint32 Value = 0;
	for (unsigned Index = 0; Index < 4; ++Index) {
		unsigned Byte = (unsigned char)It->Raw.Data[It->Position++];
		Value = Value * 16u + (Byte <= '9' ? Byte-'0' : Byte <= 'F' ? Byte-'A'+10u : Byte-'a'+10u);
	}
	return Value;
}
static bool xrpcStringNext(xrpcstringiterator* It, unsigned char* Byte)
{
	if (It->Read < It->Count) { *Byte = (unsigned char)It->Pending[It->Read++]; return true; }
	if (It->Position == It->Raw.Size-1u) return false;
	unsigned char Value = (unsigned char)It->Raw.Data[It->Position++];
	if (Value != '\\') { *Byte = Value; return true; }
	Value = (unsigned char)It->Raw.Data[It->Position++];
	if (Value == 'u') {
		uint32 Scalar = xrpcStringHex(It);
		if (Scalar >= 0xd800u && Scalar <= 0xdbffu) {
			It->Position += 2; uint32 Low = xrpcStringHex(It);
			Scalar = 0x10000u + ((Scalar-0xd800u) << 10u) + Low-0xdc00u;
		}
		It->Count = (unsigned)xrtUtf8Encode(Scalar, It->Pending); It->Read = 1;
		*Byte = (unsigned char)It->Pending[0]; return true;
	}
	if (Value == 'b') Value = '\b'; else if (Value == 'f') Value = '\f';
	else if (Value == 'n') Value = '\n'; else if (Value == 'r') Value = '\r'; else if (Value == 't') Value = '\t';
	*Byte = Value; return true;
}
#endif
#endif
