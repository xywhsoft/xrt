/*
 * MIT License
 *
 * Copyright (c) 2025 xLeaves [xywhsoft] <xywhsoft@qq.com>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

/* 此文件由 tools/amalgamate.py 生成，请勿直接修改。 */
/* Supply XRT and selected extension dependencies before this header. */
#if !defined(XRT_CORE_H)
#error "xredis requires XRT; include xrt.h or xrt_decl.h first"
#endif
#if defined(XREDIS_IMPLEMENTATION) && defined(_MSC_VER) && \
	!defined(_CRT_SECURE_NO_WARNINGS)
	#define _CRT_SECURE_NO_WARNINGS
#endif
#if defined(XREDIS_IMPLEMENTATION) && \
	!defined(_WIN32) && !defined(_WIN64)
	#if defined(__linux__) && !defined(_GNU_SOURCE)
		#define _GNU_SOURCE 1
	#endif
	#if defined(__APPLE__) && !defined(_DARWIN_C_SOURCE)
		#define _DARWIN_C_SOURCE 1
	#endif
	#if !defined(_POSIX_C_SOURCE)
		#define _POSIX_C_SOURCE 200809L
	#endif
	#if !defined(_FILE_OFFSET_BITS)
		#define _FILE_OFFSET_BITS 64
	#endif
#endif
#ifndef XREDIS_SINGLE_HEADER_H
#define XREDIS_SINGLE_HEADER_H
#define XREDIS_SINGLE_HEADER 1


/* ========================================================================== */
/* feature selection: extlibs/xredis/include/xredis/features.h */
/* ========================================================================== */

/* 此文件由 tools/generate_extension_features.py 生成，请勿直接修改。 */
#ifndef XREDIS_FEATURES_H
#define XREDIS_FEATURES_H

/* xredis 及其直接依赖。 */
#if defined(XREDIS_MODULE_ALL) || defined(XREDIS_MODULE_XREDIS)
#ifndef XREDIS_FEATURE_XREDIS
#define XREDIS_FEATURE_XREDIS
#endif
#ifndef XREDIS_MODULE_XREDIS_COMMAND
#define XREDIS_MODULE_XREDIS_COMMAND
#endif
#endif

/* xredis_command 及其直接依赖。 */
#if defined(XREDIS_MODULE_ALL) || defined(XREDIS_MODULE_XREDIS_COMMAND)
#ifndef XREDIS_FEATURE_XREDIS_COMMAND
#define XREDIS_FEATURE_XREDIS_COMMAND
#endif
#endif

#endif /* XREDIS_FEATURES_H */


/* ========================================================================== */
/* public: extlibs/xredis/include/xredis/export.h */
/* ========================================================================== */

#ifndef XREDIS_EXPORT_H
#define XREDIS_EXPORT_H


#if defined(_WIN32) || defined(_WIN64)
 #if defined(XREDIS_BUILD_SHARED)
  #define XREDIS_API __declspec(dllexport)
 #elif defined(XREDIS_USE_SHARED)
  #define XREDIS_API __declspec(dllimport)
 #else
  #define XREDIS_API
 #endif
#elif defined(__GNUC__) || defined(__clang__)
 #if defined(XREDIS_BUILD_SHARED)
  #define XREDIS_API __attribute__((visibility("default")))
 #else
  #define XREDIS_API
 #endif
#else
 #define XREDIS_API
#endif

#endif


/* ========================================================================== */
/* public: extlibs/xredis/include/xredis/command.h */
/* ========================================================================== */

#ifndef XREDIS_COMMAND_H
#define XREDIS_COMMAND_H


#if defined(XREDIS_FEATURE_XREDIS_COMMAND)

/* These error codes belong to the xrt.redis domain, not server error replies. */
typedef enum xrediscommanderror {
	XREDIS_COMMAND_ARGUMENT = 1,
	XREDIS_COMMAND_OVERFLOW,
	XREDIS_COMMAND_CAPACITY,
	XREDIS_COMMAND_OVERLAP
} xrediscommanderror;

XRT_EXTERN_C_BEGIN

/* Measure one RESP array of bulk strings, shared by RESP2 and RESP3 commands.
 * Count must be positive; argument zero is the command name. Arguments borrow
 * stable, readable memory for this call. NULL/0 is a valid empty argument;
 * NULL/nonzero is invalid. Embedded NUL and CRLF are ordinary payload bytes.
 * A length/count greater than INT64_MAX or size/address arithmetic overflow is
 * rejected. Required is unchanged on failure and must not alias an input.
 * Success allocates nothing and preserves the ambient XRT error. */
XREDIS_API bool xredisCommandSize(const xbytesview* Arguments, size_t Count,
	size_t* Required);

/* Write the exact measured wire bytes into caller-owned Output; no trailing NUL.
 * Written is optional. Output and Written must not overlap input descriptors,
 * input payloads, or each other. Only the actual wire span is an output span.
 * All validation precedes the first write: Output and Written are unchanged on
 * failure. Success copies each payload exactly once, allocates nothing, retains
 * nothing and preserves the ambient error. The caller supplies actual readable/
 * writable objects; address-wrap checks do not validate arbitrary addresses.
 * This contiguous convenience encoder is not a zero-copy send API. */
XREDIS_API bool xredisCommandWrite(ptr Output, size_t Capacity,
	const xbytesview* Arguments, size_t Count, size_t* Written);

XRT_EXTERN_C_END

#endif
#endif


/* ========================================================================== */
/* public: extlibs/xredis/include/xredis.h */
/* ========================================================================== */

#ifndef XREDIS_H
#define XREDIS_H


#if defined(XREDIS_FEATURE_XREDIS) && !defined(XREDIS_FEATURE_XREDIS_COMMAND)
#error "XREDIS_FEATURE_XREDIS requires XREDIS_FEATURE_XREDIS_COMMAND"
#endif


#endif

#endif

#if defined(XREDIS_IMPLEMENTATION) && !defined(XREDIS_IMPLEMENTATION_ONCE)
#define XREDIS_IMPLEMENTATION_ONCE 1


/* ========================================================================== */
/* source: extlibs/xredis/src/command/command.c */
/* ========================================================================== */

#if defined(XREDIS_FEATURE_XREDIS_COMMAND)
#include <string.h>

#if defined(XREDIS_FEATURE_XREDIS_COMMAND)

static bool xredisCommandFail(xerrkind Kind, xrediscommanderror Code,
	cstr Message)
{
	xrtSetErrorInfo(Kind, "xrt.redis", (int32)Code, Message);
	return false;
}

static size_t xredisCommandDigits(size_t Value)
{
	size_t Count = 1;
	while (Value >= 10u) { Value /= 10u; ++Count; }
	return Count;
}

static bool xredisCommandMeasure(const xbytesview* Arguments, size_t Count,
	size_t* Size)
{
	size_t Total;
	if (Count == 0 || Arguments == NULL)
		return xredisCommandFail(XERR_ARGUMENT, XREDIS_COMMAND_ARGUMENT,
			"a command requires a nonempty argument vector");
	if (Count > (uint64)INT64_MAX || Count > SIZE_MAX / sizeof(*Arguments) ||
		!xrtMemRangeValid(Arguments, Count * sizeof(*Arguments)))
		return xredisCommandFail(XERR_RANGE, XREDIS_COMMAND_OVERFLOW,
			"command argument count or descriptor span overflows");
	Total = 3u + xredisCommandDigits(Count); /* *Count CRLF */
	for (size_t Index = 0; Index < Count; ++Index) {
		size_t Bytes = Arguments[Index].Size;
		size_t Overhead;
		if (Arguments[Index].Data == NULL && Bytes != 0)
			return xredisCommandFail(XERR_ARGUMENT, XREDIS_COMMAND_ARGUMENT,
				"a nonempty command argument requires data");
		if (Bytes > (uint64)INT64_MAX ||
			!xrtMemRangeValid(Arguments[Index].Data, Bytes))
			return xredisCommandFail(XERR_RANGE, XREDIS_COMMAND_OVERFLOW,
				"command argument length or address span overflows");
		Overhead = 5u + xredisCommandDigits(Bytes); /* $Bytes CRLF data CRLF */
		if (Bytes > SIZE_MAX - Overhead || Total > SIZE_MAX - Bytes - Overhead)
			return xredisCommandFail(XERR_RANGE, XREDIS_COMMAND_OVERFLOW,
				"encoded command size overflows");
		Total += Bytes + Overhead;
	}
	*Size = Total;
	return true;
}

static bool xredisCommandOutputSafe(const void* Output, size_t Size,
	const xbytesview* Arguments, size_t Count)
{
	if (!xrtMemRangeValid(Output, Size))
		return xredisCommandFail(XERR_ARGUMENT, XREDIS_COMMAND_ARGUMENT,
			"command output address span is invalid");
	if (xrtMemRangesOverlap(Output, Size, Arguments,
		Count * sizeof(*Arguments)))
		return xredisCommandFail(XERR_ARGUMENT, XREDIS_COMMAND_OVERLAP,
			"command output overlaps argument descriptors");
	for (size_t Index = 0; Index < Count; ++Index) {
		if (xrtMemRangesOverlap(Output, Size,
			Arguments[Index].Data, Arguments[Index].Size))
			return xredisCommandFail(XERR_ARGUMENT, XREDIS_COMMAND_OVERLAP,
				"command output overlaps argument data");
	}
	return true;
}

static unsigned char* xredisCommandNumber(unsigned char* Output, size_t Value)
{
	unsigned char Digits[3u * sizeof(size_t)];
	size_t Count = 0;
	do { Digits[Count++] = (unsigned char)('0' + Value % 10u); Value /= 10u; }
	while (Value != 0);
	while (Count != 0) *Output++ = Digits[--Count];
	*Output++ = '\r'; *Output++ = '\n';
	return Output;
}

XREDIS_API bool xredisCommandSize(const xbytesview* Arguments, size_t Count,
	size_t* Required)
{
	size_t Size;
	if (Required == NULL)
		return xredisCommandFail(XERR_ARGUMENT, XREDIS_COMMAND_ARGUMENT,
			"command size requires an output");
	if (!xredisCommandMeasure(Arguments, Count, &Size) ||
		!xredisCommandOutputSafe(Required, sizeof(*Required), Arguments, Count))
		return false;
	*Required = Size;
	return true;
}

XREDIS_API bool xredisCommandWrite(ptr Output, size_t Capacity,
	const xbytesview* Arguments, size_t Count, size_t* Written)
{
	size_t Size;
	unsigned char* Cursor;
	if (!xredisCommandMeasure(Arguments, Count, &Size)) return false;
	if (Output == NULL)
		return xredisCommandFail(XERR_ARGUMENT, XREDIS_COMMAND_ARGUMENT,
			"command write requires an output buffer");
	if (Capacity < Size)
		return xredisCommandFail(XERR_RANGE, XREDIS_COMMAND_CAPACITY,
			"command output capacity is too small");
	if (!xredisCommandOutputSafe(Output, Size, Arguments, Count)) return false;
	if (Written != NULL) {
		if (!xredisCommandOutputSafe(Written, sizeof(*Written), Arguments, Count))
			return false;
		if (xrtMemRangesOverlap(Output, Size, Written, sizeof(*Written)))
			return xredisCommandFail(XERR_ARGUMENT, XREDIS_COMMAND_OVERLAP,
				"command size output overlaps wire bytes");
	}
	Cursor = Output;
	*Cursor++ = '*'; Cursor = xredisCommandNumber(Cursor, Count);
	for (size_t Index = 0; Index < Count; ++Index) {
		*Cursor++ = '$'; Cursor = xredisCommandNumber(Cursor, Arguments[Index].Size);
		if (Arguments[Index].Size != 0) {
			memcpy(Cursor, Arguments[Index].Data, Arguments[Index].Size);
			Cursor += Arguments[Index].Size;
		}
		*Cursor++ = '\r'; *Cursor++ = '\n';
	}
	if (Written != NULL) *Written = Size;
	return true;
}

#endif
#endif

#endif
