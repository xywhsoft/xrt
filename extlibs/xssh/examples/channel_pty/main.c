#include <stdio.h>

#include <xssh.h>



/* 构建一组可复用 terminal modes 和 PTY request。 */
int main(void)
{
	unsigned char arrModes[16];
	unsigned char arrPayload[128];
	xsshwriter ModeWriter;
	xsshwriter Writer;

	if ( !xrtSshWriterInit(&ModeWriter, arrModes, sizeof(arrModes)) ||
		(xrtSshTerminalModeWrite(
			&ModeWriter,
			XSSH_TTY_OP_ECHO,
			1u
		) != XSSH_OK) || (xrtSshTerminalModeEnd(&ModeWriter) != XSSH_OK) ||
		!xrtSshWriterInit(&Writer, arrPayload, sizeof(arrPayload)) ||
		(xrtSshChannelPtyWrite(
			&Writer,
			0u,
			true,
			XRT_BYTES_LITERAL("xterm-256color"),
			120u,
			40u,
			0u,
			0u,
			(xbytesview){ arrModes, ModeWriter.Size }
		) != XSSH_OK) ) {
		return 1;
	}
	printf("pty-payload=%zu modes=%zu\n", Writer.Size, ModeWriter.Size);
	return 0;
}
