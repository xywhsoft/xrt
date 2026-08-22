#include <stdio.h>

#include <xrt/ssh_client_pty.h>



/* 展示 PTY 能力独立于普通 exec 裁剪。 */
int main(void)
{
	unsigned char arrModes[8];
	xsshwriter Writer;

	if ( !xrtSshWriterInit(&Writer, arrModes, sizeof(arrModes)) ||
		(xrtSshTerminalModeEnd(&Writer) != XSSH_OK) ) {
		return 1;
	}
	printf("pty-modes=%zu\n", Writer.Size);
	return 0;
}
