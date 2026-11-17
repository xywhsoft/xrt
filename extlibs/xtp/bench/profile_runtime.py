"""Linux perf counters restricted to a known acceptance child; no payload sampling."""
import argparse
import ctypes
import fcntl
import json
import os
from pathlib import Path
import platform
import struct
import time


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--run-report',type=Path,required=True)
    parser.add_argument('--report',type=Path,required=True)
    parser.add_argument('--seconds',type=int,default=30)
    args=parser.parse_args()
    if platform.machine()!='x86_64' or not 1<=args.seconds<=300:parser.error('Linux x86_64 and bounded duration required')
    if args.report.exists():parser.error('preserve the previous report')
    run=json.loads(args.run_report.read_text());pid=run['child_pid'];proc=Path('/proc')/str(pid)
    if run['status']!='running' or (proc/'exe').resolve()!=Path(run['command'][0]).resolve():
        parser.error('not the expected running acceptance child')
    threads=sorted(int(p.name) for p in (proc/'task').iterdir());libc=ctypes.CDLL(None,use_errno=True)
    counters=[];errors=[]
    def counter(name,kind,config,tid,cpu=-1,filter_text=None):
        attr=bytearray(128);struct.pack_into('IIQ',attr,0,kind,128,config);struct.pack_into('Q',attr,40,1)
        buffer=(ctypes.c_ubyte*len(attr)).from_buffer(attr)
        fd=libc.syscall(298,ctypes.byref(buffer),tid,cpu,-1,8)
        if fd<0:
            errors.append(dict(name=name,tid=tid,cpu=cpu,errno=ctypes.get_errno()));return
        try:
            if filter_text:fcntl.ioctl(fd,0x40082406,filter_text.encode()+b'\0')
            fcntl.ioctl(fd,0x2400,0);counters.append((name,fd))
        except OSError as error:
            os.close(fd);errors.append(dict(name=name,tid=tid,cpu=cpu,error=str(error)))
    try:
        for tid in threads:
            for name,config in [('task_clock_ns',1),('context_switches',3),('page_faults',2),('cpu_migrations',4)]:
                counter(name,1,config,tid)
            for name in ('sendto','recvfrom','sendmsg','recvmsg','write','read','writev','readv','epoll_wait','pselect6','poll','ppoll'):
                event=Path('/sys/kernel/tracing/events/syscalls')/('sys_enter_'+name)/'id'
                if event.exists():counter('syscall_'+name,2,int(event.read_text()),tid)
        wakeup=Path('/sys/kernel/tracing/events/sched/sched_wakeup/id')
        if wakeup.exists():
            expression=' || '.join('pid == '+str(tid) for tid in threads)
            for cpu in sorted(os.sched_getaffinity(0)):counter('sched_wakeup',2,int(wakeup.read_text()),-1,cpu,expression)
        started=time.monotonic();time.sleep(args.seconds);elapsed=time.monotonic()-started
        totals={}
        for name,fd in counters:totals[name]=totals.get(name,0)+struct.unpack('Q',os.read(fd,8))[0]
        if not totals.get('sched_wakeup'):
            errors.append(dict(name='sched_wakeup',error='zero wakeup count requires a successful positive control; not qualified as a valid wakeup measurement'))
        after=json.loads(args.run_report.read_text());same_threads=threads==sorted(int(p.name) for p in (proc/'task').iterdir())
        before_completed=run.get('last_checkpoint',{}).get('completed',0);after_completed=after.get('last_checkpoint',{}).get('completed',0)
        result=dict(schema=1,status='passed' if not errors and same_threads and after_completed>before_completed else 'partial',
                    pid=pid,thread_ids=threads,seconds=elapsed,completed_delta=after_completed-before_completed,
                    source_run=str(args.run_report),exe_sha256=run['exe_sha256'],totals=totals,errors=errors,
                    same_threads=same_threads,scope='aggregate software/per-thread syscall counters; sched_wakeup filtered to these target TIDs across allowed CPUs; no payload/address/stack samples; includes test instrumentation, excludes other processes; not a memcpy instruction census')
        args.report.parent.mkdir(parents=True,exist_ok=True);args.report.write_text(json.dumps(result,indent=2)+'\n')
        print(json.dumps(result));return 0 if result['status']=='passed' else 1
    finally:
        for _,fd in counters:os.close(fd)


if __name__=='__main__':
    raise SystemExit(main())
