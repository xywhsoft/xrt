"""Bounded ASan/UBSan/libFuzzer acceptance; retain corpus, diagnostics and hashes."""
import argparse
from datetime import datetime,timezone
import hashlib
import json
from pathlib import Path
import shutil
import struct
import subprocess
import sys
import time

ROOT=Path(__file__).resolve().parents[3]


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--compiler',default='clang')
    parser.add_argument('--seconds',type=int,default=600,help='real libFuzzer seconds per target')
    parser.add_argument('--report',type=Path,required=True)
    parser.add_argument('--target',choices=('all','codec','reader','session','delivery'),default='all')
    parser.add_argument('--allocator',choices=('system','xrt'),default='system')
    args=parser.parse_args()
    if not 1<=args.seconds<=86400:parser.error('bounded seconds required')
    report=args.report.resolve()
    if report.exists():parser.error('preserve the previous report')
    compiler=shutil.which(args.compiler)
    if not compiler:parser.error('compiler unavailable')
    output=report.parent/(report.stem+'-artifacts');output.mkdir(parents=True,exist_ok=False)
    def digest(path):return hashlib.sha256(path.read_bytes()).hexdigest()
    result=dict(schema=1,status='running',utc=datetime.now(timezone.utc).isoformat(),
                head=subprocess.check_output(['git','rev-parse','HEAD'],cwd=ROOT,text=True).strip(),
                working_status=subprocess.check_output(['git','status','--short'],cwd=ROOT,text=True).strip(),
                compiler=subprocess.check_output([compiler,'--version'],text=True).splitlines()[0],
                seconds_per_target=args.seconds,allocator=args.allocator,targets=[],sources={p.relative_to(ROOT).as_posix():digest(p) for directory in ('include','src','tests')
                for p in (ROOT/'extlibs/xtp'/directory).rglob('*') if p.suffix in ('.c','.h')})
    result['amalgamated_sha256']={p:digest(ROOT/p) for p in ('single/xrt.h','single/extlibs/xtp.h')}
    def save():report.write_text(json.dumps(result,indent=2)+'\n',encoding='utf-8')
    save()
    for target in ('codec','reader','session','delivery'):
        if args.target not in ('all',target):continue
        source=output/(target+'.c');exe=output/target;log=output/(target+'.log');corpus=output/(target+'-corpus');corpus.mkdir()
        # Independent wire seeds reach valid envelopes immediately; retain bad
        # envelopes too so resource/unsupported-version rejection stays covered.
        for name,kind,identity,flags,status,size in [('request',1,1,1,0,32),('notify',5,0,1,0,32),
                ('ack',4,1,0,0,32),('retired-result',4,1,4,-8,32),('huge',1,1,1,0,0xffffffff)]:
            (corpus/name).write_bytes(struct.pack('<4sIQBBHIiI',b'xtp\x03',size,identity,kind,flags,0,0,status,0))
        allocation=''
        if args.allocator=='system' and target!='codec':
            kind,init=('xtpreaderconfig','xtpReaderConfigInit') if target=='reader' else ('xtpconfig','xtpConfigInit')
            allocation=f'''#include <stdlib.h>
static void* qa_realloc(void* user,void* data,size_t bytes){{(void)user;if(!bytes){{free(data);return NULL;}}return realloc(data,bytes);}}
static void qa_init({kind}* c){{{init}(c);c->Realloc=qa_realloc;}}
#define {init} qa_init
'''
        source.write_text(f'''#define XTP_MODULE_XTP_{target.upper()}
#include <xtp/features.h>
#define XRT_IMPLEMENTATION
#include "{(ROOT/'single/xrt.h').as_posix()}"
#define XTP_IMPLEMENTATION
#include "{(ROOT/'single/extlibs/xtp.h').as_posix()}"
{allocation}
#include "{(ROOT/'extlibs/xtp/tests'/('fuzz_'+target+'.c')).as_posix()}"
''',encoding='utf-8')
        compile_command=[compiler,'-std=c99','-Wall','-Wextra','-Werror','-O1','-g','-fsanitize=fuzzer,address,undefined',
                         '-fno-sanitize-recover=all','-I'+str(ROOT/'include'),'-I'+str(ROOT/'extlibs/xtp/include'),str(source),'-o',str(exe)]
        with log.open('w',encoding='utf-8') as stream:
            built=subprocess.run(compile_command,cwd=ROOT,stdout=stream,stderr=stream)
            if built.returncode:
                result.update(status='failed',failure='compile '+target);save();return 1
            command=[str(exe),str(corpus),f'-max_total_time={args.seconds}','-timeout=10','-rss_limit_mb=2048','-max_len=65536',
                     '-print_final_stats=1','-artifact_prefix='+str(output)+'/', '-seed=271828']
            start=time.monotonic()
            try:
                run=subprocess.run(command,cwd=ROOT,stdout=stream,stderr=stream,timeout=args.seconds+120)
            except subprocess.TimeoutExpired:
                result.update(status='failed',failure='timeout '+target);save();return 1
            elapsed=time.monotonic()-start
        text=log.read_text(encoding='utf-8',errors='replace')
        completed=any(line.startswith('Done ') and ' runs in ' in line for line in text.splitlines())
        failed=run.returncode!=0 or not completed or elapsed<args.seconds or any(marker in text for marker in ('ERROR:','runtime error:','SUMMARY: UndefinedBehaviorSanitizer'))
        result['targets'].append(dict(target=target,status='failed' if failed else 'passed',returncode=run.returncode,
                                      command=command,real_seconds=elapsed,exe_sha256=digest(exe),log_sha256=digest(log),
                                      final_statistics=[line for line in text.splitlines() if line.startswith(('Done ','stat::'))]))
        if failed:result.update(status='failed',failure=target);save();return 1
        save();print(target+' passed',flush=True)
    result['status']='passed';save();return 0


if __name__=='__main__':
    sys.exit(main())
