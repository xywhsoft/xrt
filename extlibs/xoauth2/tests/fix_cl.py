#!/usr/bin/env python3
"""修正测试 canned 响应的 Content-Length（工具计数，勿手写）+ 清理调试行。"""
import io
import re

p = 'test_oauth2.c'
s = io.open(p, encoding='utf-8').read()


def fix_cl(text, body):
    n = len(body.encode())
    idx = text.find(body)
    assert idx >= 0, body[:30]
    head_start = text.rfind('HTTP/1.1', 0, idx)
    seg = text[head_start:idx]
    seg2 = re.sub(r'Content-Length: \d+', 'Content-Length: %d' % n, seg, count=1)
    return text[:head_start] + seg2 + text[idx:]


Q = chr(34)  # 引号
bodies = [
    '{' + Q + 'access_token' + Q + ':' + Q + 'loop_tok' + Q + ','
        + Q + 'token_type' + Q + ':' + Q + 'bearer' + Q + ','
        + Q + 'expires_in' + Q + ':900}',
    '{' + Q + 'error' + Q + ':' + Q + 'invalid_grant' + Q + '}',
    '{' + Q + 'access_token' + Q + ':' + Q + 'rf2' + Q + ','
        + Q + 'expires_in' + Q + ':600}',
    '{' + Q + 'access_token' + Q + ':' + Q + 'heap-rt' + Q + ','
        + Q + 'expires_in' + Q + ':600}',
]
for b in bodies:
    s = fix_cl(s, b)
    print('CL ->', len(b.encode()), 'for', b[:40])

marks = ['[mark] p4b-part-start', '[mark] fuzz-start', '[mark] p4b-heap-start',
         '[mark] heap-created', '[mark] heap-refresh', '[mark] heap-refresh-done',
         '[mark] heap-destroyed']
for m in marks:
    lines = [l for l in s.split(chr(10)) if m in l]
    for l in lines:
        s = s.replace('	' + l + chr(10), '', 1)
        s = s.replace(l + chr(10), '', 1)
s = s.replace('	setvbuf(stdout, NULL, _IONBF, 0);' + chr(10), '', 1)
io.open(p, 'w', encoding='utf-8', newline='').write(s)
print('cleaned')
