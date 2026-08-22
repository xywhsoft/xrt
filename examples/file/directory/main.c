#include <stdio.h>

#include <xrt.h>



/* 展示零额外 stat 的目录枚举。 */
int main(void)
{
	xdir Dir = xrtDirOpen(".", 0u);
	xdirentry Entry;
	xdirnext Next;

	if ( Dir == NULL ) {
		return 1;
	}
	while ( (Next = xrtDirNext(Dir, &Entry)) == XDIR_NEXT_ITEM ) {
		printf("%.*s\n", (int)Entry.Name.Size, Entry.Name.Data);
	}
	if ( Next == XDIR_NEXT_ERROR ) {
		(void)xrtDirClose(Dir);
		return 1;
	}
	return xrtDirClose(Dir) ? 0 : 1;
}
