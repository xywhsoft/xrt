#include <xcron.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char** argv)
{
	uint64 Count=argc>1 ? strtoull(argv[1],NULL,10) : 100000u, Checksum=0;
	if (!Count || Count>1000000000u) return 1;
	const cstr Texts[]={"* * * * * *","0 */15 9-17 * * MON-FRI","0 0 0 29 FEB *","0 0 0 30 FEB *"}; xcronexpression Expressions[4];
	for (unsigned Index=0; Index<4; ++Index) if (!xcronExpressionCompile((xstrview){Texts[Index],strlen(Texts[Index])},XCRON_SECONDS6,&Expressions[Index],NULL)) return 2;
	xtime Begin, End; if (!xrtDateTime(2024,3,1,9,0,0,123,&Begin) || !xrtDate(2425,1,1,&End)) return 3;
	double Start=xrtTimer();
	for (uint64 Index=0; Index<Count; ++Index) {
		xcroncalendarissue Issue; xtime Next=0; unsigned Case=(unsigned)(Index%4u);
		xcroncalendarstatus Status=xcronCalendarNext(&Expressions[Case],Begin+(xtime)(Index%1000u),End,0,&Next,&Issue);
		if (Status!=(Case==3 ? XCRON_CALENDAR_NONE : XCRON_CALENDAR_FOUND)) return 4;
		Checksum+=(uint64)Next+Issue.MonthsExamined;
	}
	double Seconds=xrtTimer()-Start; if (!(Seconds>0)) return 5;
	printf("searches_per_sec: %.3f\nchecksum: %" PRIu64 "\n",(double)Count/Seconds,Checksum); return 0;
}
