#define XRT_IMPLEMENTATION
#include "../../singlehead/xrt.h"
#include "xssh.h"

static int xssh_packet_test_fail(const char* sName, const char* sDetail)
{
	printf("FAIL %s: %s\n", sName, sDetail ? sDetail : "");
	return 1;
}

static int xssh_packet_expect_mem(const char* sName, const void* pA, const void* pB, size_t iLen)
{
	if ( memcmp(pA, pB, iLen) != 0 ) {
		return xssh_packet_test_fail(sName, "memory mismatch");
	}
	return 0;
}

static int xssh_packet_test_roundtrip(void)
{
	const uint8 arrPayload[] = { 20u, 'a', 'b', 'c' };
	uint8 arrBuf[64];
	xsshwriter wr;
	xsshreader rd;
	xsshpacket pkt;
	uint32 iWriteSeq = 7u;
	uint32 iReadSeq = 7u;

	xsshWriterInit(&wr, arrBuf, sizeof(arrBuf));
	if ( xsshPacketWritePlain(&wr, arrPayload, sizeof(arrPayload), 8u, &iWriteSeq) != XSSH_OK ) {
		return xssh_packet_test_fail("packet_roundtrip", "write failed");
	}
	if ( iWriteSeq != 8u ) {
		return xssh_packet_test_fail("packet_roundtrip", "write seq not incremented");
	}
	if ( wr.iLen != 16u || arrBuf[0] != 0u || arrBuf[1] != 0u || arrBuf[2] != 0u || arrBuf[3] != 12u || arrBuf[4] != 7u ) {
		return xssh_packet_test_fail("packet_roundtrip", "encoded header mismatch");
	}

	xsshReaderInit(&rd, arrBuf, wr.iLen);
	if ( xsshPacketReadPlain(&rd, 8u, 0u, &iReadSeq, &pkt) != XSSH_OK ) {
		return xssh_packet_test_fail("packet_roundtrip", "read failed");
	}
	if ( iReadSeq != 8u || pkt.iSeqNo != 7u || pkt.iPacketLen != 12u || pkt.iPaddingLen != 7u ) {
		return xssh_packet_test_fail("packet_roundtrip", "metadata mismatch");
	}
	if ( pkt.tPayload.iLen != sizeof(arrPayload) || pkt.tPadding.iLen != 7u || rd.iPos != wr.iLen ) {
		return xssh_packet_test_fail("packet_roundtrip", "slice length mismatch");
	}
	return xssh_packet_expect_mem("packet_roundtrip", pkt.tPayload.pData, arrPayload, sizeof(arrPayload));
}

static int xssh_packet_test_padding_boundaries(void)
{
	uint8 iPadding = 0u;
	uint32 iPacketLen = 0u;

	if ( xsshPacketCalcPlainPadding(0u, 8u, &iPadding, &iPacketLen) != XSSH_OK || iPadding != 11u || iPacketLen != 12u ) {
		return xssh_packet_test_fail("packet_padding", "payload 0 mismatch");
	}
	if ( xsshPacketCalcPlainPadding(3u, 8u, &iPadding, &iPacketLen) != XSSH_OK || iPadding != 8u || iPacketLen != 12u ) {
		return xssh_packet_test_fail("packet_padding", "payload 3 mismatch");
	}
	if ( xsshPacketCalcPlainPadding(4u, 8u, &iPadding, &iPacketLen) != XSSH_OK || iPadding != 7u || iPacketLen != 12u ) {
		return xssh_packet_test_fail("packet_padding", "payload 4 mismatch");
	}
	if ( xsshPacketCalcPlainPadding(8u, 8u, &iPadding, &iPacketLen) != XSSH_OK || iPadding != 11u || iPacketLen != 20u ) {
		return xssh_packet_test_fail("packet_padding", "payload 8 mismatch");
	}
	if ( xsshPacketCalcPlainPadding(4u, 16u, &iPadding, &iPacketLen) != XSSH_OK || iPadding != 7u || iPacketLen != 12u ) {
		return xssh_packet_test_fail("packet_padding", "block 16 mismatch");
	}
	return 0;
}

static int xssh_packet_test_no_space_is_atomic(void)
{
	const uint8 arrPayload[] = { 1u, 2u, 3u, 4u };
	uint8 arrBuf[8];
	xsshwriter wr;
	uint32 iSeq = 3u;

	xsshWriterInit(&wr, arrBuf, sizeof(arrBuf));
	if ( xsshPacketWritePlain(&wr, arrPayload, sizeof(arrPayload), 8u, &iSeq) != XSSH_ERR_NO_SPACE ) {
		return xssh_packet_test_fail("packet_no_space", "expected no space");
	}
	if ( wr.iLen != 0u || iSeq != 3u ) {
		return xssh_packet_test_fail("packet_no_space", "writer or seq changed after failure");
	}
	return 0;
}

static int xssh_packet_test_seq_wrap(void)
{
	const uint8 arrPayload[] = { 94u };
	uint8 arrBuf[32];
	xsshwriter wr;
	xsshreader rd;
	xsshpacket pkt;
	uint32 iWriteSeq = 0xffffffffu;
	uint32 iReadSeq = 0xffffffffu;

	xsshWriterInit(&wr, arrBuf, sizeof(arrBuf));
	if ( xsshPacketWritePlain(&wr, arrPayload, sizeof(arrPayload), 8u, &iWriteSeq) != XSSH_OK || iWriteSeq != 0u ) {
		return xssh_packet_test_fail("packet_seq_wrap", "write wrap mismatch");
	}
	xsshReaderInit(&rd, arrBuf, wr.iLen);
	if ( xsshPacketReadPlain(&rd, 8u, 0u, &iReadSeq, &pkt) != XSSH_OK || iReadSeq != 0u || pkt.iSeqNo != 0xffffffffu ) {
		return xssh_packet_test_fail("packet_seq_wrap", "read wrap mismatch");
	}
	return 0;
}

static int xssh_packet_test_bad_packet_length(void)
{
	const uint8 arrBadTooSmall[] = { 0u, 0u, 0u, 4u, 4u, 0u, 0u, 0u };
	const uint8 arrPayload[] = { 1u, 2u, 3u, 4u };
	uint8 arrBuf[64];
	xsshwriter wr;
	xsshreader rd;
	xsshpacket pkt;

	xsshReaderInit(&rd, arrBadTooSmall, sizeof(arrBadTooSmall));
	if ( xsshPacketReadPlain(&rd, 8u, 0u, NULL, &pkt) != XSSH_ERR_BAD_PACKET || rd.iPos != 0u ) {
		return xssh_packet_test_fail("packet_bad_length", "too small packet accepted");
	}

	xsshWriterInit(&wr, arrBuf, sizeof(arrBuf));
	if ( xsshPacketWritePlain(&wr, arrPayload, sizeof(arrPayload), 8u, NULL) != XSSH_OK ) {
		return xssh_packet_test_fail("packet_bad_length", "setup write failed");
	}
	xsshReaderInit(&rd, arrBuf, wr.iLen);
	if ( xsshPacketReadPlain(&rd, 8u, 8u, NULL, &pkt) != XSSH_ERR_BAD_PACKET || rd.iPos != 0u ) {
		return xssh_packet_test_fail("packet_bad_length", "max packet check failed");
	}
	return 0;
}

static int xssh_packet_test_bad_padding(void)
{
	uint8 arrBadPadding[16] = {
		0u, 0u, 0u, 12u,
		3u,
		1u, 2u, 3u, 4u, 5u, 6u, 7u, 8u,
		0u, 0u, 0u
	};
	xsshreader rd;
	xsshpacket pkt;

	xsshReaderInit(&rd, arrBadPadding, sizeof(arrBadPadding));
	if ( xsshPacketReadPlain(&rd, 8u, 0u, NULL, &pkt) != XSSH_ERR_BAD_PACKET || rd.iPos != 0u ) {
		return xssh_packet_test_fail("packet_bad_padding", "bad padding accepted");
	}
	return 0;
}

static int xssh_packet_test_truncated_packet(void)
{
	const uint8 arrPayload[] = { 1u, 2u, 3u, 4u };
	uint8 arrBuf[64];
	xsshwriter wr;
	xsshreader rd;
	xsshpacket pkt;
	uint32 iSeq = 10u;

	xsshWriterInit(&wr, arrBuf, sizeof(arrBuf));
	if ( xsshPacketWritePlain(&wr, arrPayload, sizeof(arrPayload), 8u, NULL) != XSSH_OK ) {
		return xssh_packet_test_fail("packet_truncated", "setup write failed");
	}
	xsshReaderInit(&rd, arrBuf, wr.iLen - 1u);
	if ( xsshPacketReadPlain(&rd, 8u, 0u, &iSeq, &pkt) != XSSH_WIRE_NEED_MORE || rd.iPos != 0u || iSeq != 10u ) {
		return xssh_packet_test_fail("packet_truncated", "truncated packet changed state");
	}
	return 0;
}

static int xssh_packet_test_concat_packets(void)
{
	const uint8 arrPayloadA[] = { 'a' };
	const uint8 arrPayloadB[] = { 'b', 'c' };
	uint8 arrBuf[96];
	xsshwriter wr;
	xsshreader rd;
	xsshpacket pkt;
	uint32 iSeq = 0u;

	xsshWriterInit(&wr, arrBuf, sizeof(arrBuf));
	if ( xsshPacketWritePlain(&wr, arrPayloadA, sizeof(arrPayloadA), 8u, NULL) != XSSH_OK ||
		xsshPacketWritePlain(&wr, arrPayloadB, sizeof(arrPayloadB), 8u, NULL) != XSSH_OK ) {
		return xssh_packet_test_fail("packet_concat", "write failed");
	}

	xsshReaderInit(&rd, arrBuf, wr.iLen);
	if ( xsshPacketReadPlain(&rd, 8u, 0u, &iSeq, &pkt) != XSSH_OK ||
		pkt.tPayload.iLen != sizeof(arrPayloadA) ||
		memcmp(pkt.tPayload.pData, arrPayloadA, sizeof(arrPayloadA)) != 0 ||
		iSeq != 1u ) {
		return xssh_packet_test_fail("packet_concat", "first packet mismatch");
	}
	if ( xsshPacketReadPlain(&rd, 8u, 0u, &iSeq, &pkt) != XSSH_OK ||
		pkt.tPayload.iLen != sizeof(arrPayloadB) ||
		memcmp(pkt.tPayload.pData, arrPayloadB, sizeof(arrPayloadB)) != 0 ||
		iSeq != 2u ||
		rd.iPos != wr.iLen ) {
		return xssh_packet_test_fail("packet_concat", "second packet mismatch");
	}
	return 0;
}

static int xssh_packet_test_aead_padding_boundaries(void)
{
	uint8 iPadding = 0u;
	uint32 iPacketLen = 0u;

	if ( xsshPacketCalcAEADPadding(0u, &iPadding, &iPacketLen) != XSSH_OK || iPadding != 15u || iPacketLen != 16u ) {
		return xssh_packet_test_fail("packet_aead_padding", "payload 0 mismatch");
	}
	if ( xsshPacketCalcAEADPadding(11u, &iPadding, &iPacketLen) != XSSH_OK || iPadding != 4u || iPacketLen != 16u ) {
		return xssh_packet_test_fail("packet_aead_padding", "payload 11 mismatch");
	}
	if ( xsshPacketCalcAEADPadding(12u, &iPadding, &iPacketLen) != XSSH_OK || iPadding != 19u || iPacketLen != 32u ) {
		return xssh_packet_test_fail("packet_aead_padding", "payload 12 mismatch");
	}
	return 0;
}

static int xssh_packet_test_fill_padding(void* pUser, uint8* pOut, size_t iLen)
{
	uint8 iBase = pUser ? *(uint8*)pUser : 0u;
	size_t i;

	for ( i = 0u; i < iLen; ++i ) {
		pOut[i] = (uint8)(iBase + i);
	}
	return XSSH_OK;
}

static int xssh_packet_test_aesgcm128_vector_and_roundtrip(void)
{
	static const uint8 arrExpectedPacket[] = {
		0x00u, 0x00u, 0x00u, 0x10u, 0x54u, 0x52u, 0x9bu, 0xf4u,
		0xdau, 0x70u, 0xceu, 0xf7u, 0xddu, 0x64u, 0x49u, 0xa8u,
		0xfcu, 0x63u, 0x1au, 0x0fu, 0x54u, 0xe8u, 0xdau, 0x32u,
		0xe3u, 0xe3u, 0xb3u, 0x29u, 0x6du, 0x34u, 0x5cu, 0x28u,
		0x31u, 0xa8u, 0x52u, 0xbau
	};
	const uint8 arrPayload[] = { XSSH_MSG_KEXINIT, 'a', 'e', 'a', 'd' };
	uint8 arrKey[XSSH_AES128_KEY_SIZE];
	uint8 arrIV[XSSH_GCM_NONCE_SIZE] = { 0x10u, 0x11u, 0x12u, 0x13u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 1u };
	uint8 arrBuf[128];
	uint8 arrPlain[64];
	uint8 iPadBase = 0xe0u;
	xsshaesgcmstate enc;
	xsshaesgcmstate dec;
	xsshwriter wr;
	xsshreader rd;
	xsshpacket pkt;
	uint32 iWriteSeq = 9u;
	uint32 iReadSeq = 9u;
	size_t i;

	for ( i = 0u; i < sizeof(arrKey); ++i ) arrKey[i] = (uint8)i;
	if ( xsshAESGCMStateInit(&enc, arrKey, sizeof(arrKey), arrIV) != XSSH_OK ||
		xsshAESGCMStateInit(&dec, arrKey, sizeof(arrKey), arrIV) != XSSH_OK ) {
		return xssh_packet_test_fail("packet_aesgcm128_vector", "state init failed");
	}

	xsshWriterInit(&wr, arrBuf, sizeof(arrBuf));
	if ( xsshPacketWriteAESGCMEx(&wr, arrPayload, sizeof(arrPayload), &enc, &iWriteSeq, xssh_packet_test_fill_padding, &iPadBase) != XSSH_OK ) {
		return xssh_packet_test_fail("packet_aesgcm128_vector", "write failed");
	}
	if ( wr.iLen != sizeof(arrExpectedPacket) || iWriteSeq != 10u || enc.iInvocationCounter != 2u ) {
		return xssh_packet_test_fail("packet_aesgcm128_vector", "write metadata mismatch");
	}
	if ( xssh_packet_expect_mem("packet_aesgcm128_vector", arrBuf, arrExpectedPacket, sizeof(arrExpectedPacket)) != 0 ) {
		return 1;
	}

	xsshReaderInit(&rd, arrBuf, wr.iLen);
	memset(arrPlain, 0xcc, sizeof(arrPlain));
	if ( xsshPacketReadAESGCM(&rd, &dec, 0u, &iReadSeq, &pkt, arrPlain, sizeof(arrPlain)) != XSSH_OK ) {
		return xssh_packet_test_fail("packet_aesgcm128_vector", "read failed");
	}
	if ( rd.iPos != wr.iLen || iReadSeq != 10u || dec.iInvocationCounter != 2u ||
		pkt.iSeqNo != 9u || pkt.iPacketLen != 16u || pkt.iPaddingLen != 10u ||
		pkt.tPayload.pData != arrPlain + 1u || pkt.tPayload.iLen != sizeof(arrPayload) ||
		pkt.tPadding.iLen != 10u ) {
		return xssh_packet_test_fail("packet_aesgcm128_vector", "read metadata mismatch");
	}
	if ( xssh_packet_expect_mem("packet_aesgcm128_vector", pkt.tPayload.pData, arrPayload, sizeof(arrPayload)) != 0 ||
		xssh_packet_expect_mem("packet_aesgcm128_vector", pkt.tPadding.pData, "\xe0\xe1\xe2\xe3\xe4\xe5\xe6\xe7\xe8\xe9", 10u) != 0 ) {
		return 1;
	}
	xsshAESGCMStateClear(&enc);
	xsshAESGCMStateClear(&dec);
	return 0;
}

static int xssh_packet_test_aesgcm256_roundtrip(void)
{
	const uint8 arrPayload[] = { XSSH_MSG_IGNORE, '2', '5', '6' };
	uint8 arrKey[XSSH_AES256_KEY_SIZE];
	uint8 arrIV[XSSH_GCM_NONCE_SIZE] = { 0x20u, 0x21u, 0x22u, 0x23u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 5u };
	uint8 arrBuf[128];
	uint8 arrPlain[64];
	uint8 iPadBase = 0x70u;
	xsshaesgcmstate enc;
	xsshaesgcmstate dec;
	xsshwriter wr;
	xsshreader rd;
	xsshpacket pkt;
	uint32 iSeq = 0u;
	size_t i;

	for ( i = 0u; i < sizeof(arrKey); ++i ) arrKey[i] = (uint8)(0x80u + i);
	if ( xsshAESGCMStateInit(&enc, arrKey, sizeof(arrKey), arrIV) != XSSH_OK ||
		xsshAESGCMStateInit(&dec, arrKey, sizeof(arrKey), arrIV) != XSSH_OK ) {
		return xssh_packet_test_fail("packet_aesgcm256_roundtrip", "state init failed");
	}
	xsshWriterInit(&wr, arrBuf, sizeof(arrBuf));
	if ( xsshPacketWriteAESGCMEx(&wr, arrPayload, sizeof(arrPayload), &enc, &iSeq, xssh_packet_test_fill_padding, &iPadBase) != XSSH_OK ) {
		return xssh_packet_test_fail("packet_aesgcm256_roundtrip", "write failed");
	}
	iSeq = 0u;
	xsshReaderInit(&rd, arrBuf, wr.iLen);
	if ( xsshPacketReadAESGCM(&rd, &dec, 0u, &iSeq, &pkt, arrPlain, sizeof(arrPlain)) != XSSH_OK ||
		pkt.tPayload.iLen != sizeof(arrPayload) ||
		memcmp(pkt.tPayload.pData, arrPayload, sizeof(arrPayload)) != 0 ||
		enc.iInvocationCounter != 6u || dec.iInvocationCounter != 6u || iSeq != 1u ) {
		return xssh_packet_test_fail("packet_aesgcm256_roundtrip", "roundtrip mismatch");
	}
	xsshAESGCMStateClear(&enc);
	xsshAESGCMStateClear(&dec);
	return 0;
}

static int xssh_packet_test_aesgcm_rejects_bad_tag_and_truncation(void)
{
	const uint8 arrPayload[] = { 'b', 'a', 'd' };
	uint8 arrKey[XSSH_AES128_KEY_SIZE];
	uint8 arrIV[XSSH_GCM_NONCE_SIZE] = { 0x30u, 0x31u, 0x32u, 0x33u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 7u };
	uint8 arrBuf[128];
	uint8 arrPlain[64];
	uint8 iPadBase = 0x40u;
	xsshaesgcmstate enc;
	xsshaesgcmstate dec;
	xsshwriter wr;
	xsshreader rd;
	xsshpacket pkt;
	uint32 iSeq = 3u;
	size_t i;

	for ( i = 0u; i < sizeof(arrKey); ++i ) arrKey[i] = (uint8)(0x30u + i);
	if ( xsshAESGCMStateInit(&enc, arrKey, sizeof(arrKey), arrIV) != XSSH_OK ||
		xsshAESGCMStateInit(&dec, arrKey, sizeof(arrKey), arrIV) != XSSH_OK ) {
		return xssh_packet_test_fail("packet_aesgcm_bad_tag", "state init failed");
	}
	xsshWriterInit(&wr, arrBuf, sizeof(arrBuf));
	if ( xsshPacketWriteAESGCMEx(&wr, arrPayload, sizeof(arrPayload), &enc, &iSeq, xssh_packet_test_fill_padding, &iPadBase) != XSSH_OK ) {
		return xssh_packet_test_fail("packet_aesgcm_bad_tag", "write failed");
	}

	xsshReaderInit(&rd, arrBuf, wr.iLen - 1u);
	iSeq = 3u;
	if ( xsshPacketReadAESGCM(&rd, &dec, 0u, &iSeq, &pkt, arrPlain, sizeof(arrPlain)) != XSSH_WIRE_NEED_MORE ||
		rd.iPos != 0u || iSeq != 3u || dec.iInvocationCounter != 7u ) {
		return xssh_packet_test_fail("packet_aesgcm_truncated", "truncated packet changed state");
	}

	arrBuf[wr.iLen - 1u] ^= 0x01u;
	xsshReaderInit(&rd, arrBuf, wr.iLen);
	memset(arrPlain, 0xcc, sizeof(arrPlain));
	if ( xsshPacketReadAESGCM(&rd, &dec, 0u, &iSeq, &pkt, arrPlain, sizeof(arrPlain)) != XSSH_ERR_BAD_PACKET ||
		rd.iPos != 0u || iSeq != 3u || dec.iInvocationCounter != 7u ) {
		return xssh_packet_test_fail("packet_aesgcm_bad_tag", "bad tag accepted or state changed");
	}
	for ( i = 0u; i < 16u; ++i ) {
		if ( arrPlain[i] != 0u ) {
			return xssh_packet_test_fail("packet_aesgcm_bad_tag", "plaintext buffer not cleared");
		}
	}
	xsshAESGCMStateClear(&enc);
	xsshAESGCMStateClear(&dec);
	return 0;
}

static int xssh_packet_test_aesgcm_read_plain_buffer_too_small(void)
{
	const uint8 arrPayload[] = { 's', 'm', 'a', 'l', 'l' };
	uint8 arrKey[XSSH_AES128_KEY_SIZE];
	uint8 arrIV[XSSH_GCM_NONCE_SIZE] = { 0x40u, 0x41u, 0x42u, 0x43u };
	uint8 arrBuf[128];
	uint8 arrPlain[4];
	uint8 iPadBase = 0x10u;
	xsshaesgcmstate enc;
	xsshaesgcmstate dec;
	xsshwriter wr;
	xsshreader rd;
	xsshpacket pkt;
	uint32 iSeq = 0u;
	size_t i;

	for ( i = 0u; i < sizeof(arrKey); ++i ) arrKey[i] = (uint8)(0x50u + i);
	if ( xsshAESGCMStateInit(&enc, arrKey, sizeof(arrKey), arrIV) != XSSH_OK ||
		xsshAESGCMStateInit(&dec, arrKey, sizeof(arrKey), arrIV) != XSSH_OK ) {
		return xssh_packet_test_fail("packet_aesgcm_no_space", "state init failed");
	}
	xsshWriterInit(&wr, arrBuf, sizeof(arrBuf));
	if ( xsshPacketWriteAESGCMEx(&wr, arrPayload, sizeof(arrPayload), &enc, &iSeq, xssh_packet_test_fill_padding, &iPadBase) != XSSH_OK ) {
		return xssh_packet_test_fail("packet_aesgcm_no_space", "write failed");
	}
	iSeq = 0u;
	xsshReaderInit(&rd, arrBuf, wr.iLen);
	if ( xsshPacketReadAESGCM(&rd, &dec, 0u, &iSeq, &pkt, arrPlain, sizeof(arrPlain)) != XSSH_ERR_NO_SPACE ||
		rd.iPos != 0u || iSeq != 0u || dec.iInvocationCounter != 0u ) {
		return xssh_packet_test_fail("packet_aesgcm_no_space", "small decrypt buffer accepted");
	}
	xsshAESGCMStateClear(&enc);
	xsshAESGCMStateClear(&dec);
	return 0;
}

int main(void)
{
	if ( xssh_packet_test_roundtrip() ) return 1;
	if ( xssh_packet_test_padding_boundaries() ) return 1;
	if ( xssh_packet_test_no_space_is_atomic() ) return 1;
	if ( xssh_packet_test_seq_wrap() ) return 1;
	if ( xssh_packet_test_bad_packet_length() ) return 1;
	if ( xssh_packet_test_bad_padding() ) return 1;
	if ( xssh_packet_test_truncated_packet() ) return 1;
	if ( xssh_packet_test_concat_packets() ) return 1;
	if ( xssh_packet_test_aead_padding_boundaries() ) return 1;
	if ( xssh_packet_test_aesgcm128_vector_and_roundtrip() ) return 1;
	if ( xssh_packet_test_aesgcm256_roundtrip() ) return 1;
	if ( xssh_packet_test_aesgcm_rejects_bad_tag_and_truncation() ) return 1;
	if ( xssh_packet_test_aesgcm_read_plain_buffer_too_small() ) return 1;

	printf("xssh_packet_test: PASS\n");
	return 0;
}
