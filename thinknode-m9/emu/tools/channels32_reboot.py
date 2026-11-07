#!/usr/bin/env python3
# Usage: emu/tools/channels32_reboot.py <tree>   (emu/tests/channels32_plus.sh)
"""32 conversations across a reboot: import channels until the T-Deck holds 32, restart both emulators on the same
data, send on the last slot (>= 24) from Alpha, Bravo must receive it; then a CLEAR row for that chat on Bravo must
survive another restart (retained=0)."""
import os,re,shutil,socket,struct,subprocess,sys,time,tempfile
sys.path.insert(0,os.path.join(sys.argv[1],'emu/tools'))
from companion_client import Companion
TREE=sys.argv[1];os.chdir(TREE)
d=tempfile.mkdtemp();port=7900+os.getpid()%400
air=subprocess.Popen(['artifacts/emu-air','--port',str(port),'--log',d+'/air.log'],stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL);time.sleep(0.5)
names={'Alpha':1100+os.getpid()%300,'Bravo':1500+os.getpid()%300}
for n in names:
    os.makedirs(d+'/'+n);shutil.copytree('emu/profiles/configured/sd',d+'/'+n+'/sd');shutil.copy('emu/profiles/configured/nvs.json',d+'/'+n)
    open(d+'/'+n+'/sd/mattermesh/radio.conf','w').write('869.618\t62.50\t8\t8\t22\t%s\t2\n'%n)
def start(n,extra,quit_ms,first=False):
    # "app wifi" toggles the saved setting: only on the first boot
    s=d+'/'+n+'.emu';open(s,'w').write(('3000 serial wifi on\n3500 serial wifi join {"ssid":"EMU-WiFi","password":"meshcore"}\n7200 serial app wifi\n' if first else '')+extra+'%d quit\n'%quit_ms)
    log=open(d+'/'+n+'.log','w')
    p=subprocess.Popen(['artifacts/emu-plus','--headless','--data',d+'/'+n,'--air',str(port),'--port-offset',str(names[n]+port%100*0),'--script',s],stdin=subprocess.DEVNULL,stdout=log,stderr=subprocess.STDOUT)
    return p
def app(n):
    for _ in range(60):
        if 'EMU_WIFI listening' in open(d+'/'+n+'.log').read(): break
        time.sleep(0.5)
    c=None
    for _ in range(40):
        try: c=Companion('127.0.0.1',5000+names[n],timeout=10);break
        except OSError: time.sleep(0.5)
    assert c,n+' app link not up';c.request(bytes([22,3]),want={13});c.request(bytes([1])+bytes(7)+b't',want={5});return c
def channels(c):
    out=[]
    for i in range(64):
        r=c.request(bytes([31,i]),want={1,18},timeout=2)
        if not r or r[0]!=18: break
        name=r[2:34].split(b'\0')[0].decode()
        if name: out.append(name)
    return out
try:
    # run 1: import on both
    ps={n:start(n,'',240000,True) for n in names}
    for n in names:
        c=app(n);i=0
        while len(channels(c))<32 and i<40:
            sec=bytes([0x50+i])+bytes([i])*15
            assert c.request(bytes([32,0xFF])+('Imp%02d'%i).encode().ljust(32,b'\0')+sec,want={0,1})==bytes([0]);i+=1
        print(n,'channels before reboot',len(channels(c)))
    time.sleep(3)  # last catalog writes
    for p in ps.values(): p.terminate();p.wait(30)
    os.rename(d+'/Alpha.log',d+'/Alpha.run1.log')
    # run 2: reboot, check slots >= 24 ready, send on the last one
    ps={n:start(n,'9000 serial channel list\n',70000) for n in names}
    ca,cb=app('Alpha'),app('Bravo');time.sleep(3)
    for _ in range(40):
        la=open(d+'/Alpha.log').read()
        if 'CHANNEL_LIST' in la: break
        time.sleep(0.5)
    m=re.search(r'(\[\{.*\}\])',la[la.find('"id":'):] if False else la)
    ready=re.findall(r'"id":(\d+),"name":"([^"]*)","direct":(?:true|false),"muted":(?:true|false),"ready":(true|false)',la)
    high=[(int(i),nm,r) for i,nm,r in ready if int(i)>=24]
    print('Alpha slots >= 24 after reboot:',high)
    assert high and all(r=='true' for _,_,r in high),'channels in slots 24..31 not ready after reboot'
    chs=channels(ca);idx=len(chs)-1;name=chs[idx]
    assert name==channels(cb)[idx]
    ts=int(time.time());ok=ca.request(bytes([3,0,idx])+struct.pack('<I',ts)+b'hello slot31',want={0,1})
    assert ok==bytes([0]),('send failed',ok)
    got=None;end=time.time()+25
    while time.time()<end and not got:
        f=cb.request(bytes([10]),want={10,8,17},timeout=3)
        if f and f[0] in (8,17) and b'hello slot31' in f: got=f
        else: time.sleep(1)
    assert got,'Bravo did not receive on the last channel'
    print('Bravo received on app channel',idx,'(%s)'%name)
    for p in ps.values(): p.wait(80)
    # run 3: CLEAR row for that chat on Bravo (as clearChat writes it), restart: history must stay empty
    slot=max(int(i) for i,nm,r in ready if nm==name)
    open(d+'/Bravo/sd/mattermesh/messages.tsv','a').write('D1\t%d\t%d\tCLEAR\n'%(slot,ts+5))
    p=start('Bravo','12000 serial catalog status\n',16000);p.wait(30)
    row=re.search(r'CATALOG_ROW id=%d name=\S+ direct=0 retained=(\d+)'%slot,open(d+'/Bravo.log').read())
    print('Bravo chat',slot,'retained after CLEAR + reboot:',row and row.group(1))
    assert row and row.group(1)=='0','cleared chat >= 24 got its history back'
    print('PASS: 32 conversations survive a reboot; slot',slot,'sends/receives and stays cleared')
finally:
    air.kill();subprocess.run(['pkill','-f','--data '+d],stderr=subprocess.DEVNULL);shutil.rmtree(d,ignore_errors=True)
