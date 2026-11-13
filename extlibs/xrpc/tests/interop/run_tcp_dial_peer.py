"""Independent Python JSON/framing server for numeric/prepared/DNS TCP peers."""
import argparse,hashlib,json,platform,queue,socket,struct,subprocess,sys,threading
from pathlib import Path
def exact(s,n):
 data=b''
 while len(data)<n:
  part=s.recv(n-len(data))
  if not part:raise EOFError('partial application frame')
  data+=part
 return data
def receive(s,fmt):
 if fmt==2:n=struct.unpack('!I',exact(s,4))[0]
 else:
  head=b''
  while not head.endswith(b'\r\n\r\n'):
   head+=exact(s,1);assert len(head)<8192
  fields=dict(x.split(b':',1) for x in head[:-4].split(b'\r\n'));n=int(fields[b'Content-Length'])
 assert n<1048576
 return json.loads(exact(s,n))
def send(s,fmt,value,fragment):
 body=json.dumps(value,ensure_ascii=False,separators=(',',':')).encode();wire=(struct.pack('!I',len(body)) if fmt==2 else b'Content-Length: '+str(len(body)).encode()+b'\r\n\r\n')+body
 for i in range(0,len(wire),fragment):s.sendall(wire[i:i+fragment])
def exchange(s,fmt,fragment):
 send(s,fmt,{'jsonrpc':'2.0','id':'py\x00\u4e2d\U0001f63a','method':'echo','params':{'text':'hello\u4e16\u754c','n':2**70}},fragment)
 send(s,fmt,[{'jsonrpc':'2.0','id':23,'method':'echo','params':[1,2]},{'jsonrpc':'2.0','method':'note'},{'jsonrpc':'2.0','id':None,'method':'echo','params':[]}],fragment)
 send(s,fmt,{'jsonrpc':'2.0','id':2**70+17,'method':'echo','params':['wide-id']},fragment)
 single=batch=wide=callback=False
 while not (single and batch and wide and callback):
  msg=receive(s,fmt)
  if isinstance(msg,list):assert len(msg)==2 and {x['id']:x['result'] for x in msg}=={23:[1,2],None:[]};batch=True
  elif 'method' in msg:
   assert msg['method']=='python_echo' and msg['params']==[42];send(s,fmt,{'jsonrpc':'2.0','id':msg['id'],'result':msg['params']},fragment);callback=True
  elif msg['id']==2**70+17:assert msg['result']==['wide-id'];wide=True
  else:assert msg['id']=='py\x00\u4e2d\U0001f63a' and msg['result']=={'text':'hello\u4e16\u754c','n':2**70};single=True
 s.shutdown(socket.SHUT_WR)
 try:
  while s.recv(4096):pass
 except ConnectionResetError:pass # Aborting peer close, only after all replies.
def main():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('binary',type=Path);p.add_argument('--report',type=Path);a=p.parse_args();binary=a.binary.absolute();assert binary.is_file();cases=[]
 for mode in range(3):
  for fmt in (1,2):
   for fragment in (1,65536):
    process=None;reader=None
    try:
     with socket.socket() as listener:
      listener.bind(('127.0.0.1',0));listener.listen(1);listener.settimeout(15);port=listener.getsockname()[1]
      process=subprocess.Popen([str(binary),str(fmt),str(mode),str(port)],stdout=subprocess.PIPE,stderr=subprocess.PIPE,text=True)
      lines=queue.Queue()
      def read_lines(proc=process,q=lines):
       for line in proc.stdout:q.put(line)
       q.put(None)
      reader=threading.Thread(target=read_lines);reader.start();line=lines.get(timeout=15);assert line and line.startswith('READY'),line
      with listener.accept()[0] as s:s.settimeout(15);exchange(s,fmt,fragment)
      code=process.wait(timeout=20);reader.join(timeout=5);error=process.stderr.read();remaining=[]
      while not lines.empty():remaining.append(lines.get())
      assert code==0 and any(line and line.startswith('PASS native framed candidate') for line in remaining),(code,remaining,error)
      cases.append(dict(mode=mode,frame_format=fmt,fragment_bytes=fragment,exit_code=code))
    finally:
     if process and process.poll() is None:process.kill();process.wait(timeout=10)
     if reader:reader.join(timeout=5)
     if process:
      if process.stdout:process.stdout.close()
      if process.stderr:process.stderr.close()
 report=dict(binary=str(binary),binary_sha256=hashlib.sha256(binary.read_bytes()).hexdigest(),python=sys.version,platform=platform.platform(),cases=cases,scope='12 real independent Python-server/native-client numeric/prepared/OS DNS sessions, both frames and 1B/coalesced sends, queued bidirectional call/notify/batch/null/Unicode/NUL/wide numeric IDs and exact payloads/EOF retirement. Local functional evidence, no production throughput/long-run claim.')
 if a.report:a.report.parent.mkdir(parents=True,exist_ok=True);a.report.write_text(json.dumps(report,indent=2)+'\n')
 print('[PASS]',len(cases),'independent candidate TCP sessions');return 0
if __name__=='__main__':raise SystemExit(main())
