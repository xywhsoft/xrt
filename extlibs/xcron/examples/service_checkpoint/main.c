#include <xcron.h>
#include <stdio.h>
#include <string.h>

static void backup(void* Context)
{
    unsigned* Count = Context;
    ++*Count;
}

static bool save(FILE* State)
{
    return fputs("backup\t* * * * * *\n", State) >= 0 && fflush(State) == 0;
}

static int run(FILE* State)
{
    /* Rebind the callback and its process-local context from the saved task name. */
    char Line[96];
    if (!fgets(Line, sizeof(Line), State) || !strchr(Line, '\n')) return 2;
    char* Text = strchr(Line, '\t');
    if (!Text) return 3;
    *Text++ = 0;
    if (strcmp(Line, "backup") != 0) return 3;
    Text[strcspn(Text, "\r\n")] = 0;

    xcronscheduler* Scheduler = NULL;
    unsigned Count = 0;
    xcronjobid Job;
    bool OK = xcronSchedulerCreate(1, XCRON_SECONDS6, &Scheduler)
        && xcronSchedulerAdd(Scheduler, (xstrview){Text, strlen(Text)}, backup, &Count, &Job)
        && xcronSchedulerWait(Scheduler, 3000, NULL) == XCRON_SCHEDULE_DISPATCHED
        && Count == 1;
    if (Scheduler && !xcronSchedulerDestroy(Scheduler)) OK = false;
    if (!OK) return 4;
    puts("APP reloaded task configuration and rebound its callback");
    return 0;
}

int main(int Argc, char** Argv)
{
    if (Argc == 1) {
        FILE* State = tmpfile();
        if (!State) return 1;
        int Result = save(State) && fseek(State, 0, SEEK_SET) == 0 ? run(State) : 1;
        return fclose(State) == 0 ? Result : 1;
    }
    if (Argc != 3) return 1;
    bool Save = strcmp(Argv[1], "save") == 0;
    if (!Save && strcmp(Argv[1], "run") != 0) return 1;
    FILE* State = fopen(Argv[2], Save ? "w" : "r");
    if (!State) return 1;
    int Result = Save ? (save(State) ? 0 : 1) : run(State);
    return fclose(State) == 0 ? Result : 1;
}
