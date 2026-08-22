#include <stdio.h>
#include <xmail.h>



/* 直接取得并解码 Content-Disposition 文件名。 */
int main(void)
{
	xmaildispositionview Disposition;
	xmailparaminfo Info;
	str sFilename;

	if ( !xrtMailDispositionParse(
		XRT_STR_LITERAL(
			"attachment; filename*=UTF-8''report%20%E4%B8%AD%E6%96%87.txt"
		),
		&Disposition
	) ) {
		return 1;
	}
	sFilename = xrtMailParamFind(
		Disposition.Parameters,
		XRT_STR_LITERAL("filename"),
		NULL,
		&Info
	);
	if ( sFilename == NULL ) {
		return 2;
	}
	printf("%s\n", sFilename);
	xrtFree(sFilename);
	return 0;
}
