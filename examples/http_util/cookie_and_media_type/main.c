/*
 * XRT Example - Cookie And Media Type
 * XRT 范例 - Cookie 与 Media Type
 *
 * Description / 说明:
 *   EN: Demonstrates parsing Set-Cookie, media type, multipart boundary, and Content-Disposition with UTF-8 filename decoding.
 *   CN: 演示解析 Set-Cookie、media type、multipart boundary，以及带 UTF-8 文件名扩展的 Content-Disposition。
 *
 * Build / 编译:
 *   tcc main.c -o ../../bin/http_util_cookie_and_media_type.exe -lWs2_32 -lIPHLPAPI -lShell32
 *   gcc main.c -o ../../bin/http_util_cookie_and_media_type -lm -lpthread
 */

#include "../../../xrt.c"
#include <stdio.h>


static const char* procSameSiteName(uint8 iSameSite)
{
	switch ( iSameSite ) {
		case XRT_SAME_SITE_LAX: return "Lax";
		case XRT_SAME_SITE_STRICT: return "Strict";
		case XRT_SAME_SITE_NONE: return "None";
		default: return "Unspecified";
	}
}


static const char* procPriorityName(uint8 iPriority)
{
	switch ( iPriority ) {
		case XRT_COOKIE_PRIORITY_LOW: return "Low";
		case XRT_COOKIE_PRIORITY_MEDIUM: return "Medium";
		case XRT_COOKIE_PRIORITY_HIGH: return "High";
		default: return "Unspecified";
	}
}


int main(void)
{
	xrtsetcookieview tSetCookie;
	xrtmediatypeview tMediaType;
	xrthttpparam tParam;
	xrtcontentdispositionview tDisposition;
	xrtstrview tBoundary;
	char sBuilt[256];
	char sFileName[128];
	size_t iBuiltLen = 0u;
	size_t iFileNameLen = 0u;
	int iRet = 0;

	xrtInit();
	memset(&tSetCookie, 0, sizeof(tSetCookie));
	memset(&tMediaType, 0, sizeof(tMediaType));
	memset(&tParam, 0, sizeof(tParam));
	memset(&tDisposition, 0, sizeof(tDisposition));
	memset(&tBoundary, 0, sizeof(tBoundary));
	memset(sBuilt, 0, sizeof(sBuilt));
	memset(sFileName, 0, sizeof(sFileName));

	if ( !xrtSetCookieParse("sid=abc123; Path=/; HttpOnly; Secure; SameSite=Lax; Max-Age=60", &tSetCookie) ) {
		printf("parse_set_cookie = failed\n");
		iRet = 1;
		goto cleanup;
	}

	printf("cookie.name = %.*s\n", (int)tSetCookie.tName.iLen, tSetCookie.tName.sPtr);
	printf("cookie.value = %.*s\n", (int)tSetCookie.tValue.iLen, tSetCookie.tValue.sPtr);
	printf("cookie.path = %.*s\n", (int)tSetCookie.tPath.iLen, tSetCookie.tPath.sPtr);
	printf("cookie.same_site = %s\n", procSameSiteName(tSetCookie.iSameSite));
	printf("cookie.max_age = %d\n", (int)tSetCookie.iMaxAge);
	printf("cookie.secure = %s\n", (tSetCookie.iFlags & XRT_SET_COOKIE_F_SECURE) != 0u ? "yes" : "no");
	printf("cookie.http_only = %s\n", (tSetCookie.iFlags & XRT_SET_COOKIE_F_HTTP_ONLY) != 0u ? "yes" : "no");

	if ( !xrtSetCookieParse("sid=abc123; SameParty; Priority=High", &tSetCookie) ) {
		printf("parse_cookie_priority = failed\n");
		iRet = 1;
		goto cleanup;
	}
	printf("cookie.priority = %s\n", procPriorityName(tSetCookie.iPriority));

	if ( !xrtHttpMediaTypeParse("multipart/form-data; boundary=uploadB; charset=UTF-8", &tMediaType) ) {
		printf("parse_media_type = failed\n");
		iRet = 1;
		goto cleanup;
	}
	if ( !xrtHttpMediaTypeFindParam(&tMediaType, "boundary", &tParam) ) {
		printf("find_boundary_param = failed\n");
		iRet = 1;
		goto cleanup;
	}
	iBuiltLen = 0u;
	if ( !xrtHttpMediaTypeBuild(&tMediaType, sBuilt, sizeof(sBuilt), &iBuiltLen) ) {
		printf("build_media_type = failed\n");
		iRet = 1;
		goto cleanup;
	}

	printf("media_type.type = %.*s/%.*s\n",
		(int)tMediaType.tType.iLen, tMediaType.tType.sPtr,
		(int)tMediaType.tSubType.iLen, tMediaType.tSubType.sPtr);
	printf("media_type.boundary = %.*s\n", (int)tParam.tValue.iLen, tParam.tValue.sPtr);
	printf("media_type.built = %s\n", sBuilt);

	if ( !xrtMultipartBoundaryFromContentType(sBuilt, &tBoundary) ) {
		printf("boundary_from_content_type = failed\n");
		iRet = 1;
		goto cleanup;
	}
	printf("content_type.boundary = %.*s\n", (int)tBoundary.iLen, tBoundary.sPtr);

	if ( !xrtHttpContentDispositionParse("form-data; name=\"file\"; filename=\"fallback.txt\"; filename*=UTF-8''%E4%B8%AD%E6%96%87.txt", &tDisposition) ) {
		printf("parse_content_disposition = failed\n");
		iRet = 1;
		goto cleanup;
	}
	if ( !xrtHttpContentDispositionDecodeFileNameTo(&tDisposition, sFileName, sizeof(sFileName), &iFileNameLen) ) {
		printf("decode_file_name = failed\n");
		iRet = 1;
		goto cleanup;
	}

	printf("disposition.type = %.*s\n", (int)tDisposition.tType.iLen, tDisposition.tType.sPtr);
	printf("disposition.name = %.*s\n", (int)tDisposition.tName.iLen, tDisposition.tName.sPtr);
	printf("disposition.filename = %.*s\n", (int)tDisposition.tFileName.iLen, tDisposition.tFileName.sPtr);
	printf("disposition.filename_ext = %.*s\n", (int)tDisposition.tFileNameExt.iLen, tDisposition.tFileNameExt.sPtr);
	printf("disposition.filename_utf8 = %s\n", sFileName);

cleanup:
	xrtUnit();
	return iRet;
}
