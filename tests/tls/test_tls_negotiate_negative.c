#include "../test.h"



/* 检查失败结果不会修改调用方输出。 */
static void testTlsNegotiationFailureAtomic(void)
{
	static const uint8 OddIds[] = { 0x03, 0x04, 0x03 };
	static const uint8 GoodIds[] = { 0x03, 0x04, 0x03, 0x03 };
	static const xtlsversion DuplicateVersions[] = {
		XTLS_VERSION_13, XTLS_VERSION_13
	};
	static const xtlscipher BadCipher[] = { (xtlscipher)0xFFFF };
	static const xtlssignature BadSignature[] = {
		(xtlssignature)0x1234
	};
	xtlsids Odd = { { OddIds, sizeof(OddIds) } };
	xtlsids Good = { { GoodIds, sizeof(GoodIds) } };
	xtlsversion Version = (xtlsversion)0x7777;
	xtlscipher Cipher = (xtlscipher)0x7777;
	xtlssignature Signature = (xtlssignature)0x7777;

	testRequire(xrtTlsVersionSelect(
		&Odd, DuplicateVersions, 2, &Version
	) == XTLS_ITEM_ERROR, "invalid TLS version selection was accepted");
	testRequire(Version == (xtlsversion)0x7777,
		"failed TLS version selection changed output");

	testRequire(xrtTlsCipherSelect(
		XTLS_VERSION_13, &Good, XTLS_IDENTITY_NONE,
		BadCipher, 1, &Cipher
	) == XTLS_ITEM_ERROR, "unknown local TLS cipher was accepted");
	testRequire(Cipher == (xtlscipher)0x7777,
		"failed TLS cipher selection changed output");

	testRequire(xrtTlsSignatureSelect(
		XTLS_VERSION_13, &Good, XTLS_IDENTITY_RSA,
		BadSignature, 1, &Signature
	) == XTLS_ITEM_ERROR, "unknown local TLS signature was accepted");
	testRequire(Signature == (xtlssignature)0x7777,
		"failed TLS signature selection changed output");
}



/* 畸形、越组和乱序 key_share 必须返回协议错误。 */
static void testTlsKeyShareErrors(void)
{
	static const uint8 GroupData[] = {
		0x00, 0x1D, 0x00, 0x17
	};
	static const uint8 UnknownGroup[] = {
		0x00, 0x05, 0x00, 0x18, 0x00, 0x01, 0x42
	};
	static const uint8 WrongOrder[] = {
		0x00, 0x0A,
		0x00, 0x17, 0x00, 0x01, 0x42,
		0x00, 0x1D, 0x00, 0x01, 0x24
	};
	static const uint8 Truncated[] = {
		0x00, 0x05, 0x00, 0x1D, 0x00, 0x02, 0x42
	};
	static const uint16 Preferred[] = {
		XTLS_GROUP_X25519, XTLS_GROUP_SECP256R1
	};
	xtlsids Groups = { { GroupData, sizeof(GroupData) } };
	xtlskeyshareselection Selection;
	xtlskeyshareselection Before;

	memset(&Selection, 0xA5, sizeof(Selection));
	Before = Selection;
	testRequire(xrtTlsKeyShareSelect(
		&Groups, (xbytesview) { UnknownGroup, sizeof(UnknownGroup) },
		Preferred, 2, XTLS_KEY_SHARE_PREFER_READY, &Selection
	) == XTLS_ITEM_ERROR, "key share outside supported_groups was accepted");
	testRequire(memcmp(&Selection, &Before, sizeof(Selection)) == 0,
		"failed out-of-group key-share selection changed output");

	testRequire(xrtTlsKeyShareSelect(
		&Groups, (xbytesview) { WrongOrder, sizeof(WrongOrder) },
		Preferred, 2, XTLS_KEY_SHARE_PREFER_READY, &Selection
	) == XTLS_ITEM_ERROR, "out-of-order key shares were accepted");
	testRequire(memcmp(&Selection, &Before, sizeof(Selection)) == 0,
		"failed ordered key-share selection changed output");

	testRequire(xrtTlsKeyShareSelect(
		&Groups, (xbytesview) { Truncated, sizeof(Truncated) },
		Preferred, 2, XTLS_KEY_SHARE_PREFER_READY, &Selection
	) == XTLS_ITEM_ERROR, "truncated key share was accepted");
	testRequire(memcmp(&Selection, &Before, sizeof(Selection)) == 0,
		"failed truncated key-share selection changed output");

	testRequire(xrtTlsKeyShareSelect(
		&Groups, (xbytesview) { NULL, 0 },
		Preferred, 2, (xtlskeysharepolicy)2, &Selection
	) == XTLS_ITEM_ERROR, "invalid key-share policy was accepted");
}



/* 无交集是正常结果，不得设置错误或修改输出。 */
static void testTlsNegotiationMiss(void)
{
	static const uint8 OfferedData[] = { 0x13, 0x01 };
	static const xtlscipher Preferred[] = {
		XTLS_ECDHE_RSA_AES_128_GCM_SHA256
	};
	xtlsids Offered = { { OfferedData, sizeof(OfferedData) } };
	xtlscipher Cipher = (xtlscipher)0x7777;

	xrtClearError();
	testRequire(xrtTlsCipherSelect(
		XTLS_VERSION_12, &Offered, XTLS_IDENTITY_RSA,
		Preferred, 1, &Cipher
	) == XTLS_ITEM_DONE, "TLS negotiation miss was not reported");
	testRequire((Cipher == (xtlscipher)0x7777) &&
		(xrtGetError() == NULL),
		"TLS negotiation miss changed output or set an error");
}



/* 执行 TLS 协商错误和边界回归。 */
int main(void)
{
	testTlsNegotiationFailureAtomic();
	testTlsKeyShareErrors();
	testTlsNegotiationMiss();
	return 0;
}
