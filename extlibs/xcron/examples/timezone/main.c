#include <xcron.h>
#include <xrt/time.h>
#include <inttypes.h>
#include <stdio.h>

int main(void)
{
    /* The APP chooses how a civil time becomes a physical instant. This sample
     * resolves one civil label with the host zone and prefers the earlier fold. */
    xcronexpression Rule;
    xdatetime Local, Next, Display;
    xtime Instant;
    if (!xrtTimeLocal(xrtNow(), &Local) ||
        !xcronExpressionCompile(XRT_STR_LITERAL("0 9 * * *"), XCRON_UNIX5, &Rule, NULL))
        return 1;
    xdatetime End = {.Year = Local.Year + 2, .Month = 1, .Day = 1};
    if (xcronCalendarNextCivil(&Rule, &Local, &End, XCRON_SEARCH_EXCLUSIVE, &Next, NULL)
        != XCRON_CALENDAR_FOUND) return 2;
    if (!xrtTimeFromLocal(&Next, XTIME_FOLD_EARLIER, &Instant)) {
        puts("APP zone rejected this local label (such as a DST gap)");
        return 0;
    }
    if (!xrtTimeLocal(Instant, &Display)) return 3;
    printf("APP local time=%" PRId64 "-%02d-%02d %02d:%02d\n",
        Display.Year, Display.Month, Display.Day, Display.Hour, Display.Minute);
    return 0;
}
