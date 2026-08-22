#include <stdio.h>
#include <string.h>

#include <xssh.h>



/* 展示动态发送队列按远端窗口生成一条最终 channel payload。 */
int main(void)
{
	unsigned char arrPayload[64];
	xsshchannelconfirmation Confirmation;
	xsshchannelioconfig Config;
	xsshchannelcore Channel;
	xsshchannelio Io;
	xsshwriter Writer;
	xbytesview Payload;

	if ( !xrtSshChannelCoreOpenInit(
		&Channel,
		1u,
		1024u,
		256u,
		512u
	) ) {
		return 1;
	}
	memset(&Confirmation, 0, sizeof(Confirmation));
	Confirmation.Recipient = 1u;
	Confirmation.Sender = 9u;
	Confirmation.Window = 1024u;
	Confirmation.MaxPacket = 256u;
	if ( (xrtSshChannelCoreConfirmationCommit(
		&Channel,
		&Confirmation
	) != XSSH_OK) ) {
		return 1;
	}
	xrtSshChannelIoConfigInit(&Config);
	Config.ReceiveLimit = 1024u;
	Config.SendLimit = 4096u;
	if ( !xrtSshChannelIoInit(&Io, NULL, &Channel, &Config) ||
		(xrtSshChannelIoWrite(
			&Io,
			XSSH_CHANNEL_IO_DATA,
			"hello",
			5u
		) != XSSH_OK) ||
		!xrtSshWriterInit(&Writer, arrPayload, sizeof(arrPayload)) ||
		(xrtSshChannelIoSendPrepare(
			&Io,
			XSSH_CHANNEL_IO_DATA,
			&Writer,
			&Payload
		) != XSSH_OK) ) {
		xrtSshChannelIoClear(&Io);
		return 1;
	}
	printf("queued=%zu payload=%zu remote=%u\n",
		xrtSshChannelIoQueued(&Io, XSSH_CHANNEL_IO_DATA),
		Payload.Size,
		Channel.Remote);
	(void)xrtSshChannelIoSendAbort(&Io);
	xrtSshChannelIoClear(&Io);
	return 0;
}
