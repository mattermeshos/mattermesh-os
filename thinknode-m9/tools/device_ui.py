"""USB integration client for MatterMesh OS. Never prints private file contents."""
import time,re
from pathlib import Path
import serial
from PIL import Image
class Device:
 def __init__(self,port='/dev/ttyACM0'):
  self.s=serial.Serial(port,115200,timeout=.4);self.s.dtr=False;self.s.rts=False
  self.sequence=0;self.boot='';end=time.monotonic()+25
  while time.monotonic()<end:
   self.boot+=self.s.readline().decode(errors='replace')
   if 'STATUS' in self.boot and self.boot.endswith('\n'):break
  else:raise RuntimeError('Boot/status timeout')
  self.s.reset_input_buffer()
 def close(self):self.s.close()
 def wait(self,marker,timeout=10):
  data='';end=time.monotonic()+timeout
  while time.monotonic()<end:
   data+=self.s.readline().decode(errors='replace')
   if marker in data and data.endswith('\n'):return data
  if data:
   Path('build-reports/usb-timeout.log').write_text(data)
  raise RuntimeError('USB response timeout: '+marker+' '+data[-500:])
 def send(self,command):
  self.sequence+=1;time.sleep(.025);self.s.write((f'@{self.sequence} '+command+'\n').encode());self.s.flush();return self.sequence
 def done(self,sequence):return self.wait(f'USB_DONE {sequence}')
 def cmd(self,command,marker='UI_READY'):
  sequence=self.send(command);data=self.wait(marker);data+=self.done(sequence);return data
 def status(self):
  data=self.cmd('status','STATUS');line=[v for v in data.splitlines() if v.startswith('STATUS')][-1]
  return {k:int(v) for k,v in re.findall(r'(\w+)=(\d+)',line)}
 def tap(self,x,y):return self.cmd(f'tap {x} {y}')
 def event(self,code,pressed=True):
  sequence=self.send('event '+str(code|(128 if pressed else 0)));self.done(sequence)
 def keycode(self,code):self.event(code);self.event(code,False);time.sleep(.15)
 def capture(self,path):
  sequence=self.send('shot');line=b'';end=time.monotonic()+8
  while time.monotonic()<end:
   line+=self.s.readline()
   if b'PBM_BEGIN ' in line and line.endswith(b'\n'):break
  else:raise RuntimeError('Screenshot timeout')
  n=int(line.split(b'PBM_BEGIN ')[-1].split()[0]);header=self.s.readline()+self.s.readline();data=bytearray()
  while len(data)<n:
   chunk=self.s.read(n-len(data))
   if not chunk:raise RuntimeError('Incomplete framebuffer')
   data.extend(chunk)
  self.s.readline();assert self.s.readline().strip()==b'PBM_END';self.done(sequence)
  p=Path(path);p.parent.mkdir(parents=True,exist_ok=True);p.with_suffix('.pbm').write_bytes(header+data)
  im=Image.open(p.with_suffix('.pbm'));im.save(p.with_suffix('.png'));im.resize((im.width*3,im.height*3),Image.Resampling.NEAREST).save(p.with_name(p.stem+'-3x.png'))
  return im
 def read_file(self,path):
  sequence=self.send('read '+path);line=b'';end=time.monotonic()+5
  while time.monotonic()<end:
   line+=self.s.readline()
   if b'FILE_ERROR' in line:self.done(sequence);return None
   if b'FILE_BEGIN ' in line and line.endswith(b'\n'):break
  else:raise RuntimeError('File header timeout')
  n=int(line.split(b'FILE_BEGIN ')[-1].split()[0]);data=bytearray()
  idle=time.monotonic()+5
  while len(data)<n:
   b=self.s.read(min(8192,n-len(data)))
   if not b:
    # The device may pause for SD/e-paper work; only give up after 5 s of silence.
    if time.monotonic()>idle:raise RuntimeError('File transfer timeout')
    continue
   idle=time.monotonic()+5;data.extend(b)
  self.s.readline();assert self.s.readline().strip()==b'FILE_END';self.done(sequence);return bytes(data)
