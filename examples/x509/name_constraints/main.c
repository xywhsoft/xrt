#include <stdio.h>

#include <xrt.h>



/* 演示解析并遍历一组独立 NameConstraints DER。 */
int main(void)
{
	static const uint8 Der[] = {
		0x30, 0x12, 0xA0, 0x10, 0x30, 0x0E, 0x82, 0x0C,
		'e', 'x', 'a', 'm', 'p', 'l', 'e', '.', 't', 'e', 's', 't'
	};
	xx509nameconstraints Constraints;
	xx509subtreecursor Cursor;
	xx509subtree Subtree;
	xx509result Result;

	if ( !xrtX509NameConstraintsParse(
		(xbytesview) { Der, sizeof(Der) }, &Constraints
	) ) {
		return 1;
	}
	Cursor = Constraints.Permitted;
	while ( (Result = xrtX509SubtreeRead(
		&Cursor, &Subtree
	)) == X509_VALUE ) {
		printf("type=%d size=%zu\n",
			(int)Subtree.Base.Type, Subtree.Base.Value.Size);
	}
	return Result == X509_DONE ? 0 : 1;
}
