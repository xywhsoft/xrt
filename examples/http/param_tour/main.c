/*
 * 范例：http/param_tour —— 参数与 quoted-string 全接口巡礼
 * ----------------------------------------------------------------
 * 演示 API：
 *   xrtHttpParamCount / ParamFind       参数计数 / 查找
 *   xrtHttpParamBuild                   单参数序列化（拥有式）
 *   xrtHttpParamWrite                   参数写出（缓冲版）
 *   xrtHttpParamValueCursorInit / ValueNext   值语义逐字节游标
 *   xrtHttpParamTokenValid / TokenEqual / HostValid（均收 Param 描述符）
 *   xrtHttpQuotedValid / QuotedRead / QuotedWrite / QuotedBuild
 *   xrtHttpDirectiveCount / Find / Next   指令族（Cache-Control 风格）
 * 模块宏：XRT_MODULE_HTTP
 * 编译（单头形态，Windows）：
 *   gcc -O1 -DXRT_MODULE_ALL -I single -include xrt.h impl.c \
 *       examples/http/param_tour/main.c -lws2_32 -liphlpapi
 * 预期输出：
 *   count=2 find=boundary write=charset=UTF-8
 *   value-bytes=7 quoted-read=part;42 quoted-build="a\"b"
 *   token(quoted)=0 token(ok)=1 eq=1 host=1
 *   directives: no-cache no-store count=2 find=1
 *
 * 谓词族 Flags 必须 XHTTP_PARAM_HAS_VALUE；quoted 值不是 token
 *   → TokenValid 拒——对比演示两个方向的判定。
 */

#include <stdio.h>
#include <xrt.h>

#define SV(x) XRT_STR_LITERAL(x)

int main(void)
{
	static const char sParams[] = "charset=UTF-8; boundary=\"part;42\"";
	xhttpparam Param;
	xhttpparamvaluecursor Cursor;
	uint8 iByte;
	size_t iOffset = 0;
	size_t iCount = 0;
	size_t iSize = 0;
	char Buffer[64];
	str sBuilt;

	/* Count + Find。 */
	if ( !xrtHttpParamCount(SV(sParams), &iCount) || (iCount != 2u) ) {
		return 1;
	}
	printf("count=%zu", iCount);
	if ( xrtHttpParamFind(SV(sParams), SV("boundary"), &Param) ==
		XHTTP_NEXT_ITEM ) {
		printf(" find=%.*s", (int)Param.Name.Size, Param.Name.Data);
	}

	/* Write（缓冲版）；Build（拥有式）同语义。 */
	if ( xrtHttpParamWrite(SV("charset"), SV("UTF-8"), XHTTP_PARAM_HAS_VALUE, Buffer,
		sizeof(Buffer), &iSize) ) {
		Buffer[iSize] = 0;
		printf(" write=%s\n", Buffer);
	}
	sBuilt = xrtHttpParamBuild(SV("charset"), SV("UTF-8"), XHTTP_PARAM_HAS_VALUE, NULL);
	xrtFree(sBuilt);

	/* 值语义逐字节游标（quoted 值的解码字节流）。 */
	{
		size_t iBytes = 0;

		xrtHttpParamValueCursorInit(&Cursor);
		while ( xrtHttpParamValueNext(&Param, &Cursor, &iByte) ==
			XHTTP_NEXT_ITEM ) {
			iBytes++;
		}
		printf("value-bytes=%zu", iBytes);
	}

	/* Quoted 四动作。 */
	(void)xrtHttpQuotedValid(SV("\"part;42\""));
	if ( xrtHttpQuotedRead(SV("\"part;42\""), Buffer, sizeof(Buffer) - 1u,
		&iSize) ) {
		Buffer[iSize] = 0;
		printf(" quoted-read=%s", Buffer);
	}
	sBuilt = xrtHttpQuotedBuild(SV("a\"b"), NULL);
	printf(" quoted-build=%s\n", sBuilt ? sBuilt : "?");
	xrtFree(sBuilt);
	(void)xrtHttpQuotedWrite(SV("a\"b"), Buffer, sizeof(Buffer), &iSize);

	/* Token/Host 谓词族（均收 Param 描述符，Flags 含 HAS_VALUE）。 */
	{
		xhttpparam TokenParam = { SV("charset"), SV("utf-8"),
			XHTTP_PARAM_HAS_VALUE };
		xhttpparam HostParam = { SV("Host"), SV("example.com"),
			XHTTP_PARAM_HAS_VALUE };

		printf("token(quoted)=%d token(ok)=%d eq=%d host=%d\n",
			xrtHttpParamTokenValid(&Param) ? 1 : 0,
			xrtHttpParamTokenValid(&TokenParam) ? 1 : 0,
			xrtHttpParamTokenEqual(&TokenParam, SV("utf-8")) ? 1 : 0,
			xrtHttpParamHostValid(&HostParam) ? 1 : 0);
	}

	/* 指令族（Cache-Control 风格）。 */
	{
		static const char sDirectives[] = "no-cache, no-store";
		xhttpparam Directive;

		printf("directives:");
		while ( xrtHttpDirectiveNext(SV(sDirectives), &iOffset,
			&Directive) == XHTTP_NEXT_ITEM ) {
			printf(" %.*s", (int)Directive.Name.Size, Directive.Name.Data);
		}
		(void)xrtHttpDirectiveCount(SV(sDirectives), &iCount);
		printf(" count=%zu", iCount);
		printf(" find=%d\n",
			xrtHttpDirectiveFind(SV(sDirectives), SV("no-store"),
				&Directive) == XHTTP_NEXT_ITEM ? 1 : 0);
	}
	return 0;
}
