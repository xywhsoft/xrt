#include <xcron.h>
#include <xrt/file.h>
#include <xrt/memory.h>
#include <errno.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static bool parseTime(const char* Text, xtime* Result)
{
    char* End;
    if (*Text != '-' && (*Text < '0' || *Text > '9')) return false;
    errno = 0;
    intmax_t Value = strtoimax(Text, &End, 10);
    if (Text == End || *End || errno == ERANGE || Value < INT64_MIN || Value > INT64_MAX)
        return false;
    *Result = (xtime)Value;
    return true;
}

static bool save(const char* Path, xtime Last)
{
    char Bytes[32];
    int Count = snprintf(Bytes, sizeof(Bytes), "%" PRId64 "\n", Last);
    return Count > 0 && (size_t)Count < sizeof(Bytes) &&
        xrtFileWriteAtomic(Path, (xbytesview){(cbytes)Bytes, (size_t)Count});
}

static bool load(const char* Path, xtime* Last)
{
    size_t Size;
    bytes Bytes = xrtFileReadAllLimit(Path, 31, &Size);
    if (!Bytes) return false;
    bool Valid = Size >= 2 && Bytes[Size - 1] == '\n' && !memchr(Bytes, '\0', Size);
    if (Valid) {
        Bytes[Size - 1] = '\0';
        Valid = parseTime((const char*)Bytes, Last);
    }
    xrtFree(Bytes);
    return Valid;
}

static bool catchUp(const char* Path, xtime Last, xtime Through, unsigned Budget,
    const xtime* SimulatedFailure)
{
    xcronexpression Expression;
    if (!xcronExpressionCompile(XRT_STR_LITERAL("* * * * * *"), XCRON_SECONDS6, &Expression, NULL))
        return false;

    unsigned Count = 0;
    bool Pending = false;
    while (Last < Through) {
        xtime Next;
        xcroncalendarstatus Status = xcronCalendarNext(&Expression, Last, Through,
            XCRON_SEARCH_EXCLUSIVE, &Next, NULL);
        if (Status == XCRON_CALENDAR_NONE) break;
        if (Status != XCRON_CALENDAR_FOUND) return false;
        if (Count == Budget) {
            Pending = true;
            break;
        }
        if (SimulatedFailure && Next == *SimulatedFailure) {
            fprintf(stderr, "task failed at %" PRId64 "\n", Next);
            return false;
        }
        /* Scheduled UTC time identifies the APP action across restarts. Save
         * progress after success so a failed or interrupted job can retry. */
        if (printf("ran %" PRId64 "\n", Next) < 0 || fflush(stdout) != 0) return false;
        if (Path && !save(Path, Next)) return false;
        Last = Next;
        ++Count;
    }
    return printf("last=%" PRId64 " ran=%u pending=%u\n", Last, Count, Pending ? 1u : 0u) >= 0;
}

int main(int Argc, char** Argv)
{
    if (Argc == 1) return catchUp(NULL, 1000, 4000, 2, NULL) ? 0 : 1;
    xtime Last, Through, Budget, Failure;
    if (Argc == 4 && strcmp(Argv[1], "seed") == 0 && parseTime(Argv[3], &Last))
        return save(Argv[2], Last) ? 0 : 1;
    if ((Argc == 5 || Argc == 6) && strcmp(Argv[1], "replay") == 0 &&
        parseTime(Argv[3], &Through) && parseTime(Argv[4], &Budget) && Budget > 0 && Budget <= 1000 &&
        (Argc == 5 || parseTime(Argv[5], &Failure)))
        return load(Argv[2], &Last) &&
            catchUp(Argv[2], Last, Through, (unsigned)Budget, Argc == 6 ? &Failure : NULL) ? 0 : 1;
    fputs("seed FILE LAST_MS | replay FILE THROUGH_MS MAX_OCCURRENCES [FAIL_MS]\n", stderr);
    return 2;
}
