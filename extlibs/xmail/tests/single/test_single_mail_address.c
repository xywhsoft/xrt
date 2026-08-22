#ifdef XMAIL_MODULE_XMAIL
	#undef XMAIL_MODULE_XMAIL
#endif
#define XMAIL_MODULE_MAIL_ADDRESS
#define XMAIL_IMPLEMENTATION
#include "../../single/xmail.h"



/* 地址裁剪入口必须闭包编码词，但不拉入字段游标。 */
int main(void)
{
	const xmailaddress arrAddresses[] = {
		{ XRT_STR_INIT("User"), XRT_STR_INIT("user@example.com") },
		{ XRT_STR_INIT(""), XRT_STR_INIT("next@example.net") }
	};
	xmailaddresscursor Cursor;
	xmailaddressview Address;
	char arrOutput[96];
	size_t iSize;

	#if !defined(XMAIL_FEATURE_MAIL_ADDRESS) || \
		!defined(XMAIL_FEATURE_MAIL_WORD) || \
		!defined(XRT_FEATURE_UNICODE)
		#error "XMAIL_MODULE_MAIL_ADDRESS dependency closure is incomplete"
	#endif
	#if defined(XMAIL_FEATURE_MAIL_HEADER)
		#error "mail address unexpectedly enabled mail header"
	#endif

	if ( !xrtMailAddressCursorInit(
		&Cursor,
		XRT_STR_LITERAL("User <user@example.com>"),
		XMAIL_ADDRESS_DEFAULT
	) ) {
		return 1;
	}
	if ( xrtMailAddressNext(&Cursor, &Address) != XMAIL_NEXT_ITEM ) {
		return 2;
	}
	if ( !xrtMailAddressListWrite(
		arrAddresses,
		2u,
		XMAIL_WORD_Q,
		XMAIL_ADDRESS_DEFAULT,
		arrOutput,
		sizeof(arrOutput),
		&iSize
	) ) {
		return 3;
	}
	return strcmp(
		arrOutput,
		"User <user@example.com>, next@example.net"
	) == 0 ? 0 : 4;
}
