"""Independent binary stdin/stdout framing oracle. No third-party implementation source."""
from pathlib import Path
import argparse,hashlib,json,struct,subprocess,decimal
def sha(p):return hashlib.sha256(Path(p).read_bytes()).hexdigest()
def encode(body,form,extra=b''):
    return (struct.pack('>I',len(body)) if form==2 else b'Content-Length: '+str(len(body)).encode()+b'\r\n'+extra+b'\r\n')+body
def decode(wire,form):
    out=[]
    while wire:
        if form==2:
            assert len(wire)>=4;n=struct.unpack('>I',wire[:4])[0];wire=wire[4:]
        else:
            head,sep,wire=wire.partition(b'\r\n\r\n');assert sep and head.startswith(b'Content-Length: ');n=int(head[16:]);assert head==b'Content-Length: '+str(n).encode()
        assert n<=len(wire);out.append(wire[:n]);wire=wire[n:]
    return out
def parsed(body):return json.loads(body,parse_float=decimal.Decimal)
def main():
    p=argparse.ArgumentParser();p.add_argument('--peer',type=Path,action='append',required=True);p.add_argument('--report',type=Path,required=True);a=p.parse_args();assert not a.report.exists();a.report.parent.mkdir(parents=True,exist_ok=True)
    report=dict(complete=False,runner_sha256=sha(__file__),peers={str(x):sha(x) for x in a.peer},cases=[],limits=['Real finite stdin/stdout fixture using the production framer plus synchronous dispatch. Not a production process/stdio adapter, asynchronous bidirectional runtime, cancellation/deadline negotiation, backpressure or performance acceptance.']);a.report.write_text(json.dumps(report,indent=2)+'\n')
    def run(peer,name,form,chunk,wire,expected=None,code=0):
        child=subprocess.Popen([str(peer),str(form),str(chunk)],stdin=subprocess.PIPE,stdout=subprocess.PIPE,stderr=subprocess.PIPE)
        try:out,err=child.communicate(wire,timeout=15)
        except BaseException:
            child.kill();child.communicate();raise
        row=dict(peer=str(peer),name=name,format=form,chunk=chunk,input_sha256=hashlib.sha256(wire).hexdigest(),output_sha256=hashlib.sha256(out).hexdigest(),exit_code=child.returncode,expected_exit_code=code,stderr=err.decode(errors='replace'),terminal=child.poll() is not None)
        report['cases'].append(row);a.report.write_text(json.dumps(report,indent=2)+'\n');assert child.returncode==code and row['terminal'],row
        if code:assert not out,row;return
        bodies=decode(out,form);assert len(bodies)==len(expected),(name,bodies,expected)
        for body,want in zip(bodies,expected):assert parsed(body)==want,(name,body,want)
        if name=='lossless-number':assert b'1.2300e+999' in bodies[0]
    for peer in a.peer:
        for form in (1,2):
            examples=[('unicode',b'{"jsonrpc":"2.0","method":"echo","params":{"s":"'+ '雪'.encode()+b'"},"id":"a\\u0000b"}',{'jsonrpc':'2.0','result':{'s':'雪'},'id':'a\0b'}),('lossless-number',b'{"jsonrpc":"2.0","method":"echo","params":[1],"id":1.2300e+999}',{'jsonrpc':'2.0','result':[1],'id':decimal.Decimal('1.2300e+999')}),('notification',b'{"jsonrpc":"2.0","method":"echo","params":[]}',None),('empty-body',b'',{'jsonrpc':'2.0','error':{'code':-32700,'message':'Parse error'},'id':None}),('malformed-tail',b'[{"jsonrpc":"2.0","method":"echo","id":1},',{'jsonrpc':'2.0','error':{'code':-32700,'message':'Parse error'},'id':None}),('mixed-batch',b'[{"jsonrpc":"2.0","method":"echo","id":3,"params":[7]},{"jsonrpc":"2.0","method":"echo"}]',[{'jsonrpc':'2.0','id':3,'result':[7]}])]
            for chunk in (1,7,4096):
                for name,body,want in examples:run(peer,name,form,chunk,encode(body,form),[] if want is None else [want])
                wire=b''.join(encode(body,form) for _,body,_ in examples);run(peer,'coalesced',form,chunk,wire,[want for _,_,want in examples if want is not None])
                run(peer,'truncated-body',form,chunk,encode(b'{}',form)[:-1],code=26)
            run(peer,'over-cap',form,1,struct.pack('>I',65537) if form==2 else b'Content-Length: 65537\r\n\r\n',code=23)
        for name,head,code in [('duplicate-length',b'Content-Length: 2\r\nContent-Length: 2\r\n\r\n{}',22),('bare-lf',b'Content-Length: 2\n\n{}',22),('unsupported-charset',b'Content-Length: 2\r\nContent-Type: application/json;charset=utf-16\r\n\r\n{}',25),('transfer-coding',b'Content-Length: 2\r\nTransfer-Encoding: chunked\r\n\r\n{}',25)]:run(peer,name,1,1,head,code=code)
    report['complete']=True;report['all_processes_terminal']=all(x['terminal'] for x in report['cases']);a.report.write_text(json.dumps(report,indent=2)+'\n');print('[PASS]',len(report['cases']),'independent stdin/stdout framing/JSON cases',flush=True)
if __name__=='__main__':main()
