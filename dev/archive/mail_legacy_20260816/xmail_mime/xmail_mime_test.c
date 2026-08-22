#define XRT_IMPLEMENTATION
#include "../../singlehead/xrt.h"
#include "xmail_mime.h"

static int xmail_mime_test_equal(const char* sName, const char* sGot, const char* sExpect)
{
	if ( (sGot == NULL) || (strcmp(sGot, sExpect) != 0) ) {
		printf("FAIL %s\n", sName);
		printf("got   : %s\n", sGot ? sGot : "(null)");
		printf("expect: %s\n", sExpect);
		return 0;
	}
	printf("PASS %s\n", sName);
	return 1;
}

static int xmail_mime_test_contains(const char* sName, const char* sText, const char* sNeedle)
{
	if ( (sText == NULL) || (strstr(sText, sNeedle) == NULL) ) {
		printf("FAIL %s\n", sName);
		printf("text  : %s\n", sText ? sText : "(null)");
		printf("needle: %s\n", sNeedle);
		return 0;
	}
	printf("PASS %s\n", sName);
	return 1;
}

static int xmail_mime_test_true(const char* sName, int bValue)
{
	if ( !bValue ) {
		printf("FAIL %s\n", sName);
		return 0;
	}
	printf("PASS %s\n", sName);
	return 1;
}

int main(void)
{
	int bPass = 1;
	str sText;
	str sDecoded;
	str sWrapped;
	str sJoined;
	xmailaddr tAddr;
	xmailaddr tParsedAddr;
	xmailaddr* arrParsedAddr;
	size_t iParsedAddrCount;
	xmailaddr arrAddr[2];
	xmailmessage tMsg;
	xmailmimeattachment tAttachment;
	xmailmimeattachment arrRelated[2];
	xmailheader arrHeaders[2];
	xmailheader arrPartHeaders[1];
	xmailparsedmessage tParsed;
	xmailmimepart tRootPart;
	xmailmimepart arrRootPartChildren[2];
	xmailmimepart arrAltPartChildren[2];
	xmailaddr tBcc;
	const char sAttachmentData[] = "attachment bytes";
	const char sInlineData[] = "inline image";
	const char sInvalidUtf8[] = { (char)0xC3, (char)0x28, '\0' };

	setvbuf(stdout, NULL, _IONBF, 0);
	printf("xmail_mime_test\n");

	sText = xmailMimeNormalizeCrlf("a\nb\rc\r\nd");
	bPass = bPass && xmail_mime_test_equal("normalize_crlf", (const char*)sText, "a\r\nb\r\nc\r\nd");
	xmailMimeFree(sText);

	sText = xmailMimeEncodeHeaderWord("中文主题");
	bPass = bPass && xmail_mime_test_equal("header_word_encode", (const char*)sText, "=?UTF-8?B?5Lit5paH5Li76aKY?=");
	sDecoded = xmailMimeDecodeHeaderWord((const char*)sText);
	bPass = bPass && xmail_mime_test_equal("header_word_decode_b", (const char*)sDecoded, "中文主题");
	xmailMimeFree(sText);
	xmailMimeFree(sDecoded);

	sDecoded = xmailMimeDecodeHeaderWord("=?UTF-8?Q?hello_=E4=B8=AD=E6=96=87?=");
	bPass = bPass && xmail_mime_test_equal("header_word_decode_q", (const char*)sDecoded, "hello 中文");
	xmailMimeFree(sDecoded);

	sText = xmailMimeQuotedPrintableEncode("hello 中文 \nend\t\n");
	bPass = bPass && xmail_mime_test_contains("qp_encode_utf8", (const char*)sText, "=E4=B8=AD=E6=96=87");
	bPass = bPass && xmail_mime_test_contains("qp_encode_trailing_space", (const char*)sText, "=20\r\n");
	sDecoded = xmailMimeQuotedPrintableDecode((const char*)sText);
	bPass = bPass && xmail_mime_test_equal("qp_decode", (const char*)sDecoded, "hello 中文 \r\nend\t\r\n");
	xmailMimeFree(sText);
	xmailMimeFree(sDecoded);

	sWrapped = xmailMimeBase64Wrap("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789", 62u, 16u);
	bPass = bPass && xmail_mime_test_contains("base64_wrap_crlf", (const char*)sWrapped, "\r\n");
	xmailMimeFree(sWrapped);

	bPass = bPass && (xmailMimeHasHeaderInjection("safe subject") == FALSE);
	bPass = bPass && (xmailMimeHasHeaderInjection("bad\r\nBcc: x") == TRUE);
	sText = xmailMimeEncodeHeaderWord("bad\nsubject");
	bPass = bPass && (sText == NULL);
	xmailMimeFree(sText);
	sText = xmailMimeEncodeHeaderWord(sInvalidUtf8);
	bPass = bPass && xmail_mime_test_true("header_word_reject_invalid_utf8", sText == NULL);
	xmailMimeFree(sText);

	sText = xmailMimeFoldHeaderLine("Subject", "alpha beta gamma delta epsilon zeta eta theta", 30u);
	bPass = bPass && xmail_mime_test_contains("header_fold", (const char*)sText, "\r\n ");
	sDecoded = xmailMimeUnfoldHeaderBlock((const char*)sText);
	bPass = bPass && xmail_mime_test_equal("header_unfold", (const char*)sDecoded, "Subject: alpha beta gamma delta epsilon zeta eta theta\r\n");
	xmailMimeFree(sText);
	xmailMimeFree(sDecoded);

	sText = xmailMimeRfc2822DateNow();
	bPass = bPass && xmail_mime_test_contains("rfc2822_date_comma", (const char*)sText, ", ");
	xmailMimeFree(sText);

	sText = xmailMimeBuildMessageId("example.com");
	bPass = bPass && xmail_mime_test_contains("message_id_domain", (const char*)sText, "@example.com>");
	xmailMimeFree(sText);

	memset(&tAddr, 0, sizeof(tAddr));
	tAddr.sEmail = "sender@example.com";
	tAddr.sName = "中文 Sender";
	sText = xmailMimeFormatAddress(&tAddr);
	bPass = bPass && xmail_mime_test_equal("format_address_utf8", (const char*)sText, "=?UTF-8?B?5Lit5paHIFNlbmRlcg==?= <sender@example.com>");
	xmailMimeFree(sText);
	tAddr.sName = sInvalidUtf8;
	sText = xmailMimeFormatAddress(&tAddr);
	bPass = bPass && xmail_mime_test_true("format_address_reject_invalid_utf8", sText == NULL);
	xmailMimeFree(sText);

	memset(arrAddr, 0, sizeof(arrAddr));
	arrAddr[0].sEmail = "a@example.com";
	arrAddr[0].sName = "A User";
	arrAddr[1].sEmail = "b@example.com";
	arrAddr[1].sName = "Comma, User";
	sText = xmailMimeFormatAddressList(arrAddr, 2u);
	bPass = bPass && xmail_mime_test_equal("format_address_list", (const char*)sText, "A User <a@example.com>, \"Comma, User\" <b@example.com>");
	xmailMimeFree(sText);

	memset(&tParsedAddr, 0, sizeof(tParsedAddr));
	bPass = bPass && xmailMimeParseAddress("\"Comma, User\" <comma@example.com>", &tParsedAddr);
	bPass = bPass && xmail_mime_test_equal("parse_address_name", tParsedAddr.sName, "Comma, User");
	bPass = bPass && xmail_mime_test_equal("parse_address_email", tParsedAddr.sEmail, "comma@example.com");
	xmailMimeAddressFree(&tParsedAddr);

	memset(&tParsedAddr, 0, sizeof(tParsedAddr));
	bPass = bPass && xmail_mime_test_true("parse_address_simple_name", xmailMimeParseAddress("A User <a@example.com>", &tParsedAddr));
	xmailMimeAddressFree(&tParsedAddr);

	memset(&tParsedAddr, 0, sizeof(tParsedAddr));
	bPass = bPass && xmail_mime_test_true("parse_address_bare", xmailMimeParseAddress("bare@example.com", &tParsedAddr));
	xmailMimeAddressFree(&tParsedAddr);

	arrParsedAddr = NULL;
	iParsedAddrCount = 0u;
	bPass = bPass && xmail_mime_test_true("parse_address_list", xmailMimeParseAddressList("A User <a@example.com>, \"Comma, User\" <b@example.com>, bare@example.com", &arrParsedAddr, &iParsedAddrCount));
	bPass = bPass && xmail_mime_test_true("parse_address_list_count", iParsedAddrCount == 3u);
	bPass = bPass && xmail_mime_test_equal("parse_address_list_0_name", arrParsedAddr[0].sName, "A User");
	bPass = bPass && xmail_mime_test_equal("parse_address_list_1_email", arrParsedAddr[1].sEmail, "b@example.com");
	bPass = bPass && xmail_mime_test_equal("parse_address_list_2_bare", arrParsedAddr[2].sEmail, "bare@example.com");
	xmailMimeAddressListFree(arrParsedAddr, iParsedAddrCount);

	memset(&tParsedAddr, 0, sizeof(tParsedAddr));
	bPass = bPass && xmailMimeParseAddress("=?UTF-8?B?5Lit5paHIFNlbmRlcg==?= <utf8@example.com>", &tParsedAddr);
	bPass = bPass && xmail_mime_test_equal("parse_address_encoded_name", tParsedAddr.sName, "中文 Sender");
	xmailMimeAddressFree(&tParsedAddr);

	memset(&tMsg, 0, sizeof(tMsg));
	tMsg.tFrom.sEmail = "sender@example.com";
	tMsg.tFrom.sName = "Sender";
	tMsg.tReplyTo.sEmail = "reply@example.com";
	tMsg.tReplyTo.sName = "Reply User";
	tMsg.arrTo = arrAddr;
	tMsg.iToCount = 1u;
	tBcc.sEmail = "hidden@example.com";
	tBcc.sName = "Hidden User";
	tMsg.arrBcc = &tBcc;
	tMsg.iBccCount = 1u;
	tMsg.sSubject = "Hello 中文";
	tMsg.sTextBody = "plain body";
	arrHeaders[0].sName = "X-Mailer";
	arrHeaders[0].sValue = "xmail_mime_test";
	arrHeaders[1].sName = "X-Trace";
	arrHeaders[1].sValue = "abc123";
	tMsg.arrHeaders = arrHeaders;
	tMsg.iHeaderCount = 2u;
	tMsg.sMessageIdDomain = "example.com";
	sText = xmailMimeBuildMessage(&tMsg);
	bPass = bPass && xmail_mime_test_contains("message_text_type", (const char*)sText, "Content-Type: text/plain; charset=UTF-8");
	bPass = bPass && xmail_mime_test_contains("message_subject_encoded", (const char*)sText, "Subject: =?UTF-8?B?SGVsbG8g5Lit5paH?=");
	bPass = bPass && xmail_mime_test_contains("message_reply_to", (const char*)sText, "Reply-To: Reply User <reply@example.com>");
	bPass = bPass && xmail_mime_test_contains("message_custom_header", (const char*)sText, "X-Mailer: xmail_mime_test");
	bPass = bPass && (strstr((const char*)sText, "Bcc:") == NULL);
	bPass = bPass && (strstr((const char*)sText, "hidden@example.com") == NULL);
	xmailMimeFree(sText);

	tMsg.sTextBody = sInvalidUtf8;
	sText = xmailMimeBuildMessage(&tMsg);
	bPass = bPass && xmail_mime_test_true("message_reject_invalid_utf8_body", sText == NULL);
	xmailMimeFree(sText);
	tMsg.sTextBody = "plain body";
	arrHeaders[1].sValue = sInvalidUtf8;
	sText = xmailMimeBuildMessage(&tMsg);
	bPass = bPass && xmail_mime_test_true("message_reject_invalid_utf8_header", sText == NULL);
	xmailMimeFree(sText);
	arrHeaders[1].sValue = "abc123";

	memset(&tMsg, 0, sizeof(tMsg));
	tMsg.tFrom.sEmail = "sender@example.com";
	tMsg.arrTo = arrAddr;
	tMsg.iToCount = 1u;
	tMsg.sSubject = "Alt";
	tMsg.sTextBody = "plain";
	tMsg.sHtmlBody = "<p>html</p>";
	tMsg.sMessageIdDomain = "example.com";
	sText = xmailMimeBuildMessage(&tMsg);
	bPass = bPass && xmail_mime_test_contains("message_alternative", (const char*)sText, "Content-Type: multipart/alternative; boundary=\"xmail-alt-");
	bPass = bPass && xmail_mime_test_contains("message_html_part", (const char*)sText, "Content-Type: text/html; charset=UTF-8");
	xmailMimeFree(sText);

	memset(&tMsg, 0, sizeof(tMsg));
	memset(&tAttachment, 0, sizeof(tAttachment));
	tAttachment.sFileName = "report.txt";
	tAttachment.sContentType = "text/plain";
	tAttachment.pData = sAttachmentData;
	tAttachment.iDataLen = sizeof(sAttachmentData) - 1u;
	tMsg.tFrom.sEmail = "sender@example.com";
	tMsg.arrTo = arrAddr;
	tMsg.iToCount = 1u;
	tMsg.sSubject = "Mixed";
	tMsg.sTextBody = "with attachment";
	tMsg.arrAttachments = &tAttachment;
	tMsg.iAttachmentCount = 1u;
	tMsg.sMessageIdDomain = "example.com";
	sText = xmailMimeBuildMessage(&tMsg);
	bPass = bPass && xmail_mime_test_contains("message_mixed", (const char*)sText, "Content-Type: multipart/mixed; boundary=\"xmail-mixed-");
	bPass = bPass && xmail_mime_test_contains("message_attachment_disposition", (const char*)sText, "Content-Disposition: attachment; filename=\"report.txt\"; filename*=UTF-8''report.txt");
	xmailMimeFree(sText);

	memset(&tMsg, 0, sizeof(tMsg));
	memset(&tAttachment, 0, sizeof(tAttachment));
	tAttachment.sFileName = "报告.txt";
	tAttachment.sContentType = "text/plain";
	tAttachment.pData = sAttachmentData;
	tAttachment.iDataLen = sizeof(sAttachmentData) - 1u;
	tMsg.tFrom.sEmail = "sender@example.com";
	tMsg.arrTo = arrAddr;
	tMsg.iToCount = 1u;
	tMsg.sSubject = "UTF8 filename";
	tMsg.sTextBody = "utf8 attachment";
	tMsg.arrAttachments = &tAttachment;
	tMsg.iAttachmentCount = 1u;
	tMsg.sMessageIdDomain = "example.com";
	sText = xmailMimeBuildMessage(&tMsg);
	bPass = bPass && xmail_mime_test_contains("message_utf8_filename_ext", (const char*)sText, "filename*=UTF-8''%E6%8A%A5%E5%91%8A.txt");
	xmailMimeFree(sText);
	tAttachment.sFileName = sInvalidUtf8;
	sText = xmailMimeBuildMessage(&tMsg);
	bPass = bPass && xmail_mime_test_true("message_reject_invalid_utf8_filename", sText == NULL);
	xmailMimeFree(sText);

	memset(&tMsg, 0, sizeof(tMsg));
	memset(arrRelated, 0, sizeof(arrRelated));
	arrRelated[0].sFileName = "logo.png";
	arrRelated[0].sContentType = "image/png";
	arrRelated[0].sContentId = "logo-img";
	arrRelated[0].pData = sInlineData;
	arrRelated[0].iDataLen = sizeof(sInlineData) - 1u;
	arrRelated[0].bInline = TRUE;
	arrRelated[1].sFileName = "report.txt";
	arrRelated[1].sContentType = "text/plain";
	arrRelated[1].pData = sAttachmentData;
	arrRelated[1].iDataLen = sizeof(sAttachmentData) - 1u;
	tMsg.tFrom.sEmail = "sender@example.com";
	tMsg.arrTo = arrAddr;
	tMsg.iToCount = 1u;
	tMsg.sSubject = "Related";
	tMsg.sTextBody = "plain related";
	tMsg.sHtmlBody = "<p><img src=\"cid:logo-img\"></p>";
	tMsg.arrAttachments = arrRelated;
	tMsg.iAttachmentCount = 2u;
	tMsg.sMessageIdDomain = "example.com";
	sText = xmailMimeBuildMessage(&tMsg);
	bPass = bPass && xmail_mime_test_contains("message_related", (const char*)sText, "Content-Type: multipart/related; boundary=\"xmail-related-");
	bPass = bPass && xmail_mime_test_contains("message_inline_disposition", (const char*)sText, "Content-Disposition: inline; filename=\"logo.png\"; filename*=UTF-8''logo.png");
	bPass = bPass && xmail_mime_test_contains("message_inline_content_id", (const char*)sText, "Content-ID: <logo-img>");
	bPass = bPass && xmail_mime_test_contains("message_related_keeps_attachment", (const char*)sText, "Content-Disposition: attachment; filename=\"report.txt\"; filename*=UTF-8''report.txt");
	xmailMimeFree(sText);

	memset(&tRootPart, 0, sizeof(tRootPart));
	memset(arrRootPartChildren, 0, sizeof(arrRootPartChildren));
	memset(arrAltPartChildren, 0, sizeof(arrAltPartChildren));
	memset(arrPartHeaders, 0, sizeof(arrPartHeaders));
	strcpy(tRootPart.sMediaType, "multipart");
	strcpy(tRootPart.sSubType, "mixed");
	strcpy(tRootPart.sBoundary, "tree-mix");
	arrPartHeaders[0].sName = "X-Tree";
	arrPartHeaders[0].sValue = "recursive";
	tRootPart.arrHeaders = arrPartHeaders;
	tRootPart.iHeaderCount = 1u;
	tRootPart.arrChildren = arrRootPartChildren;
	tRootPart.iChildCount = 2u;
	strcpy(arrRootPartChildren[0].sContentType, "text/plain; charset=UTF-8");
	strcpy(arrRootPartChildren[0].sTransferEncoding, "quoted-printable");
	arrRootPartChildren[0].sBody = (str)"tree plain 中文";
	strcpy(arrRootPartChildren[1].sMediaType, "multipart");
	strcpy(arrRootPartChildren[1].sSubType, "alternative");
	strcpy(arrRootPartChildren[1].sBoundary, "tree-alt");
	arrRootPartChildren[1].arrChildren = arrAltPartChildren;
	arrRootPartChildren[1].iChildCount = 2u;
	strcpy(arrAltPartChildren[0].sContentType, "text/plain; charset=UTF-8");
	arrAltPartChildren[0].sBody = (str)"alt plain";
	strcpy(arrAltPartChildren[1].sContentType, "text/html; charset=UTF-8");
	arrAltPartChildren[1].sBody = (str)"<p>alt html</p>";
	sText = xmailMimeBuildPart(&tRootPart);
	bPass = bPass && xmail_mime_test_contains("part_tree_build_root", (const char*)sText, "Content-Type: multipart/mixed; boundary=\"tree-mix\"");
	bPass = bPass && xmail_mime_test_contains("part_tree_build_custom_header", (const char*)sText, "X-Tree: recursive");
	bPass = bPass && xmail_mime_test_contains("part_tree_build_nested", (const char*)sText, "Content-Type: multipart/alternative; boundary=\"tree-alt\"");
	bPass = bPass && xmail_mime_test_contains("part_tree_build_qp", (const char*)sText, "=E4=B8=AD=E6=96=87");
	memset(&tParsed, 0, sizeof(tParsed));
	bPass = bPass && xmailMimeParseMessage((const char*)sText, &tParsed);
	bPass = bPass && xmail_mime_test_true("part_tree_parse_root_children", tParsed.pRootPart != NULL && tParsed.pRootPart->iChildCount == 2u);
	bPass = bPass && xmail_mime_test_true("part_tree_parse_nested_children", tParsed.pRootPart != NULL && tParsed.pRootPart->iChildCount > 1u && tParsed.pRootPart->arrChildren[1].iChildCount == 2u);
	bPass = bPass && xmail_mime_test_equal("part_tree_parse_qp_body", (tParsed.pRootPart && tParsed.pRootPart->iChildCount > 0u) ? (const char*)tParsed.pRootPart->arrChildren[0].sDecodedBody : "", "tree plain 中文");
	bPass = bPass && xmail_mime_test_equal("part_tree_parse_html_body", (tParsed.pRootPart && tParsed.pRootPart->iChildCount > 1u && tParsed.pRootPart->arrChildren[1].iChildCount > 1u) ? (const char*)tParsed.pRootPart->arrChildren[1].arrChildren[1].sBody : "", "<p>alt html</p>");
	xmailMimeParsedMessageFree(&tParsed);
	xmailMimeFree(sText);

	memset(&tParsed, 0, sizeof(tParsed));
	bPass = bPass && xmailMimeParseMessage("Subject: =?UTF-8?B?5Lit5paH?=\r\nX-Test: a\r\n b\r\nReceived: one\r\nReceived: two\r\n\r\nbody\r\n", &tParsed);
	bPass = bPass && xmail_mime_test_equal("parse_subject_header", xmailMimeParsedGetHeader(&tParsed, "subject"), "=?UTF-8?B?5Lit5paH?=");
	bPass = bPass && xmail_mime_test_equal("parse_folded_header", xmailMimeParsedGetHeader(&tParsed, "X-Test"), "a b");
	bPass = bPass && (xmailMimeParsedGetHeaderCount(&tParsed, "received") == 2u);
	bPass = bPass && xmail_mime_test_equal("parse_multi_header_0", xmailMimeParsedGetHeaderAt(&tParsed, "Received", 0u), "one");
	bPass = bPass && xmail_mime_test_equal("parse_multi_header_1", xmailMimeParsedGetHeaderAt(&tParsed, "Received", 1u), "two");
	sJoined = xmailMimeParsedJoinHeaders(&tParsed, "Received", ", ");
	bPass = bPass && xmail_mime_test_equal("parse_multi_header_join", (const char*)sJoined, "one, two");
	xmailMimeFree(sJoined);
	bPass = bPass && xmail_mime_test_equal("parse_body", (const char*)tParsed.sBody, "body\r\n");
	xmailMimeParsedMessageFree(&tParsed);

	memset(&tParsed, 0, sizeof(tParsed));
	bPass = bPass && xmailMimeParseMessage(
		"Content-Type: multipart/alternative; boundary=\"b1\"\r\n\r\n"
		"--b1\r\nContent-Type: text/plain; charset=UTF-8\r\n\r\nplain\r\n"
		"--b1\r\nContent-Type: text/html; charset=UTF-8\r\n\r\n<p>html</p>\r\n"
		"--b1--\r\n", &tParsed);
	bPass = bPass && xmail_mime_test_true("parse_multipart_root", tParsed.pRootPart != NULL);
	bPass = bPass && xmail_mime_test_true("parse_multipart_children", tParsed.pRootPart != NULL && tParsed.pRootPart->iChildCount == 2u);
	bPass = bPass && xmail_mime_test_equal("parse_multipart_type", tParsed.pRootPart ? tParsed.pRootPart->sSubType : "", "alternative");
	bPass = bPass && xmail_mime_test_equal("parse_multipart_child0_type", (tParsed.pRootPart && tParsed.pRootPart->iChildCount > 0u) ? tParsed.pRootPart->arrChildren[0].sContentType : "", "text/plain");
	bPass = bPass && xmail_mime_test_equal("parse_multipart_child1_body", (tParsed.pRootPart && tParsed.pRootPart->iChildCount > 1u) ? (const char*)tParsed.pRootPart->arrChildren[1].sBody : "", "<p>html</p>");
	xmailMimeParsedMessageFree(&tParsed);

	memset(&tParsed, 0, sizeof(tParsed));
	bPass = bPass && xmailMimeParseMessage(
		"Content-Type: multipart/mixed; boundary=\"mix\"\r\n\r\n"
		"--mix\r\nContent-Type: multipart/alternative; boundary=\"alt\"\r\n\r\n"
		"--alt\r\nContent-Type: text/plain\r\n\r\nplain\r\n"
		"--alt\r\nContent-Type: text/html\r\n\r\n<html></html>\r\n"
		"--alt--\r\n"
		"--mix\r\nContent-Type: text/plain\r\nContent-Disposition: attachment; filename=\"a.txt\"\r\n\r\nfile\r\n"
		"--mix--\r\n", &tParsed);
	bPass = bPass && xmail_mime_test_true("parse_nested_root_children", tParsed.pRootPart != NULL && tParsed.pRootPart->iChildCount == 2u);
	bPass = bPass && xmail_mime_test_true("parse_nested_alt_children", tParsed.pRootPart != NULL && tParsed.pRootPart->iChildCount > 0u && tParsed.pRootPart->arrChildren[0].iChildCount == 2u);
	bPass = bPass && xmail_mime_test_equal("parse_nested_attachment_body", (tParsed.pRootPart && tParsed.pRootPart->iChildCount > 1u) ? (const char*)tParsed.pRootPart->arrChildren[1].sBody : "", "file");
	bPass = bPass && xmail_mime_test_true("parse_nested_attachment_flag", tParsed.pRootPart != NULL && tParsed.pRootPart->iChildCount > 1u && tParsed.pRootPart->arrChildren[1].bAttachment);
	bPass = bPass && xmail_mime_test_equal("parse_nested_attachment_disposition", (tParsed.pRootPart && tParsed.pRootPart->iChildCount > 1u) ? tParsed.pRootPart->arrChildren[1].sDisposition : "", "attachment");
	bPass = bPass && xmail_mime_test_equal("parse_nested_attachment_filename", (tParsed.pRootPart && tParsed.pRootPart->iChildCount > 1u) ? tParsed.pRootPart->arrChildren[1].sFileName : "", "a.txt");
	xmailMimeParsedMessageFree(&tParsed);

	memset(&tParsed, 0, sizeof(tParsed));
	bPass = bPass && xmailMimeParseMessage(
		"Content-Type: multipart/alternative; boundary=\"enc\"\r\n\r\n"
		"--enc\r\nContent-Type: text/plain\r\nContent-Transfer-Encoding: quoted-printable\r\n\r\nhello=20=E4=B8=AD=E6=96=87\r\n"
		"--enc\r\nContent-Type: text/plain\r\nContent-Transfer-Encoding: base64\r\n\r\naGVsbG8=\r\n"
		"--enc--\r\n", &tParsed);
	bPass = bPass && xmail_mime_test_equal("parse_qp_decoded_body", (tParsed.pRootPart && tParsed.pRootPart->iChildCount > 0u) ? (const char*)tParsed.pRootPart->arrChildren[0].sDecodedBody : "", "hello 中文");
	bPass = bPass && xmail_mime_test_equal("parse_base64_decoded_body", (tParsed.pRootPart && tParsed.pRootPart->iChildCount > 1u) ? (const char*)tParsed.pRootPart->arrChildren[1].sDecodedBody : "", "hello");
	xmailMimeParsedMessageFree(&tParsed);

	memset(&tParsed, 0, sizeof(tParsed));
	bPass = bPass && xmailMimeParseMessage(
		"Content-Type: multipart/related; boundary=\"rel\"\r\n\r\n"
		"--rel\r\nContent-Type: image/png; name=\"logo.png\"\r\nContent-Disposition: inline; filename=\"logo.png\"\r\nContent-ID: <logo-img>\r\nContent-Transfer-Encoding: base64\r\n\r\naW1n\r\n"
		"--rel\r\nContent-Type: application/pdf; name=\"fallback.pdf\"\r\nContent-Disposition: attachment; filename=\"fallback.pdf\"; filename*=UTF-8''%E6%8A%A5%E5%91%8A.pdf\r\nContent-Transfer-Encoding: base64\r\n\r\ncGRm\r\n"
		"--rel--\r\n", &tParsed);
	bPass = bPass && xmail_mime_test_true("parse_inline_flag", tParsed.pRootPart != NULL && tParsed.pRootPart->iChildCount > 0u && tParsed.pRootPart->arrChildren[0].bInline);
	bPass = bPass && xmail_mime_test_equal("parse_inline_content_id", (tParsed.pRootPart && tParsed.pRootPart->iChildCount > 0u) ? tParsed.pRootPart->arrChildren[0].sContentId : "", "logo-img");
	bPass = bPass && xmail_mime_test_equal("parse_inline_filename", (tParsed.pRootPart && tParsed.pRootPart->iChildCount > 0u) ? tParsed.pRootPart->arrChildren[0].sFileName : "", "logo.png");
	bPass = bPass && xmail_mime_test_true("parse_rfc5987_attachment_flag", tParsed.pRootPart != NULL && tParsed.pRootPart->iChildCount > 1u && tParsed.pRootPart->arrChildren[1].bAttachment);
	bPass = bPass && xmail_mime_test_equal("parse_rfc5987_filename", (tParsed.pRootPart && tParsed.pRootPart->iChildCount > 1u) ? tParsed.pRootPart->arrChildren[1].sFileName : "", "报告.pdf");
	xmailMimeParsedMessageFree(&tParsed);

	memset(&tParsed, 0, sizeof(tParsed));
	bPass = bPass && xmailMimeParseMessage(
		"Content-Type: multipart/mixed; boundary=\"msg\"\r\n\r\n"
		"--msg\r\nContent-Type: text/plain\r\n\r\nouter\r\n"
		"--msg\r\nContent-Type: message/rfc822\r\n\r\n"
		"Subject: forwarded\r\nContent-Type: text/plain\r\n\r\ninner body\r\n"
		"--msg--\r\n", &tParsed);
	bPass = bPass && xmail_mime_test_true("parse_message_rfc822_part", tParsed.pRootPart != NULL && tParsed.pRootPart->iChildCount > 1u && tParsed.pRootPart->arrChildren[1].pMessage != NULL);
	bPass = bPass && xmail_mime_test_equal("parse_message_rfc822_subject", (tParsed.pRootPart && tParsed.pRootPart->iChildCount > 1u && tParsed.pRootPart->arrChildren[1].pMessage) ? xmailMimeParsedGetHeader(tParsed.pRootPart->arrChildren[1].pMessage, "Subject") : "", "forwarded");
	bPass = bPass && xmail_mime_test_equal("parse_message_rfc822_body", (tParsed.pRootPart && tParsed.pRootPart->iChildCount > 1u && tParsed.pRootPart->arrChildren[1].pMessage && tParsed.pRootPart->arrChildren[1].pMessage->pRootPart) ? (const char*)tParsed.pRootPart->arrChildren[1].pMessage->pRootPart->sDecodedBody : "", "inner body");
	xmailMimeParsedMessageFree(&tParsed);

	printf("xmail_mime_test: %s\n", bPass ? "PASS" : "FAIL");
	return bPass ? 0 : 1;
}
