"""Independent Python socket/JSON peer; binary takes framing format 1 or 2."""
import json,socket,struct,subprocess,sys

def exact(s,n):
    data=b''
    while len(data)<n:
        part=s.recv(n-len(data))
        if not part:raise EOFError('partial frame')
        data+=part
    return data

def receive(s,fmt):
    if fmt==2:n=struct.unpack('!I',exact(s,4))[0]
    else:
        head=b''
        while not head.endswith(b'\r\n\r\n'):
            head+=exact(s,1)
            assert len(head)<8192
        fields=dict(x.split(b':',1) for x in head[:-4].split(b'\r\n'));n=int(fields[b'Content-Length'])
    assert n<1048576
    return json.loads(exact(s,n))

def send(s,fmt,value,fragment):
    body=json.dumps(value,ensure_ascii=False,separators=(',',':')).encode();wire=(struct.pack('!I',len(body)) if fmt==2 else b'Content-Length: '+str(len(body)).encode()+b'\r\n\r\n')+body
    for i in range(0,len(wire),fragment):s.sendall(wire[i:i+fragment])

for fmt in (1,2):
    for fragment in (1,65536):
        process=subprocess.Popen([sys.argv[1],str(fmt)],stdout=subprocess.PIPE,stderr=subprocess.PIPE,text=True)
        try:
            line=process.stdout.readline();assert line.startswith('PORT '),line
            with socket.create_connection(('127.0.0.1',int(line.split()[1])),timeout=10) as s:
                s.settimeout(10)
                send(s,fmt,{'jsonrpc':'2.0','id':'py\x00\u4e2d\U0001f63a','method':'echo','params':{'text':'hello\u4e16\u754c','n':2**70}},fragment)
                send(s,fmt,[{'jsonrpc':'2.0','id':23,'method':'echo','params':[1,2]},{'jsonrpc':'2.0','method':'note'},{'jsonrpc':'2.0','id':None,'method':'echo','params':[]}],fragment)
                single=batch=callback=False
                while not (single and batch and callback):
                    msg=receive(s,fmt)
                    if isinstance(msg,list):
                        assert len(msg)==2;byid={x['id']:x['result'] for x in msg};assert byid=={23:[1,2],None:[]};batch=True
                    elif 'method' in msg:
                        assert msg['method']=='python_echo' and msg['params']==[42];send(s,fmt,{'jsonrpc':'2.0','id':msg['id'],'result':msg['params']},fragment);callback=True
                    else:
                        assert msg['id']=='py\x00\u4e2d\U0001f63a' and msg['result']=={'text':'hello\u4e16\u754c','n':2**70};single=True
                s.shutdown(socket.SHUT_WR)
                try:
                    while s.recv(4096):pass
                except ConnectionResetError:pass  # Peer contract uses aborting EOF close.
            out,err=process.communicate(timeout=20);assert process.returncode==0,(process.returncode,out,err)
        finally:
            if process.poll() is None:process.kill();process.communicate()
print('[PASS] 4 independent Python/C TCP sessions: two framing formats, 1-byte/coalesced sends, bidirectional calls, Unicode/NUL/large integer IDs and payloads, notification/batch/null ID, EOF retirement')
