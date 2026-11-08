#include <xrt.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>

static unsigned reads,writes,zeros,cancels,closes,graphs,oomFailures,oomAccepted;
static xnetengine* fileEngine(xnetportbackend backend)
{
    xnetengineconfig config; xrtNetEngineConfigInit(&config);
    config.Workers=1; config.NodeCacheBytes=0; config.Backend=backend;
    xnetengine* engine=xrtNetEngineCreate(&config); assert(engine);
    assert(xrtNetEngineStart(engine)); return engine;
}
static xnetportbackend fileBackend(void)
{
#if defined(_WIN32)
    return XNET_PORT_IOCP;
#else
    return XNET_PORT_URING;
#endif
}
static xnetfile* fileOpen(xnetengine* engine, const char* path)
{
    xfileoptions options; xrtFileOptionsInit(&options);
    options.Flags=XFILE_READ|XFILE_WRITE|XFILE_CREATE|XFILE_TRUNCATE;
    return xrtNetFileCreate(engine,0,path,&options);
}
static void fileStop(xnetengine* engine)
{ assert(xrtNetEngineStop(engine)); assert(xrtNetEngineDestroy(engine)); xrtClearError(); }

static void fileFutureWait(xfuture* future)
{
    assert(future);
    double end=xrtTimer()+10.0;
    while (xrtFutureState(future)==XFUTURE_PENDING) {
        assert(xrtTimer()<end); xrtThreadYield();
    }
}
static void fileClose(xnetfile* file)
{
    xfuture* close=xrtNetFileCloseFuture(file); fileFutureWait(close);
    assert(xrtFutureState(close)==XFUTURE_RESOLVED); ++closes;
    xfuture* same=xrtNetFileCloseFuture(file); assert(same==close); xrtFutureDestroy(same);
    xrtFutureDestroy(close); xrtNetFileDestroy(file);
}
static size_t childCount;
static xrtownershipref children[128];
static bool fileVisit(xrtownershipref ref, ptr unused)
{ (void)unused; assert(ref.Data&&ref.Ops&&childCount<128); children[childCount++]=ref; return true; }
static void fileFreeze(xnetfile* file, xrtownershipscope* freeze, size_t wanted)
{
    double end=xrtTimer()+10.0; xrtownershipref ref=xrtNetFileOwnership(file); size_t count;
    for (;;) {
        if (xrtOwnershipFreezeTryBegin(freeze)) {
            if (ref.Ops->Count(file,&count) && count==wanted) return;
            assert(xrtOwnershipScopeEnd(freeze));
        }
        assert(xrtTimer()<end); xrtThreadYield();
    }
}
static void fileGraph(xnetengine* engine, xnetfile* file)
{
    xrtownershipscope freeze={0}; fileFreeze(file,&freeze,3);
    xrtownershipref ref=xrtNetFileOwnership(file); childCount=0;
    assert(ref.Ops->Trace(file,fileVisit,NULL)&&childCount==3);
    assert(children[0].Data==engine);
    assert(children[1].Data==children[2].Data&&children[1].Ops==children[2].Ops);
    const xnetruntimeownershipv1* runtime=xrtNetFileRuntimeOwnershipV1Get();
    const xrtownershippreparationv1* preparation=(const xrtownershippreparationv1*)file;
    assert(!xrtNetEngineOwnershipAdapterV1(xrtNetEngineOwnership(engine),NULL,0,NULL,0,&preparation));
    assert(preparation==(const xrtownershippreparationv1*)file);
    assert(xrtNetEngineOwnershipAdapterV3(xrtNetEngineOwnership(engine),NULL,0,NULL,0,NULL,0,&runtime,1,&preparation));
    childCount=0; assert(xrtNetEngineOwnership(engine).Ops->Trace(engine,fileVisit,NULL));
    unsigned registrations=0; for (size_t i=0;i<childCount;++i) if (children[i].Data==file) ++registrations;
    assert(registrations==1); assert(xrtOwnershipScopeEnd(&freeze));
    assert(xrtNetFileRef(file)==file); fileFreeze(file,&freeze,4);
    assert(xrtOwnershipScopeEnd(&freeze)); xrtNetFileDestroy(file); ++graphs;
}
static void fileBasic(void)
{
    const char* path="owned-file-future.bin";
    xnetengine* engine=fileEngine(fileBackend());
    xnetfile* file=fileOpen(engine,path); assert(file); fileGraph(engine,file);
    char source[3]={'a',0,'b'};
    xfuture* write=xrtNetFileWriteFuture(file,0,(xbytesview){(cbytes)source,3},NULL);
    memset(source,'X',3); /* copy input cannot become a long-lived borrow */
    fileFutureWait(write); assert(xrtFutureState(write)==XFUTURE_RESOLVED);
    assert((uintptr_t)xrtFutureValue(write)==3); xrtFutureDestroy(write); ++writes;
    xfuture* read=xrtNetFileReadFuture(file,0,8,NULL);
    fileFutureWait(read); assert(xrtFutureState(read)==XFUTURE_RESOLVED);
    xbuffer* buffer=xrtFutureValue(read); assert(buffer&&buffer->Size==3&&!memcmp(buffer->Data,"a\0b",3));
    xrtFutureDestroy(read); ++reads;
    read=xrtNetFileReadFuture(file,3,8,NULL); fileFutureWait(read);
    assert(xrtFutureState(read)==XFUTURE_RESOLVED);
    buffer=xrtFutureValue(read); assert(buffer&&buffer->Size==0); xrtFutureDestroy(read); ++reads;
    read=xrtNetFileReadFuture(file,100,8,NULL); fileFutureWait(read);
    assert(xrtFutureState(read)==XFUTURE_RESOLVED);
    buffer=xrtFutureValue(read); assert(buffer&&buffer->Size==0); xrtFutureDestroy(read); ++reads;
    read=xrtNetFileReadFuture(file,0,0,NULL); fileFutureWait(read);
    assert(xrtFutureState(read)==XFUTURE_RESOLVED);
    buffer=xrtFutureValue(read); assert(buffer&&buffer->Size==0); xrtFutureDestroy(read); ++zeros;
    write=xrtNetFileWriteFuture(file,0,(xbytesview){NULL,0},NULL); fileFutureWait(write);
    assert(xrtFutureState(write)==XFUTURE_RESOLVED&&!xrtFutureValue(write)); xrtFutureDestroy(write); ++zeros;
    fileGraph(engine,file); /* owned completion must not seal the raw port */
    assert(!xrtNetFileReadFuture(file,UINT64_MAX,0,NULL)); xrtClearError();
    assert(!xrtNetFileReadFuture(file,INT64_MAX,2,NULL)); xrtClearError();
    xfuture* close=xrtNetFileCloseFuture(file); fileFutureWait(close);
    assert(xrtFutureState(close)==XFUTURE_RESOLVED); ++closes;
    assert(!xrtNetFileReadFuture(file,0,0,NULL)); xrtClearError();
    xrtownershipscope freeze={0}; fileFreeze(file,&freeze,1);
    const xrtownershippreparationv1* prep=NULL;
    const xrtownershipadapterv1* adapter=xrtNetFileOwnershipAdapterV1(xrtNetFileOwnership(file),&prep);
    assert(adapter&&prep&&prep->Ready(file));
    assert(xrtOwnershipScopeEnd(&freeze));
    assert(adapter->Hold(file)); fileFreeze(file,&freeze,2);
    int token; assert(adapter->Claim(file,&token)); adapter->Clear(file,&token);
    assert(xrtOwnershipScopeEnd(&freeze));
    assert(adapter->Finish(file,&token)); adapter->Drop(file);
    xrtFutureDestroy(close); xrtNetFileDestroy(file); ++graphs;
    fileStop(engine);
    assert(remove(path)==0); xrtClearError();
}
static void fileUnsupported(void)
{
    const char* path="owned-file-unsupported.bin";
    FILE* seed=fopen(path,"wb"); assert(seed); assert(fwrite("keep",1,4,seed)==4&&fclose(seed)==0);
    xnetengine* engine=fileEngine(XNET_PORT_SELECT);
    assert(!fileOpen(engine,path)&&xrtErrorKind(xrtGetError())==XERR_UNSUPPORTED); xrtClearError();
    fileStop(engine);
    seed=fopen(path,"rb"); char value[5]={0}; assert(seed&&fread(value,1,5,seed)==4&&fclose(seed)==0&&!strcmp(value,"keep"));
    assert(remove(path)==0);
}
static void fileUnreachablePreparation(void)
{
    const char* path="owned-file-prepare.bin"; xnetengine* engine=fileEngine(fileBackend());
    xnetfile* file=fileOpen(engine,path); assert(file);
    xrtownershipscope freeze={0}; fileFreeze(file,&freeze,3);
    const xrtownershippreparationv1* prep=NULL;
    const xrtownershipadapterv1* adapter=xrtNetFileOwnershipAdapterV1(xrtNetFileOwnership(file),&prep);
    assert(adapter&&prep&&!prep->Ready(file)); assert(xrtOwnershipScopeEnd(&freeze));
    assert(adapter->Hold(file)); fileFreeze(file,&freeze,4);
    int token; assert(adapter->Claim(file,&token)); assert(xrtOwnershipScopeEnd(&freeze));
    /* The adapter caller authorizes this native duty. A separate XLIR planner
     * fixture proves actual unreachability/closed-graph ordering end to end. */
    assert(prep->Prepare(file,&token)==XRT_OWNERSHIP_PREPARE_BUSY);
    double end=xrtTimer()+10.0;
    for (;;) {
        if (xrtOwnershipFreezeTryBegin(&freeze)) {
            if (prep->Ready(file)) break;
            assert(xrtOwnershipScopeEnd(&freeze));
        }
        assert(xrtTimer()<end); xrtThreadYield();
    }
    adapter->Clear(file,&token); assert(xrtOwnershipScopeEnd(&freeze));
    assert(adapter->Finish(file,&token)); adapter->Drop(file); xrtNetFileDestroy(file);
    fileStop(engine); assert(remove(path)==0); ++graphs;
}
typedef struct FileGate { xatomic32 entered,release,done; bool failSubmit; } FileGate;
static void fileGate(xnetworker* worker,ptr data)
{
    FileGate* gate=data; assert(worker); xrtAtomic32Store(&gate->entered,1,XMEMORY_RELEASE);
    double end=xrtTimer()+10.0;
    while (!xrtAtomic32Load(&gate->release,XMEMORY_ACQUIRE)) { assert(xrtTimer()<end); xrtThreadYield(); }
#if defined(XRT_FEATURE_MEMORY_DEBUG)
    if (gate->failSubmit) assert(xrtMemDebugFailAfter(0));
#endif
    xrtAtomic32Store(&gate->done,1,XMEMORY_RELEASE);
}
static void fileGateBegin(xnetengine* engine, FileGate* gate)
{
    memset(gate,0,sizeof(*gate)); assert(xrtNetEnginePost(engine,0,fileGate,gate));
    double end=xrtTimer()+10.0;
    while (!xrtAtomic32Load(&gate->entered,XMEMORY_ACQUIRE)) { assert(xrtTimer()<end); xrtThreadYield(); }
}
static void fileCancellation(void)
{
    const char* path="owned-file-cancel.bin"; xnetengine* engine=fileEngine(fileBackend());
    xnetfile* file=fileOpen(engine,path); assert(file);
    xcancel* cancel=xrtCancelCreate(); assert(cancel&&xrtCancelRequest(cancel));
    xfuture* future=xrtNetFileWriteFuture(file,0,XRT_BYTES_LITERAL("no"),cancel);
    fileFutureWait(future); assert(xrtFutureState(future)==XFUTURE_CANCELLED); ++cancels;
    xrtFutureDestroy(future); xrtCancelDestroy(cancel);
    FileGate gate; fileGateBegin(engine,&gate);
    future=xrtNetFileWriteFuture(file,0,XRT_BYTES_LITERAL("no"),NULL); assert(future);
    assert(xrtFutureCancel(future));
    xrtAtomic32Store(&gate.release,1,XMEMORY_RELEASE); fileFutureWait(future);
    assert(xrtFutureState(future)==XFUTURE_CANCELLED); ++cancels; xrtFutureDestroy(future);
    future=xrtNetFileReadFuture(file,0,8,NULL); fileFutureWait(future);
    assert(xrtFutureState(future)==XFUTURE_RESOLVED&&((xbuffer*)xrtFutureValue(future))->Size==0);
    xrtFutureDestroy(future); ++reads;
    fileGateBegin(engine,&gate);
    xfuture* pending[16];
    for (unsigned i=0;i<16;++i) { pending[i]=xrtNetFileReadFuture(file,0,8,NULL); assert(pending[i]); }
    xfuture* close=xrtNetFileCloseFuture(file); assert(close);
    xfuture* same=xrtNetFileCloseFuture(file); assert(same==close); xrtFutureDestroy(same);
    xrtNetFileDestroy(file); /* actual producers/directory/commands keep it alive */
    for (unsigned i=0;i<8;++i) { xrtFutureDestroy(pending[i]); pending[i]=NULL; }
    xrtAtomic32Store(&gate.release,1,XMEMORY_RELEASE);
    for (unsigned i=8;i<16;++i) { fileFutureWait(pending[i]); assert(xrtFutureState(pending[i])==XFUTURE_CANCELLED); ++cancels; xrtFutureDestroy(pending[i]); }
    fileFutureWait(close); assert(xrtFutureState(close)==XFUTURE_RESOLVED); ++closes; xrtFutureDestroy(close);
    fileStop(engine); assert(remove(path)==0);
}
typedef struct FileInput { volatile int32 refs; xatomic32 drops,request,reply,stop; bool inspect; char bytes[4]; } FileInput;
static int32 fileInspector(ptr data)
{
    FileInput* input=data;
    while (!xrtAtomic32Load(&input->stop,XMEMORY_ACQUIRE)) {
        uint32 request=xrtAtomic32Load(&input->request,XMEMORY_ACQUIRE);
        if (request!=xrtAtomic32Load(&input->reply,XMEMORY_ACQUIRE)) {
            xrtownershipscope freeze={0};
            if (xrtOwnershipFreezeTryBegin(&freeze)) {
                assert(xrtOwnershipScopeEnd(&freeze)); xrtAtomic32Store(&input->reply,request,XMEMORY_RELEASE);
            }
        }
        xrtThreadYield();
    }
    return 0;
}
static bool inputCount(const void* data,size_t* count)
{ const FileInput* p=data; if (!count||p->refs<=0) return false; *count=(size_t)p->refs; return true; }
static bool inputTrace(const void* data,xrtownershipvisitor visit,ptr context)
{ (void)context; return data&&visit; }
static void inputDrop(ptr data)
{
    FileInput* p=data;
    if (p->inspect) {
        uint32 request=xrtAtomic32FetchAdd(&p->request,1,XMEMORY_ACQ_REL)+1;
        double end=xrtTimer()+10.0;
        while (xrtAtomic32Load(&p->reply,XMEMORY_ACQUIRE)!=request) { assert(xrtTimer()<end); xrtThreadYield(); }
    }
    xrtownershipscope scope={0}; assert(xrtOwnershipMutationBegin(&scope));
    assert(xrtRefRelease(&p->refs)==1); xrtAtomic32FetchAdd(&p->drops,1,XMEMORY_RELEASE);
    assert(xrtOwnershipScopeEnd(&scope));
}
static const xrtownershipops inputOps={inputCount,inputTrace};
static const xnetfiledataownershipv1 inputPolicy={sizeof(inputPolicy),inputDrop,&inputOps};
static void fileReferenceInput(void)
{
    const char* path="owned-file-ref.bin"; xnetengine* engine=fileEngine(fileBackend());
    xnetfile* file=fileOpen(engine,path); assert(file);
    FileInput input={0}; input.refs=2; memcpy(input.bytes,"a\0b",4);
    input.inspect=true; xthread* inspector=xrtThreadCreate(fileInspector,&input,0); assert(inspector);
    xfuture* future=xrtNetFileWriteRefPolicyV1(file,0,(xbytesview){(cbytes)input.bytes,3},&input,&inputPolicy,NULL);
    fileFutureWait(future); assert(xrtFutureState(future)==XFUTURE_RESOLVED&&(uintptr_t)xrtFutureValue(future)==3);
    assert(input.refs==1&&xrtAtomic32Load(&input.drops,XMEMORY_ACQUIRE)==1); xrtFutureDestroy(future); ++writes;
    assert(xrtAtomic32Load(&input.reply,XMEMORY_ACQUIRE)==1);
    xrtAtomic32Store(&input.stop,1,XMEMORY_RELEASE);
    assert(xrtThreadWait(inspector)==XWAIT_OK); xrtThreadDestroy(inspector);
    future=xrtNetFileReadFuture(file,0,8,NULL); fileFutureWait(future);
    xbuffer* b=xrtFutureValue(future); assert(b&&b->Size==3&&!memcmp(b->Data,input.bytes,3));
    /* Result storage can be transferred without copying the whole payload. */
    size_t size=0,capacity=0; bytes data=xrtBufferTake(b,&size,&capacity);
    assert(data&&size==3&&capacity>=3&&!memcmp(data,"a\0b",3)); xrtFree(data);
    xrtFutureDestroy(future); ++reads;
    fileClose(file); fileStop(engine); assert(remove(path)==0);
}
typedef struct FileRacer { xnetfile* file; xatomic32* started; unsigned submitted,rejected; } FileRacer;
static int32 fileRace(ptr data)
{
    FileRacer* racer=data;
    for (unsigned i=0;i<16;++i) {
        xfuture* future=xrtNetFileWriteFuture(racer->file,i,XRT_BYTES_LITERAL("x"),NULL);
        xrtAtomic32FetchAdd(racer->started,1,XMEMORY_RELEASE);
        if (future) {
            fileFutureWait(future); xfuturestate state=xrtFutureState(future);
            assert(state==XFUTURE_RESOLVED||state==XFUTURE_CANCELLED);
            if (state==XFUTURE_RESOLVED) assert((uintptr_t)xrtFutureValue(future)==1);
            xrtFutureDestroy(future); ++racer->submitted;
        } else { assert(xrtGetError()&&xrtErrorKind(xrtGetError())==XERR_STATE); xrtClearError(); ++racer->rejected; }
    }
    xrtNetFileDestroy(racer->file); return 0;
}
static void fileRaces(void)
{
    for (unsigned round=0;round<8;++round) {
        const char* path="owned-file-race.bin"; xnetengine* engine=fileEngine(fileBackend());
        xnetfile* file=fileOpen(engine,path); assert(file);
        xatomic32 started; xrtAtomic32Init(&started,0);
        FileRacer racers[2]={{xrtNetFileRef(file),&started,0,0},{xrtNetFileRef(file),&started,0,0}};
        xthread* threads[2]={xrtThreadCreate(fileRace,&racers[0],0),xrtThreadCreate(fileRace,&racers[1],0)};
        assert(threads[0]&&threads[1]); double end=xrtTimer()+10.0;
        while (xrtAtomic32Load(&started,XMEMORY_ACQUIRE)<2) { assert(xrtTimer()<end); xrtThreadYield(); }
        xfuture* close=xrtNetFileCloseFuture(file); assert(close); xrtNetFileDestroy(file);
        for (unsigned i=0;i<2;++i) { assert(xrtThreadWait(threads[i])==XWAIT_OK); xrtThreadDestroy(threads[i]); assert(racers[i].submitted+racers[i].rejected==16); }
        fileFutureWait(close); assert(xrtFutureState(close)==XFUTURE_RESOLVED); ++closes;
        xrtFutureDestroy(close); fileStop(engine); assert(remove(path)==0);
    }
}
#if defined(XRT_FEATURE_MEMORY_DEBUG)
static void fileBackendOom(void)
{
    const char* path="owned-file-backend-oom.bin";
    xnetengine* engine=fileEngine(fileBackend()); xnetfile* file=fileOpen(engine,path); assert(file);
    FileGate gate; fileGateBegin(engine,&gate);
    xfuture* future=xrtNetFileReadFuture(file,0,8,NULL); assert(future);
    /* Fresh port: unlike earlier successful I/O, no cached native operation
     * can bypass this first real backend allocation. */
    gate.failSubmit=true; xrtAtomic32Store(&gate.release,1,XMEMORY_RELEASE);
    fileFutureWait(future); assert(xrtFutureState(future)==XFUTURE_FAILED);
    assert(xrtErrorKind(xrtFutureError(future))==XERR_MEMORY); xrtFutureDestroy(future);
    fileGraph(engine,file); fileClose(file); fileStop(engine); assert(remove(path)==0);
}
static void fileBalanced(xmemdebugsnapshot before)
{
    xmemdebugsnapshot after; xrtClearError(); xrtMemDebugSnapshot(&after);
    assert(before.LiveCount==after.LiveCount&&before.LiveBytes==after.LiveBytes&&
        before.InvalidFreeCount==after.InvalidFreeCount&&before.DoubleFreeCount==after.DoubleFreeCount&&
        before.UseAfterFreeCount==after.UseAfterFreeCount);
}
static void fileOom(unsigned kind,unsigned budget)
{
    xmemdebugsnapshot before; xrtClearError(); xrtMemDebugSnapshot(&before);
    const char* path="owned-file-oom.bin";
    FILE* seed=fopen(path,"wb"); assert(seed&&fwrite("keep",1,4,seed)==4&&fclose(seed)==0);
    xnetengine* engine=fileEngine(fileBackend()); xnetfile* file=NULL;
    if (kind) { file=fileOpen(engine,path); assert(file); }
    FileInput input={0}; input.refs=2; memcpy(input.bytes,"a\0b",4);
    assert(xrtMemDebugFailAfter(budget));
    xfuture* future=NULL;
    if (!kind) file=fileOpen(engine,path);
    else if (kind==1) future=xrtNetFileReadFuture(file,0,8,NULL);
    else if (kind==2) future=xrtNetFileWriteFuture(file,0,XRT_BYTES_LITERAL("a\0b"),NULL);
    else future=xrtNetFileWriteRefPolicyV1(file,0,(xbytesview){(cbytes)input.bytes,3},&input,&inputPolicy,NULL);
    xrtMemDebugFailClear();
    bool accepted=kind?future!=NULL:file!=NULL;
    if (accepted) ++oomAccepted;
    else { assert(xrtGetError()); ++oomFailures; xrtClearError(); }
    if (future) { fileFutureWait(future); assert(xrtFutureState(future)==XFUTURE_RESOLVED); xrtFutureDestroy(future); }
    if (file) fileClose(file);
    fileStop(engine);
    if (kind==3) {
        assert(input.refs==(accepted?1:2));
        assert(xrtAtomic32Load(&input.drops,XMEMORY_ACQUIRE)==(accepted?1u:0u));
    }
    if (!kind && !accepted) {
        char value[5]={0}; seed=fopen(path,"rb"); assert(seed&&fread(value,1,5,seed)==4&&fclose(seed)==0&&!strcmp(value,"keep"));
    }
    assert(remove(path)==0); fileBalanced(before);
}
#endif
int main(void)
{
#if defined(XRT_FEATURE_MEMORY_DEBUG)
    fileBackendOom();
    assert(xrtMemDebugEnable(true));
#endif
    fileBasic(); fileUnsupported(); fileCancellation(); fileReferenceInput(); fileRaces(); fileUnreachablePreparation();
#if defined(XRT_FEATURE_MEMORY_DEBUG)
    assert(xrtMemDebugReset());
    for (unsigned kind=0;kind<4;++kind) for (unsigned budget=0;budget<32;++budget) fileOom(kind,budget);
    assert(oomFailures&&oomAccepted&&xrtMemDebugReset());
#endif
    printf("Owned NetFile: reads=%u writes=%u zero=%u cancelled=%u close=%u graphs=%u OOM-rejected=%u accepted=%u\n",
        reads,writes,zeros,cancels,closes,graphs,oomFailures,oomAccepted);
    puts("[PASS] owned native file Future"); return 0;
}
