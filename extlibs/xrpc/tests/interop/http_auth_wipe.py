"""Production-TU authorization erasure and real allocation-budget probes.

Build xrpc_http_server_tests first. Only this probe TU observes its allocator
calls and scopes child-token construction budgets; no shipped hooks are added.
"""
import hashlib,json,os,shutil,subprocess,sys
from pathlib import Path
root=Path.cwd();out=root/'out/service-extensions/xrpc-http-auth';name,arch,suffix=('windows','native','.exe') if os.name=='nt' else ('wsl','x64','')
sys.path.insert(0,str(root/'tools'));from test_protocol_fuzz import _closure
sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
template=r'''
#include <xrt.h>
#include <xrpc.h>
#include <xrt/memory.h>
#define main unusedAuthUnitMain
#include "@TEST@"
#undef main
#include "@SERVER@"
static ptr spyCalloc(size_t n,size_t bytes);
static void spyFree(ptr p);
static void spyZero(ptr p,size_t n);
static xcancel* spyChild(xrpcserver* s);
#define xrtCalloc spyCalloc
#define xrtFree spyFree
#define xrtSecureZero spyZero
#define xrpcServerCancelChild spyChild
#include "@HTTP@"
#undef xrtCalloc
#undef xrtFree
#undef xrtSecureZero
#undef xrpcServerCancelChild
typedef struct record {ptr P;size_t Bytes;bool Zero;} record;
static record records[128];static xmutex mapGate;
static xatomic32 capture,failAllocation,wiped,allocationFailures,childFailures;
static xatomicptr childBudget;
static ptr spyCalloc(size_t n,size_t bytes)
{
    ptr p;
    if(xrtAtomic32Load(&capture,XMEMORY_ACQUIRE) && xrtAtomic32Exchange(&failAllocation,0,XMEMORY_ACQ_REL)){
        xallocationbudget* b=NULL;xallocationscope scope={0};testRequire(xrtAllocationBudgetCreate(1,&b) && xrtAllocationBudgetEnter(b,&scope),"auth allocation failure scope");
        p=xrtCalloc(n,bytes);testRequire(xrtAllocationBudgetLeave(&scope) && !p,"auth snapshot unexpectedly fit one-byte budget");
        xallocationbudgetinfo i={0};i.Size=sizeof(i);testRequire(xrtAllocationBudgetInfo(b,&i) && !i.LiveBytes,"auth failed allocation ledger");xrtAllocationBudgetFree(b);xrtAtomic32FetchAdd(&allocationFailures,1,XMEMORY_RELEASE);return p;
    }
    p=xrtCalloc(n,bytes);
    if(p && xrtAtomic32Load(&capture,XMEMORY_ACQUIRE)){
        xrtMutexLock(&mapGate);size_t k=0;while(k<128 && records[k].P)++k;testRequire(k<128,"auth probe map full");records[k]=(record){p,n*bytes,false};xrtMutexUnlock(&mapGate);
    }
    return p;
}
static void spyZero(ptr p,size_t bytes)
{
    xrtSecureZero(p,bytes);xrtMutexLock(&mapGate);
    for(size_t k=0;k<128;++k)if(records[k].P==p && records[k].Bytes==bytes)records[k].Zero=true;
    xrtMutexUnlock(&mapGate);
}
static void spyFree(ptr p)
{
    xrtMutexLock(&mapGate);
    for(size_t k=0;k<128;++k)if(p && records[k].P==p){
        if(records[k].Zero){const unsigned char* b=p;for(size_t n=0;n<records[k].Bytes;++n)testRequire(!b[n],"authorization snapshot not zero before real free");xrtAtomic32FetchAdd(&wiped,1,XMEMORY_RELEASE);}
        records[k]=(record){0};break;
    }
    xrtMutexUnlock(&mapGate);xrtFree(p);
}
static xcancel* spyChild(xrpcserver* s)
{
    xallocationbudget* b=xrtAtomicPtrLoad(&childBudget,XMEMORY_ACQUIRE);xallocationscope scope={0};
    if(b)testRequire(xrtAllocationBudgetEnter(b,&scope),"child token budget enter");
    xcancel* c=xrpcServerCancelChild(s);
    if(b){testRequire(xrtAllocationBudgetLeave(&scope),"child token budget leave");if(!c)xrtAtomic32FetchAdd(&childFailures,1,XMEMORY_RELEASE);}
    return c;
}
int main(void)
{
    testRequire(xrtMutexInit(&mapGate),"auth probe map mutex");xrtAtomic32Init(&capture,0);xrtAtomic32Init(&failAllocation,0);xrtAtomic32Init(&wiped,0);xrtAtomic32Init(&allocationFailures,0);xrtAtomic32Init(&childFailures,0);xrtAtomicPtrInit(&childBudget,NULL);
    const size_t limits[]={1,32,64,128,256,512,1024,4096};unsigned allowed=0,refused=0;
    for(unsigned mode=0;mode<3+sizeof(limits)/sizeof(*limits);++mode){
        rpcService s;authTest a;authInit(&a,&s);a.Mode=mode==2?2:0;xrpchttpserverconfig k;xrpcHttpServerConfigInit(&k);k.Authorize=authorizeTest;k.AuthorizationUser=&a;
        rpcServiceStart(&s,&k);serviceRaw r;serviceRawConnect(&s,&r);double end=xrtTimer()+10;
        for(;;){xrpchttpserverinfo i={0};i.Size=sizeof(i);testRequire(xrpcHttpServerInfo(s.Server,&i),"auth probe accepted info");if(i.Accepted==1)break;testRequire(xrtTimer()<end,"auth probe accept timeout");xrtSleep(1);}
        xallocationbudget* budget=NULL;
        if(mode>=3){testRequire(xrtAllocationBudgetCreate(limits[mode-3],&budget),"child budget create");xrtAtomicPtrStore(&childBudget,budget,XMEMORY_RELEASE);}
        uint32 before=xrtAtomic32Load(&wiped,XMEMORY_ACQUIRE);xrtAtomic32Store(&failAllocation,mode==1,XMEMORY_RELEASE);xrtAtomic32Store(&capture,1,XMEMORY_RELEASE);
        cstr input="{\"jsonrpc\":\"2.0\",\"id\":1,\"method\":\"identity\"}";char wire[1024];int n=snprintf(wire,sizeof(wire),"POST /rpc HTTP/1.1\r\nHost: localhost\r\nContent-Type: application/json\r\nContent-Length: %zu\r\nAuthorization: Bearer public-fixture\r\n\r\n%s",strlen(input),input);serviceRawSend(&r,wire,(size_t)n);
        if(mode==2){authWait(&a.Entered,1);serviceRawDestroy(&r);testRequire(xrpcHttpServerClose(s.Server) && xrpcHttpServerWaitFor(s.Server,0)==XWAIT_TIMEOUT,"authorization probe lost physical owner");testRequire(xrtAtomic32Load(&wiped,XMEMORY_ACQUIRE)==before,"authorization wiped while callback still borrowed its head");xrtAtomic32Store(&a.Permit,1,XMEMORY_RELEASE);}
        else{unsigned status;size_t bytes;char body[128];testRequire(serviceRawResponse(&r,&status,body,sizeof(body),&bytes),"auth probe response");testRequire(status==200 || status==503,"auth probe allocation outcome");if(status==200)++allowed;else ++refused;xrpchttpserverinfo info={0};info.Size=sizeof(info);testRequire(xrpcHttpServerInfo(s.Server,&info) && info.AuthorizationRejected==(status==503?1u:0u) && info.AuthorizationAccepted==(status==200?1u:0u),"authorization constructor/admission counter");serviceRawDestroy(&r);}
        rpcServiceStop(&s);xrtAtomic32Store(&capture,0,XMEMORY_RELEASE);xrtAtomicPtrStore(&childBudget,NULL,XMEMORY_RELEASE);
        if(budget){xallocationbudgetinfo i={0};i.Size=sizeof(i);testRequire(xrtAllocationBudgetInfo(budget,&i) && !i.LiveBytes,"authorization child token budget leaked");xrtAllocationBudgetFree(budget);}
        xrtMutexLock(&mapGate);for(size_t p=0;p<128;++p)testRequire(!records[p].P,"authorization tracked owner leaked");xrtMutexUnlock(&mapGate);
    }
    testRequire(allowed && refused && xrtAtomic32Load(&allocationFailures,XMEMORY_ACQUIRE)==1 && xrtAtomic32Load(&childFailures,XMEMORY_ACQUIRE) && xrtAtomic32Load(&wiped,XMEMORY_ACQUIRE)>=10,"authorization probe missed a boundary");xrtMutexUnit(&mapGate);
    printf("wiped_snapshots: %u\nchild_constructor_failures: %u\n",xrtAtomic32Load(&wiped,XMEMORY_ACQUIRE),xrtAtomic32Load(&childFailures,XMEMORY_ACQUIRE));puts("rejection_counters: verified");puts("[PASS] actual authorization pre-free wipe, held callback physical lifetime, one-byte snapshot OOM, child-token quota sweep and zero charged owners");return 0;
}
'''
for key,path in [('TEST',root/'extlibs/xrpc/tests/test_http_server.c'),('SERVER',root/'extlibs/xrpc/src/server/server.c'),('HTTP',root/'extlibs/xrpc/src/http_server/http_server.c')]:template=template.replace('@'+key+'@',path.as_posix())
source=out/(name+'-auth-wipe.c');source.write_text(template,encoding='utf-8')
sources,defines,links,_=_closure('xrpc_http_server_tests','extlibs/xrpc/config/modules.json')
objects=sorted(p for p in (root/'out/gcc'/arch/'xrpc_http_server_tests/obj').glob('*.o') if p.name not in ['extlibs__xrpc__src__server__server.o','extlibs__xrpc__src__http_server__http_server.o'])
assert objects
paths=[root/p for p in sources]+list((root/'include').rglob('*.h'))+list((root/'src').rglob('*.h'))+list((root/'extlibs/xrpc').rglob('*.h'))+[root/'extlibs/xrpc/tests/test_http_server.c',Path(__file__),root/'tests/test.h',root/'config/modules.json',root/'extlibs/xrpc/config/modules.json']
hashes={p.relative_to(root).as_posix():sha(p) for p in paths};binary=out/'bin'/(name+'-auth-wipe'+suffix)
cmd=[shutil.which('gcc'),'-std=c11','-O2','-Wall','-Wextra','-Werror',*['-D'+d for d in defines],'-Iinclude','-Iextlibs/xrpc/include',str(source),*map(str,objects),*['-l'+l for l in links],*(['-fsanitize=address,undefined'] if os.name!='nt' else []),'-o',str(binary)]
with (out/(name+'-wipe-build.log')).open('w',encoding='utf-8') as log:subprocess.run(cmd,stdout=log,stderr=subprocess.STDOUT,check=True,timeout=180)
env=os.environ.copy();env.update(ASAN_OPTIONS='detect_leaks=1:halt_on_error=1',UBSAN_OPTIONS='halt_on_error=1');r=subprocess.run([str(binary)],text=True,capture_output=True,env=env,timeout=180)
(out/(name+'-wipe-run.log')).write_text(r.stdout+r.stderr,encoding='utf-8');assert r.returncode==0,r.stdout+r.stderr;assert hashes=={p:sha(root/p) for p in hashes}
(out/(name+'-wipe.json')).write_text(json.dumps({'command':cmd,'inputs':hashes,'objects':{p.relative_to(root).as_posix():sha(p) for p in objects},'binary_sha256':sha(binary),'exit_code':r.returncode,'stdout':r.stdout},indent=2)+'\n',encoding='utf-8');print(r.stdout.strip())
