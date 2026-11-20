#include "test.h"
#include "../examples/common/journal_files.h"
#include <xmqtt/store_write.h>

#if defined(_WIN32) || defined(_WIN64) || defined(__linux__)
static const char fileName[]="session.wal";
static const char aliasName[]="alias.wal";
static const xbytesview payload=XRT_BYTES_LITERAL("committed-before-process-exit");

static void status(xmqttjournalstatus Actual,xmqttjournalstatus Expected,const char* Step)
{
    if(Actual!=Expected)
        fprintf(stderr,"%s: status %d, expected %d, error %s\n",Step,(int)Actual,(int)Expected,
                xrtGetError()?xrtErrorMessage(xrtGetError()):"none");
    testRequire(Actual==Expected,Step);
}

static xmqttjournalconfig config(xmqttversion Version,uint32 Schema)
{
    xmqttjournalconfig C;
    testRequire(xmqttJournalConfigInit(&C),"journal defaults");
    C.Version=Version;
    C.Binding=XRT_BYTES_LITERAL("locked/broker/principal/session");
    C.Generation=7;
    C.MaxTransactionBytes=1024;
    C.PayloadSchema=Schema;
    return C;
}

static void busy(xroot Root,const char* Name,const xmqttjournalconfig* C)
{
    xfile LockedFile=NULL;
    xmqttjournal* Out=(xmqttjournal*)(uintptr_t)1;
    xrtSetErrorKind(XERR_TIMEOUT);
    const xerror* Ambient=xrtGetError();
    status(mqttExampleOpenNativeJournal(Root,Name,C,false,&LockedFile,&Out),XMQTT_JOURNAL_BUSY,"same object is still locked");
    testRequire(Out==(xmqttjournal*)(uintptr_t)1&&!LockedFile&&xrtGetError()==Ambient,
                "busy preserves output and ambient error");
    xrtClearError();
}

static void append(xmqttjournal* Journal)
{
    uint64 Sequence=0;
    status(xmqttJournalAppend(Journal,payload,&Sequence),XMQTT_JOURNAL_DONE,"native file commit under lock");
    testRequire(Sequence==1,"first native commit sequence");
}

static void replay(xmqttjournal* Journal)
{
    xmqttjournalrecord Row;
    status(xmqttJournalNext(Journal,&Row),XMQTT_JOURNAL_DONE,"first committed transaction");
    testRequire(Row.Sequence==1&&Row.Data.Size==payload.Size&&!memcmp(Row.Data.Data,payload.Data,payload.Size),
                "exact durable bytes after the original owner left");
    status(xmqttJournalNext(Journal,&Row),XMQTT_JOURNAL_EMPTY,"the complete committed history");
}

static void attempts(xroot Root,const xmqttjournalconfig* C)
{
    const size_t Limits[]={1,256,4096,32768};
    for(size_t I=0;I<sizeof(Limits)/sizeof(Limits[0]);++I){
        xallocationbudget* Budget=NULL;
        xallocationscope Scope={0};
        testRequire(xrtAllocationBudgetCreate(Limits[I],&Budget)&&xrtAllocationBudgetEnter(Budget,&Scope),
                    "bounded construction allocation scope");
        xfile LockedFile=NULL;
        xmqttjournal* Journal=(xmqttjournal*)(uintptr_t)1;
        xmqttjournalstatus S=mqttExampleOpenNativeJournal(Root,fileName,C,false,&LockedFile,&Journal);
        testRequire(xrtAllocationBudgetLeave(&Scope),"leave construction scope");
        if(S==XMQTT_JOURNAL_DONE)testRequire(mqttExampleCloseNativeJournal(Journal,&LockedFile),"owned file under allocation scope");
        else testRequire((S==XMQTT_JOURNAL_IO||S==XMQTT_JOURNAL_ERROR)&&Journal==(xmqttjournal*)(uintptr_t)1,
                         "allocation refusal preserves output");
        xrtClearError();
        xallocationbudgetinfo Info={.Size=sizeof(Info)};
        testRequire(xrtAllocationBudgetInfo(Budget,&Info)&&!Info.LiveBytes&&!Info.LiveCount,
                    "no failed-construction allocation or native wrapper held");
        xrtAllocationBudgetFree(Budget);
        status(mqttExampleOpenNativeJournal(Root,fileName,C,false,&LockedFile,&Journal),XMQTT_JOURNAL_DONE,"lock available after failed open");
        replay(Journal);
        testRequire(mqttExampleCloseNativeJournal(Journal,&LockedFile),"release recovery probe");
    }
}

static void provision(xroot Root)
{
    xfileoptions Options;
    xrtFileOptionsInit(&Options);
    Options.Flags=XFILE_READ|XFILE_WRITE|XFILE_CREATE|XFILE_EXCLUSIVE;
    xfile File=xrtRootFileOpen(Root,fileName,&Options);
    testRequire(File&&xrtClose(File),"empty native file provisioned by the fixture");
    testRequire(xrtRootLinkHard(Root,fileName,aliasName),"independent hard-link name for the same file");
}

static void storeOwner(xroot Root,xmqttversion Version)
{
    static const char Name[]="typed.wal";
    xfileoptions Options;
    xrtFileOptionsInit(&Options);
    Options.Flags=XFILE_READ|XFILE_WRITE|XFILE_CREATE|XFILE_EXCLUSIVE;
    xfile File=xrtRootFileOpen(Root,Name,&Options);
    testRequire(File&&xrtClose(File),"empty typed native file");
    xmqttjournalconfig C=config(Version,XMQTT_STORE_DELTA_SCHEMA);
    xfile LockedFile=NULL;
    xmqttjournal* Journal=NULL;
    status(mqttExampleOpenNativeJournal(Root,Name,&C,true,&LockedFile,&Journal),XMQTT_JOURNAL_DONE,"initialize typed owned journal");

    xmqttstorestateconfig SC;
    xmqttStoreStateConfigInit(&SC);
    SC.Delta.Version=Version;
    SC.Binding=C.Binding;
    SC.Generation=C.Generation;
    SC.ClientId=XRT_STR_LITERAL("guarded-client");
    xmqttstorestate* State=NULL;
    testRequire(xmqttStoreStateRecover(Journal,&SC,&State)==XMQTT_STORE_DONE,"real typed empty recovery");
    xtaskpoolconfig PC={.Threads=1,.QueueLimit=16};
    xtaskpool* Pool=xrtTaskPoolCreate(&PC);
    xmqttstorewriteconfig WC;
    xmqttStoreWriteConfigInit(&WC);
    xmqttstorewrite* Writer=NULL;
    testRequire(Pool&&xmqttStoreWriteCreate(Pool,Journal,State,&WC,&Writer),"Writer takes an owned ready Journal");
    testRequire(!xmqttJournalDestroy(Journal),"borrowed alias cannot bypass the Writer");
    xrtClearError();
    busy(Root,Name,&C);

    uint32 Expiry=Version==XMQTT_VERSION_5?60:UINT32_MAX;
    const xmqttstoredelta Operations[]={
        {.Type=XMQTT_STORE_BIND,.Epoch=1,.WallMilliseconds=1000,.Data=XRT_BYTES_LITERAL("guarded-client")},
        {.Type=XMQTT_STORE_OPEN,.Epoch=1,.WallMilliseconds=1001,.Expiry=Expiry},
        {.Type=XMQTT_STORE_END,.Epoch=1,.WallMilliseconds=1002,.Expiry=Expiry},
    };
    unsigned char Bytes[512];
    size_t Length=0;
    xfuture* Future=NULL;
    testRequire(xmqttStoreDeltaWrite(&SC.Delta,Operations,sizeof(Operations)/sizeof(Operations[0]),
                    NULL,0,Bytes,sizeof(Bytes),&Length)==XMQTT_STORE_DELTA_DONE&&
                xmqttStoreWriteAppendAsync(Writer,(xbytesview){Bytes,Length},5000,&Future)==XMQTT_JWORK_ACCEPTED,
                "real asynchronous typed commit to the locked file");
    testRequire(xrtFutureWaitFor(Future,5000)==XWAIT_OK,"native commit completes");
    const xmqttcommitresult* Result=xrtFutureValue(Future);
    testRequire(Result&&Result->Status==XMQTT_COMMIT_OK&&Result->Effect==XMQTT_JOURNAL_COMMITTED&&
                Result->Sequence==1,"durable effect before ownership retirement");
    testRequire(xmqttStoreWriteClose(Writer)&&xmqttStoreWriteWaitFor(Writer,5000)==XWAIT_OK,
                "join native work before releasing the owner");
    busy(Root,Name,&C);
    testRequire(xmqttStoreWriteDestroy(Writer),"destroy releases the logical Writer and Journal");
    busy(Root,Name,&C);
    testRequire(mqttExampleCloseJournalFile(&LockedFile),"APP releases the file after Writer retirement");
    testRequire(xrtTaskPoolClose(Pool)&&xrtTaskPoolWaitFor(Pool,5000)==XWAIT_OK&&xrtTaskPoolDestroy(Pool),
                "retire borrowed pool");
    testRequire(Result->Effect==XMQTT_JOURNAL_COMMITTED,"retained result does not own the native file");
    status(mqttExampleOpenNativeJournal(Root,Name,&C,false,&LockedFile,&Journal),XMQTT_JOURNAL_DONE,"reacquire after Writer destroy");
    SC.WallMilliseconds=2000;
    testRequire(xmqttStoreStateRecover(Journal,&SC,&State)==XMQTT_STORE_DONE,"fresh typed recovery");
    xmqttstorestateinfo Info;
    testRequire(xmqttStoreStateInfo(State,&Info)&&Info.Transactions==1&&Info.HasSession&&!Info.LiveAtCut,
                "typed history preserved by the owner transition");
    xmqttStoreStateDestroy(State);
    xrtFutureDestroy(Future);
    testRequire(mqttExampleCloseNativeJournal(Journal,&LockedFile)&&xrtRootRemove(Root,Name),"release typed fixture");
}

static void local(xmqttversion Version)
{
    str Directory=xrtDirTemp(NULL,"xmqtt-locked-journal-",NULL);
    xroot Root=Directory?xrtRootOpen(Directory):NULL;
    testRequire(Root!=NULL,"anchored private fixture directory");
    xmqttjournalconfig C=config(Version,1);
    xfile LockedFile=NULL;
    xmqttjournal* Journal=(xmqttjournal*)(uintptr_t)1;
    status(mqttExampleOpenNativeJournal(Root,fileName,&C,true,&LockedFile,&Journal),XMQTT_JOURNAL_IO,"missing name is not implicitly created");
    testRequire(Journal==(xmqttjournal*)(uintptr_t)1,"failed constructor preserves output");
    xrtClearError();

    testRequire(xrtRootDirCreate(Root,"subdir",0700),"fixture directory entry");
    status(mqttExampleOpenNativeJournal(Root,"subdir",&C,false,&LockedFile,&Journal),XMQTT_JOURNAL_ERROR,
           "nonregular entries are rejected before open");
    testRequire(Journal==(xmqttjournal*)(uintptr_t)1&&xrtRootRemove(Root,"subdir"),
                "directory failure preserves output");
    xrtClearError();

    provision(Root);
    status(mqttExampleOpenNativeJournal(Root,fileName,&C,true,&LockedFile,&Journal),XMQTT_JOURNAL_DONE,"lock before initialization");
    busy(Root,fileName,&C);
    busy(Root,aliasName,&C);
    testRequire(xrtRootClose(Root),"borrowed root can close after construction");
    append(Journal);
    Root=xrtRootOpen(Directory);
    testRequire(Root!=NULL,"independent native root");
    busy(Root,aliasName,&C);
    testRequire(mqttExampleCloseNativeJournal(Journal,&LockedFile),"owner physically closes");
    xmqttjournalconfig Other=C;
    Other.Generation++;
    status(mqttExampleOpenNativeJournal(Root,fileName,&Other,false,&LockedFile,&Journal),XMQTT_JOURNAL_CORRUPT,
           "descriptor mismatch closes the newly locked handle");
    xrtClearError();
    attempts(Root,&C);
    storeOwner(Root,Version);
    testRequire(xrtRootRemove(Root,aliasName)&&xrtRootRemove(Root,fileName)&&xrtRootClose(Root)&&xrtDirRemove(Directory),
                "remove only the fixture's native files and directory");
    xrtFree(Directory);
}

static int process(int Argc,char** Argv)
{
    if(Argc!=5)return 2;
    const char* Mode=Argv[1];
    xroot Root=xrtRootOpen(Argv[2]);
    testRequire(Root!=NULL,"existing process fixture root");
    xmqttjournalconfig C=config(!strcmp(Argv[4],"5")?XMQTT_VERSION_5:XMQTT_VERSION_311,1);
    xfile LockedFile=NULL;
    xmqttjournal* Journal=NULL;
    if(!strcmp(Mode,"busy")){
        busy(Root,Argv[3],&C);
        puts("BUSY");
    }else if(!strcmp(Mode,"hold")){
        xmqttjournalstatus S=mqttExampleOpenNativeJournal(Root,Argv[3],&C,true,&LockedFile,&Journal);
        if(S==XMQTT_JOURNAL_BUSY){
            puts("BUSY");
            testRequire(xrtRootClose(Root),"contender root close");
            return 3;
        }
        status(S,XMQTT_JOURNAL_DONE,"exclusive process initializer");
        append(Journal);
        puts("READY");
        fflush(stdout);
        (void)getchar();
    }else if(!strcmp(Mode,"recover")){
        status(mqttExampleOpenNativeJournal(Root,Argv[3],&C,false,&LockedFile,&Journal),XMQTT_JOURNAL_DONE,"fresh process reclaims file");
        replay(Journal);
        puts("RECOVERED");
    }else return 2;
    testRequire(mqttExampleCloseNativeJournal(Journal,&LockedFile)&&xrtRootClose(Root),"process cleanup");
    return 0;
}
#endif

int main(int Argc,char** Argv)
{
#if defined(_WIN32) || defined(_WIN64) || defined(__linux__)
    if(Argc>1)return process(Argc,Argv);
    local(XMQTT_VERSION_5);
    local(XMQTT_VERSION_311);
    puts("[PASS] locked native journal, hard-link contention, OOM and typed Writer retirement");
#else
    (void)Argc;(void)Argv;
    puts("[SKIP] native locked journals require Windows or Linux");
#endif
    return 0;
}
