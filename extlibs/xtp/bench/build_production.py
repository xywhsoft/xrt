"""Build the dedicated persistent-connection acceptance probe."""
import argparse
from pathlib import Path
import sys

ROOT=Path(__file__).resolve().parents[3]
sys.path.insert(0,str(ROOT/'tools'))
import measure_performance as performance


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--compiler',default='gcc')
    parser.add_argument('--output',type=Path,required=True)
    parser.add_argument('--sanitizers',action='store_true')
    parser.add_argument('--inject-test-corruption',action='store_true',help='negative test binary; never use for acceptance')
    args=parser.parse_args()
    cflags=['-O1','-g','-fsanitize=address,undefined','-fno-sanitize-recover=all'] if args.sanitizers else ['-O2']
    ldflags=['-fsanitize=address,undefined'] if args.sanitizers else []
    if args.inject_test_corruption:cflags.append('-DXTP_PRODUCTION_TEST_CORRUPT')
    performance._compile_benchmark(performance.xrt_build._compiler(args.compiler),'native',
        {'source':'extlibs/xtp/bench/bench_production.c','suite':'xtp_wss_tests,xtp_delivery,time'},
        {'cflags':cflags,'ldflags':ldflags},args.output.resolve(),
        tuple(performance.expand_manifest_paths([Path('extlibs/xtp/config/modules.json')])))


if __name__=='__main__':
    main()
