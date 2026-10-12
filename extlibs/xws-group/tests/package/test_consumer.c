#define XWS_GROUP_MODULE_ALL
#include <xws-group.h>

int main(void)
{
    xwsgroup* Group = xrtWsGroupCreate(8);
    if (Group == NULL) return 1;
    xwsgroupsnapshot* Snapshot = xrtWsGroupSnapshotCreate(Group);
    if (Snapshot == NULL || xrtWsGroupSnapshotCount(Snapshot) != 0) return 2;
    xrtWsGroupSnapshotDestroy(Snapshot);
    xrtWsGroupDestroy(Group);
    return 0;
}
