"""Inspect the actual server job immediately before its real free.

Build xrpc_server_tests first. Only this probe TU interposes allocation/free;
the shipped allocator and production source have no test hook. Core/native
storage, caller copies and retained business results are outside the wipe scope.
"""
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys

root = Path.cwd()
out = root/'out/service-extensions/xrpc-server-context'
name, arch, suffix = ('windows', 'native', '.exe') if os.name == 'nt' else ('wsl', 'x64', '')
sys.path.insert(0, str(root/'tools'))
from test_protocol_fuzz import _closure
sha = lambda p: hashlib.sha256(p.read_bytes()).hexdigest()
source = out/(name+'-context-wipe.c')
template = r'''
#include <xrt.h>
#include <xrpc.h>
#include <xrt/memory.h>
#define main unusedServerTestMain
#include "@TEST@"
#undef main
static ptr contextMalloc(size_t bytes);
static void contextFree(ptr p);
#define xrtMalloc contextMalloc
#define xrtFree contextFree
#include "@SERVER@"
#undef xrtMalloc
#undef xrtFree
static xatomicptr owned;
static xatomic32 capture,constructing,wiped,partial,empty;
static size_t allocated,plainBytes,contextBytes;
static ptr contextMalloc(size_t bytes)
{
    ptr p=xrtMalloc(bytes);
    if(p && xrtAtomic32Load(&capture,XMEMORY_ACQUIRE)){
        testRequire(!xrtAtomicPtrLoad(&owned,XMEMORY_ACQUIRE),"overlapping captured job");
        allocated=bytes;xrtAtomicPtrStore(&owned,p,XMEMORY_RELEASE);
    }
    return p;
}
static void contextFree(ptr p)
{
    if(p && p==xrtAtomicPtrLoad(&owned,XMEMORY_ACQUIRE)){
        xrpcserverjob* j=p;xbytesview context=xrpcServerContextView(j);
        if(context.Size){
            for(size_t k=0;k<context.Size;++k)testRequire(!context.Data[k],"context not zero immediately before real free");
            xrtAtomic32FetchAdd(&wiped,1,XMEMORY_RELEASE);
            if(xrtAtomic32Load(&constructing,XMEMORY_ACQUIRE))xrtAtomic32FetchAdd(&partial,1,XMEMORY_RELEASE);
        }else{testRequire(!context.Data,"empty context retained a source pointer");xrtAtomic32FetchAdd(&empty,1,XMEMORY_RELEASE);}
        xrtAtomicPtrStore(&owned,NULL,XMEMORY_RELEASE);
    }
    xrtFree(p);
}
int main(void)
{
    xrtAtomicPtrInit(&owned,NULL);xrtAtomic32Init(&capture,0);xrtAtomic32Init(&constructing,0);xrtAtomic32Init(&wiped,0);xrtAtomic32Init(&partial,0);xrtAtomic32Init(&empty,0);
    unsigned successes=0,failures=0;
    const size_t limits[]={1,128,256,512,1024,2048,4096,8192};
    for(unsigned mode=0;mode<5+sizeof(limits)/sizeof(*limits);++mode){
        serverTestContext c;serverTestInit(&c);xrpcserverconfig k;xrpcServerConfigInit(&k);k.Executor.Threads=1;xrpcserver* s=serverTestCreate(&c,&k);
        xfuture* hold=serverTestSubmit(s,"{\"jsonrpc\":\"2.0\",\"id\":0,\"method\":\"hold\"}",5000);
        double end=xrtTimer()+10;while(!xrtAtomic32Load(&c.Entered,XMEMORY_ACQUIRE)){testRequire(xrtTimer()<end,"context probe gate timeout");xrtSleep(1);}
        unsigned char data[]={0,1,2,3,255,128,7,0};xallocationbudget* budget=NULL;xallocationscope scope={0};
        if(mode>=5)testRequire(xrtAllocationBudgetCreate(limits[mode-5],&budget) && xrtAllocationBudgetEnter(budget,&scope),"context probe budget enter");
        xrtAtomic32Store(&capture,1,XMEMORY_RELEASE);xrtAtomic32Store(&constructing,1,XMEMORY_RELEASE);
        xfuture* f=NULL;xrpcserveradmission a=xrpcServerSubmitWithContext(s,XRT_STR_LITERAL("{\"jsonrpc\":\"2.0\",\"id\":1,\"method\":\"echo\"}"),(xbytesview){data,mode?sizeof(data):0},mode==4?0:5000,&f);
        xrtAtomic32Store(&constructing,0,XMEMORY_RELEASE);xrtAtomic32Store(&capture,0,XMEMORY_RELEASE);
        if(budget)testRequire(xrtAllocationBudgetLeave(&scope),"context probe budget leave");
        if(a==XRPC_SERVER_ACCEPTED){++successes;if(mode==0)plainBytes=allocated;if(mode==1)contextBytes=allocated;
            memset(data,'x',sizeof(data));if(mode==2)testRequire(xrtFutureCancel(f),"probe queued context cancel");if(mode==3)testRequire(xrpcServerCancel(s),"probe server context cancel");
        }else{++failures;testRequire(mode>=5 && a==XRPC_SERVER_ADMISSION_ERROR && !f,"context probe failed constructor output");xrtClearError();}
        xrtAtomic32Store(&c.Permit,1,XMEMORY_RELEASE);(void)serverTestResult(hold);xrtFutureDestroy(hold);
        if(f){xrpcserverstatus expected=mode==4?XRPC_SERVER_TIMEOUT:mode==2 || mode==3?XRPC_SERVER_CANCELLED:XRPC_SERVER_DONE;
            testRequire(serverTestResult(f)->Status==expected,"context probe terminal outcome");xrtFutureDestroy(f);}
        serverTestZero(s,0);testRequire(xrpcServerDestroy(s) && !xrtAtomicPtrLoad(&owned,XMEMORY_ACQUIRE),"context job physically retained");
        if(budget){xallocationbudgetinfo i={0};i.Size=sizeof(i);testRequire(xrtAllocationBudgetInfo(budget,&i) && !i.LiveBytes,"context probe charged bytes leaked");xrtAllocationBudgetFree(budget);}
    }
    testRequire(successes && failures && xrtAtomic32Load(&partial,XMEMORY_ACQUIRE) && xrtAtomic32Load(&wiped,XMEMORY_ACQUIRE)>=4 && xrtAtomic32Load(&empty,XMEMORY_ACQUIRE)==1,"context probe missed lifetime/OOM cases");
    testRequire(contextBytes>=plainBytes+8 && contextBytes-plainBytes<8+_Alignof(max_align_t),"context copied outside single aligned job allocation");
    printf("wiped_jobs: %u\nconstructor_failure_wipes: %u\nplain_job_bytes: %zu\ncontext_job_bytes: %zu\n",xrtAtomic32Load(&wiped,XMEMORY_ACQUIRE),xrtAtomic32Load(&partial,XMEMORY_ACQUIRE),plainBytes,contextBytes);
    puts("[PASS] actual server context pre-free wipe: success/cancel/server-drop/deadline/constructor-OOM; one aligned job allocation; zero charged owner bytes");return 0;
}
'''
for token, path in [('TEST', root/'extlibs/xrpc/tests/test_server.c'), ('SERVER', root/'extlibs/xrpc/src/server/server.c')]:
    template = template.replace('@'+token+'@', path.as_posix())
source.write_text(template, encoding='utf-8')
sources, defines, links, _ = _closure('xrpc_server_tests', 'extlibs/xrpc/config/modules.json')
objects = sorted(p for p in (root/'out/gcc'/arch/'xrpc_server_tests/obj').glob('*.o') if p.name != 'extlibs__xrpc__src__server__server.o')
assert objects, 'Build xrpc_server_tests first'
inputs = [root/p for p in sources] + list((root/'include').rglob('*.h')) + list((root/'src').rglob('*.h'))
inputs += list((root/'extlibs/xrpc').rglob('*.h')) + [root/'extlibs/xrpc/tests/test_server.c', root/'tests/test.h', Path(__file__), root/'config/modules.json', root/'extlibs/xrpc/config/modules.json']
hashes = {p.relative_to(root).as_posix(): sha(p) for p in inputs}
binary = out/'bin'/(name+'-context-wipe'+suffix)
command = [shutil.which('gcc'), '-std=c11', '-O2', '-Wall', '-Wextra', '-Werror', *['-D'+d for d in defines], '-Iinclude', '-Iextlibs/xrpc/include', str(source), *map(str, objects), *['-l'+l for l in links], *(['-fsanitize=address,undefined'] if os.name != 'nt' else []), '-o', str(binary)]
with (out/(name+'-wipe-build.log')).open('w', encoding='utf-8') as log:
    subprocess.run(command, stdout=log, stderr=subprocess.STDOUT, check=True, timeout=180)
env = os.environ.copy(); env.update(ASAN_OPTIONS='detect_leaks=1:halt_on_error=1', UBSAN_OPTIONS='halt_on_error=1')
r = subprocess.run([str(binary)], text=True, capture_output=True, timeout=180, env=env)
(out/(name+'-wipe-run.log')).write_text(r.stdout+r.stderr, encoding='utf-8')
assert r.returncode == 0, r.stdout+r.stderr
assert hashes == {p:sha(root/p) for p in hashes}, 'inputs changed during probe'
(out/(name+'-wipe.json')).write_text(json.dumps({'command': command, 'inputs': hashes, 'objects': {p.relative_to(root).as_posix():sha(p) for p in objects}, 'binary_sha256':sha(binary), 'exit_code':r.returncode, 'stdout':r.stdout}, indent=2)+'\n', encoding='utf-8')
print(r.stdout.strip())
