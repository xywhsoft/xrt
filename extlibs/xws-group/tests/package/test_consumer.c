#define XWS_GROUP_MODULE_ALL
#include <xws-group.h>

typedef struct source {
    size_t Releases;
    bool Invalid;
    xbytesview Bytes;
} source;

static void release_source(ptr context, cbytes data, size_t size)
{
    source* Source = (source*)context;
    Source->Releases++;
    if (data != Source->Bytes.Data || size != Source->Bytes.Size) {
        Source->Invalid = true;
    }
}

int main(void)
{
    const uint8 Payload[] = {0x80, 0x00, 0xff};
    source Source = {0, false, {Payload, sizeof(Payload)}};
    xnetref Ref = {Payload, sizeof(Payload), release_source, &Source};
    xwsgroup* Group = xrtWsGroupCreate(8);
    xwsgroupop* Operation;
    xfuture* Future;
    bool Valid;

    if (Group == NULL) return 1;
    xwsgroupsnapshot* Snapshot = xrtWsGroupSnapshotCreate(Group);
    if (Snapshot == NULL || xrtWsGroupSnapshotCount(Snapshot) != 0) {
        xrtWsGroupSnapshotDestroy(Snapshot);
        xrtWsGroupDestroy(Group);
        return 2;
    }
    Operation = xrtWsGroupBinaryRefAsync(Group, &Ref);
    if (Operation == NULL) {
        xrtWsGroupSnapshotDestroy(Snapshot);
        xrtWsGroupDestroy(Group);
        return 3;
    }
    Valid = Source.Releases == 1 && !Source.Invalid &&
        xrtWsGroupOpCount(Operation) == 0 &&
        xrtWsGroupOpAccepted(Operation) == 0 &&
        xrtWsGroupOpRejected(Operation) == 0 &&
        xrtWsGroupOpWaitFor(Operation, 0) == XWAIT_OK;
    Future = xrtWsGroupOpFutureRef(Operation);
    xrtWsGroupOpDestroy(Operation);
    xrtWsGroupSnapshotDestroy(Snapshot);
    xrtWsGroupDestroy(Group);
    /* A consumer-owned Future survives the operation and group. */
    Valid = Valid && Future != NULL &&
        xrtFutureState(Future) == XFUTURE_RESOLVED &&
        xrtFutureWaitFor(Future, 0) == XWAIT_OK &&
        Source.Releases == 1 && !Source.Invalid;
    xrtFutureDestroy(Future);
    return Valid ? 0 : 4;
}
