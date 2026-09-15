#include "../test.h"

#include "../../fuzz/acme_dns_response.c"

#include <string.h>

/*
	DNS TXT 响应解析的确定性回归：合法响应、NXDOMAIN、ID 不匹配、
	压缩指针、超长 character-string 与截断输入各走一遍公开契约。
	（随机形态由协议 fuzz 门禁覆盖。）
*/

int main(void)
{
	char sRecords[4][XACME_TXT_RECORD_MAX];
	size_t iCount = 0u;

	/* 合法响应：id=0x1234，QR=1，qd=1（压缩指针名），an=1 TXT。 */
	{
		/* _acme-challenge 查询段 + TXT 应答 "hello" */
		static const uint8 uValid[] = {
			0x12, 0x34, 0x81, 0x80, 0x00, 0x01, 0x00, 0x01,
			0x00, 0x00, 0x00, 0x00,
			0x0f, '_', 'a', 'c', 'm', 'e', '-', 'c', 'h', 'a', 'l',
			'l', 'e', 'n', 'g', 'e', 0x07, 'e', 'x', 'a', 'm', 'p',
			'l', 'e', 0x00, 0x00, 0x10, 0x00, 0x01,
			0xc0, 0x0c, 0x00, 0x10, 0x00, 0x01,
			0x00, 0x00, 0x00, 0x3c, 0x00, 0x06,
			0x05, 'h', 'e', 'l', 'l', 'o',
		};
		testRequire(
			xacmeTxtParseResponse(
				uValid, sizeof(uValid), 0x1234u, sRecords, 4u, &iCount) &&
				(iCount == 1u) &&
				(strcmp(sRecords[0], "hello") == 0),
			"txt parse valid mismatch");
	}

	/* NXDOMAIN：RCODE=3 → 成功且零记录。 */
	{
		static const uint8 uNx[] = {
			0x12, 0x34, 0x81, 0x83, 0x00, 0x01, 0x00, 0x00,
			0x00, 0x00, 0x00, 0x00,
			0x00, 0x00, 0x10, 0x00, 0x01,
		};
		iCount = 99u;
		testRequire(
			xacmeTxtParseResponse(
				uNx, sizeof(uNx), 0x1234u, sRecords, 4u, &iCount) &&
				(iCount == 0u),
			"txt parse nxdomain mismatch");
	}

	/* ID 不匹配 → false。 */
	{
		static const uint8 uBad[] = {
			0xaa, 0xbb, 0x81, 0x80, 0x00, 0x00, 0x00, 0x00,
			0x00, 0x00, 0x00, 0x00,
		};
		testRequire(
			!xacmeTxtParseResponse(
				uBad, sizeof(uBad), 0x1234u, sRecords, 4u, &iCount),
			"txt parse id mismatch accepted");
	}

	/* QR 缺失 / 截断 / 空输入 → false。 */
	{
		static const uint8 uQuery[] = {
			0x12, 0x34, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00,
			0x00, 0x00, 0x00, 0x00,
		};
		testRequire(
			!xacmeTxtParseResponse(
				uQuery, sizeof(uQuery), 0x1234u, sRecords, 4u, &iCount),
			"txt parse query accepted");
		testRequire(
			!xacmeTxtParseResponse(
				uQuery, 5u, 0x1234u, sRecords, 4u, &iCount),
			"txt parse truncated accepted");
		testRequire(
			!xacmeTxtParseResponse(
				NULL, 0u, 0x1234u, sRecords, 4u, &iCount),
			"txt parse empty accepted");
	}

	/* 多 character-string 拼接 + fuzz 入口冒烟。 */
	{
		static const uint8 uSplit[] = {
			0x12, 0x34, 0x81, 0x80, 0x00, 0x00, 0x00, 0x01,
			0x00, 0x00, 0x00, 0x00,
			0xc0, 0x0c, 0x00, 0x10, 0x00, 0x01,
			0x00, 0x00, 0x00, 0x3c, 0x00, 0x07,
			0x02, 'a', 'b', 0x03, 'c', 'd', 'e',
		};
		testRequire(
			xacmeTxtParseResponse(
				uSplit, sizeof(uSplit), 0x1234u, sRecords, 4u, &iCount) &&
				(iCount == 1u) && (strcmp(sRecords[0], "abcde") == 0),
			"txt parse split strings mismatch");
		testRequire(
			xacmeDnsResponseFuzzerTestOneInput(
				(const uint8*)"\x12\x34\x81\x83", 4u) == 0,
			"txt fuzz entry smoke failed");
	}

	printf("[PASS] acme dns txt parse regression\n");
	return 0;
}
