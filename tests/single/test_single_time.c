#define XRT_IMPLEMENTATION
#include "../../single/xrt.h"

int main(void)
{
    xtime Time;
    xdatetime Parts;
    int32 Seconds;
    if ( !xrtDateTime(-1, 12, 31, 23, 59, 59, 999, &Time) || Time != -1 ||
         !xrtTimeSplit(Time, &Parts) || Parts.Year != -1 || Parts.Millisecond != 999 ) { return 1; }
    if ( !xrtTimeFromUnix32(INT32_MAX, &Time) || !xrtTimeToUnix32(Time, &Seconds) || Seconds != INT32_MAX ) { return 2; }
    return xrtTimer() >= 0 ? 0 : 3;
}
