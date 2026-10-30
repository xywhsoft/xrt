"""Inject credit return at the public mutex release in real 100-continue code.

Build the modular xrpc_http_server_tls_tests suite first. --before uses the pinned
predecessor server source and expects the missed wakeup. The probe calls the real
credit-return function and intercepts only its post attempt; no product hook.
"""
import argparse,hashlib,json,os,shutil,subprocess,sys
from pathlib import Path
root=Path.cwd();parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--before',action='store_true');args=parser.parse_args()
out=root/'out/service-extensions/xrpc-http-server-tls';out.mkdir(parents=True,exist_ok=True)
name,arch,suffix=('windows','native','.exe') if os.name=='nt' else ('wsl','x64','')
sys.path.insert(0,str(root/'tools'));from test_protocol_fuzz import _closure;from build import _run_compiler
sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest();target=root/'extlibs/xrpc/src/http_server/http_server.c'
if args.before:
    git=['git'];marker=root/'.git'
    if os.name!='nt' and marker.is_file():
        directory=marker.read_text(encoding='utf-8').strip().removeprefix('gitdir: ')
        if len(directory)>2 and directory[1]==':':
            directory='/mnt/'+directory[0].lower()+directory[2:].replace('\\','/')
            git+=['--git-dir',directory]
    raw=subprocess.check_output(git+['show','514f14203eb3a414d6b34bffff543b202428bd3f:extlibs/xrpc/src/http_server/http_server.c'])
    target=out/(name+'-continue-before-server.c');target.write_text(raw.decode('utf-8').replace('#include "../internal/','#include "'+(root/'extlibs/xrpc/src/internal').as_posix()+'/'),encoding='utf-8')
template=r'''
#include <xrt.h>
#include <xrpc.h>
#include <stdio.h>
static bool armed;
static unsigned kicks;
static void* server;
static void probeUnlock(xmutex* gate);
static bool probePost(xnetworker* w,xnetpost* p,void (*fn)(xnetworker*,ptr),ptr d);
#define xrtMutexUnlock probeUnlock
#define xrtNetPost probePost
#include "@SOURCE@"
#undef xrtMutexUnlock
#undef xrtNetPost
static void probeUnlock(xmutex* gate)
{
    xrtMutexUnlock(gate);
    if(armed && gate==&((xrpchttpserver*)server)->Gate){armed=false;xrpcHsCredit(server,25);}
}
static bool probePost(xnetworker* w,xnetpost* p,void (*fn)(xnetworker*,ptr),ptr d)
{(void)w;(void)p;(void)fn;(void)d;++kicks;return false;}
int main(void)
{
    xrpchttpserver* h=xrtCalloc(1,sizeof(*h)+sizeof(xrpchsshard));
    if(!h || !xrtMutexInit(&h->Gate) || !xrtCondInit(&h->Idle))return 2;
    xrtAtomic32Init(&h->References,1);h->ShardCount=1;h->Config.MaxQueuedBytes=25;h->Info.QueuedBytes=25;
    h->Shards[0].Server=h;xrtAtomic32Init(&h->Shards[0].Posted,0);xrtNetPostInit(&h->Shards[0].Post);
    xrpchspeer p={0};p.Server=h;p.Shard=&h->Shards[0];server=h;armed=true;
    xrpcHsContinue(&p);
    bool ok=kicks==1 && !h->Info.QueuedBytes && p.Waiting;
    xrpcHsUnwait(&p);ok=ok && !h->WaitingWriters && xrtAtomic32Load(&h->References,XMEMORY_ACQUIRE)==1;
    xrtCondUnit(&h->Idle);xrtMutexUnit(&h->Gate);xrtFree(h);
    puts(ok?"[PASS] interim waiter visible before real credit return":"[FAIL] interim waiter missed real credit return");return ok?0:1;
}
'''
tag=name+'-continue-credit'+('-before' if args.before else '')
source=out/(tag+'.c');source.write_text(template.replace('@SOURCE@',target.as_posix()),encoding='utf-8')
_,defines,links,_=_closure('xrpc_http_server_tests' if args.before else 'xrpc_http_server_tls_tests','extlibs/xrpc/config/modules.json')
excluded={'extlibs__xrpc__src__http_server__http_server.o'}
if args.before:excluded.add('extlibs__xrpc__src__http_server_tls__http_server_tls.o')
objects=sorted(p for p in (root/'out/gcc'/arch/'xrpc_http_server_tls_tests/obj').glob('*.o') if p.name not in excluded);assert objects
inputs={p.relative_to(root).as_posix():sha(p) for p in objects}
for base in ['include','src','extlibs/xrpc']:
    for p in (root/base).rglob('*.h'):inputs[p.relative_to(root).as_posix()]=sha(p)
for p in [source,target,Path(__file__).resolve()]:inputs[p.relative_to(root).as_posix()]=sha(p)
binary=out/'bin'/(tag+suffix);command=[shutil.which('gcc'),'-std=c11','-O2','-Wall','-Wextra','-Werror',*['-D'+v for v in defines],'-Iinclude','-Iextlibs/xrpc/include',str(source),*map(str,objects),*['-l'+v for v in links],*(['-fsanitize=address,undefined'] if os.name!='nt' else []),'-o',str(binary)]
with (out/(tag+'-build.log')).open('w',encoding='utf-8') as log:
    prior=sys.stdout;sys.stdout=log
    try:_run_compiler(command,out/(tag+'.rsp'))
    finally:sys.stdout=prior
env=os.environ.copy();env.update(ASAN_OPTIONS='detect_leaks=1:halt_on_error=1',UBSAN_OPTIONS='halt_on_error=1')
r=subprocess.run([str(binary)],text=True,capture_output=True,env=env,timeout=30);(out/(tag+'-run.log')).write_text(r.stdout+r.stderr,encoding='utf-8')
assert r.returncode==(1 if args.before else 0),(r.returncode,r.stdout,r.stderr)
assert ('[FAIL] interim waiter missed' if args.before else '[PASS] interim waiter visible') in r.stdout
assert all(sha(root/p)==h for p,h in inputs.items()),'inputs changed'
(out/(tag+'.json')).write_text(json.dumps({'inputs':inputs,'command':command,'binary_sha256':sha(binary),'expected_exit':1 if args.before else 0,'exit_code':r.returncode},indent=2)+'\n',encoding='utf-8')
print(r.stdout.strip())
