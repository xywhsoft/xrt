"""Compile the real HTTP client with pre-free inspection in this TU only.

Build xrpc_http_client_tests first. Native/other-TU buffers are deliberately
outside this probe. No production test hook, freed-memory read or allocator
replacement is added to the library.
"""
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys

root = Path.cwd()
out = root/'out/service-extensions/xrpc-http-headers'
name, arch, suffix = ('windows', 'native', '.exe') if os.name == 'nt' else ('wsl', 'x64', '')
sys.path.insert(0, str(root/'tools'))
from test_protocol_fuzz import _closure
from build import _run_compiler
sha = lambda p: hashlib.sha256(p.read_bytes()).hexdigest()
source = out/(name+'-header-wipe.c')
template = r'''
#include <xrt.h>
#include <xrpc.h>
#include "@PEER@"
#include "@TEST@"
static void probeFree(ptr p);
static ptr probeCalloc(size_t n,size_t bytes);
#define xrtFree probeFree
#define xrtCalloc probeCalloc
#include "@CLIENT@"
#undef xrtFree
#undef xrtCalloc
static xatomicptr client,wire;
static xatomic32 snapshots,wires,encoded_failures,in_factory;
static size_t wirebytes;
static void zeroBytes(const void* p,size_t n)
{const unsigned char* b=p;for(size_t i=0;i<n;++i)testRequire(!b[i],"custom credential bytes were not wiped before free");}
static ptr probeCalloc(size_t n,size_t bytes)
{
    ptr p=xrtCalloc(n,bytes);
    if(p && n==1 && bytes>=sizeof(xrpchttpclient)){
        testRequire(!xrtAtomicPtrLoad(&client,XMEMORY_ACQUIRE),"overlapping probe client allocation");
        xrtAtomicPtrStore(&client,p,XMEMORY_RELEASE);
    }
    return p;
}
static void probeFree(ptr p)
{
    if(p && p==xrtAtomicPtrLoad(&wire,XMEMORY_ACQUIRE)){
        zeroBytes(p,wirebytes);xrtAtomicPtrStore(&wire,NULL,XMEMORY_RELEASE);xrtAtomic32FetchAdd(&wires,1,XMEMORY_RELEASE);
    }
    if(p && p==xrtAtomicPtrLoad(&client,XMEMORY_ACQUIRE)){
        xrpchttpclient* c=p;
        if(c->CustomHeaders.Size){zeroBytes(c->Text,c->OwnedBytes-sizeof(*c));xrtAtomic32FetchAdd(&snapshots,1,XMEMORY_RELEASE);
            if(xrtAtomic32Load(&in_factory,XMEMORY_ACQUIRE))xrtAtomic32FetchAdd(&encoded_failures,1,XMEMORY_RELEASE);}
        xrtAtomicPtrStore(&client,NULL,XMEMORY_RELEASE);
    }
    xrtFree(p);
}
typedef struct gate {xnetpost Post;xatomic32 Entered,Go;} gate;
static void gated(xnetworker* w,ptr data)
{(void)w;gate* g=data;xrtAtomic32Store(&g->Entered,1,XMEMORY_RELEASE);while(!xrtAtomic32Load(&g->Go,XMEMORY_ACQUIRE))xrtSleep(1);}
static void config(xrpchttpconfig* k,xhttpfield* h)
{
    xrpcHttpConfigInit(k);*h=(xhttpfield){XRT_STR_LITERAL("Authorization"),XRT_STR_LITERAL("Bearer public-wipe-fixture")};k->RequestHeaders=h;k->RequestHeaderCount=1;
}
int main(void)
{
    xrtAtomicPtrInit(&client,NULL);xrtAtomicPtrInit(&wire,NULL);xrtAtomic32Init(&snapshots,0);xrtAtomic32Init(&wires,0);xrtAtomic32Init(&encoded_failures,0);xrtAtomic32Init(&in_factory,0);
    for(unsigned mode=0;mode<3;++mode){rpcHttpPeer p;rpcHttpPeerStart(&p);gate g={0};xrtAtomic32Init(&g.Entered,0);xrtAtomic32Init(&g.Go,0);
        testRequire(xrtNetPostInit(&g.Post) && xrtNetPost(xrtNetEngineWorker(p.Engine,1),&g.Post,gated,&g),"header wipe owner gate");rpcHttpWait(&g.Entered,1);
        xrpchttpconfig k;xhttpfield h;config(&k,&h);xrpchttpclient* c=rpcHttpConnect(&p,&k);
        xfuture* f=rpcHttpSubmit(c,mode==2?"drop":"echo","[]",5000);
        xrtMutexLock(&c->Gate);xrpchttprequest* r=c->SubmitHead;testRequire(r && r->Wire && r->WireBytes,"unpublished wire capture");
        wirebytes=r->WireBytes;xrtAtomicPtrStore(&wire,r->Wire,XMEMORY_RELEASE);xrtMutexUnlock(&c->Gate);
        if(mode==1)testRequire(xrtFutureCancel(f),"cancel unpublished credential wire");
        xrtAtomic32Store(&g.Go,1,XMEMORY_RELEASE);const xrpccallresult* result=rpcHttpResult(f);
        testRequire(mode==0?result->Status==XRPC_CALL_OK:mode==1?result->Status==XRPC_CALL_CANCELLED && result->Delivery==XRPC_NOT_SENT:result->Status!=XRPC_CALL_OK,"wipe scenario result");
        rpcHttpClose(&p,c);xrtFutureDestroy(f);testRequire(!xrtAtomicPtrLoad(&client,XMEMORY_ACQUIRE) && !xrtAtomicPtrLoad(&wire,XMEMORY_ACQUIRE),"wipe probe duties remained");
    }
    unsigned success=0,failure=0;const size_t limits[]={1,512,1024,2048,4096,8192,65536,1048576};
    for(size_t i=0;i<sizeof(limits)/sizeof(*limits);++i){rpcHttpPeer p;rpcHttpPeerStart(&p);xallocationbudget* b=NULL;xallocationscope scope={0};
        testRequire(xrtAllocationBudgetCreate(limits[i],&b) && xrtAllocationBudgetEnter(b,&scope),"wipe factory quota enter");xrpchttpconfig k;xhttpfield h;config(&k,&h);xrpchttpclient* c=NULL;
        xrtAtomic32Store(&in_factory,1,XMEMORY_RELEASE);bool ok=xrpcHttpConnect(p.Engine,&p.Address,1,XRT_STR_LITERAL("localhost"),XRT_STR_LITERAL("/rpc"),&k,&c);
        xrtAtomic32Store(&in_factory,0,XMEMORY_RELEASE);testRequire(xrtAllocationBudgetLeave(&scope),"wipe factory quota leave");
        if(ok){++success;rpcHttpClose(&p,c);}else{++failure;testRequire(!c,"wipe failed factory output");rpcHttpPeerStop(&p);}
        xrtClearError();xallocationbudgetinfo info={0};info.Size=sizeof(info);testRequire(xrtAllocationBudgetInfo(b,&info) && !info.LiveBytes,"wipe factory budget retained");xrtAllocationBudgetFree(b);
        testRequire(!xrtAtomicPtrLoad(&client,XMEMORY_ACQUIRE),"wipe factory owner retained");
    }
    testRequire(success && failure && xrtAtomic32Load(&snapshots,XMEMORY_ACQUIRE)>=3 && xrtAtomic32Load(&wires,XMEMORY_ACQUIRE)==3 && xrtAtomic32Load(&encoded_failures,XMEMORY_ACQUIRE)>0,"wipe success/failure coverage");
    puts("[PASS] actual HTTP client pre-free wipe: successful/cancelled/dropped wire, client snapshots and partially constructed snapshots under OOM; zero charged owner bytes");return 0;
}
'''
source.write_text(template.replace('@PEER@', (root/'extlibs/xrpc/examples/common/http_peer.h').as_posix())
                  .replace('@TEST@', (root/'tests/test.h').as_posix())
                  .replace('@CLIENT@', (root/'extlibs/xrpc/src/http_client/http_client.c').as_posix()), encoding='utf-8')
_, defines, links, _ = _closure('xrpc_http_client_tests', 'extlibs/xrpc/config/modules.json')
objects = sorted(p for p in (root/'out/gcc'/arch/'xrpc_http_client_tests/obj').glob('*.o')
                 if p.name != 'extlibs__xrpc__src__http_client__http_client.o')
assert objects
inputs = {p.relative_to(root).as_posix(): sha(p) for p in objects}
for base in ['include', 'src', 'extlibs/xrpc']:
    for p in (root/base).rglob('*.h'):
        inputs[p.relative_to(root).as_posix()] = sha(p)
for p in [source, root/'extlibs/xrpc/src/http_client/http_client.c', Path(__file__).resolve()]:
    inputs[p.relative_to(root).as_posix()] = sha(p)
binary = out/'bin'/(name+'-header-wipe'+suffix)
command = [shutil.which('gcc'), '-std=c11', '-O2', '-Wall', '-Wextra', '-Werror',
           *['-D'+v for v in defines], '-Iinclude', '-Iextlibs/xrpc/include', str(source),
           *map(str, objects), *['-l'+v for v in links],
           *(['-fsanitize=address,undefined'] if os.name != 'nt' else []), '-o', str(binary)]
with (out/(name+'-wipe-build.log')).open('w', encoding='utf-8') as log:
    saved = sys.stdout
    sys.stdout = log
    try:
        _run_compiler(command, out/(name+'-wipe.rsp'))
    finally:
        sys.stdout = saved
env = os.environ.copy()
env.update(ASAN_OPTIONS='detect_leaks=1:halt_on_error=1', UBSAN_OPTIONS='halt_on_error=1')
r = subprocess.run([str(binary)], env=env, capture_output=True, text=True, timeout=120)
(out/(name+'-wipe-run.log')).write_text(r.stdout+r.stderr, encoding='utf-8')
assert r.returncode == 0 and '[PASS]' in r.stdout and not r.stderr, (r.returncode, r.stdout, r.stderr)
assert all(sha(root/p) == h for p, h in inputs.items()), 'probe inputs changed'
(out/(name+'-wipe.json')).write_text(json.dumps({'inputs': inputs, 'command': command,
    'binary_sha256': sha(binary), 'exit_code': r.returncode}, indent=2)+'\n', encoding='utf-8')
print(r.stdout.strip())
