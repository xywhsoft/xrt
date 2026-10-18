"""Independent UDP STUN oracle for the native xrudp socket, using stdlib only.

Uses the shared manifest/compiler helpers, never generates or copies SDK headers.
No Internet access: IPv4/IPv6 loopback, with optional fingerprint and deliberately
poisoned addresses from wrong source/transaction/framing/CRC. Client must return
its actual bound port. Nonces and raw packets remain in memory.
"""
from __future__ import annotations
import argparse
import json
import os
from pathlib import Path
import socket
import struct
import subprocess
import sys
import threading
import zlib

ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'tools'))
import build

def shift(port,delta):
 return port+delta if port+delta<=65535 else port-delta

def main():
 parser=argparse.ArgumentParser(description=__doc__)
 parser.add_argument('--compiler',default='gcc' if sys.platform=='win32' else 'clang')
 parser.add_argument('--sanitize',action='store_true')
 args=parser.parse_args();os.chdir(ROOT)
 compiler=build._compiler(args.compiler);platform=build.platform_name()
 plan=build._load_modules('rudp_endpoint',[ROOT/'extlibs/xrudp/config/modules.json'],platform=platform)
 sources,_,_,_,defines,links,_,_,_,headers=plan
 work=ROOT/'out'/'xrudp-discovery-interop'/Path(compiler).stem
 work.mkdir(parents=True,exist_ok=True)
 flags=['-O2','-I',str(ROOT/'extlibs/xrudp/include')]
 if args.sanitize:flags+=['-fsanitize=address,undefined','-fno-sanitize-recover=all','-fno-omit-frame-pointer']
 objects=build._compile_objects(compiler,'native',sources,defines,work/'obj',False,extra_cflags=flags,header_roots=headers,platform=platform)
 binary=str(work/('client.exe' if platform=='windows' else 'client'))
 build._compile_program(compiler,'native','extlibs/xrudp/tests/test_discovery_interop_client.c',objects,defines,links,Path(binary),extra_cflags=flags,platform=platform)
 for family in [socket.AF_INET,socket.AF_INET6]:
  for fingerprint in [False,True]:
   host='127.0.0.1' if family==socket.AF_INET else '::1'
   server=socket.socket(family,socket.SOCK_DGRAM);server.bind((host,0));server.settimeout(6)
   wrong=socket.socket(family,socket.SOCK_DGRAM);wrong.bind((host,0))
   errors=[];observed=[]
   def response(tx,address,fp):
    packed=socket.inet_pton(family,address[0]);mask=bytes.fromhex('2112a442')+tx
    value=bytes([0,1 if family==socket.AF_INET else 2])+struct.pack('!H',address[1]^0x2112)+bytes(a^b for a,b in zip(packed,mask))
    attrs=struct.pack('!HH',0x20,len(value))+value
    p=struct.pack('!HHI',0x101,len(attrs)+(8 if fp else 0),0x2112a442)+tx+attrs
    if fp:p+=struct.pack('!HHI',0x8028,4,zlib.crc32(p)^0x5354554e)
    return p
   def serve():
    try:
     data,address=server.recvfrom(512);observed.append(address[1]);typ,n,cookie=struct.unpack('!HHI',data[:8]);assert typ==1 and cookie==0x2112a442 and n==len(data)-20
     if fingerprint:
      assert len(data)==28 and struct.unpack('!HHI',data[20:])==(0x8028,4,zlib.crc32(data[:20])^0x5354554e)
     else:assert len(data)==20
     tx=data[8:20];wire=response(tx,address,fingerprint)
     wrong.sendto(response(tx,(address[0],shift(address[1],1)),fingerprint),address)
     fake=bytes([tx[0]^1])+tx[1:];server.sendto(response(fake,(address[0],shift(address[1],2)),fingerprint),address)
     corrupt=bytearray(response(tx,(address[0],shift(address[1],3)),fingerprint));corrupt[3]^=4;server.sendto(corrupt,address)
     if fingerprint:
      crc_bad=bytearray(response(tx,(address[0],shift(address[1],4)),True));crc_bad[-1]^=1;server.sendto(crc_bad,address)
     server.sendto(wire,address)
    except BaseException as e:errors.append(repr(e))
   t=threading.Thread(target=serve);t.start()
   try:
    p=subprocess.run([binary,host,str(server.getsockname()[1]),str(int(fingerprint))],capture_output=True,text=True,timeout=10)
    t.join(7);assert not t.is_alive() and not errors,errors
    assert p.returncode==0,(p.returncode,p.stdout,p.stderr)
    state=json.loads(p.stdout.strip().splitlines()[-1]);assert observed==[state['local']] and state['mapped']==state['local']
    print(json.dumps({'family':4 if family==socket.AF_INET else 6,'fingerprint':fingerprint,'passed':True,'source_port':state['local']}))
   finally:server.close();wrong.close()

if __name__=='__main__':
 main()
