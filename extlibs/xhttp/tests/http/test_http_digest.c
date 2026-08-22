#include "../test.h"

#include <xrt/http_digest.h>



/* 按字节比较借用视图。 */
static bool testHttpDigestViewEqual(
	xstrview Left,
	xstrview Right
)
{
	return (Left.Size == Right.Size) &&
		((Left.Size == 0) ||
		 (memcmp(Left.Data, Right.Data, Left.Size) == 0));
}



/* 验证多算法摘要解析、解码和重复 key 的最后值。 */
static void testHttpDigestVectors(void)
{
	xstrview Value = XRT_STR_LITERAL(
		"sha-256=:AA==:, sha-512=:AQI=:;source=edge, "
		"sha-256=:Aw==:"
	);
	xhttpdigestcursor Cursor;
	xhttpdigest Digest;
	uint8 arrDigest[8];
	size_t iSize;

	testRequire(
		xrtHttpDigestValid(Value),
		"valid digest Dictionary was rejected"
	);
	xrtHttpDigestCursorInit(&Cursor);
	testRequire(
		(xrtHttpDigestNext(
			Value, &Cursor, &Digest
		) == XHTTP_NEXT_ITEM) &&
		testHttpDigestViewEqual(
			Digest.Algorithm, XRT_STR_LITERAL("sha-256")
		) && xrtHttpDigestRead(
			&Digest, arrDigest, sizeof(arrDigest), &iSize
		) && (iSize == 1u) && (arrDigest[0] == 3u),
		"digest duplicate final value mismatch"
	);
	testRequire(
		(xrtHttpDigestNext(
			Value, &Cursor, &Digest
		) == XHTTP_NEXT_ITEM) &&
		testHttpDigestViewEqual(
			Digest.Algorithm, XRT_STR_LITERAL("sha-512")
		) && (Digest.Parameters.Size != 0) &&
		xrtHttpDigestRead(
			&Digest, arrDigest, sizeof(arrDigest), &iSize
		) && (iSize == 2u) &&
		(arrDigest[0] == 1u) && (arrDigest[1] == 2u),
		"second digest member mismatch"
	);
	testRequire(
		xrtHttpDigestNext(
			Value, &Cursor, &Digest
		) == XHTTP_NEXT_END,
		"digest iterator did not end"
	);
	testRequire(
		xrtHttpDigestValid(XRT_STR_LITERAL(
			"sha-256=token, sha-256=:AA==:"
		)),
		"digest duplicate final value did not replace an earlier type"
	);
}



/* 验证重复字段行按一个逻辑 Dictionary 去重组合。 */
static void testHttpDigestFields(void)
{
	static const xhttpfield Fields[] = {
		{
			XRT_STR_INIT("Content-Digest"),
			XRT_STR_INIT("sha-256=:AA==:")
		},
		{ XRT_STR_INIT("Other"), XRT_STR_INIT("ignored") },
		{
			XRT_STR_INIT("content-digest"),
			XRT_STR_INIT("sha-512=:AQ==:, sha-256=:Ag==:")
		},
		{
			XRT_STR_INIT("Repr-Digest"),
			XRT_STR_INIT("sha-256=:Aw==:")
		}
	};
	xhttpdigestcursor Cursor;
	xhttpdigest Digest;
	uint8 iDigest;
	size_t iSize;

	xrtHttpDigestCursorInit(&Cursor);
	testRequire(
		(xrtHttpDigestFieldNext(
			Fields, 4u, XHTTP_DIGEST_CONTENT,
			&Cursor, &Digest
		) == XHTTP_NEXT_ITEM) &&
		xrtHttpDigestRead(
			&Digest, &iDigest, 1u, &iSize
		) && (iDigest == 2u),
		"repeated Content-Digest final value mismatch"
	);
	testRequire(
		(xrtHttpDigestFieldNext(
			Fields, 4u, XHTTP_DIGEST_CONTENT,
			&Cursor, &Digest
		) == XHTTP_NEXT_ITEM) &&
		testHttpDigestViewEqual(
			Digest.Algorithm, XRT_STR_LITERAL("sha-512")
		),
		"repeated Content-Digest order mismatch"
	);
	xrtHttpDigestCursorInit(&Cursor);
	testRequire(
		(xrtHttpDigestFieldNext(
			Fields, 4u, XHTTP_DIGEST_REPRESENTATION,
			&Cursor, &Digest
		) == XHTTP_NEXT_ITEM) &&
		testHttpDigestViewEqual(
			Digest.Algorithm, XRT_STR_LITERAL("sha-256")
		),
		"Repr-Digest target selection mismatch"
	);
}



/* 验证摘要偏好类型、范围与扩展参数。 */
static void testHttpDigestPreferences(void)
{
	xstrview Value = XRT_STR_LITERAL(
		"sha-512=3;source=edge, sha-256=10, unixsum=0"
	);
	xhttpdigestcursor Cursor;
	xhttpdigestpreference Preference;

	testRequire(
		xrtHttpDigestPreferenceValid(Value),
		"valid digest preferences were rejected"
	);
	xrtHttpDigestCursorInit(&Cursor);
	testRequire(
		(xrtHttpDigestPreferenceNext(
			Value, &Cursor, &Preference
		) == XHTTP_NEXT_ITEM) &&
		(Preference.Weight == 3u) &&
		(Preference.Parameters.Size != 0),
		"first digest preference mismatch"
	);
	testRequire(
		(xrtHttpDigestPreferenceNext(
			Value, &Cursor, &Preference
		) == XHTTP_NEXT_ITEM) &&
		(Preference.Weight == 10u),
		"second digest preference mismatch"
	);
	testRequire(
		(xrtHttpDigestPreferenceNext(
			Value, &Cursor, &Preference
		) == XHTTP_NEXT_ITEM) &&
		(Preference.Weight == 0u),
		"zero digest preference mismatch"
	);
}



/* 验证成员类型、偏好范围和首次预校验失败原子性。 */
static void testHttpDigestFailure(void)
{
	static const xstrview InvalidDigest[] = {
		XRT_STR_INIT("sha-256=token"),
		XRT_STR_INIT("sha-256=\"digest\""),
		XRT_STR_INIT("sha-256=(1 2)")
	};
	static const xstrview InvalidPreference[] = {
		XRT_STR_INIT("sha-256=-1"),
		XRT_STR_INIT("sha-256=11"),
		XRT_STR_INIT("sha-256=?1"),
		XRT_STR_INIT("sha-256=:AA==:")
	};
	xhttpdigestcursor Cursor;
	xhttpdigestcursor SavedCursor;
	xhttpdigest Digest;
	xhttpdigest SavedDigest;
	size_t i;

	for ( i = 0; i < sizeof(InvalidDigest) /
		sizeof(InvalidDigest[0]); i++ ) {
		xrtClearError();
		testRequire(
			!xrtHttpDigestValid(InvalidDigest[i]) &&
			(xrtErrorKind(xrtGetError()) == XERR_VALUE),
			"invalid digest member was accepted"
		);
	}
	for ( i = 0; i < sizeof(InvalidPreference) /
		sizeof(InvalidPreference[0]); i++ ) {
		xrtClearError();
		testRequire(
			!xrtHttpDigestPreferenceValid(
				InvalidPreference[i]
			) && (xrtErrorKind(xrtGetError()) == XERR_VALUE),
			"invalid digest preference was accepted"
		);
	}
	xrtHttpDigestCursorInit(&Cursor);
	SavedCursor = Cursor;
	memset(&Digest, 0xA5, sizeof(Digest));
	SavedDigest = Digest;
	testRequire(
		xrtHttpDigestNext(
			XRT_STR_LITERAL("sha-256=:AA==:, bad=token"),
			&Cursor, &Digest
		) == XHTTP_NEXT_ERROR,
		"digest iterator published a valid prefix"
	);
	testRequire(
		(memcmp(&Cursor, &SavedCursor, sizeof(Cursor)) == 0) &&
		(memcmp(&Digest, &SavedDigest, sizeof(Digest)) == 0),
		"digest semantic failure was not atomic"
	);
}



/* 验证摘要游标只能继续迭代首次绑定的不可变来源和用途。 */
static void testHttpDigestCursorBinding(void)
{
	xstrview First = XRT_STR_LITERAL(
		"a=:AA==:, b=:AQ==:"
	);
	xstrview Second = XRT_STR_LITERAL(
		"c=:Ag==:, d=:Aw==:"
	);
	static const xhttpfield Fields[] = {
		{
			XRT_STR_INIT("Content-Digest"),
			XRT_STR_INIT("a=:AA==:")
		},
		{
			XRT_STR_INIT("Repr-Digest"),
			XRT_STR_INIT("a=:AQ==:")
		}
	};
	xhttpdigestcursor Cursor;
	xhttpdigestcursor SavedCursor;
	xhttpdigest Digest;
	xhttpdigest SavedDigest;
	xhttpdigestpreference Preference;
	xhttpdigestpreference SavedPreference;

	xrtHttpDigestCursorInit(&Cursor);
	testRequire(
		xrtHttpDigestNext(
			First, &Cursor, &Digest
		) == XHTTP_NEXT_ITEM,
		"digest cursor binding setup failed"
	);
	SavedCursor = Cursor;
	SavedDigest = Digest;
	xrtClearError();
	testRequire(
		(xrtHttpDigestNext(
			Second, &Cursor, &Digest
		) == XHTTP_NEXT_ERROR) &&
		(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT) &&
		(memcmp(&Cursor, &SavedCursor, sizeof(Cursor)) == 0) &&
		(memcmp(&Digest, &SavedDigest, sizeof(Digest)) == 0),
		"digest cursor switched value sources"
	);

	memset(&Preference, 0xA5, sizeof(Preference));
	SavedPreference = Preference;
	xrtClearError();
	testRequire(
		(xrtHttpDigestPreferenceNext(
			First, &Cursor, &Preference
		) == XHTTP_NEXT_ERROR) &&
		(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT) &&
		(memcmp(&Cursor, &SavedCursor, sizeof(Cursor)) == 0) &&
		(memcmp(
			&Preference, &SavedPreference, sizeof(Preference)
		) == 0),
		"digest cursor switched parser purposes"
	);

	xrtHttpDigestCursorInit(&Cursor);
	testRequire(
		xrtHttpDigestFieldNext(
			Fields, 2u, XHTTP_DIGEST_CONTENT,
			&Cursor, &Digest
		) == XHTTP_NEXT_ITEM,
		"digest field cursor binding setup failed"
	);
	SavedCursor = Cursor;
	SavedDigest = Digest;
	xrtClearError();
	testRequire(
		(xrtHttpDigestFieldNext(
			Fields, 2u, XHTTP_DIGEST_REPRESENTATION,
			&Cursor, &Digest
		) == XHTTP_NEXT_ERROR) &&
		(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT) &&
		(memcmp(&Cursor, &SavedCursor, sizeof(Cursor)) == 0) &&
		(memcmp(&Digest, &SavedDigest, sizeof(Digest)) == 0),
		"digest field cursor switched targets"
	);
	xrtClearError();
}



/* 验证未对齐对象可用，并在回绕或重叠范围下保持失败原子性。 */
static void testHttpDigestMemory(void)
{
	union {
		uint64 Align;
		uint8 Bytes[sizeof(xhttpdigestcursor) + 1u];
	} CursorStorage;
	union {
		uint64 Align;
		uint8 Bytes[sizeof(xhttpdigest) + 1u];
	} DigestStorage;
	union {
		uint64 Align;
		uint8 Bytes[sizeof(size_t) + 1u];
	} SizeStorage;
	xhttpdigestcursor* pCursor = (xhttpdigestcursor*)(
		CursorStorage.Bytes + 1u
	);
	xhttpdigest* pDigest = (xhttpdigest*)(
		DigestStorage.Bytes + 1u
	);
	size_t* pUnalignedSize = (size_t*)(SizeStorage.Bytes + 1u);
	xhttpdigest Digest;
	xhttpdigest SavedDigest;
	xhttpdigestcursor Cursor;
	xhttpdigestcursor SavedCursor;
	char arrOutput[16];
	char arrMutable[] = "sha-256=:AQI=:;p=1";
	size_t iSize;

	xrtHttpDigestCursorInit(pCursor);
	testRequire(
		xrtHttpDigestNext(
			XRT_STR_LITERAL("sha-256=:AQI=:;p=1"),
			pCursor, pDigest
		) == XHTTP_NEXT_ITEM,
		"digest parser rejected unaligned objects"
	);
	memcpy(&Digest, pDigest, sizeof(Digest));
	testRequire(
		xrtHttpDigestRead(
			pDigest, arrOutput, sizeof(arrOutput),
			pUnalignedSize
		),
		"digest reader rejected unaligned descriptors"
	);
	memcpy(&iSize, pUnalignedSize, sizeof(iSize));
	testRequire(
		(iSize == 2u) &&
		((uint8)arrOutput[0] == 1u) &&
		((uint8)arrOutput[1] == 2u),
		"digest unaligned result mismatch"
	);

	SavedDigest = Digest;
	iSize = 77u;
	xrtClearError();
	testRequire(
		!xrtHttpDigestRead(
			&Digest, (uint8*)&Digest + 1u,
			1u, &iSize
		) && (xrtErrorKind(xrtGetError()) == XERR_ARGUMENT) &&
		(iSize == 77u) &&
		(memcmp(&Digest, &SavedDigest, sizeof(Digest)) == 0),
		"digest reader accepted descriptor/output overlap"
	);
	xrtClearError();
	testRequire(
		!xrtHttpDigestRead(
			&Digest, NULL, 0,
			(size_t*)((uint8*)&Digest + 1u)
		) && (xrtErrorKind(xrtGetError()) == XERR_ARGUMENT) &&
		(memcmp(&Digest, &SavedDigest, sizeof(Digest)) == 0),
		"digest reader accepted descriptor/size overlap"
	);

	iSize = 77u;
	xrtClearError();
	testRequire(
		!xrtHttpDigestRead(
			&Digest, (void*)Digest.Algorithm.Data,
			1u, &iSize
		) && (xrtErrorKind(xrtGetError()) == XERR_ARGUMENT) &&
		(iSize == 77u),
		"digest reader accepted algorithm/output overlap"
	);
	iSize = 77u;
	xrtClearError();
	testRequire(
		!xrtHttpDigestRead(
			&Digest, (void*)Digest.Parameters.Data,
			1u, &iSize
		) && (xrtErrorKind(xrtGetError()) == XERR_ARGUMENT) &&
		(iSize == 77u),
		"digest reader accepted parameters/output overlap"
	);

	Digest = SavedDigest;
	Digest.Algorithm = (xstrview){
		(cstr)(uintptr_t)(UINTPTR_MAX - 1u), 4u
	};
	iSize = 77u;
	xrtClearError();
	testRequire(
		!xrtHttpDigestRead(
			&Digest, NULL, 0, &iSize
		) && (xrtErrorKind(xrtGetError()) == XERR_ARGUMENT) &&
		(iSize == 77u),
		"digest reader accepted wrapped algorithm"
	);
	Digest = SavedDigest;
	Digest.Parameters = (xstrview){
		(cstr)(uintptr_t)(UINTPTR_MAX - 1u), 4u
	};
	iSize = 77u;
	xrtClearError();
	testRequire(
		!xrtHttpDigestRead(
			&Digest, NULL, 0, &iSize
		) && (xrtErrorKind(xrtGetError()) == XERR_ARGUMENT) &&
		(iSize == 77u),
		"digest reader accepted wrapped parameters"
	);

	Digest = SavedDigest;
	Digest.Value.Number = 1;
	iSize = 77u;
	xrtClearError();
	testRequire(
		!xrtHttpDigestRead(
			&Digest, NULL, 0, &iSize
		) && (xrtErrorKind(xrtGetError()) == XERR_VALUE) &&
		(iSize == 77u),
		"digest reader accepted an incoherent Byte Sequence"
	);
	Digest = SavedDigest;
	Digest.Parameters = XRT_STR_LITERAL(";=");
	iSize = 77u;
	xrtClearError();
	testRequire(
		!xrtHttpDigestRead(
			&Digest, NULL, 0, &iSize
		) && (xrtErrorKind(xrtGetError()) == XERR_VALUE) &&
		(iSize == 77u),
		"digest reader accepted malformed parameters"
	);

	xrtHttpDigestCursorInit(&Cursor);
	testRequire(
		xrtHttpDigestNext(
			(xstrview){ arrMutable, sizeof(arrMutable) - 1u },
			&Cursor, &Digest
		) == XHTTP_NEXT_ITEM,
		"mutable digest parse failed"
	);
	testRequire(
		xrtHttpDigestRead(
			&Digest, (void*)Digest.Value.Encoded.Data,
			Digest.Value.Encoded.Size, &iSize
		) && (iSize == 2u) &&
		((uint8)Digest.Value.Encoded.Data[0] == 1u) &&
		((uint8)Digest.Value.Encoded.Data[1] == 2u),
		"digest reader rejected exact in-place decoding"
	);

	xrtHttpDigestCursorInit(&Cursor);
	SavedCursor = Cursor;
	memset(&Digest, 0xA5, sizeof(Digest));
	SavedDigest = Digest;
	xrtClearError();
	testRequire(
		(xrtHttpDigestNext(
			(xstrview){
				(cstr)(uintptr_t)(UINTPTR_MAX - 1u), 4u
			}, &Cursor, &Digest
		) == XHTTP_NEXT_ERROR) &&
		(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT) &&
		(memcmp(&Cursor, &SavedCursor, sizeof(Cursor)) == 0) &&
		(memcmp(&Digest, &SavedDigest, sizeof(Digest)) == 0),
		"digest parser accepted a wrapped value"
	);
	xrtClearError();
	xrtHttpDigestCursorInit(
		(xhttpdigestcursor*)(uintptr_t)(UINTPTR_MAX - 1u)
	);
	testRequire(
		xrtErrorKind(xrtGetError()) == XERR_ARGUMENT,
		"digest cursor initializer accepted wrapped storage"
	);
	xrtClearError();
}



/* 验证实现可迭代 RFC 9651 要求规模的唯一摘要成员。 */
static void testHttpDigestScale(void)
{
	char arrValue[(1024u * 13u) - 1u];
	xstrview Value = { arrValue, sizeof(arrValue) };
	xhttpdigestcursor Cursor;
	xhttpdigest Digest;
	xhttpnext Next;
	size_t iOffset = 0;
	size_t iCount = 0;
	size_t i;

	for ( i = 0; i < 1024u; i++ ) {
		arrValue[iOffset++] = 'a';
		arrValue[iOffset++] = (char)('0' + ((i / 1000u) % 10u));
		arrValue[iOffset++] = (char)('0' + ((i / 100u) % 10u));
		arrValue[iOffset++] = (char)('0' + ((i / 10u) % 10u));
		arrValue[iOffset++] = (char)('0' + (i % 10u));
		memcpy(arrValue + iOffset, "=:AA==:", 7u);
		iOffset += 7u;
		if ( i != 1023u ) {
			arrValue[iOffset++] = ',';
		}
	}
	testRequire(
		iOffset == sizeof(arrValue),
		"digest scale fixture size mismatch"
	);
	xrtHttpDigestCursorInit(&Cursor);
	while ( (Next = xrtHttpDigestNext(
		Value, &Cursor, &Digest
	)) == XHTTP_NEXT_ITEM ) {
		iCount++;
	}
	testRequire(
		(Next == XHTTP_NEXT_END) && (iCount == 1024u),
		"digest iterator rejected 1024 unique members"
	);
}



/* 运行 RFC 9530 Digest 字段解析测试。 */
int main(void)
{
	testHttpDigestVectors();
	testHttpDigestFields();
	testHttpDigestPreferences();
	testHttpDigestFailure();
	testHttpDigestCursorBinding();
	testHttpDigestMemory();
	testHttpDigestScale();
	printf("[PASS] http_digest\n");
	return 0;
}
