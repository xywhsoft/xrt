"""Deterministically close between dial completion's state read and publication.

Run from repository root after the modular xrpc_tests build. The probe replaces
only product-client calls to the public mutex entry, links the other actual test
objects, and includes the production client source. No production hook is added.
"""
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys

root = Path.cwd()
out = root / 'out/service-extensions/xrpc-http-dial'
name, arch, suffix = ('windows', 'native', '.exe') if os.name == 'nt' else ('wsl', 'x64', '')
sys.path.insert(0, str(root / 'tools'))
from test_protocol_fuzz import _closure

template = r'''
#include "test.h"
#include "@SERVICE@"
typedef struct raceInjection {xrpchttpclient* Client;xmutex* Gate;unsigned Hits;} raceInjection;
static _Thread_local raceInjection* Inject;
static bool raceLock(xmutex* Gate)
{
    if(Inject && Inject->Gate==Gate && ++Inject->Hits==2){
        raceInjection* Saved=Inject;Inject=NULL;
        testRequire(xrpcHttpClose(Saved->Client),"injected close");Inject=Saved;
    }
    return xrtMutexLock(Gate);
}
#define xrtMutexLock raceLock
#include "@CLIENT@"
#undef xrtMutexLock
typedef struct raceWork {xrpchttpclient* Client;xnetpost Post;xatomic32 Done;} raceWork;
static void raceRun(xnetworker* Worker,ptr Data)
{
    (void)Worker;raceWork* W=Data;raceInjection I={W->Client,&W->Client->Gate,0};Inject=&I;
    xrpcHttpConnected(W->Client,XNET_RESULT_OK,W->Client->Stream,NULL);Inject=NULL;
    xrpchttpinfo Info={0};Info.Size=sizeof(Info);
    testRequire(I.Hits>=2 && xrpcHttpInfo(W->Client,&Info) && Info.State>=XRPC_HTTP_CLOSING,
        "close request overwritten by dial completion");
    xrtAtomic32Store(&W->Done,1,XMEMORY_RELEASE);
}
int main(void)
{
    rpcService S;rpcServiceStart(&S,NULL);xrpchttpclient* C=rpcServiceClient(&S,NULL);double End=xrtTimer()+10;
    for(;;){xrpchttpinfo I={0};I.Size=sizeof(I);testRequire(xrpcHttpInfo(C,&I),"race open info");
        if(I.State==XRPC_HTTP_OPEN)break;
        testRequire(xrtTimer()<End,"race connect timeout");xrtSleep(1);}
    raceWork W={0};W.Client=C;xrtAtomic32Init(&W.Done,0);
    testRequire(xrtNetPostInit(&W.Post) && xrtNetPost(xrtNetEngineWorker(S.Engine,1),&W.Post,raceRun,&W),"race post");
    while(!xrtAtomic32Load(&W.Done,XMEMORY_ACQUIRE)){testRequire(xrtTimer()<End,"race callback timeout");xrtSleep(1);}
    rpcServiceClientClose(C);rpcServiceStop(&S);puts("[PASS] dial completion preserves concurrent close");return 0;
}
'''

source = out / (name + '-close-race.c')
source.write_text(template.replace('@SERVICE@', (root / 'extlibs/xrpc/examples/common/http_service.h').as_posix())
                  .replace('@CLIENT@', (root / 'extlibs/xrpc/src/http_client/http_client.c').as_posix()), encoding='utf-8')
_, defines, links, _ = _closure('xrpc_tests', 'extlibs/xrpc/config/modules.json')
objects = sorted(p for p in (root / 'out/gcc' / arch / 'xrpc_tests/obj').glob('*.o')
                 if p.name != 'extlibs__xrpc__src__http_client__http_client.o')
assert objects
sha = lambda p: hashlib.sha256(p.read_bytes()).hexdigest()
inputs = {p.relative_to(root).as_posix(): sha(p) for p in objects}
for base in ['include', 'src', 'extlibs/xrpc']:
    for p in (root / base).rglob('*.h'):
        inputs[p.relative_to(root).as_posix()] = sha(p)
for p in [Path(__file__).resolve(), root / 'extlibs/xrpc/src/http_client/http_client.c', source]:
    inputs[p.relative_to(root).as_posix()] = sha(p)
binary = out / (name + '-close-race' + suffix)
command = [shutil.which('gcc'), '-std=c11', '-O2', '-Wall', '-Wextra', '-Werror',
           *['-D' + v for v in defines], '-Iinclude', '-Iextlibs/xrpc/include', '-Itests',
           str(source), *map(str, objects), *['-l' + v for v in links],
           *(['-fsanitize=address,undefined', '-fno-omit-frame-pointer'] if os.name != 'nt' else []),
           '-o', str(binary)]
with (out / (name + '-close-race-build.log')).open('w', encoding='utf-8') as log:
    subprocess.run(command, stdout=log, stderr=subprocess.STDOUT, check=True, timeout=180)
env = os.environ.copy()
env.update(ASAN_OPTIONS='detect_leaks=1:halt_on_error=1', UBSAN_OPTIONS='halt_on_error=1')
result = subprocess.run([str(binary)], env=env, text=True, capture_output=True, timeout=30)
(out / (name + '-close-race-run.log')).write_text(result.stdout + result.stderr, encoding='utf-8')
assert all(sha(root / p) == h for p, h in inputs.items()), 'probe inputs changed'
(out / (name + '-close-race.json')).write_text(json.dumps({'command': command, 'inputs': inputs,
    'binary_sha256': sha(binary), 'exit_code': result.returncode}, indent=2) + '\n', encoding='utf-8')
print(result.stdout + result.stderr)
raise SystemExit(result.returncode)
