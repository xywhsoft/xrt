#include <xcron.h>
#include <stdio.h>
int main(void)
{
    xcronzone* Z=NULL;xcronexpression E;int Exit=1;xtime A,B;
    if(!xcronZoneCreatePosix(XRT_STR_LITERAL("EST5EDT,M3.2.0,M11.1.0"),XCRON_ZONE_TZIF3,&Z,NULL) || !xcronExpressionCompile(XRT_STR_LITERAL("0 30 1 * * *"),XCRON_SECONDS6,&E,NULL) || !xrtDateTime(2024,11,3,5,30,0,0,&A) || !xrtDateTime(2024,11,3,6,30,0,0,&B))goto Done;
    xcronoccurrencecursor C;xcronoccurrence Next;xcronoccurrencestatus S;
    if(!xcronOccurrenceCursorInit(&C,&E,Z,A,B,0,(xcrondeliverypolicy){XCRON_FOLD_LAST,XCRON_MISSING_REJECT},NULL))goto Done;
    do { S=xcronOccurrenceCursorStep(&C,Z,(xcronoccurrencebudget){1,1,1,1,1},&Next,NULL); }while(S==XCRON_OCCURRENCE_PENDING);
    if(S!=XCRON_OCCURRENCE_FOUND || Next.When!=B || Next.Candidates!=2)goto Done;
    puts("Selected global last fold through budgeted recurring search.");Exit=0;
Done:xcronZoneRelease(Z);return Exit;
}
