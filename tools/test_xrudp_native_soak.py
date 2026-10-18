"""Real wall-clock native UDP, five streams, messages, game datagrams and idle soak.

Uses synthetic loopback identities, bounded private processes and the public API.
No shortened run, process restart or suspended clock can pass the 72h gate.
"""
from pathlib import Path
import argparse
import ctypes
from ctypes import wintypes
import hashlib
import json
import os
import queue
import select
import socket
import subprocess
import threading
import time

ROOT=Path(__file__).resolve().parents[1]


class FaultProxy:
    """Bounded bidirectional real UDP loss/duplicate/reorder, no product codec."""
    def __init__(self,family):
        self.host='127.0.0.1' if family==4 else '::1'; self.sockets=[]; self.thread=None; self.error=''
        self.stop=threading.Event(); self.active=False; self.closing=False; self.pending=[]; self.index=0
        self.stats=dict(received=0,forwarded=0,dropped=0,duplicated=0,delayed=0,peak_pending=0,icmp_errors=0)
        for _ in range(2):
            sock=socket.socket(socket.AF_INET if family==4 else socket.AF_INET6,socket.SOCK_DGRAM)
            sock.bind((self.host,0)); sock.setblocking(False)
            self.sockets.append(sock)
    @property
    def port(self): return self.sockets[0].getsockname()[1]
    def snapshot(self): return {**self.stats,'pending':len(self.pending),'error':self.error}
    def start(self,ports):
        assert self.thread is None and len(ports)==2 and all(isinstance(p,int) and 0<p<65536 for p in ports)
        def send(sock,data,port):
            assert sock.sendto(data,(self.host,port))==len(data),'proxy short UDP output'
            self.stats['forwarded']+=1
        def pump():
            try:
                while not self.stop.is_set():
                    now=time.monotonic()
                    for item in list(self.pending):
                        if item[0]<=now: self.pending.remove(item); send(*item[1:])
                    readable,_,_=select.select(self.sockets,[],[],.01)
                    for source in readable:
                        side=self.sockets.index(source)
                        for _ in range(64):
                            try: data,address=source.recvfrom(1201)
                            except BlockingIOError: break
                            except ConnectionResetError:
                                # Windows reports late ICMP for unconnected UDP
                                # after an endpoint closes; this is not a record.
                                self.stats['icmp_errors']+=1; break
                            assert address[1]==ports[1-side] and len(data)<=1200,'unexpected proxy input'
                            self.stats['received']+=1; target=self.sockets[1-side]; port=ports[side]
                            fault=self.active and len(data)>=48 and data[:8]==b'XRUDP001' and data[8] in (4,5)
                            if fault:
                                self.index+=1
                                if self.index%211==0: self.stats['dropped']+=1; continue
                                if self.index%127==0:
                                    assert len(self.pending)<32,'proxy delay capacity exceeded'
                                    self.pending.append((now+.05,target,data,port)); self.stats['delayed']+=1
                                    self.stats['peak_pending']=max(self.stats['peak_pending'],len(self.pending)); continue
                            send(target,data,port)
                            if fault and self.index%307==0: send(target,data,port); self.stats['duplicated']+=1
            except BaseException as exc:
                if not self.stop.is_set(): self.error=type(exc).__name__
        self.thread=threading.Thread(target=pump,daemon=True); self.thread.start()
    def close(self):
        self.stop.set()
        if self.thread: self.thread.join(2); assert not self.thread.is_alive(),'proxy thread not joined'
        for sock in self.sockets: sock.close()


def resources(pid):
    if os.name!='nt':
        lines=Path(f'/proc/{pid}/status').read_text().splitlines()
        values={line.split(':')[0]:line.split(':')[1].strip() for line in lines if ':' in line}
        resident={line.split(':')[0]:line.split(':')[1].strip() for line in Path(f'/proc/{pid}/smaps_rollup').read_text().splitlines() if ':' in line}
        return {'private':sum(int(resident[name].split()[0])*1024 for name in ('Private_Clean','Private_Dirty')),
            'rss':int(values['VmRSS'].split()[0])*1024,'handles':len(list(Path(f'/proc/{pid}/fd').iterdir())),
            'private_metric':'resident_private'}
    class Counters(ctypes.Structure):
        _fields_=[('cb',wintypes.DWORD),('faults',wintypes.DWORD)]+[(name,ctypes.c_size_t) for name in
            ('peak_rss','rss','peak_paged','paged','peak_nonpaged','nonpaged','pagefile','peak_pagefile','private')]
    kernel=ctypes.WinDLL('kernel32',use_last_error=True); psapi=ctypes.WinDLL('psapi',use_last_error=True)
    kernel.OpenProcess.argtypes=[wintypes.DWORD,wintypes.BOOL,wintypes.DWORD]; kernel.OpenProcess.restype=wintypes.HANDLE
    kernel.CloseHandle.argtypes=[wintypes.HANDLE]
    psapi.GetProcessMemoryInfo.argtypes=[wintypes.HANDLE,ctypes.POINTER(Counters),wintypes.DWORD]
    kernel.GetProcessHandleCount.argtypes=[wintypes.HANDLE,ctypes.POINTER(wintypes.DWORD)]
    handle=kernel.OpenProcess(0x400|0x10,False,pid); assert handle,'cannot query owned native process'
    try:
        value=Counters(); value.cb=ctypes.sizeof(value); count=wintypes.DWORD()
        assert psapi.GetProcessMemoryInfo(handle,ctypes.byref(value),value.cb)
        assert kernel.GetProcessHandleCount(handle,ctypes.byref(count))
        return {'private':value.private,'rss':value.rss,'handles':count.value}
    finally: kernel.CloseHandle(handle)


def build(args):
    sdk=args.sdk.resolve(); core=sdk/'lib/xrt.h'; declaration=sdk/'lib/xrt_decl.h'
    source_commit=subprocess.check_output(['git','rev-parse','--verify','HEAD^{commit}'],cwd=ROOT,text=True).strip()
    for local,source in ((core,'single/xrt.h'),(declaration,'single/xrt_decl.h')):
        assert local.read_bytes()==subprocess.check_output(['git','show',source_commit+':'+source],cwd=ROOT),'SDK core must match committed upstream bytes'
    fixture=ROOT/'extlibs/xrudp/tests/test_native_soak.c'
    output=ROOT/'out/xrudp-native-soak'; output.mkdir(parents=True,exist_ok=True)
    binary=output/('native.exe' if os.name=='nt' else 'native')
    command=[args.compiler,'-std=gnu11','-O2','-g','-Wall','-Wextra','-Werror','-DXRT_MODULE_ALL=1','-DXRUDP_MODULE_ALL=1',
        '-I'+str(sdk/'lib'),'-I'+str(ROOT/'extlibs/xrudp/include'),'-I'+str(sdk/'lib/xrtshim')]
    if os.name=='nt': command+=['--param=ggc-min-expand=10','--param=ggc-min-heapsize=4096']
    if args.sanitize: command+=['-fsanitize=address,undefined','-fno-sanitize-recover=all','-fno-omit-frame-pointer']
    command+=[str(fixture),*map(str,sorted((ROOT/'extlibs/xrudp/src').rglob('*.c'))),'-o',str(binary)]
    command+=['-lws2_32','-liphlpapi'] if os.name=='nt' else ['-pthread','-lm','-ldl']
    paths=[core,declaration,fixture]
    for folder,suffix in ((ROOT/'extlibs/xrudp/src','*.c'),(ROOT/'extlibs/xrudp/src','*.h'),
            (ROOT/'extlibs/xrudp/include','*.h'),(sdk/'lib/xrtshim','*.h')):
        paths+=sorted(folder.rglob(suffix))
    for path in paths:
        if path.is_relative_to(ROOT/'extlibs/xrudp/src') or path.is_relative_to(ROOT/'extlibs/xrudp/include'):
            upstream=subprocess.check_output(['git','show',source_commit+':'+path.relative_to(ROOT).as_posix()],cwd=ROOT)
            assert path.read_bytes().replace(b'\r\n',b'\n')==upstream.replace(b'\r\n',b'\n'),'commit runtime changes before an acceptance build'
    if getattr(args,'seconds',0)>=72*3600:
        for path in (fixture,Path(__file__).resolve()):
            committed=subprocess.check_output(['git','show',source_commit+':'+path.relative_to(ROOT).as_posix()],cwd=ROOT)
            assert path.read_bytes().replace(b'\r\n',b'\n')==committed.replace(b'\r\n',b'\n'),'commit the acceptance harness before a 72h run'
    hashes={str(path):hashlib.sha256(path.read_bytes()).hexdigest() for path in paths}
    version=subprocess.check_output([args.compiler,'--version'],text=True).splitlines()[0]
    key=hashlib.sha256(json.dumps([command,version,hashes],sort_keys=True).encode()).hexdigest()
    receipt=binary.with_name(binary.name+'.build.json')
    if args.reuse:
        stored=json.loads(receipt.read_text())
        assert stored['input_key']==key and stored['binary_sha256']==hashlib.sha256(binary.read_bytes()).hexdigest(),'native build provenance changed; rebuild explicitly'
        return binary,{**stored,'verified_source_commit':source_commit}
    print(json.dumps({'build_started':True,'pid':os.getpid(),'compiler':version,'input_key':key}),flush=True)
    subprocess.run(command,cwd=ROOT,check=True)
    assert all(hashlib.sha256(Path(name).read_bytes()).hexdigest()==value for name,value in hashes.items()),'source changed while compiling'
    value={'input_key':key,'compiler':version,'command':command,'source_hashes':hashes,'source_commit':source_commit,
        'binary_sha256':hashlib.sha256(binary.read_bytes()).hexdigest()}
    receipt.write_text(json.dumps(value,indent=2)+'\n',encoding='utf-8',newline='\n')
    return binary,{**value,'verified_source_commit':source_commit}


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--sdk',type=Path,required=True); parser.add_argument('--compiler',default='gcc')
    parser.add_argument('--sanitize',action='store_true'); parser.add_argument('--reuse',action='store_true')
    parser.add_argument('--seconds',type=int,default=72*3600); parser.add_argument('--state',type=Path,required=True)
    parser.add_argument('--family',type=int,choices=(4,6),default=4); parser.add_argument('--backend',choices=('select','native'),default='native')
    parser.add_argument('--idle-every',type=int,default=1800); parser.add_argument('--idle-seconds',type=int,default=70)
    parser.add_argument('--pause-every',type=int,default=120); parser.add_argument('--report-every',type=int,default=60)
    args=parser.parse_args(); assert 20<=args.seconds<=604800
    assert min(args.idle_every,args.idle_seconds,args.pause_every,args.report_every)>0 and args.report_every<=60
    state_path=args.state.resolve(); state_path.parent.mkdir(parents=True,exist_ok=True)
    assert not state_path.exists(),'new receipt path required; never resume interrupted acceptance'
    driver_hash=hashlib.sha256(Path(__file__).read_bytes()).hexdigest()
    binary,provenance=build(args)
    assert driver_hash==hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),'driver changed during native build'
    state={'schema':1,'status':'starting','acceptance_72h':False,'seconds':args.seconds,'family':args.family,'backend':args.backend,
        'pid':os.getpid(),'root_commit':provenance['verified_source_commit'],
        'input_key':provenance['input_key'],'binary_sha256':provenance['binary_sha256'],'started_unix':time.time()}
    state['driver_sha256']=driver_hash
    state['backend_name']=('iocp' if os.name=='nt' else 'epoll') if args.backend=='native' else 'select'
    stop=state_path.with_name('stop'); progress=state_path.with_name('progress.jsonl'); saved=0
    def save(force=False):
        nonlocal saved
        now=time.monotonic()
        if not force and now-saved<60: return
        state['updated_unix']=time.time()
        temporary=state_path.with_suffix('.tmp'); temporary.write_text(json.dumps(state,indent=2)+'\n',encoding='utf-8',newline='\n')
        os.replace(temporary,state_path)
        with progress.open('a',encoding='utf-8') as stream: stream.write(json.dumps(state)+'\n')
        print(json.dumps({name:state.get(name) for name in ('status','native','resources','acceptance_72h')}),flush=True); saved=now
    values=queue.Queue(maxsize=256); child=None; inhibited=False; reader=None; baseline=None; error=None; proxy=None
    try:
        if os.name=='nt' and args.seconds>=3600:
            kernel=ctypes.WinDLL('kernel32',use_last_error=True)
            kernel.SetThreadExecutionState.argtypes=[wintypes.DWORD]; kernel.SetThreadExecutionState.restype=wintypes.DWORD
            assert kernel.SetThreadExecutionState(0x80000001); inhibited=True
        error=state_path.with_name('native.stderr.log').open('w',encoding='utf-8')
        proxy=FaultProxy(args.family)
        argv=[str(binary),str(args.seconds),str(args.family),str(int(args.backend=='native')),str(args.idle_every),
            str(args.idle_seconds),str(args.pause_every),str(args.report_every),str(proxy.port)]
        child=subprocess.Popen(argv,stdin=subprocess.PIPE,stdout=subprocess.PIPE,stderr=error,text=True,env={**os.environ,
            'ASAN_OPTIONS':'detect_leaks=1:halt_on_error=1','UBSAN_OPTIONS':'halt_on_error=1:print_stacktrace=1'},
            creationflags=subprocess.CREATE_NO_WINDOW if os.name=='nt' else 0)
        state['native_pid']=child.pid; state['native_argv']=argv; save(True)
        def read():
            for line in child.stdout:
                try: values.put(line,timeout=2)
                except queue.Full: return
        reader=threading.Thread(target=read,daemon=True); reader.start()
        start=None; wall=None; last=time.monotonic(); last_report=last; terminal=None
        while True:
            now=time.monotonic(); assert now-last<15,'observation execution gap invalidates real-time gate'; last=now
            if stop.exists(): raise InterruptedError('stop requested')
            try: line=values.get(timeout=1)
            except queue.Empty: line=None
            if line:
                value=json.loads(line); last_report=time.monotonic(); state['native']=value
                if value['phase'] in ('running','idle','draining'): state['last_sample']=value
                if value['phase']=='bound':
                    proxy.start(value['ports']); child.stdin.write('GO\n'); child.stdin.flush()
                elif value['phase']=='ready':
                    assert start is None,'persistent native session restarted'; start=time.monotonic(); wall=time.time(); state['status']='running'
                    state['business_started_unix']=wall; proxy.active=True; save(True)
                elif value['phase']=='drain_begin': proxy.closing=True
                elif value['phase']=='terminal': terminal=value; break
            state['proxy']=proxy.snapshot(); assert not proxy.error,'UDP fault proxy failed'
            if child.poll() is not None:
                reader.join(2)
                assert not reader.is_alive(),'native output did not terminate'
                if values.empty(): break
            if start is not None:
                elapsed=time.monotonic()-start
                assert abs((time.time()-wall)-elapsed)<5,'wall/monotonic clock disagreement'
                assert elapsed<args.seconds+45,'native drain deadline exceeded'
                state['elapsed_seconds']=elapsed
            assert time.monotonic()-last_report<(args.report_every+20 if start else 30),'native reporting stopped'
            try: value=resources(child.pid)
            except (OSError,AssertionError):
                if child.poll() is not None: continue
                raise
            state['resources']=value
            assert value['private']<384*1024*1024 and value['handles']<512,'native resource ceiling exceeded'
            if start and time.monotonic()-start>=1800:
                if baseline is None: baseline=value
                assert value['private']-baseline['private']<64*1024*1024 and value['handles']-baseline['handles']<64,'native resource growth exceeded'
            save()
        code=child.wait(timeout=15); reader.join(2); error.close()
        assert code==0 and terminal and terminal['passed'] and terminal['joined'] and terminal['duration_passed'],'native terminal/drain failed'
        assert start is not None and time.monotonic()-start>=args.seconds,'persistent real duration did not pass'
        assert proxy.stats['dropped'] and proxy.stats['duplicated'] and proxy.stats['delayed'],'real fault classes were not exercised'
        sample=state['last_sample']
        assert sample['read']==sample['written'] and min(sample[name] for name in ('read','messages','games','backpressure','rekeys'))>0,'mixed application evidence incomplete'
        state['duration_passed']=True; state['acceptance_72h']=args.seconds>=72*3600
        state['status']='passed' if state['acceptance_72h'] else 'smoke_passed'; state['exit_code']=code; save(True)
        return 0
    except BaseException as exc:
        state['status']='cancelled' if isinstance(exc,InterruptedError) else 'failed'; state['error_type']=type(exc).__name__
        state['acceptance_72h']=False; save(True); print('native soak failed; inspect private native stderr and receipt',flush=True); return 1
    finally:
        if child:
            if child.poll() is None: child.terminate(); child.wait(timeout=10)
            if reader: reader.join(2)
            child.stdout.close(); child.stdin.close()
        if proxy: proxy.close()
        if error: error.close()
        if inhibited: kernel.SetThreadExecutionState(0x80000000)


if __name__=='__main__': raise SystemExit(main())
