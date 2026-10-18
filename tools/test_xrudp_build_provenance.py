"""Reject edited private runtime headers before consulting any test compiler."""
from pathlib import Path
from types import SimpleNamespace
import importlib.util
import json
import subprocess
import tempfile
import unittest
from unittest.mock import patch

ROOT=Path(__file__).resolve().parents[1]


def module(name):
    spec=importlib.util.spec_from_file_location(name,ROOT/'tools'/('test_xrudp_'+name+'.py'))
    value=importlib.util.module_from_spec(spec); spec.loader.exec_module(value); return value


class PrivateHeaderProvenance(unittest.TestCase):
    def setUp(self):
        self.temp=tempfile.TemporaryDirectory(prefix='xrudp-build-pin-')
        self.addCleanup(self.temp.cleanup)
        self.root=Path(self.temp.name)/'source'; self.sdk=Path(self.temp.name)/'sdk'
        files={
            'single/xrt.h':b'committed core\n','single/xrt_decl.h':b'committed declarations\n',
            'extlibs/xrudp/include/xrudp.h':b'int fixture(void);\n',
            'extlibs/xrudp/src/native.c':b'#include "internal/xrudp_noise.h"\n',
            'extlibs/xrudp/src/internal/xrudp_noise.h':b'int noise_guard;\n',
            'extlibs/xrudp/src/internal/xrudp_path.h':b'int path_guard;\n',
            'extlibs/xrudp/tests/test_native_soak.c':b'int main(void) { return 0; }\n',
            'extlibs/xrudp/tests/test_wire_interop_server.c':b'int main(void) { return 0; }\n',
        }
        for name,data in files.items():
            path=self.root/name; path.parent.mkdir(parents=True,exist_ok=True); path.write_bytes(data)
        (self.sdk/'lib').mkdir(parents=True)
        for name in ('xrt.h','xrt_decl.h'): (self.sdk/'lib'/name).write_bytes(files['single/'+name])
        self.git('init','--initial-branch=master')
        self.git('config','user.name','Fixture'); self.git('config','user.email','fixture@example.invalid')
        self.git('config','core.autocrlf','false')
        self.git('add','--',*files); self.git('commit','-m','fixture source')
        self.args=SimpleNamespace(sdk=self.sdk,compiler=str(self.root/'compiler-must-not-run'),sanitize=False,reuse=True)

    def git(self,*args):
        return subprocess.check_output(['git',*args],cwd=self.root,stderr=subprocess.DEVNULL)

    def test_private_header_edits_fail_before_compiler_lookup(self):
        for name in ('native_soak','wire_interop'):
            tool=module(name)
            for header in ('xrudp_noise.h','xrudp_path.h'):
                with self.subTest(tool=name,header=header),patch.object(tool,'ROOT',self.root):
                    path=self.root/'extlibs/xrudp/src/internal'/header; original=path.read_bytes()
                    path.write_bytes(original+b'/* edited private dependency */\n')
                    try:
                        with self.assertRaisesRegex(AssertionError,'commit runtime changes'):
                            tool.build(self.args)
                    finally: path.write_bytes(original)

    def test_clean_runtime_reaches_compiler_lookup(self):
        for name in ('native_soak','wire_interop'):
            tool=module(name)
            with self.subTest(tool=name),patch.object(tool,'ROOT',self.root):
                with self.assertRaises(FileNotFoundError): tool.build(self.args)

    def test_record_keeps_captured_commit_when_head_advances_during_compile(self):
        for name in ('native_soak','wire_interop'):
            tool=module(name); old=self.git('rev-parse','HEAD').decode().strip()
            args=SimpleNamespace(sdk=self.sdk,compiler='fixture-compiler',sanitize=False,reuse=False)
            real_run=subprocess.run; real_output=subprocess.check_output
            def output(command,*pos,**kw):
                if command==['fixture-compiler','--version']: return 'fixture compiler\n'
                return real_output(command,*pos,**kw)
            def compile(command,*pos,**kw):
                if command[0]!='fixture-compiler': return real_run(command,*pos,**kw)
                (self.root/'unrelated.txt').write_text(old+'\n')
                self.git('add','--','unrelated.txt'); self.git('commit','-m','unrelated metadata update')
                Path(command[command.index('-o')+1]).write_bytes(b'fixture compiled binary')
                return subprocess.CompletedProcess(command,0)
            with self.subTest(tool=name),patch.object(tool,'ROOT',self.root),\
                    patch.object(tool.subprocess,'run',side_effect=compile),\
                    patch.object(tool.subprocess,'check_output',side_effect=output):
                result=tool.build(args); binary=result[0] if name=='native_soak' else result
            receipt=json.loads(binary.with_name(binary.name+'.build.json').read_text())
            self.assertNotEqual(self.git('rev-parse','HEAD').decode().strip(),old)
            self.assertEqual(receipt['source_commit'],old)

    def test_committed_private_header_change_invalidates_binary_reuse(self):
        for name in ('native_soak','wire_interop'):
            tool=module(name); args=SimpleNamespace(sdk=self.sdk,compiler='fixture-compiler',sanitize=False,reuse=False)
            real_run=subprocess.run; real_output=subprocess.check_output; compiled=[]
            def output(command,*pos,**kw):
                if command==['fixture-compiler','--version']: return 'fixture compiler\n'
                return real_output(command,*pos,**kw)
            def compile(command,*pos,**kw):
                if command[0]!='fixture-compiler': return real_run(command,*pos,**kw)
                compiled.append(command); Path(command[command.index('-o')+1]).write_bytes(b'fixture compiled binary')
                return subprocess.CompletedProcess(command,0)
            with self.subTest(tool=name),patch.object(tool,'ROOT',self.root),\
                    patch.object(tool.subprocess,'run',side_effect=compile),\
                    patch.object(tool.subprocess,'check_output',side_effect=output):
                tool.build(args)
                header=self.root/'extlibs/xrudp/src/internal/xrudp_noise.h'
                header.write_bytes(header.read_bytes()+b'/* committed dependency change */\n')
                self.git('add','--',header.relative_to(self.root).as_posix()); self.git('commit','-m','private layout update')
                args.reuse=True
                with self.assertRaisesRegex(AssertionError,'native (build provenance|inputs) changed'):
                    tool.build(args)
                self.assertEqual(len(compiled),1,'reuse must reject rather than silently recompile')


if __name__=='__main__': unittest.main()
