#include <xmail.h>
#include <stdio.h>



/* 展示拥有型 MIME 树的最短解析路径。 */
int main(void)
{
	static const char sMessage[] =
		"Subject: example\r\n"
		"Content-Type: text/plain; charset=UTF-8\r\n"
		"\r\nhello";
	xmailtree Tree;

	if ( !xrtMailTreeParse(XRT_STR_LITERAL(sMessage), NULL, &Tree) ) {
		return 1;
	}
	printf(
		"type=%.*s/%.*s body=%.*s\n",
		(int)Tree.Root->ContentType.Type.Size,
		Tree.Root->ContentType.Type.Data,
		(int)Tree.Root->ContentType.Subtype.Size,
		Tree.Root->ContentType.Subtype.Data,
		(int)Tree.Root->Data.Size,
		(const char*)Tree.Root->Data.Data
	);
	xrtMailTreeFree(&Tree);
	return 0;
}
