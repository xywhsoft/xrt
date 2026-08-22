#include "../../../dev/bench/bench_common.h"

#define XMAIL_MODULE_SMTP
#define XMAIL_MODULE_POP3
#define XMAIL_MODULE_IMAP
#include <xmail.h>



/* 测量邮件线路与三种协议的零分配解析和命令写出热路径。 */
int main(int argc, char** argv)
{
	static const char sWire[] =
		"Header: value\r\n\r\n..first\r\nsecond\r\n";
	uint32 iIterations = xbenchArgU32(argc, argv, 1, 200000u);
	char arrOutput[256];
	size_t iOutputSize;
	uint64 iChecksum = 0;
	xbenchtimer Timer;
	uint64 iWireElapsed;
	uint64 iSmtpElapsed;
	uint64 iPop3Elapsed;
	uint64 iImapElapsed;

	if ( iIterations == 0 ) {
		return 1;
	}

	xbenchTimerStart(&Timer);
	for ( uint32 i = 0; i < iIterations; i++ ) {
		xstrview Line;
		size_t iConsumed;

		if ( (xrtMailLineRead(
			XRT_STR_LITERAL(sWire),
			0,
			&Line,
			&iConsumed
		) != XMAIL_NEXT_ITEM) || !xrtMailDotWrite(
			XRT_STR_LITERAL(sWire),
			true,
			arrOutput,
			sizeof(arrOutput),
			&iOutputSize
		) ) {
			return 2;
		}
		iChecksum += Line.Size + iConsumed + iOutputSize;
	}
	xbenchTimerStop(&Timer);
	iWireElapsed = xbenchTimerElapsedNs(&Timer);

	xbenchTimerStart(&Timer);
	for ( uint32 i = 0; i < iIterations; i++ ) {
		xsmtpreplyline Reply;
		xsmtpcapabilityview Capability;
		uint64 iCapabilities = 0;
		uint64 iSizeLimit = 0;

		if ( !xrtSmtpReplyLineParse(
			XRT_STR_LITERAL("250-SIZE 10485760"),
			&Reply
		) || !xrtSmtpCapabilityParse(Reply.Text, &Capability) ||
			!xrtSmtpCapabilityAdd(
				&Capability,
				&iCapabilities,
				&iSizeLimit
			) || !xrtSmtpCommandWrite(
				XRT_STR_LITERAL("MAIL"),
				XRT_STR_LITERAL("FROM:<sender@example.com> SIZE=128"),
				arrOutput,
				sizeof(arrOutput),
				&iOutputSize
			) ) {
			return 3;
		}
		iChecksum += (uint64)Reply.Code + iCapabilities +
			iSizeLimit + iOutputSize;
	}
	xbenchTimerStop(&Timer);
	iSmtpElapsed = xbenchTimerElapsedNs(&Timer);

	xbenchTimerStart(&Timer);
	for ( uint32 i = 0; i < iIterations; i++ ) {
		xpop3replyview Reply;
		xpop3stat Stat;
		xpop3capabilityview Capability;

		if ( !xrtPop3ReplyParse(
			XRT_STR_LITERAL("+OK mailbox ready"),
			&Reply
		) || !xrtPop3StatParse(
			XRT_STR_LITERAL("+OK 42 1048576"),
			&Stat
		) || !xrtPop3CapabilityParse(
			XRT_STR_LITERAL("SASL PLAIN XOAUTH2"),
			&Capability
		) || !xrtPop3CommandWrite(
			XRT_STR_LITERAL("RETR"),
			XRT_STR_LITERAL("42"),
			arrOutput,
			sizeof(arrOutput),
			&iOutputSize
		) ) {
			return 4;
		}
		iChecksum += Reply.Text.Size + Stat.Messages + Stat.Bytes +
			xrtPop3Capability(Capability.Name) + iOutputSize;
	}
	xbenchTimerStop(&Timer);
	iPop3Elapsed = xbenchTimerElapsedNs(&Timer);

	xbenchTimerStart(&Timer);
	for ( uint32 i = 0; i < iIterations; i++ ) {
		ximapresponseview Response;
		ximapliteralview Literal;
		ximapnumberview Number;

		if ( !xrtImapResponseParse(
			XRT_STR_LITERAL("* 23 FETCH (BODY[] {4096}"),
			&Response
		) || (xrtImapLiteralParse(
			XRT_STR_LITERAL("* 23 FETCH (BODY[] {4096}"),
			&Literal
		) != XMAIL_NEXT_ITEM) || (xrtImapNumberParse(
			Response.Text,
			&Number
		) != XMAIL_NEXT_ITEM) || !xrtImapCommandWrite(
			XRT_STR_LITERAL("A0000001"),
			XRT_STR_LITERAL("UID"),
			XRT_STR_LITERAL("FETCH 1:* (FLAGS BODY.PEEK[] UID)"),
			XIMAP_COMMAND_LINE_DEFAULT,
			arrOutput,
			sizeof(arrOutput),
			&iOutputSize
		) ) {
			return 5;
		}
		iChecksum += Response.Text.Size + Literal.Size + Number.Number +
			iOutputSize;
	}
	xbenchTimerStop(&Timer);
	iImapElapsed = xbenchTimerElapsedNs(&Timer);

	printf("xrt Mail transport benchmark\n");
	xbenchPrintMetricDouble(
		"wire_mib_per_sec",
		xbenchSafeRate(
			(uint64)iIterations * (sizeof(sWire) - 1u),
			iWireElapsed
		) / (1024.0 * 1024.0)
	);
	xbenchPrintMetricDouble(
		"smtp_ops_per_sec",
		xbenchSafeRate(iIterations, iSmtpElapsed)
	);
	xbenchPrintMetricDouble(
		"pop3_ops_per_sec",
		xbenchSafeRate(iIterations, iPop3Elapsed)
	);
	xbenchPrintMetricDouble(
		"imap_ops_per_sec",
		xbenchSafeRate(iIterations, iImapElapsed)
	);
	xbenchPrintMetricU64("checksum", iChecksum);
	return 0;
}
