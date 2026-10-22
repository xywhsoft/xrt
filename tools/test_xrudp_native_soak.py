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
import traceback

ROOT=Path(__file__).resolve().parents[1]
_WINDOWS_MEMORY=None


class SoakGateError(AssertionError):
    """A fixed diagnostic code; never serialize arbitrary exception messages."""
    def __init__(self, code):
        super().__init__(code)
        self.code=code


def observe_clock(state, now, previous, started=None, wall=None, wall_now=None):
    clock={}
    if previous is not None:
        gap=now-previous
        clock['tick_gap_seconds']=gap
        state['largest_tick_gap']=max(state.get('largest_tick_gap',0),gap)
    if started is not None:
        clock.update(monotonic_elapsed_seconds=now-started,wall_elapsed_seconds=wall_now-wall)
        clock['offset_seconds']=clock['wall_elapsed_seconds']-clock['monotonic_elapsed_seconds']
    state['clock']=clock
    if previous is not None and not 0<=clock['tick_gap_seconds']<15:
        raise SoakGateError('observation_gap')
    if started is not None and not abs(clock['offset_seconds'])<5:
        raise SoakGateError('clock_diverged')


def record_failure(state, error):
    state.update(status='cancelled' if isinstance(error,InterruptedError) else 'failed',
        acceptance_72h=False,error_type=type(error).__name__,
        failure_code=error.code if isinstance(error,SoakGateError) else 'unclassified')
    state['error_frames']=[{'file':Path(v.filename).name,'line':v.lineno,'function':v.name}
        for v in traceback.extract_tb(error.__traceback__)]


def windows_memory_backend():
    global _WINDOWS_MEMORY
    if _WINDOWS_MEMORY is not None: return _WINDOWS_MEMORY
    class Counters(ctypes.Structure):
        _fields_=[('cb',wintypes.DWORD),('faults',wintypes.DWORD)]+[(name,ctypes.c_size_t) for name in
            ('peak_rss','rss','peak_paged','paged','peak_nonpaged','nonpaged','pagefile','peak_pagefile','private')]
    kernel=ctypes.WinDLL('kernel32',use_last_error=True); psapi=ctypes.WinDLL('psapi',use_last_error=True)
    kernel.OpenProcess.argtypes=[wintypes.DWORD,wintypes.BOOL,wintypes.DWORD]; kernel.OpenProcess.restype=wintypes.HANDLE
    kernel.CloseHandle.argtypes=[wintypes.HANDLE]; kernel.CloseHandle.restype=wintypes.BOOL
    psapi.GetProcessMemoryInfo.argtypes=[wintypes.HANDLE,ctypes.POINTER(Counters),wintypes.DWORD]
    psapi.GetProcessMemoryInfo.restype=wintypes.BOOL
    kernel.GetProcessHandleCount.argtypes=[wintypes.HANDLE,ctypes.POINTER(wintypes.DWORD)]
    kernel.GetProcessHandleCount.restype=wintypes.BOOL
    _WINDOWS_MEMORY=kernel,psapi,Counters
    return _WINDOWS_MEMORY


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
    kernel,psapi,Counters=windows_memory_backend()
    handle=kernel.OpenProcess(0x400|0x10,False,pid); assert handle,'cannot query owned native process'
    try:
        value=Counters(); value.cb=ctypes.sizeof(value); count=wintypes.DWORD()
        assert psapi.GetProcessMemoryInfo(handle,ctypes.byref(value),value.cb)
        assert kernel.GetProcessHandleCount(handle,ctypes.byref(count))
        return {'private':value.private,'rss':value.rss,'handles':count.value}
    finally: kernel.CloseHandle(handle)


def observe_resources(child,pending):
    now=time.monotonic()
    try: return resources(child.pid),None
    except (OSError,AssertionError,KeyError):
        # exit_mm may remove /proc data before waitpid reports terminal. Keep
        # reading the same child's output/status for a bounded settling turn.
        if child.poll() is not None: return None,pending
        if pending is None: pending=now
        if now-pending>=2: raise
        return None,pending


def compile_new(binary, command):
    """An existing/running acceptance executable is never a compiler output."""
    binary.parent.mkdir(parents=True,exist_ok=True)
    marker=binary.with_name(binary.name+'.building')
    guard=marker.open('x')
    try:
        if binary.exists() or binary.with_name(binary.name+'.build.json').exists():
            raise FileExistsError('immutable build already exists; use --reuse or a fresh --build-dir')
        subprocess.run(command,cwd=ROOT,check=True)
    finally:
        guard.close(); marker.unlink()


def build(args):
    sdk=args.sdk.resolve(); core=sdk/'lib/xrt.h'; declaration=sdk/'lib/xrt_decl.h'
    source_commit=subprocess.check_output(['git','rev-parse','--verify','HEAD^{commit}'],cwd=ROOT,text=True).strip()
    for local,source in ((core,'single/xrt.h'),(declaration,'single/xrt_decl.h')):
        assert local.read_bytes()==subprocess.check_output(['git','show',source_commit+':'+source],cwd=ROOT),'SDK core must match committed upstream bytes'
    fixture=ROOT/'extlibs/xrudp/tests/test_native_soak.c'
    command=[args.compiler,'-std=gnu11','-O2','-g','-Wall','-Wextra','-Werror','-DXRT_MODULE_ALL=1','-DXRUDP_MODULE_ALL=1',
        '-I'+str(sdk/'lib'),'-I'+str(ROOT/'extlibs/xrudp/include'),'-I'+str(sdk/'lib/xrtshim')]
    if os.name=='nt': command+=['--param=ggc-min-expand=10','--param=ggc-min-heapsize=4096']
    if args.sanitize: command+=['-fsanitize=address,undefined','-fno-sanitize-recover=all','-fno-omit-frame-pointer']
    command+=[str(fixture),*map(str,sorted((ROOT/'extlibs/xrudp/src').rglob('*.c'))),'-o','<immutable-build-output>']
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
    output=(args.build_dir or ROOT/'out/xrudp-native-soak/builds').resolve()/key
    binary=output/('native.exe' if os.name=='nt' else 'native')
    command[command.index('-o')+1]=str(binary)
    receipt=binary.with_name(binary.name+'.build.json')
    if args.reuse:
        assert not binary.with_name(binary.name+'.building').exists(),'native build is still in progress'
        stored=json.loads(receipt.read_text())
        assert stored['input_key']==key and stored['binary_sha256']==hashlib.sha256(binary.read_bytes()).hexdigest(),'native build provenance changed; rebuild explicitly'
        return binary,{**stored,'verified_source_commit':source_commit}
    print(json.dumps({'build_started':True,'pid':os.getpid(),'compiler':version,'input_key':key}),flush=True)
    compile_new(binary,command)
    assert all(hashlib.sha256(Path(name).read_bytes()).hexdigest()==value for name,value in hashes.items()),'source changed while compiling'
    value={'input_key':key,'compiler':version,'command':command,'source_hashes':hashes,'source_commit':source_commit,
        'binary_sha256':hashlib.sha256(binary.read_bytes()).hexdigest()}
    receipt.write_text(json.dumps(value,indent=2)+'\n',encoding='utf-8',newline='\n')
    return binary,{**value,'verified_source_commit':source_commit}


def verify_final_sample(sample, future_mode):
    assert sample['read']==sample['written'] and min(sample[name] for name in ('read','messages','games','backpressure','rekeys'))>0,'mixed application evidence incomplete'
    if future_mode:
        assert sample.get('future_mode') is True and sample.get('future_submitted',0)>0,'cross-thread Future work not exercised'
        assert sample.get('future_completed')==sample['future_submitted'],'Future operations did not all complete'
        assert all(sample.get(name)==0 for name in ('future_pending','future_retained','future_bytes')),'Future results/quota not released'


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--sdk',type=Path,required=True); parser.add_argument('--compiler',default='gcc')
    parser.add_argument('--sanitize',action='store_true'); parser.add_argument('--reuse',action='store_true')
    parser.add_argument('--build-dir',type=Path,help='private immutable build cache (default: out/xrudp-native-soak/builds)')
    parser.add_argument('--future',action='store_true',help='submit both message streams across threads using bounded Futures')
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
    state['future_mode']=args.future
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
    values=queue.Queue(maxsize=256); child=None; inhibited=False; reader=None; baseline=None; observer_baseline=None
    error=None; proxy=None; pending_resource=None
    try:
        if os.name=='nt' and args.seconds>=3600:
            kernel=ctypes.WinDLL('kernel32',use_last_error=True)
            kernel.SetThreadExecutionState.argtypes=[wintypes.DWORD]; kernel.SetThreadExecutionState.restype=wintypes.DWORD
            assert kernel.SetThreadExecutionState(0x80000001); inhibited=True
        error=state_path.with_name('native.stderr.log').open('w',encoding='utf-8')
        proxy=FaultProxy(args.family)
        argv=[str(binary),str(args.seconds),str(args.family),str(int(args.backend=='native')),str(args.idle_every),
            str(args.idle_seconds),str(args.pause_every),str(args.report_every),str(proxy.port)]
        if args.future: argv.append('1')
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
            now=time.monotonic(); observe_clock(state,now,last,start,wall,time.time()); last=now
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
                assert elapsed<args.seconds+45,'native drain deadline exceeded'
                state['elapsed_seconds']=elapsed
            assert time.monotonic()-last_report<(args.report_every+20 if start else 30),'native reporting stopped'
            value,pending_resource=observe_resources(child,pending_resource)
            if value is None: continue
            state['resources']=value
            assert value['private']<384*1024*1024 and value['handles']<512,'native resource ceiling exceeded'
            observer=resources(os.getpid()); state['observer_resources']=observer
            assert observer['private']<256*1024*1024 and observer['handles']<512,'observer resource ceiling exceeded'
            if start and time.monotonic()-start>=1800:
                if baseline is None: baseline=value
                assert value['private']-baseline['private']<64*1024*1024 and value['handles']-baseline['handles']<64,'native resource growth exceeded'
                if observer_baseline is None: observer_baseline=observer
                assert observer['private']-observer_baseline['private']<32*1024*1024 and observer['handles']-observer_baseline['handles']<32,'observer resource growth exceeded'
            save()
        code=child.wait(timeout=15); reader.join(2); error.close()
        assert code==0 and terminal and terminal['passed'] and terminal['joined'] and terminal['duration_passed'],'native terminal/drain failed'
        assert start is not None and time.monotonic()-start>=args.seconds,'persistent real duration did not pass'
        assert proxy.stats['dropped'] and proxy.stats['duplicated'] and proxy.stats['delayed'],'real fault classes were not exercised'
        sample=state['last_sample']
        verify_final_sample(sample,args.future)
        state['duration_passed']=True; state['acceptance_72h']=args.seconds>=72*3600
        state['status']='passed' if state['acceptance_72h'] else 'smoke_passed'; state['exit_code']=code; save(True)
        return 0
    except BaseException as exc:
        record_failure(state,exc); save(True)
        print('native soak failed; inspect sanitized clock, failure code and frames',flush=True); return 1
    finally:
        if child:
            if child.poll() is None: child.terminate(); child.wait(timeout=10)
            if reader: reader.join(2)
            child.stdout.close(); child.stdin.close()
        if proxy: proxy.close()
        if error: error.close()
        if inhibited: kernel.SetThreadExecutionState(0x80000000)


if __name__=='__main__': raise SystemExit(main())
