"""Finite local UDP ingress pressure beside eight authenticated native streams.

No external traffic/accounts. Records offered/received bad traffic separately;
bounded packets are real OS UDP, never interposed. A passing run proves only
the measured traffic intensity and hardware, not DDoS immunity.
"""
from pathlib import Path
import argparse,ctypes,hashlib,json,math,os,queue,socket,struct,subprocess,sys,threading,time
from ctypes import wintypes
from test_xrudp_native_soak import resources as memory

ROOT=Path(__file__).resolve().parents[1]
def sha(p): return hashlib.sha256(p.read_bytes()).hexdigest()
def cpu(pid):
    if os.name!='nt':
        fields=Path(f'/proc/{pid}/stat').read_text().split(') ',1)[1].split()
        return (int(fields[11])+int(fields[12]))/os.sysconf('SC_CLK_TCK')
    kernel=ctypes.WinDLL('kernel32',use_last_error=True)
    kernel.OpenProcess.argtypes=[wintypes.DWORD,wintypes.BOOL,wintypes.DWORD]; kernel.OpenProcess.restype=wintypes.HANDLE
    kernel.CloseHandle.argtypes=[wintypes.HANDLE]; kernel.CloseHandle.restype=wintypes.BOOL
    kernel.GetProcessTimes.argtypes=[wintypes.HANDLE]+[ctypes.POINTER(wintypes.FILETIME)]*4; kernel.GetProcessTimes.restype=wintypes.BOOL
    handle=kernel.OpenProcess(0x1000,False,pid); assert handle
    values=[wintypes.FILETIME() for _ in range(4)]
    try: assert kernel.GetProcessTimes(handle,*[ctypes.byref(v) for v in values])
    finally: kernel.CloseHandle(handle)
    return sum((v.dwHighDateTime<<32)|v.dwLowDateTime for v in values[2:])/10000000

def build(args):
    sdk=args.sdk.resolve(); output=ROOT/'out/xrudp-ingress'; output.mkdir(exist_ok=True)
    revision=subprocess.check_output(['git','rev-parse','HEAD'],cwd=ROOT,text=True).strip()
    for name in ('xrt.h','xrt_decl.h'):
        assert (sdk/'lib'/name).read_bytes()==subprocess.check_output(['git','show',revision+':single/'+name],cwd=ROOT)
    compiler=subprocess.check_output([args.compiler,'--version'],text=True).splitlines()[0]
    flags=[args.compiler,'-std=gnu11','-O2','-g','-Wall','-Wextra','-Werror','-DXRT_MODULE_ALL=1','-DXRUDP_MODULE_ALL=1',
           '-I'+str(sdk/'lib'),'-I'+str(sdk/'lib/xrtshim'),'-I'+str(ROOT/'extlibs/xrudp/include')]
    if args.sanitize: flags+=['-fsanitize=address,undefined','-fno-sanitize-recover=all','-fno-omit-frame-pointer']
    core_source=output/('core-'+('windows' if os.name=='nt' else 'linux')+'.c')
    source_bytes=b'#define XRT_IMPLEMENTATION\n#include <xrt.h>\n'
    if not core_source.exists() or core_source.read_bytes()!=source_bytes: core_source.write_bytes(source_bytes)
    core_inputs=[core_source,sdk/'lib/xrt.h',sdk/'lib/xrt_decl.h']+sorted((sdk/'lib/xrtshim').rglob('*.h'))
    core_hashes={str(p):sha(p) for p in core_inputs}
    core_key=hashlib.sha256(json.dumps([flags,compiler,core_hashes],sort_keys=True).encode()).hexdigest()
    core=output/('core-'+core_key+'.o'); core_meta=core.with_suffix('.json')
    if not core.exists():
        print(json.dumps({'stage':'compile-core','key':core_key}),flush=True)
        pending=core.with_name(core.name+f'.{os.getpid()}.building'); assert not pending.exists()
        core_command=[*flags,'-c',str(core_source),'-o',str(pending)]
        subprocess.run(core_command,cwd=ROOT,check=True)
        assert all(sha(Path(p))==h for p,h in core_hashes.items())
        pending.replace(core)
        core_meta.write_text(json.dumps({'source_hashes':core_hashes,'compiler':compiler,'command':core_command,'object_sha256':sha(core)},indent=2)+'\n')
    assert sha(core)==json.loads(core_meta.read_text())['object_sha256']
    sources=sorted((ROOT/'extlibs/xrudp/src').rglob('*.c'))
    inputs=sources+[ROOT/'extlibs/xrudp/tests/test_ingress_server.c',Path(__file__).resolve(),core]+core_inputs
    inputs+=sorted((ROOT/'extlibs/xrudp/include').rglob('*.h'))+sorted((ROOT/'extlibs/xrudp/src/internal').rglob('*.h'))
    inputs+=[ROOT/'tools/test_xrudp_native_soak.py']
    dirty=[]
    for p in inputs:
        if p.is_relative_to(ROOT/'extlibs/xrudp') or p.is_relative_to(ROOT/'tools'):
            committed=subprocess.run(['git','show',revision+':'+p.relative_to(ROOT).as_posix()],cwd=ROOT,capture_output=True)
            if committed.returncode or p.read_bytes().replace(b'\r\n',b'\n')!=committed.stdout.replace(b'\r\n',b'\n'): dirty.append(p.relative_to(ROOT).as_posix())
    assert args.development or not dirty,'commit fixture/runtime before fixed-source acceptance'
    hashes={str(p):sha(p) for p in inputs}; key=hashlib.sha256(json.dumps([flags,compiler,hashes,args.development],sort_keys=True).encode()).hexdigest()
    folder=output/key; folder.mkdir(exist_ok=True); binary=folder/('server.exe' if os.name=='nt' else 'server')
    command=[*flags,str(ROOT/'extlibs/xrudp/tests/test_ingress_server.c'),str(core),*map(str,sources),'-o',str(binary)]
    command+=['-lws2_32','-liphlpapi'] if os.name=='nt' else ['-pthread','-lm','-ldl']
    receipt=binary.with_name(binary.name+'.build.json')
    if binary.exists():
        assert args.reuse,'immutable existing binary: use explicit --reuse with matching source'
        record=json.loads(receipt.read_text()); assert record['input_key']==key and record['binary_sha256']==sha(binary)
    else:
        pending=binary.with_name(binary.stem+f'.{os.getpid()}.building'+binary.suffix); assert not pending.exists()
        actual=list(command); actual[actual.index('-o')+1]=str(pending)
        print(json.dumps({'stage':'compile-fixture','key':key}),flush=True); subprocess.run(actual,cwd=ROOT,check=True)
        assert all(sha(Path(p))==h for p,h in hashes.items())
        pending.replace(binary)
        record={'input_key':key,'source_commit':revision,'command':actual,'binary':str(binary),'compiler':compiler,'source_hashes':hashes,
                'binary_sha256':sha(binary),'development_only':args.development,'dirty_inputs':dirty,'core_receipt':str(core_meta)}
        receipt.write_text(json.dumps(record,indent=2)+'\n')
    return binary,record

class Ingress:
    def __init__(self,family,ready,rate):
        self.socket=socket.socket(socket.AF_INET if family==4 else socket.AF_INET6,socket.SOCK_DGRAM)
        self.address=('127.0.0.1' if family==4 else '::1',ready['port']); self.socket.bind((self.address[0],0)); self.socket.setblocking(False)
        self.ready,self.rate=ready,rate; self.stop=threading.Event(); self.sent=[0,0,0]; self.replies=0; self.reply_bytes=0; self.error=''; self.icmp=0
        self.icmp_times=[]; self.thread=threading.Thread(target=self.run); self.started=time.monotonic(); self.thread.start()
    def packet(self,index):
        mode=index%3
        if mode==0: return b'not-xrudp'+bytes(503)
        if mode==1:
            body=bytes([0x88])*16+bytes.fromhex(self.ready['public'])+bytes([0x99])*16+bytes(32)
            kind,epoch,cid,pn=1,0,bytes.fromhex(self.ready['scope']),1
        else: body=bytes(32); kind,epoch,cid,pn=4,1,bytes.fromhex(self.ready['cid']),2**40+index
        return struct.pack('!8sBBHI16sQ8s',b'XRUDP001',kind,0,len(body),epoch,cid,pn,bytes(8))+body
    def run(self):
        try:
            index=0; due=time.monotonic(); batch=256
            while not self.stop.is_set():
                for _ in range(batch):
                    self.socket.sendto(self.packet(index),self.address); self.sent[index%3]+=1; index+=1
                for _ in range(128):
                    try: packet,source=self.socket.recvfrom(2048)
                    except BlockingIOError: break
                    except ConnectionResetError:
                        self.icmp+=1
                        if len(self.icmp_times)<8: self.icmp_times.append(time.monotonic()-self.started)
                        break
                    assert source[1]==self.address[1] and len(packet)==136 and packet[:9]==b'XRUDP001\x02'
                    self.replies+=1; self.reply_bytes+=len(packet)
                # Larger, bounded batches avoid requesting sub-tick Windows
                # sleeps; no global timer-resolution setting is changed.
                due=max(due+batch/self.rate,time.monotonic()); self.stop.wait(max(0,due-time.monotonic()))
        except Exception as error: self.error=repr(error)
    def close(self):
        self.stop.set(); self.thread.join(3); assert not self.thread.is_alive(); self.socket.close()
        assert not self.error,self.error

def run_case(binary,args,family,backend):
    child=subprocess.Popen([str(binary),str(family),str(int(backend)),str(args.seconds)],stdout=subprocess.PIPE,stderr=subprocess.PIPE,text=True,
       env={**os.environ,'ASAN_OPTIONS':'detect_leaks=1:halt_on_error=1:quarantine_size_mb=8:thread_local_quarantine_size_kb=64','UBSAN_OPTIONS':'halt_on_error=1:print_stacktrace=1'})
    lines=queue.Queue(); errors=[]
    def stdout():
        for line in child.stdout: lines.put(line)
        lines.put(None)
    def stderr():
        for line in child.stderr:
            if sum(map(len,errors))<16384: errors.append(line)
    readers=[threading.Thread(target=stdout),threading.Thread(target=stderr)]
    for t in readers: t.start()
    flood=None; transcript=[]
    def event():
        line=lines.get(timeout=15)
        assert line is not None,'native fixture exited before terminal proof: '+''.join(errors)
        value=json.loads(line); transcript.append(value); return value
    try:
        ready=event(); assert ready['ready'] and ready['clients']==8
        initial=memory(child.pid); cpu_begin=cpu(child.pid); started=time.monotonic(); samples=[]; terminal=None
        flood=Ingress(family,ready,args.rate)
        while terminal is None:
            row=event(); assert time.monotonic()-started<args.seconds+15
            if row.get('terminal'): terminal=row; break
            assert row['sample'] and row['reserved']==ready['reserved']
            assert abs((time.monotonic()-started)-row['elapsed'])<.5,'native/observer phase gap invalidates the offered load interval'
            try: resource=memory(child.pid); observed_cpu=cpu(child.pid)-cpu_begin
            except (OSError,AssertionError,KeyError):
                assert len(samples)+1==args.seconds*2,'resource sampler failed before the final output/exit race'
                row['resource_omitted_pending_normal_exit']=True
            else:
                assert resource['private']<512*1024*1024 and resource['private']-initial['private']<32*1024*1024
                assert resource['handles']-initial['handles']<16
                row['cpu_seconds']=observed_cpu; row['resources']=resource
            if samples:
                assert all(a>b for a,b in zip(row['reads'],samples[-1]['reads'])),'authenticated stream starved for a sample interval'
            samples.append(row)
        child.wait(timeout=10); flood.close(); elapsed=time.monotonic()-started
        for t in readers: t.join(3)
        assert child.returncode==0 and not errors and terminal['joined'] and terminal['max_tick_gap']<1
        assert len(samples)==args.seconds*2 and min(samples[-1]['reads'])>=16384
        fairness=min(samples[-1]['reads'])/max(samples[-1]['reads']); assert fairness>=.25,'one stream received only a trickle beside equal-window peers'
        assert sum(flood.sent)>=args.rate*args.seconds*.6,('generator did not reach target intensity',flood.sent,args.rate,args.seconds)
        rejected=samples[-1]['invalid']+samples[-1]['rate_dropped']; assert rejected>=sum(flood.sent)//4
        assert samples[-1]['address_dropped']>=flood.sent[2]//4,'known-CID wrong-source path did not receive the test traffic'
        assert flood.replies<=8*(math.ceil(elapsed)+2),'retry rate/reflection budget exceeded'
        assert not flood.icmp_times or min(flood.icmp_times)>=args.seconds-.5,'ICMP arrived before normal fixture close'
        measured=[row for row in samples if 'resources' in row]; assert len(measured)>=len(samples)-1
        record={'family':family,'backend':('iocp' if os.name=='nt' else 'epoll') if backend else 'select',
          'seconds':args.seconds,'offered_rate':args.rate,'actual_offered_rate':sum(flood.sent)/elapsed,'sent_classes':flood.sent,'received_bad_or_rate_dropped':rejected,
          'known_cid_address_dropped':samples[-1]['address_dropped'],
          'retry_replies':flood.replies,'retry_bytes':flood.reply_bytes,'icmp':flood.icmp,'icmp_times':flood.icmp_times,'max_tick_gap':terminal['max_tick_gap'],
          'native_cpu_seconds':measured[-1]['cpu_seconds'],'cpu_observed_through_native_elapsed':measured[-1]['elapsed'],'native_wall_seconds':samples[-1]['elapsed'],
          'stream_bytes':samples[-1]['reads'],'min_max_stream_ratio':fairness,'memory_growth':measured[-1]['resources']['private']-initial['private'],
          'all_sample_intervals_progress':True,'joined':True,'exit':0}
        print(json.dumps(record),flush=True); return record
    finally:
        if flood and flood.thread.is_alive(): flood.close()
        if child.poll() is None: child.terminate(); child.wait(timeout=5)
        for t in readers: t.join(3)
        if child.returncode: print('native diagnostic: '+''.join(errors),file=sys.stderr,flush=True)
        (binary.parent/f'case-{family}-{int(backend)}-transcript.json').write_text(json.dumps({'events':transcript,'stderr':errors,'exit':child.returncode,
          'flood_sent':flood.sent if flood else None,'retry_replies':flood.replies if flood else 0,'flood_error':flood.error if flood else None},indent=2)+'\n')
        child.stdout.close(); child.stderr.close()

def main():
    p=argparse.ArgumentParser(description=__doc__); p.add_argument('--sdk',type=Path,required=True); p.add_argument('--compiler',default='gcc')
    p.add_argument('--sanitize',action='store_true'); p.add_argument('--development',action='store_true'); p.add_argument('--reuse',action='store_true')
    p.add_argument('--seconds',type=int,default=8); p.add_argument('--rate',type=int,default=6000)
    p.add_argument('--families',choices=('both','4','6'),default='both'); p.add_argument('--backends',choices=('both','select','native'),default='both')
    args=p.parse_args(); assert 2<=args.seconds<=20 and 1000<=args.rate<=20000
    binary,build_record=build(args); cases=[]
    for family in ((4,6) if args.families=='both' else (int(args.families),)):
        for backend in ((False,True) if args.backends=='both' else (args.backends=='native',)):
            cases.append(run_case(binary,args,family,backend))
    out=ROOT/'out/xrudp-ingress'/('development-' if args.development else 'acceptance-')
    record={'build':build_record,'cases':cases,'committed_acceptance':not args.development,'scope':'measured local loopback ingress, no DDoS guarantee','acceptance_72h':False}
    Path(str(out)+('windows' if os.name=='nt' else 'linux')+'.json').write_text(json.dumps(record,indent=2)+'\n')
    return 0
if __name__=='__main__': raise SystemExit(main())
