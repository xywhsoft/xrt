"""Run the unchanged quality gates with declared sample size and CPU affinity."""
import argparse
import ctypes
import os
from pathlib import Path
import subprocess
import sys

ROOT=Path(__file__).resolve().parents[3]


def pin(cpu):
    if hasattr(os,'sched_getaffinity'):
        before=os.sched_getaffinity(0)
        chosen=cpu if cpu in before else min(before)
        os.sched_setaffinity(0,{chosen})
        return lambda:os.sched_setaffinity(0,before)
    if sys.platform=='win32':
        kernel=ctypes.WinDLL('kernel32',use_last_error=True)
        kernel.GetCurrentProcess.restype=ctypes.c_void_p
        kernel.GetProcessAffinityMask.argtypes=[ctypes.c_void_p,ctypes.POINTER(ctypes.c_size_t),ctypes.POINTER(ctypes.c_size_t)]
        kernel.SetProcessAffinityMask.argtypes=[ctypes.c_void_p,ctypes.c_size_t]
        process=kernel.GetCurrentProcess();before=ctypes.c_size_t();system=ctypes.c_size_t()
        if not kernel.GetProcessAffinityMask(process,ctypes.byref(before),ctypes.byref(system)):
            raise ctypes.WinError(ctypes.get_last_error())
        mask=1<<cpu
        if not mask&before.value:mask=before.value&-before.value
        if not kernel.SetProcessAffinityMask(process,mask):raise ctypes.WinError(ctypes.get_last_error())
        def restore():
            if not kernel.SetProcessAffinityMask(process,before.value):raise ctypes.WinError(ctypes.get_last_error())
        return restore
    raise RuntimeError('affinity control unavailable; do not label an unpinned run as pinned')


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--compiler',default='gcc')
    parser.add_argument('--cpu',type=int,default=4)
    parser.add_argument('--report',type=Path,required=True)
    args=parser.parse_args()
    if not 0<=args.cpu<1024:parser.error('CPU index out of range')
    if args.report.exists():parser.error('preserve the existing report; use a new path')
    restore=pin(args.cpu)
    try:
        return subprocess.call([sys.executable,str(ROOT/'tools/measure_performance.py'),'--compiler',args.compiler,
                                '--config',str(ROOT/'extlibs/xtp/config/qualification_profiles.json'),'--report',str(args.report.resolve())],cwd=ROOT)
    finally:
        restore()


if __name__=='__main__':
    sys.exit(main())
