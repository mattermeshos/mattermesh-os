"""A // comment ends the line: code written after it on the same line is silently dropped (in 0.6.0.0 - 0.6.0.2 a
comment in the middle of the long uiState() line hid the region scope picker, the clear-chat confirmation and the
lock-screen preview). Fails when a line comment is followed, on the same line, by something that looks like code (also a bare break; /
continue; / return; glued to the comment text, as "...the e-inkbreak;", or a declaration such as "...(GPS on)bool gpsActive();")."""
import re,sys,pathlib
code=re.compile(r'(;[A-Za-z_][\w:.\[\]>-]*(=[^=]|\()|[A-Za-z_]\w*(\.|->)[A-Za-z_]\w*=[^=]|\)\{|\b(if|for|while)\([^)]*\)\s*[A-Za-z_{]|(break|continue|return)\s*;|\b(bool|void|int|double|float|String|u?int\d+_t)\s+[A-Za-z_]\w*\s*\([^)]*\)\s*[;{]|\bconstexpr\s+[A-Za-z_][\w:<>]*\s+[A-Za-z_]\w*\s*[=({\[])')
def strip_strings(l):
    out=[];i=0;q=None
    while i<len(l):
        ch=l[i]
        if q:
            if ch=='\\':i+=2;continue
            if ch==q:q=None
            out.append(' ');i+=1;continue
        if ch in '"\'':q=ch;out.append(' ');i+=1;continue
        out.append(ch);i+=1
    return ''.join(out)
hits=0
for root in sys.argv[1:] or ['src']:
    for p in pathlib.Path(root).rglob('*'):
        if p.suffix not in ('.cpp','.h','.inc','.c') or 'qrcodegen' in p.name or '_font' in p.name or 'bitmap' in p.name: continue
        for n,l in enumerate(p.read_text(errors='replace').splitlines(),1):
            s=re.sub(r'/\*.*?\*/',lambda m:' '*len(m.group()),strip_strings(l));i=s.find('//')
            if i<0:continue
            if code.search(s[i+2:]):hits+=1;print(f'{p}:{n}: {l[max(0,i-40):i+120]}')
if hits:print(f'FAIL: {hits} line comment(s) followed by code');sys.exit(1)
print('PASS: no // comment hides code')
