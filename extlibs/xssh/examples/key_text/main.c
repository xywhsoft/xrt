#include <stdio.h>
#include <xssh.h>



/* 解析带 options 的 authorized_keys 公钥行。 */
int main(void)
{
	static const char sLine[] =
		"restrict ssh-ed25519 "
		"AAAAC3NzaC1lZDI1NTE5AAAAIAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA "
		"example";
	unsigned char arrBlob[64];
	xsshopensshkeyline KeyLine;
	xsshpublickey PublicKey;

	if ( (xrtSshPublicKeyLineRead(
		XRT_STR_LITERAL(sLine),
		&KeyLine
	) != XSSH_OK) || (xrtSshPublicKeyLineDecode(
		&KeyLine,
		arrBlob,
		sizeof(arrBlob),
		&PublicKey
	) != XSSH_OK) ) {
		return 1;
	}
	printf(
		"algorithm=%.*s blob=%zu options=%.*s\n",
		(int)PublicKey.Algorithm.Size,
		PublicKey.Algorithm.Data,
		KeyLine.BlobSize,
		(int)KeyLine.Options.Size,
		KeyLine.Options.Data
	);
	return 0;
}
