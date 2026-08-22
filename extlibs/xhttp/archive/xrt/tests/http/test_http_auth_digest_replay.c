#include "../test.h"

#include <xrt/http_auth.h>



/* 验证独立 SHA-256 向量、字段长度边界和未对齐输出。 */
static void testHttpDigestReplayKey(void)
{
	static const uint8 Expected[XRT_HTTP_DIGEST_REPLAY_KEY_SIZE] = {
		0xABu, 0x3Bu, 0x62u, 0xCBu, 0x64u, 0xF0u, 0x51u, 0x98u,
		0xD2u, 0xD4u, 0x51u, 0x07u, 0x30u, 0xBDu, 0xD0u, 0x3Eu,
		0xAAu, 0x28u, 0x01u, 0x88u, 0xAFu, 0x4Bu, 0xFCu, 0x4Eu,
		0xC0u, 0xD6u, 0xC0u, 0x16u, 0x91u, 0xE8u, 0x58u, 0x89u
	};
	uint8 Storage[sizeof(xhttpdigestreplaykey) + 2u];
	xhttpdigestreplaykey* pKey =
		(xhttpdigestreplaykey*)(void*)(Storage + 1u);
	xhttpdigestreplaykey Left;
	xhttpdigestreplaykey Right;

	memset(Storage, 0xA5, sizeof(Storage));
	testRequire(xrtHttpDigestReplayKey(
		XRT_STR_LITERAL("Mufasa"),
		XRT_STR_LITERAL("server-nonce"),
		XRT_STR_LITERAL("client-nonce"),
		pKey
	) && (Storage[0] == 0xA5u) &&
		(Storage[sizeof(Storage) - 1u] == 0xA5u) &&
		(memcmp(pKey->Bytes, Expected, sizeof(Expected)) == 0),
		"HTTP Digest replay key vector mismatch");
	testRequire(xrtHttpDigestReplayKey(
		XRT_STR_LITERAL("ab"),
		XRT_STR_LITERAL("c"),
		XRT_STR_LITERAL("d"),
		&Left
	) && xrtHttpDigestReplayKey(
		XRT_STR_LITERAL("a"),
		XRT_STR_LITERAL("bc"),
		XRT_STR_LITERAL("d"),
		&Right
	) && (memcmp(&Left, &Right, sizeof(Left)) != 0),
		"HTTP Digest replay key lost field boundaries");
}



/* 验证 nc 单调性、硬容量、过期清理和统计。 */
static void testHttpDigestReplayLifecycle(void)
{
	xhttpdigestreplayconfig Config = { 1u, 2u, 60 };
	xhttpdigestreplay* pReplay = xrtHttpDigestReplayCreate(&Config);
	xhttpdigestreplaykey Keys[3];
	xhttpdigestreplaystats Stats;

	testRequire(pReplay != NULL,
		"HTTP Digest replay create failed");
	for ( size_t i = 0; i < 3u; i++ ) {
		char User[2] = { (char)('a' + i), '\0' };

		testRequire(xrtHttpDigestReplayKey(
			(xstrview){ User, 1u },
			XRT_STR_LITERAL("nonce"),
			XRT_STR_LITERAL("cnonce"),
			&Keys[i]
		), "HTTP Digest replay fixture key failed");
	}
	testRequire(xrtHttpDigestReplayCheckKey(
		pReplay, &Keys[0], 1u, 1000, 1000
	) == XHTTP_DIGEST_REPLAY_ACCEPTED,
		"HTTP Digest replay rejected first nc");
	testRequire(xrtHttpDigestReplayCheckKey(
		pReplay, &Keys[0], 1u, 1000, 1001
	) == XHTTP_DIGEST_REPLAY_REPLAY,
		"HTTP Digest replay accepted repeated nc");
	testRequire(xrtHttpDigestReplayCheckKey(
		pReplay, &Keys[0], 2u, 1000, 1001
	) == XHTTP_DIGEST_REPLAY_ACCEPTED,
		"HTTP Digest replay rejected increasing nc");
	testRequire(xrtHttpDigestReplayCheckKey(
		pReplay, &Keys[1], 1u, 1000, 1001
	) == XHTTP_DIGEST_REPLAY_ACCEPTED,
		"HTTP Digest replay rejected second key");
	testRequire(xrtHttpDigestReplayCheckKey(
		pReplay, &Keys[2], 1u, 1000, 1001
	) == XHTTP_DIGEST_REPLAY_FULL,
		"HTTP Digest replay did not enforce hard capacity");
	testRequire(xrtHttpDigestReplayPurge(pReplay, 1060) == 0,
		"HTTP Digest replay purged inclusive lifetime boundary");
	testRequire(xrtHttpDigestReplayPurge(pReplay, 1061) == 2u,
		"HTTP Digest replay purge count mismatch");
	testRequire(xrtHttpDigestReplayCheckKey(
		pReplay, &Keys[2], 1u, 1061, 1061
	) == XHTTP_DIGEST_REPLAY_ACCEPTED,
		"HTTP Digest replay did not reuse expired capacity");
	testRequire(xrtHttpDigestReplayCheckKey(
		pReplay, &Keys[0], 3u, 1000, 1061
	) == XHTTP_DIGEST_REPLAY_EXPIRED,
		"HTTP Digest replay accepted an expired nonce");
	testRequire(xrtHttpDigestReplayStats(pReplay, &Stats) &&
		(Stats.Entries == 1u) && (Stats.Capacity == 2u) &&
		(Stats.Accepted == 4u) && (Stats.Replayed == 1u) &&
		(Stats.Expired == 1u) && (Stats.Full == 1u) &&
		(Stats.Purged == 2u),
		"HTTP Digest replay statistics mismatch");
	xrtHttpDigestReplayClear(pReplay);
	testRequire(xrtHttpDigestReplayStats(pReplay, &Stats) &&
		(Stats.Entries == 0),
		"HTTP Digest replay clear retained entries");
	xrtHttpDigestReplayDestroy(pReplay);
}



/* 验证按凭据便利入口和多分片总容量。 */
static void testHttpDigestReplayConvenience(void)
{
	xhttpdigestreplayconfig Config = { 4u, 3u, 30 };
	xhttpdigestreplay* pReplay = xrtHttpDigestReplayCreate(&Config);
	xhttpdigestreplaystats Stats;

	testRequire(pReplay != NULL,
		"HTTP Digest replay sharded create failed");
	testRequire(xrtHttpDigestReplayCheck(
		pReplay,
		XRT_STR_LITERAL("Mufasa"),
		XRT_STR_LITERAL("nonce"),
		XRT_STR_LITERAL("cnonce"),
		7u, 2000, 2000
	) == XHTTP_DIGEST_REPLAY_ACCEPTED,
		"HTTP Digest replay convenience check failed");
	testRequire(xrtHttpDigestReplayCheck(
		pReplay,
		XRT_STR_LITERAL("Mufasa"),
		XRT_STR_LITERAL("nonce"),
		XRT_STR_LITERAL("cnonce"),
		6u, 2000, 2001
	) == XHTTP_DIGEST_REPLAY_REPLAY,
		"HTTP Digest replay convenience accepted lower nc");
	testRequire(xrtHttpDigestReplayStats(pReplay, &Stats) &&
		(Stats.Capacity == 12u) && (Stats.Entries == 1u),
		"HTTP Digest replay sharded capacity mismatch");
	xrtHttpDigestReplayDestroy(pReplay);
}



/* 验证满表同槽探测簇在删除过期根后仍可查找和复用。 */
static void testHttpDigestReplayCollision(void)
{
	xhttpdigestreplayconfig Config = { 1u, 4u, 60 };
	xhttpdigestreplay* pReplay = xrtHttpDigestReplayCreate(&Config);
	xhttpdigestreplaykey Keys[5];
	size_t iFound = 0;

	testRequire(pReplay != NULL,
		"HTTP Digest replay collision create failed");
	for ( size_t i = 0; (i < 1000u) && (iFound < 5u); i++ ) {
		char User[32];
		xhttpdigestreplaykey Key;
		uint64 iHash;
		int iSize = snprintf(User, sizeof(User), "collision-%u",
			(unsigned)i);

		testRequire((iSize > 0) && ((size_t)iSize < sizeof(User)) &&
			xrtHttpDigestReplayKey(
				(xstrview){ User, (size_t)iSize },
				XRT_STR_LITERAL("nonce"),
				XRT_STR_LITERAL("cnonce"),
				&Key
			), "HTTP Digest replay collision key failed");
		memcpy(&iHash, Key.Bytes, sizeof(iHash));
		if ( (iHash % 6u) == 0 ) {
			Keys[iFound++] = Key;
		}
	}
	testRequire(iFound == 5u,
		"HTTP Digest replay collision fixtures missing");
	for ( size_t i = 0; i < 4u; i++ ) {
		testRequire(xrtHttpDigestReplayCheckKey(
			pReplay,
			&Keys[i],
			1u,
			(int64)(1000u + (i * 10u)),
			1030
		) == XHTTP_DIGEST_REPLAY_ACCEPTED,
			"HTTP Digest replay collision insert failed");
	}
	testRequire(xrtHttpDigestReplayPurge(pReplay, 1061) == 1u,
		"HTTP Digest replay collision purge failed");
	for ( size_t i = 1; i < 4u; i++ ) {
		testRequire(xrtHttpDigestReplayCheckKey(
			pReplay,
			&Keys[i],
			1u,
			(int64)(1000u + (i * 10u)),
			1061
		) == XHTTP_DIGEST_REPLAY_REPLAY,
			"HTTP Digest replay collision lookup broke after delete");
	}
	testRequire(xrtHttpDigestReplayCheckKey(
		pReplay, &Keys[4], 1u, 1061, 1061
	) == XHTTP_DIGEST_REPLAY_ACCEPTED,
		"HTTP Digest replay collision slot was not reusable");
	testRequire((xrtHttpDigestReplayPurge(pReplay, 1091) == 3u) &&
		(xrtHttpDigestReplayCheckKey(
			pReplay, &Keys[4], 1u, 1061, 1091
		) == XHTTP_DIGEST_REPLAY_REPLAY),
		"HTTP Digest replay repeated collision purge failed");
	xrtHttpDigestReplayDestroy(pReplay);
}



/* 以独立参考状态长时间核对探测、过期、容量和 nc 推进。 */
static void testHttpDigestReplayModel(void)
{
	typedef struct test_http_digest_replay_model_entry {
		bool Active;
		uint32 NonceCount;
		int64 IssuedSeconds;
		int64 ExpiresSeconds;
	} test_http_digest_replay_model_entry;
	xhttpdigestreplayconfig Config = { 1u, 17u, 5 };
	xhttpdigestreplay* pReplay = xrtHttpDigestReplayCreate(&Config);
	xhttpdigestreplaykey Keys[64];
	test_http_digest_replay_model_entry Model[64];
	uint32 iRandom = UINT32_C(0x13579BDF);
	int64 iNow = 1000;
	size_t iActive = 0;

	testRequire(pReplay != NULL,
		"HTTP Digest replay model create failed");
	memset(Model, 0, sizeof(Model));
	for ( size_t i = 0; i < 64u; i++ ) {
		char User[32];
		int iSize = snprintf(User, sizeof(User), "model-%u",
			(unsigned)i);

		testRequire((iSize > 0) && ((size_t)iSize < sizeof(User)) &&
			xrtHttpDigestReplayKey(
				(xstrview){ User, (size_t)iSize },
				XRT_STR_LITERAL("nonce"),
				XRT_STR_LITERAL("cnonce"),
				&Keys[i]
			), "HTTP Digest replay model key failed");
	}
	for ( size_t i = 0; i < 10000u; i++ ) {
		xhttpdigestreplaycheck Expected;
		xhttpdigestreplaycheck Actual;
		xhttpdigestreplaystats Stats;
		test_http_digest_replay_model_entry* pModel;
		size_t iExpired = 0;
		size_t iIndex;
		uint32 iNonceCount;

		if ( (i % 3u) == 0 ) {
			iNow++;
		}
		for ( size_t j = 0; j < 64u; j++ ) {
			if ( Model[j].Active &&
				(Model[j].ExpiresSeconds < iNow) ) {
				Model[j].Active = false;
				iActive--;
				iExpired++;
			}
		}
		if ( (i % 29u) == 0 ) {
			testRequire(
				xrtHttpDigestReplayPurge(pReplay, iNow) == iExpired,
				"HTTP Digest replay model explicit purge mismatch"
			);
		}
		iRandom = (iRandom * UINT32_C(1664525)) +
			UINT32_C(1013904223);
		iIndex = (size_t)(iRandom % 64u);
		iNonceCount = ((iRandom >> 8u) % 8u) + 1u;
		pModel = &Model[iIndex];
		if ( pModel->Active ) {
			if ( iNonceCount <= pModel->NonceCount ) {
				Expected = XHTTP_DIGEST_REPLAY_REPLAY;
			} else {
				Expected = XHTTP_DIGEST_REPLAY_ACCEPTED;
				pModel->NonceCount = iNonceCount;
			}
		} else if ( iActive >= Config.EntriesPerShard ) {
			Expected = XHTTP_DIGEST_REPLAY_FULL;
		} else {
			Expected = XHTTP_DIGEST_REPLAY_ACCEPTED;
			pModel->Active = true;
			pModel->NonceCount = iNonceCount;
			pModel->IssuedSeconds = iNow;
			pModel->ExpiresSeconds =
				iNow + Config.LifetimeSeconds;
			iActive++;
		}
		Actual = xrtHttpDigestReplayCheckKey(
			pReplay,
			&Keys[iIndex],
			iNonceCount,
			pModel->Active ? pModel->IssuedSeconds : iNow,
			iNow
		);
		testRequire(Actual == Expected,
			"HTTP Digest replay diverged from reference model");
		if ( (i % 31u) == 0 ) {
			testRequire(xrtHttpDigestReplayStats(pReplay, &Stats) &&
				(Stats.Entries == iActive),
				"HTTP Digest replay model entry count mismatch");
		}
	}
	xrtHttpDigestReplayDestroy(pReplay);
}



/* 验证配置、输入范围、溢出与未对齐统计描述符。 */
static void testHttpDigestReplayInvalid(void)
{
	uint8 ConfigStorage[sizeof(xhttpdigestreplayconfig) + 2u];
	uint8 StatsStorage[sizeof(xhttpdigestreplaystats) + 2u];
	xhttpdigestreplayconfig* pConfig =
		(xhttpdigestreplayconfig*)(void*)(ConfigStorage + 1u);
	xhttpdigestreplaystats* pStats =
		(xhttpdigestreplaystats*)(void*)(StatsStorage + 1u);
	xhttpdigestreplayconfig Config = { 1u, 2u, 60 };
	xhttpdigestreplay* pReplay;
	xhttpdigestreplaykey Key;
	char Alias[64];

	memset(ConfigStorage, 0xA5, sizeof(ConfigStorage));
	xrtHttpDigestReplayConfigInit(pConfig);
	testRequire((ConfigStorage[0] == 0xA5u) &&
		(ConfigStorage[sizeof(ConfigStorage) - 1u] == 0xA5u),
		"HTTP Digest replay config damaged unaligned guards");
	memcpy(&Config, pConfig, sizeof(Config));
	testRequire((Config.Shards != 0) &&
		(Config.EntriesPerShard != 0) &&
		(Config.LifetimeSeconds > 0),
		"HTTP Digest replay default config invalid");

	Config.Shards = 0;
	testRequire(xrtHttpDigestReplayCreate(&Config) == NULL,
		"HTTP Digest replay accepted zero shards");
	xrtClearError();
	Config = (xhttpdigestreplayconfig){
		2u, (SIZE_MAX / 2u) + 1u, 60
	};
	testRequire(xrtHttpDigestReplayCreate(&Config) == NULL,
		"HTTP Digest replay accepted overflowing capacity");
	testRequire(xrtErrorKind(xrtGetError()) == XERR_RANGE,
		"HTTP Digest replay overflow error mismatch");
	xrtClearError();
	Config = (xhttpdigestreplayconfig){ 1u, 2u, 60 };
	pReplay = xrtHttpDigestReplayCreate(&Config);
	testRequire(pReplay != NULL,
		"HTTP Digest replay invalid fixture create failed");
	testRequire(!xrtHttpDigestReplayKey(
		XRT_STR_LITERAL("user"),
		XRT_STR_LITERAL(""),
		XRT_STR_LITERAL("cnonce"),
		&Key
	), "HTTP Digest replay accepted empty nonce");
	xrtClearError();
	memset(Alias, 'x', sizeof(Alias));
	testRequire(!xrtHttpDigestReplayKey(
		(xstrview){ Alias, 4u },
		XRT_STR_LITERAL("nonce"),
		XRT_STR_LITERAL("cnonce"),
		(xhttpdigestreplaykey*)(void*)Alias
	), "HTTP Digest replay accepted invalid output range");
	xrtClearError();
	testRequire(xrtHttpDigestReplayKey(
		XRT_STR_LITERAL("user"),
		XRT_STR_LITERAL("nonce"),
		XRT_STR_LITERAL("cnonce"),
		&Key
	), "HTTP Digest replay invalid fixture key failed");
	testRequire(xrtHttpDigestReplayCheckKey(
		pReplay, &Key, 0, 1000, 1000
	) == XHTTP_DIGEST_REPLAY_ERROR,
		"HTTP Digest replay accepted zero nc");
	xrtClearError();
	testRequire(xrtHttpDigestReplayCheckKey(
		pReplay, &Key, 1u, INT64_MAX, INT64_MAX
	) == XHTTP_DIGEST_REPLAY_ERROR,
		"HTTP Digest replay accepted expiration overflow");
	xrtClearError();
	memset(StatsStorage, 0x5A, sizeof(StatsStorage));
	testRequire(xrtHttpDigestReplayStats(pReplay, pStats) &&
		(StatsStorage[0] == 0x5Au) &&
		(StatsStorage[sizeof(StatsStorage) - 1u] == 0x5Au),
		"HTTP Digest replay rejected unaligned statistics");
	xrtHttpDigestReplayDestroy(pReplay);
}



int main(void)
{
	testHttpDigestReplayKey();
	testHttpDigestReplayLifecycle();
	testHttpDigestReplayConvenience();
	testHttpDigestReplayCollision();
	testHttpDigestReplayModel();
	testHttpDigestReplayInvalid();
	puts("[PASS] HTTP Digest replay protection");
	return 0;
}
