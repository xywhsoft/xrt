#ifndef XRPC_INTERNAL_JSON_H
#define XRPC_INTERNAL_JSON_H
#include <xrpc/types.h>

#if defined(XRPC_FEATURE_XRPC_DECODE) || defined(XRPC_FEATURE_XRPC_ENCODE)

/* The caller supplies a validated JSON number token, without whitespace.
 * Exact decimal integrality does not need native numeric conversion. Saturate
 * the exponent at token length: it already exceeds every possible mantissa. */
static bool xrpcJsonIntegral(xstrview Raw)
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

#endif
#endif
