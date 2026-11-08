"""Independent selected-occurrence oracle: frozen TZif coverage, zoneinfo states,
all distinct offsets, Gregorian M-rule transitions and explicit label enumeration.
No XCRON search/matcher, fixed horizon or two-fold assumption supplies expectations.
"""
import argparse
import bisect
import calendar
import datetime as dt
import functools
import hashlib
import io
import itertools
import json
from pathlib import Path
import re
import subprocess
from zoneinfo import ZoneInfo
from test_timezone_reference import EPOCH, UTC, XRT_EPOCH, millis
from test_tzif_reference import decode
from test_zoned_reference import PROFILES, footer_offsets


def footer_transitions(footer, year):
    parts = footer.split(',')
    if len(parts) == 1:
        return []
    offsets = footer_offsets(footer)
    assert len(parts) == 3 and len(offsets) == 2, footer
    found = []
    for index, text in enumerate(parts[1:]):
        match = re.fullmatch(r'M(\d+)\.(\d+)\.(\d+)(?:/([+-]?\d+(?::\d+(?::\d+)?)?))?', text)
        assert match, ('frozen corpus footer is not a validated M-rule form', footer)
        month, week, day = map(int, match.groups()[:3]);clock = match[4] or '2'
        pieces = list(map(int, clock.lstrip('+-').split(':')))
        seconds = sum(a*b for a,b in zip(pieces,(3600,60,1))) * (-1 if clock.startswith('-') else 1)
        first_weekday, days = calendar.monthrange(year, month)
        date = 1 + (day - (first_weekday+1)%7)%7 + (week-1)*7
        if date > days:
            date -= 7
        stamp = dt.datetime(year,month,date,tzinfo=UTC) + dt.timedelta(seconds=seconds-offsets[index])
        found.append(millis(stamp)//1000)
    return found


def main():
    parser=argparse.ArgumentParser();parser.add_argument('--probe',required=True);parser.add_argument('--data',type=Path,required=True);parser.add_argument('--output',type=Path,required=True);args=parser.parse_args()
    manifest=json.loads((args.data/'manifest.json').read_text());files=[];totals=dict(queries=0,found=0,none=0,ambiguous=0,missing=0,unknown=0,cursor_steps=0,segments=0,months=0,rule_years=0,checks=0,offsets=0,labels=0)
    for name,digest in manifest['files'].items():
        raw=(args.data/name).read_bytes();assert hashlib.sha256(raw).hexdigest()==digest
        zone=ZoneInfo.from_file(io.BytesIO(raw),key=name);times,indices,kinds,footer=decode(raw)
        offsets={k[0] for k in kinds if k[2]!='-00'}|set(footer_offsets(footer))
        @functools.lru_cache(maxsize=None)
        def state(second):
            index=bisect.bisect_right(times,second)
            if times and index==len(times):
                if not footer:return None
            elif times or not footer:
                k=kinds[indices[index-1] if index else 0];return None if k[2]=='-00' else k[0]
            local=(EPOCH+dt.timedelta(seconds=second)).astimezone(zone)
            return None if local.tzname()=='-00' else int(local.utcoffset().total_seconds())
        def first_unknown(first,last):
            if state(first) is None:return first
            for second in times[bisect.bisect_left(times,first):bisect.bisect_right(times,last)]:
                if state(second) is None:return second
            return None
        @functools.lru_cache(maxsize=None)
        def resolve(center):
            unknown=first_unknown(center-(2**31-1),center+(2**31-1)) is not None
            values=sorted(center-o for o in offsets if state(center-o)==o)
            return unknown,values
        def labels(profile,first,last):
            date=first.date()
            while date<=last.date():
                for h,m,s in itertools.product(profile[2],profile[3],profile[4]):
                    local=dt.datetime.combine(date,dt.time(h,m,s))
                    if first<=local<=last and profile[5](local):yield local
                date+=dt.timedelta(days=1)
        def local_second(label):return millis(label.replace(tzinfo=UTC))//1000
        def payload(status,code,when=0,count=0,label=None):
            return [status,code,2009 if code==9004 else 0,when*1000+XRT_EPOCH if status else 0,count,*([label.year,label.month,label.day,label.hour,label.minute,label.second,(label.weekday()+1)%7,label.timetuple().tm_yday] if label else [0]*8)]
        def expected(profile,begin,end,exclusive,fold,missing):
            first=(begin+999)//1000+(int(bool(exclusive)) if begin%1000==0 else 0);last=end//1000
            if first>last:return payload(0,0)
            unknown=first_unknown(first,last);result=payload(-1,9004,unknown) if unknown is not None else payload(0,0);anchor=unknown
            if offsets:
                start=(EPOCH+dt.timedelta(seconds=first+min(offsets))).replace(tzinfo=None);finish=(EPOCH+dt.timedelta(seconds=last+max(offsets))).replace(tzinfo=None)
                candidates=[]
                for label in labels(profile,start,finish):
                    uncertain,values=resolve(local_second(label))
                    for at in values:
                        if not first<=at<=last:continue
                        if uncertain:code=9004
                        elif len(values)>1 and fold==3:code=9005
                        elif (fold==1 and at!=values[0]) or (fold==2 and at!=values[-1]):continue
                        else:code=0
                        candidates.append((at,code,len(values) if not uncertain else 0,label))
                if candidates:
                    at,code,count,label=min(candidates,key=lambda v:v[0])
                    if anchor is None or at<anchor:result=payload(-1 if code else 1,code,at,count,label);anchor=at
            if missing:
                boundaries=set(times[bisect.bisect_left(times,first):bisect.bisect_right(times,last)])|{first}
                for year in range((EPOCH+dt.timedelta(seconds=first)).year-1,(EPOCH+dt.timedelta(seconds=last)).year+2):
                    boundaries.update(t for t in footer_transitions(footer,year) if first<=t<=last and (not times or t>times[-1]))
                for at in sorted(boundaries):
                    if anchor is not None and at>anchor:break
                    before,after=state(at-1),state(at)
                    if before is None or after is None:
                        result=payload(-1,9004,at);anchor=at;break
                    if after<=before:continue
                    start=(EPOCH+dt.timedelta(seconds=at+before)).replace(tzinfo=None);finish=(EPOCH+dt.timedelta(seconds=at+after-1)).replace(tzinfo=None)
                    for label in labels(profile,start,finish):
                        uncertain,values=resolve(local_second(label))
                        if uncertain or not values:
                            result=payload(-1,9004 if uncertain else 9006,at,0,label);anchor=at;break
                    if anchor==at and result[0]==-1:break
            return result
        selected=[s for s in times if -2208988800<=s<=4133980800];selected=sorted(set(selected[:2]+selected[-6:]+[millis(dt.datetime(y,3,1,tzinfo=UTC))//1000 for y in (2024,2100,2400)]))
        cases=[];expectations=[]
        for number,profile in enumerate(PROFILES[:4]):
            for j,second in enumerate(selected):
                begin=second*1000-12*3600000+(j%2);end=second*1000+36*3600000;flags=j%2
                for fold in range(4):
                    for missing in range(2):cases.append(f'{begin+XRT_EPOCH} {end+XRT_EPOCH} {flags} {fold} {missing} {number}\n');expectations.append(expected(profile,begin,end,flags,fold,missing))
        run=subprocess.run([args.probe,str(args.data/name)],input=''.join(cases),text=True,capture_output=True,timeout=180,check=True);lines=run.stdout.splitlines();assert lines.pop(0)=='selected_occurrence=2 sync_pair_cursor=3' and len(lines)==len(cases)
        for index,(line,want) in enumerate(zip(lines,expectations)):
            fields=list(map(int,line.split()));assert fields[:13]==want,(name,cases[index],fields[:13],want)
            totals['queries']+=1;totals['unknown' if want[1]==9004 else 'ambiguous' if want[1]==9005 else 'missing' if want[1]==9006 else 'found' if want[0] else 'none']+=1
            for key,value in zip(('cursor_steps','segments','months','rule_years','checks','offsets','labels'),fields[13:]):totals[key]+=value
        files.append(dict(name=name,sha256=digest,queries=len(cases),stdout_sha256=hashlib.sha256(run.stdout.encode()).hexdigest()))
    totals['synchronous_calls']=totals['queries']*2;totals['cursor_initializations']=totals['queries'];totals['api_calls']=totals['queries']*3+totals['cursor_steps']
    args.output.write_text(json.dumps(dict(schema=1,version=manifest['version'],zones=len(files),totals=totals,files=files,oracle='Explicit Gregorian labels, all distinct-offset zoneinfo/RFC states and global selection; gap labels anchored at complete stored and independently calculated footer M-rule transitions; per-Step bounds and pending atomicity physically checked by required probe protocol'),indent=2)+'\n');print('[PASS] selected occurrence IANA',len(files),totals)
if __name__=='__main__':main()
