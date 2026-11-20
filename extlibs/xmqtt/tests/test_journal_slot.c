#include "test.h"
#include "../examples/common/journal_files.h"
#include <xmqtt/journal_slot.h>
#include <xmqtt/journal_worker.h>
#include "support/journal_peer.h"

static void virtualSlot(void)
{
    static jpdrive Drives[3];
    jpfile Control=jpFile(&Drives[0]),A=jpFile(&Drives[1]),B=jpFile(&Drives[2]);
    xmqttjournalconfig C=jpConfig();
    xmqttjournalslotfiles Files={.Control=Control.File,.Data={A.File,A.File}};
    xmqttjournalslot* Slot=(xmqttjournalslot*)(uintptr_t)1;
    jpStatus(xmqttJournalSlotCreate(&Files,&C,&Slot),XMQTT_JOURNAL_STATE,"one handle cannot occupy two roles");
    testRequire(Slot==(xmqttjournalslot*)(uintptr_t)1,"bad handles preserve the output");
    xrtClearError();

    Files.Data[1]=B.File;
    jpStatus(xmqttJournalSlotCreate(&Files,&C,&Slot),XMQTT_JOURNAL_DONE,"create on three VFS handles");
    xmqttjournal* J=NULL;
    jpStatus(xmqttJournalSlotOpenCurrent(Slot,&J),XMQTT_JOURNAL_DONE,"open initial VFS generation");
    testRequire(jpReplay(J)==0&&xmqttJournalDestroy(J),"validate and retire initial VFS Journal");
    jpStatus(xmqttJournalSlotPrepare(Slot,&J),XMQTT_JOURNAL_DONE,"prepare the inactive VFS file");
    const xbytesview Payload=XRT_BYTES_LITERAL("VFS replacement");
    uint64 Sequence=0;
    jpStatus(xmqttJournalAppend(J,Payload,&Sequence),XMQTT_JOURNAL_DONE,"append VFS replacement");
    jpStatus(xmqttJournalSlotPublish(Slot,J),XMQTT_JOURNAL_DONE,"publish VFS generation");
    testRequire(Sequence==1&&xmqttJournalDestroy(J)&&xmqttJournalSlotDestroy(Slot),
                "retire logical owners while retaining the VFS handles");

    jpStatus(xmqttJournalSlotOpen(&Files,&C,&Slot),XMQTT_JOURNAL_DONE,"cold open with the same VFS handles");
    jpStatus(xmqttJournalSlotOpenCurrent(Slot,&J),XMQTT_JOURNAL_DONE,"open the published VFS Journal");
    xmqttjournalrecord Record;
    jpStatus(xmqttJournalNext(J,&Record),XMQTT_JOURNAL_DONE,"recover published VFS record");
    testRequire(Record.Sequence==1&&Record.Data.Size==Payload.Size&&!memcmp(Record.Data.Data,Payload.Data,Payload.Size),
                "exact VFS history");
    jpStatus(xmqttJournalNext(J,&Record),XMQTT_JOURNAL_EMPTY,"complete VFS history");
    testRequire(xmqttJournalDestroy(J)&&xmqttJournalSlotDestroy(Slot),"retire VFS owners again");

    testRequire(!Drives[0].Closes&&!Drives[1].Closes&&!Drives[2].Closes,"only the APP closes VFS files");
    jpFileClose(B);
    jpFileClose(A);
    jpFileClose(Control);
    testRequire(Drives[0].Closes==1&&Drives[1].Closes==1&&Drives[2].Closes==1,"close each borrowed file once");
}

#if defined(_WIN32) || defined(_WIN64) || defined(__linux__)
static const char* const names[]={"journal.slot","journal.0","journal.1"};

static void status(xmqttjournalstatus Got,xmqttjournalstatus Want,const char* Step)
{
    if(Got!=Want)fprintf(stderr,"%s: %d, expected %d, error: %s\n",Step,(int)Got,(int)Want,
        xrtGetError()?xrtErrorMessage(xrtGetError()):"none");
    testRequire(Got==Want,Step);
}

static xmqttjournalconfig config(xmqttversion Version,uint64 Generation)
{
    xmqttjournalconfig C;
    xmqttJournalConfigInit(&C);
    C.Version=Version;
    C.Generation=Generation;
    C.Binding=XRT_BYTES_LITERAL("slot/broker/principal");
    C.MaxTransactionBytes=512;
    return C;
}

static void provision(xroot Root)
{
    xfileoptions O;
    xrtFileOptionsInit(&O);
    O.Flags=XFILE_READ|XFILE_WRITE|XFILE_CREATE|XFILE_EXCLUSIVE;
    for(size_t I=0;I<3;++I){
        xfile F=xrtRootFileOpen(Root,names[I],&O);
        testRequire(F&&xrtClose(F),"three pre-provisioned native files");
    }
}

static void append(xmqttjournal* J,cstr Data,uint64 Expected)
{
    uint64 Seq=UINT64_MAX;
    status(xmqttJournalAppend(J,(xbytesview){(const unsigned char*)Data,strlen(Data)},&Seq),
           XMQTT_JOURNAL_DONE,"append to selected or prepared data file");
    testRequire(Seq==Expected,"independent new-generation sequence");
}

static void row(xmqttjournal* J,uint64 Sequence,cstr Data)
{
    xmqttjournalrecord R;
    status(xmqttJournalNext(J,&R),XMQTT_JOURNAL_DONE,"replay selected record");
    testRequire(R.Sequence==Sequence&&R.Data.Size==strlen(Data)&&!memcmp(R.Data.Data,Data,R.Data.Size),
                "selected content");
}

static void end(xmqttjournal* J)
{
    xmqttjournalrecord R;
    status(xmqttJournalNext(J,&R),XMQTT_JOURNAL_EMPTY,"validated end of selected history");
}

static void info(xmqttjournalslot* S,uint64 Generation,uint64 Sequence,bool Prepared)
{
    xmqttjournalslotinfo I;
    testRequire(xmqttJournalSlotInfo(S,&I)&&I.Generation==Generation&&I.PublishedSequence==Sequence&&
                I.Prepared==Prepared&&!I.Poisoned,"slot generation, prefix and preparation state");
}

static void busy(xroot Root,const xmqttjournalconfig* C)
{
    xmqttjournalslotfiles Files={0};
    xmqttjournalslot* Out=(xmqttjournalslot*)(uintptr_t)1;
    xrtSetErrorKind(XERR_TIMEOUT);
    const xerror* Ambient=xrtGetError();
    status(mqttExampleOpenNativeSlot(Root,C,false,&Files,&Out),XMQTT_JOURNAL_BUSY,"control lock covers the full rotation");
    testRequire(Out==(xmqttjournalslot*)(uintptr_t)1&&!Files.Control&&!Files.Data[0]&&!Files.Data[1]&&
                xrtGetError()==Ambient,"contention leaves output and ambient error");
    xrtClearError();
}

static void preparationBudget(xmqttjournalslot* Slot)
{
    const size_t Limits[]={1,256,4096,32768};
    for(size_t N=0;N<sizeof(Limits)/sizeof(Limits[0]);++N){
        xallocationbudget* Budget=NULL;
        xallocationscope Scope={0};
        testRequire(xrtAllocationBudgetCreate(Limits[N],&Budget)&&xrtAllocationBudgetEnter(Budget,&Scope),
                    "bounded preparation allocation scope");
        xmqttjournal* J=(xmqttjournal*)(uintptr_t)1;
        xmqttjournalstatus S=xmqttJournalSlotPrepare(Slot,&J);
        testRequire(xrtAllocationBudgetLeave(&Scope),"leave preparation scope");
        if(S==XMQTT_JOURNAL_DONE)testRequire(xmqttJournalDestroy(J),"abandon allocated inactive file");
        else testRequire((S==XMQTT_JOURNAL_IO||S==XMQTT_JOURNAL_ERROR)&&J==(xmqttjournal*)(uintptr_t)1,
                         "preparation refusal preserves output");
        xrtClearError();
        xallocationbudgetinfo I={.Size=sizeof(I)};
        testRequire(xrtAllocationBudgetInfo(Budget,&I)&&!I.LiveCount&&!I.LiveBytes,"allocation and native wrappers retire");
        xrtAllocationBudgetFree(Budget);
        info(Slot,9,0,false);
    }
}

static void local(xmqttversion Version)
{
    str Dir=xrtDirTemp(NULL,"xmqtt-journal-slot-",NULL);
    xroot Root=Dir?xrtRootOpen(Dir):NULL;
    testRequire(Root!=NULL,"native temporary directory");
    provision(Root);
    xmqttjournalconfig C=config(Version,9);
    xmqttjournalslotfiles Files={0};
    xmqttjournalslot* Slot=(xmqttjournalslot*)(uintptr_t)1;
    testRequire(xrtRootRemove(Root,names[2])&&xrtRootLinkHard(Root,names[1],names[2]),"aliased roles");
    status(mqttExampleOpenNativeSlot(Root,&C,true,&Files,&Slot),XMQTT_JOURNAL_STATE,"cannot alias two roles");
    testRequire(Slot==(xmqttjournalslot*)(uintptr_t)1,"alias refusal preserves output");
    xrtClearError();
    testRequire(xrtRootRemove(Root,names[2]),"remove rejected test alias");
    xfileoptions Options;
    xrtFileOptionsInit(&Options);
    Options.Flags=XFILE_READ|XFILE_WRITE|XFILE_CREATE|XFILE_EXCLUSIVE;
    xfile F=xrtRootFileOpen(Root,names[2],&Options);
    testRequire(F&&xrtClose(F),"separate backing for second slot");

    status(mqttExampleOpenNativeSlot(Root,&C,true,&Files,&Slot),XMQTT_JOURNAL_DONE,"slot initialization");
    info(Slot,9,0,false);
    busy(Root,&C);
    xmqttjournal* J=NULL;
    status(xmqttJournalSlotOpenCurrent(Slot,&J),XMQTT_JOURNAL_DONE,"open initial Journal");
    end(J);
    append(J,"old",1);
    xmqttjournal* Held=(xmqttjournal*)(uintptr_t)1;
    status(xmqttJournalSlotPrepare(Slot,&Held),XMQTT_JOURNAL_STATE,"cannot reclaim data while old Journal is live");
    testRequire(Held==(xmqttjournal*)(uintptr_t)1&&!mqttExampleCloseNativeSlot(Slot,&Files),"live child owns the slot");
    xrtClearError();
    testRequire(xmqttJournalDestroy(J),"retire old data owner");
    busy(Root,&C);
    preparationBudget(Slot);

    status(xmqttJournalSlotPrepare(Slot,&J),XMQTT_JOURNAL_DONE,"first preparation");
    append(J,"abandoned",1);
    info(Slot,9,0,true);
    testRequire(xmqttJournalDestroy(J),"abandon candidate without publication");
    status(xmqttJournalSlotOpenCurrent(Slot,&J),XMQTT_JOURNAL_DONE,"still selected old Journal");
    row(J,1,"old");
    end(J);
    testRequire(xmqttJournalDestroy(J),"retire verified selected owner");

    status(xmqttJournalSlotPrepare(Slot,&J),XMQTT_JOURNAL_DONE,"reuse inactive data");
    append(J,"replacement",1);
    xtaskpoolconfig PC={.Threads=1,.QueueLimit=8};
    xtaskpool* Pool=xrtTaskPoolCreate(&PC);
    xmqttjournalworkerconfig WC;
    xmqttJournalWorkerConfigInit(&WC);
    xmqttjournalworker* Worker=(xmqttjournalworker*)(uintptr_t)1;
    testRequire(Pool&&!xmqttJournalWorkerCreate(Pool,J,&WC,&Worker)&&Worker==(xmqttjournalworker*)(uintptr_t)1,
                "a prepared Journal cannot escape into a Worker");
    xrtClearError();
    status(xmqttJournalSlotPublish(Slot,J),XMQTT_JOURNAL_DONE,"publish data after its native sync");
    info(Slot,10,1,false);
    xmqttjournalslotinfo I;
    testRequire(xmqttJournalSlotInfo(Slot,&I)&&I.Effect==XMQTT_JOURNAL_COMMITTED,"actual publication effect");
    testRequire(xmqttJournalWorkerCreate(Pool,J,&WC,&Worker),"published Journal transfers to an ordinary Worker");
    xfuture* Future=NULL;
    testRequire(xmqttJournalWorkerAppendAsync(Worker,XRT_BYTES_LITERAL("tail"),5000,&Future)==XMQTT_JWORK_ACCEPTED&&
                xrtFutureWaitFor(Future,5000)==XWAIT_OK,"append after publication");
    const xmqttcommitresult* Result=xrtFutureValue(Future);
    testRequire(Result&&Result->Sequence==2&&Result->Effect==XMQTT_JOURNAL_COMMITTED,"post-publication continuation");
    testRequire(xmqttJournalWorkerDestroy(Worker)&&xrtTaskPoolClose(Pool)&&
                xrtTaskPoolWaitFor(Pool,5000)==XWAIT_OK&&xrtTaskPoolDestroy(Pool),"physically retire Worker");
    busy(Root,&C);
    testRequire(xmqttJournalSlotDestroy(Slot),"retire the logical slot");
    busy(Root,&C);
    testRequire(mqttExampleCloseJournalSlotFiles(&Files),"APP releases the three native locks");
    testRequire(Result->Effect==XMQTT_JOURNAL_COMMITTED,"retained result does not retain the slot");
    xrtFutureDestroy(Future);

    status(mqttExampleOpenNativeSlot(Root,&C,false,&Files,&Slot),XMQTT_JOURNAL_DONE,"fresh slot recovery");
    status(xmqttJournalSlotPrepare(Slot,&Held),XMQTT_JOURNAL_STATE,"verify the selected history before reusing old data");
    xrtClearError();
    status(xmqttJournalSlotOpenCurrent(Slot,&J),XMQTT_JOURNAL_DONE,"open published generation");
    row(J,1,"replacement");
    row(J,2,"tail");
    end(J);
    testRequire(xmqttJournalDestroy(J)&&mqttExampleCloseNativeSlot(Slot,&Files),"retire reopened slot");
    for(size_t N=0;N<3;++N)testRequire(xrtRootRemove(Root,names[N]),"remove exact fixture");
    testRequire(xrtRootClose(Root)&&xrtDirRemove(Dir),"retire directory fixture");
    xrtFree(Dir);
}

static void exhausted(void)
{
    str Dir=xrtDirTemp(NULL,"xmqtt-slot-maximum-",NULL);
    xroot Root=Dir?xrtRootOpen(Dir):NULL;
    testRequire(Root!=NULL,"generation limit fixture");
    provision(Root);
    xmqttjournalconfig C=config(XMQTT_VERSION_5,UINT64_MAX);
    xmqttjournalslotfiles Files={0};
    xmqttjournalslot* Slot=NULL;
    status(mqttExampleOpenNativeSlot(Root,&C,true,&Files,&Slot),XMQTT_JOURNAL_DONE,"last representable generation");
    xmqttjournal* J=(xmqttjournal*)(uintptr_t)1;
    status(xmqttJournalSlotPrepare(Slot,&J),XMQTT_JOURNAL_LIMIT,"generation cannot wrap");
    testRequire(J==(xmqttjournal*)(uintptr_t)1&&mqttExampleCloseNativeSlot(Slot,&Files),"generation limit preserves owner");
    for(size_t I=0;I<3;++I)testRequire(xrtRootRemove(Root,names[I]),"remove maximum generation fixture");
    testRequire(xrtRootClose(Root)&&xrtDirRemove(Dir),"retire maximum generation root");
    xrtFree(Dir);
}

static int process(int Argc,char** Argv)
{
    if(Argc<4||Argc>5)return 2;
    const char* Mode=Argv[1];
    xroot Root=xrtRootOpen(Argv[2]);
    testRequire(Root!=NULL,"native process root");
    xmqttjournalconfig C=config(!strcmp(Argv[3],"5")?XMQTT_VERSION_5:XMQTT_VERSION_311,9);
    xmqttjournalslotfiles Files={0};
    xmqttjournalslot* Slot=NULL;
    xmqttjournal* J=NULL;
    bool Create=!strcmp(Mode,"create"),Reject=!strcmp(Mode,"reject");
    if(!strcmp(Mode,"busy")){
        busy(Root,&C);
        puts("BUSY");
        testRequire(xrtRootClose(Root),"close contention probe root");
        return 0;
    }
    xmqttjournalstatus Status=mqttExampleOpenNativeSlot(Root,&C,Create,&Files,&Slot);
    if(Status==XMQTT_JOURNAL_DONE)Status=xmqttJournalSlotOpenCurrent(Slot,&J);
    if(Status!=XMQTT_JOURNAL_DONE&&!Reject)status(Status,XMQTT_JOURNAL_DONE,"open selected native process generation");
    if(J){
        xmqttjournalslotinfo View;
        xmqttjournalrecord R;
        testRequire(xmqttJournalSlotInfo(Slot,&View),"read selector");
        size_t Count=0;
        while((Status=xmqttJournalNext(J,&R))==XMQTT_JOURNAL_DONE){
            ++Count;
            if(!strcmp(Mode,"recover"))
                printf("%llu:%.*s\n",(unsigned long long)View.Generation,(int)R.Data.Size,(const char*)R.Data.Data);
        }
        if(!Reject)status(Status,XMQTT_JOURNAL_EMPTY,"complete native process replay");
        if(Create){
            testRequire(!Count,"empty first installation");
            append(J,"old",1);
        }
        testRequire(xmqttJournalDestroy(J),"retire selected data file");
    }
    if(!strcmp(Mode,"prepare")||!strcmp(Mode,"publish")){
        bool Publish=!strcmp(Mode,"publish");
        status(xmqttJournalSlotPrepare(Slot,&J),XMQTT_JOURNAL_DONE,"prepare native replacement");
        append(J,Argc==5?Argv[4]:"new",1);
        if(Publish){
            status(xmqttJournalSlotPublish(Slot,J),XMQTT_JOURNAL_DONE,"publish native replacement");
            append(J,"tail",2);
        }
        puts(Publish?"PUBLISHED":"PREPARED");
        fflush(stdout);
        (void)getchar();
        testRequire(xmqttJournalDestroy(J),"retire replacement data file");
    }else if(Reject){
        status(Status,XMQTT_JOURNAL_CORRUPT,"fail closed on selector or selected-history damage");
        puts("CORRUPT");
    }
    testRequire(mqttExampleCloseNativeSlot(Slot,&Files)&&xrtRootClose(Root),"native process cleanup");
    return 0;
}
#endif

int main(int Argc,char** Argv)
{
#if defined(_WIN32) || defined(_WIN64) || defined(__linux__)
    if(Argc>1)return process(Argc,Argv);
    virtualSlot();
    local(XMQTT_VERSION_5);
    local(XMQTT_VERSION_311);
    exhausted();
    puts("[PASS] native journal generation rotation, continuous guard and worker retirement");
#else
    (void)Argc;(void)Argv;
    virtualSlot();
    puts("[SKIP] native journal slots require Windows or Linux");
#endif
    return 0;
}
