/* Ordinary fixed certificates, ownership, transactional OOM and readers only.
 * No signature variation, malformed cryptographic inputs or network traffic. */
#include "../test.h"
#include "../fixtures/x509_path_legacy.h"
#if defined(_WIN32)
#include <windows.h>
#else
#include <pthread.h>
#endif

static size_t cloneBudgets,mergeBudgets,allocationFailures;
static const xbytesview entries[]={
    {X509_PATH_ROOT,sizeof(X509_PATH_ROOT)},
    {X509_PATH_INTERMEDIATE,sizeof(X509_PATH_INTERMEDIATE)},
    {X509_PATH_LEAF,sizeof(X509_PATH_LEAF)}
};
static void balanced(xmemdebugsnapshot before)
{
    xmemdebugsnapshot after;xrtMemDebugSnapshot(&after);
    testRequire(before.LiveCount==after.LiveCount && before.LiveBytes==after.LiveBytes &&
        after.AllocCount-before.AllocCount==after.FreeCount-before.FreeCount &&
        before.InvalidFreeCount==after.InvalidFreeCount && before.DoubleFreeCount==after.DoubleFreeCount &&
        before.UseAfterFreeCount==after.UseAfterFreeCount,"snapshot allocation balance");
}
static bool equal(xbytesview a,xbytesview b)
{return a.Size==b.Size && (!a.Size || !memcmp(a.Data,b.Data,a.Size));}
static bool contained(xbytesview raw,xbytesview view)
{
    uintptr_t begin=(uintptr_t)raw.Data,at=(uintptr_t)view.Data;
    return !view.Size || (at>=begin && at-begin<=raw.Size && view.Size<=raw.Size-(at-begin));
}
static void check(const xx509store* store,size_t count)
{
    testRequire(xrtX509StoreCount(store)==count,"snapshot count");
    for(size_t i=0;i<count;++i){
        const xx509cert* certificate=xrtX509StoreCertificate(store,i);
        const xx509anchor* anchor=xrtX509StoreAnchor(store,i);
        testRequire(certificate && anchor && equal(certificate->Raw,entries[i]) &&
            certificate->Raw.Data!=entries[i].Data && equal(anchor->Certificate,certificate->Raw),"snapshot owned DER");
        xbytesview raw=certificate->Raw;
        testRequire(contained(raw,certificate->Tbs) && contained(raw,certificate->Serial) &&
            contained(raw,certificate->Issuer) && contained(raw,certificate->Subject) &&
            contained(raw,certificate->SubjectPublicKeyInfo) && contained(raw,certificate->Extensions) &&
            contained(raw,certificate->Signature) && contained(raw,certificate->TbsSignature.Raw) &&
            contained(raw,certificate->TbsSignature.Oid) && contained(raw,certificate->TbsSignature.Parameters) &&
            contained(raw,certificate->SignatureAlgorithm.Raw) && contained(raw,certificate->SignatureAlgorithm.Oid) &&
            contained(raw,certificate->SignatureAlgorithm.Parameters) && contained(raw,anchor->Name),"snapshot interior views");
    }
    testRequire(!xrtGetError(),"snapshot query error");
}
static xx509store* create(size_t count)
{
    xx509store* store=xrtX509StoreCreate();testRequire(store!=NULL,"store create");
    for(size_t i=0;i<count;++i)
        testRequire(xrtX509StoreAdd(store,entries[i].Data,entries[i].Size)==X509_VALUE,"store input");
    return store;
}
static void independence(void)
{
    xx509store* source=create(1),*all=create(3),*copy=xrtX509StoreClone(source);
    testRequire(copy!=NULL,"clone root");
    const xx509cert* held=xrtX509StoreCertificate(copy,0);
    const xx509cert* original=xrtX509StoreCertificate(source,0);
    testRequire(held->Raw.Data!=original->Raw.Data && held->Subject.Data!=original->Subject.Data,"clone distinct storage");
    size_t added=99;
    testRequire(xrtX509StoreMerge(source,all,&added) && added==2,"merge new entries");
    check(source,3);check(copy,1);
    testRequire(xrtX509StoreCertificate(copy,0)==held,"source write invalidated clone");
    xrtX509StoreFree(source);xrtX509StoreFree(all);check(copy,1);
    xx509pathsource pathSource;
    testRequire(xrtX509StoreSource(copy,NULL,0,&pathSource) && pathSource.AnchorCount==1 &&
        equal(pathSource.Anchors[0].Certificate,entries[0]),"independent anchor source");
    xx509store* second=xrtX509StoreClone(copy);testRequire(second!=NULL,"clone clone");
    xrtX509StoreFree(copy);check(second,1);xrtX509StoreFree(second);
    source=create(0);copy=xrtX509StoreClone(source);testRequire(copy!=NULL,"empty clone");
    check(copy,0);xrtX509StoreFree(source);xrtX509StoreFree(copy);
}
static void cloneFaults(size_t count)
{
    xx509store* source=create(count);bool completed=false;
    for(size_t prefix=0;prefix<128;++prefix){
        xmemdebugsnapshot before;xrtMemDebugSnapshot(&before);
        testRequire(xrtMemDebugFailAfter(prefix),"clone budget");
        xx509store* copy=xrtX509StoreClone(source);bool hit=xrtMemDebugFailTriggered();
        xrtMemDebugFailClear();++cloneBudgets;
        if(hit){testRequire(!copy && xrtErrorKind(xrtGetError())==XERR_MEMORY,"clone failure publication");++allocationFailures;}
        else{testRequire(copy!=NULL && !xrtGetError(),"clone success");completed=true;check(copy,count);}
        xrtClearError();xrtX509StoreFree(copy);check(source,count);balanced(before);
        if(completed)break;
    }
    testRequire(completed,"clone fault prefix exhausted");xrtX509StoreFree(source);
}
static void mergeFaults(size_t initial)
{
    xx509store* source=create(3);bool completed=false;
    for(size_t prefix=0;prefix<128;++prefix){
        xmemdebugsnapshot before;xrtMemDebugSnapshot(&before);
        xx509store* destination=create(initial);size_t added=123;
        testRequire(xrtMemDebugFailAfter(prefix),"merge budget");
        bool ok=xrtX509StoreMerge(destination,source,&added),hit=xrtMemDebugFailTriggered();
        xrtMemDebugFailClear();++mergeBudgets;
        if(hit){
            testRequire(!ok && added==123 && xrtErrorKind(xrtGetError())==XERR_MEMORY,"merge failure publication");
            ++allocationFailures;xrtClearError();check(destination,initial);
        }else{testRequire(ok && added==3-initial && !xrtGetError(),"merge success");check(destination,3);completed=true;}
        check(source,3);xrtX509StoreFree(destination);balanced(before);if(completed)break;
    }
    testRequire(completed,"merge fault prefix exhausted");xrtX509StoreFree(source);
}
static void argumentsAndDuplicates(void)
{
    xx509store* store=create(3),*copy=xrtX509StoreClone(store);testRequire(copy!=NULL,"duplicate fixture");
    size_t added=123;
    testRequire(!xrtX509StoreClone(NULL) && xrtErrorKind(xrtGetError())==XERR_ARGUMENT,"clone missing source");
    xrtClearError();
    testRequire(!xrtX509StoreMerge(store,NULL,&added) && added==123 && xrtErrorKind(xrtGetError())==XERR_ARGUMENT,"merge missing source");
    xrtClearError();
    testRequire(!xrtX509StoreMerge(NULL,store,&added) && added==123 && xrtErrorKind(xrtGetError())==XERR_ARGUMENT,"merge missing destination");
    xrtClearError();testRequire(xrtMemDebugFailAfter(0),"duplicate no-allocation budget");
    testRequire(xrtX509StoreMerge(store,copy,&added) && added==0 &&
        xrtX509StoreMerge(store,store,NULL) && !xrtMemDebugFailTriggered() && !xrtGetError(),"duplicate merge allocated");
    xrtMemDebugFailClear();check(store,3);check(copy,3);
    xrtSetErrorKind(XERR_STATE);const xerror* prior=xrtGetError();
    xx509store* third=xrtX509StoreClone(copy);
    testRequire(third && xrtGetError()==prior && xrtX509StoreMerge(third,copy,NULL) &&
        xrtGetError()==prior,"successful operation consumed unrelated diagnostic");
    xrtClearError();check(third,3);xrtX509StoreFree(third);xrtX509StoreFree(copy);xrtX509StoreFree(store);
}
typedef struct Reader {const xx509store* source;bool done;} Reader;
static void readCopies(Reader* reader)
{
    for(size_t i=0;i<32;++i){
        xx509store* copy=xrtX509StoreClone(reader->source);testRequire(copy!=NULL,"parallel clone");
        check(copy,3);testRequire(xrtX509StoreMerge(copy,reader->source,NULL),"parallel private merge");
        xrtX509StoreFree(copy);testRequire(!xrtGetError(),"parallel clone cleanup");
    }
    reader->done=true;
}
#if defined(_WIN32)
static DWORD WINAPI readThread(LPVOID raw){readCopies(raw);return 0;}
#else
static void* readThread(void* raw){readCopies(raw);return NULL;}
#endif
static void readers(void)
{
    xx509store* source=create(3);Reader inputs[4];
#if defined(_WIN32)
    HANDLE threads[4];
#else
    pthread_t threads[4];
#endif
    for(size_t i=0;i<4;++i){
        inputs[i]=(Reader){source,false};
#if defined(_WIN32)
        threads[i]=CreateThread(NULL,0,readThread,&inputs[i],0,NULL);testRequire(threads[i]!=NULL,"reader create");
#else
        testRequire(!pthread_create(&threads[i],NULL,readThread,&inputs[i]),"reader create");
#endif
    }
    for(size_t i=0;i<4;++i){
#if defined(_WIN32)
        testRequire(WaitForSingleObject(threads[i],INFINITE)==WAIT_OBJECT_0 && CloseHandle(threads[i]),"reader join");
#else
        testRequire(!pthread_join(threads[i],NULL),"reader join");
#endif
        testRequire(inputs[i].done,"reader completion");
    }
    check(source,3);xrtX509StoreFree(source);
}
int main(void)
{
    xmemdebugsnapshot before;xrtMemDebugSnapshot(&before);
    independence();cloneFaults(0);cloneFaults(1);cloneFaults(3);mergeFaults(0);mergeFaults(1);
    argumentsAndDuplicates();readers();balanced(before);
    testRequire(xrtMemDebugReset(),"snapshot memory drain");
    printf("[PASS] x509_store_snapshot: clone_budgets=%zu merge_budgets=%zu allocation_failures=%zu; 4x32 readers; balanced\n",
        cloneBudgets,mergeBudgets,allocationFailures);return 0;
}
