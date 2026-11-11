"""Independent normative slot vectors; generated fixture, no Redis client source."""
from pathlib import Path
import argparse,binascii,random

def content():
    values=[b'',b'foo',b'bar',b'123456789',b'foo{}{bar}',b'{user1000}.following',b'{user1000}.followers',b'foo{{bar}}zap',b'foo{bar}{zap}',b'x{\0y}z',bytes(range(64))]+[b'{shared}:'+str(n).encode() for n in range(6)]
    rand=random.Random(20261111)
    while len(values)<128:values.append(bytes(rand.randrange(256) for _ in range(rand.randrange(1,65))))
    def slot(v):
        a=v.find(b'{');b=v.find(b'}',a+1) if a>=0 else -1
        if b>a+1 and a>=0:v=v[a+1:b]
        return binascii.crc_hqx(v,0)%16384
    text='/* Generated independent CRC16 oracle: Python binascii.crc_hqx, seed20261111. */\nstatic const struct {size_t Bytes;unsigned Slot;unsigned char Data[64];} subscriptionSlotVectors[]={\n'
    for v in values:text+='    {'+str(len(v))+','+str(slot(v))+',{'+','.join(str(b) for b in v)+('0' if not v else '')+'}},\n'
    return text+'};\n'
if __name__=='__main__':
    p=argparse.ArgumentParser();p.add_argument('--check',action='store_true');a=p.parse_args();path=Path(__file__).with_name('subscriptions_slot_vectors.h');text=content()
    if a.check:
        assert path.read_text(encoding='utf-8')==text,'slot vectors stale'
        print('[PASS] independent128 CRC16/hash-tag vectors current')
    else:path.write_text(text,encoding='utf-8')
