#define XMQTT_MODULE_XMQTT_JOURNAL_SLOT
#define XRT_MODULE_DIR_TEMP
#include <xmqtt/features.h>
#define XRT_IMPLEMENTATION
#include "../../../../single/xrt.h"

static unsigned selectorFault,selectorWrites;
static bool selectorWrite(xfile File,uint64 Offset,const void* Data,size_t Bytes,size_t* Written)
{
    if(selectorFault&&selectorFault<=2&&++selectorWrites==selectorFault){
        if(selectorFault==2&&!xrtFlush(File))return false;
        xrtSetErrorKind(XERR_IO);
        return false;
    }
    return xrtWriteAtFull(File,Offset,Data,Bytes,Written);
}
static bool selectorSync(xfile File)
{
    bool Done=xrtFlush(File);
    if(Done&&selectorFault==3){
        xrtSetErrorKind(XERR_IO);
        return false;
    }
    return Done;
}

#define XMQTT_IMPLEMENTATION
#define xrtWriteAtFull selectorWrite
#define xrtFlush selectorSync
#include "../../../../single/extlibs/xmqtt.h"
#undef xrtWriteAtFull
#undef xrtFlush
#include "../test.h"

#if defined(_WIN32) || defined(_WIN64) || defined(__linux__)
static void must(xmqttjournalstatus Status,xmqttjournalstatus Wanted)
{
    if(Status!=Wanted)fprintf(stderr,"selector status %d, expected %d\n",(int)Status,(int)Wanted);
    testRequire(Status==Wanted,"selector effect fixture");
}

static void clean(xroot Root,str Directory)
{
    testRequire(xrtRootRemove(Root,"journal.slot")&&xrtRootRemove(Root,"journal.0")&&
        xrtRootRemove(Root,"journal.1")&&xrtRootClose(Root)&&xrtDirRemove(Directory),"release exact fixture");
    xrtFree(Directory);
}

static void provision(xroot Root,cstr Name)
{
    xfileoptions O;
    xrtFileOptionsInit(&O);
    O.Flags=XFILE_READ|XFILE_WRITE|XFILE_CREATE|XFILE_EXCLUSIVE;
    xfile F=xrtRootFileOpen(Root,Name,&O);
    testRequire(F&&xrtClose(F),"create native fixture file");
}

static void fault(unsigned Point,xmqttversion Version)
{
    str Dir=xrtDirTemp(NULL,"xmqtt-slot-publication-fault-",NULL);
    xroot Root=Dir?xrtRootOpen(Dir):NULL;
    testRequire(Root!=NULL,"native slot fault root");
    provision(Root,"journal.slot");
    provision(Root,"journal.0");
    provision(Root,"journal.1");
    xmqttjournalconfig C;
    xmqttJournalConfigInit(&C);
    C.Version=Version;
    C.Generation=8;
    C.Binding=XRT_BYTES_LITERAL("actual/control/io");
    xmqttjournalslot* Slot=NULL;
    xmqttjournal* J=NULL;
    must(xmqttJournalSlotCreate(Root,&C,&Slot),XMQTT_JOURNAL_DONE);
    must(xmqttJournalSlotPrepare(Slot,&J),XMQTT_JOURNAL_DONE);
    uint64 Seq;
    must(xmqttJournalAppend(J,XRT_BYTES_LITERAL("next"),&Seq),XMQTT_JOURNAL_DONE);
    selectorWrites=0;
    selectorFault=Point;
    xmqttjournalstatus Result=xmqttJournalSlotPublish(Slot,J);
    selectorFault=0;
    must(Result,XMQTT_JOURNAL_UNCERTAIN);
    xmqttjournalslotinfo I;
    testRequire(xmqttJournalSlotInfo(Slot,&I)&&I.Poisoned&&I.Effect==XMQTT_JOURNAL_MAY_BE_COMMITTED,
        "publication failure fences the slot and retains uncertain effect");
    must(xmqttJournalAppend(J,XRT_BYTES_LITERAL("late"),&Seq),XMQTT_JOURNAL_STATE);
    testRequire(xmqttJournalDestroy(J)&&xmqttJournalSlotDestroy(Slot),"retire uncertain ownership");
    must(xmqttJournalSlotOpen(Root,&C,&Slot),XMQTT_JOURNAL_DONE);
    testRequire(xmqttJournalSlotInfo(Slot,&I)&&I.Generation==(Point>1?9u:8u),"select actual durable effect");
    must(xmqttJournalSlotOpenCurrent(Slot,&J),XMQTT_JOURNAL_DONE);
    xmqttjournalrecord R;
    if(Point>1){
        must(xmqttJournalNext(J,&R),XMQTT_JOURNAL_DONE);
        testRequire(R.Data.Size==4&&!memcmp(R.Data.Data,"next",4),"recover predecessor's committed replacement");
    }
    must(xmqttJournalNext(J,&R),XMQTT_JOURNAL_EMPTY);
    testRequire(xmqttJournalDestroy(J)&&xmqttJournalSlotDestroy(Slot),"retire recovered effect");
    clean(Root,Dir);
}

#endif

int main(void)
{
#if defined(_WIN32) || defined(_WIN64) || defined(__linux__)
    for(unsigned Point=1;Point<=3;++Point){
        fault(Point,XMQTT_VERSION_5);
        fault(Point,XMQTT_VERSION_311);
    }
    puts("[PASS] native selector I/O failure effects, poisoned owners and fresh recovery");
#else
    puts("[SKIP] native journal slots require Windows or Linux");
#endif
    return 0;
}
