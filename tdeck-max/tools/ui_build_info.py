Import('env')
from pathlib import Path
import subprocess,hashlib,json,re
root=Path(env['PROJECT_DIR'])
release_path=root/'firmware_version.json'
release=json.loads(release_path.read_text())
version=release['version'];build=release['build'];channel=release['channel']
if not re.fullmatch(r'\d+\.\d+\.\d+',version) or type(build) is not int or build<0 or not re.fullmatch(r'[A-Z]+',channel):raise ValueError('Invalid firmware_version.json')
build_version=version+'.'+str(build)
# Same formula as tools/ota_publish.py version_code(): the OTA server compares these integers.
major,minor,patch=(int(x) for x in version.split('.'));version_code=((major*100+minor)*100+patch)*1000+build
# "scale": 100 (Max from 0.6.0.4): every build leaves 99 codes below it for its test builds ("test": n, test channel),
# so 0.6.0.5 TEST 1, TEST 2... come after 0.6.0.4 and before 0.6.0.5. Codes stay above every older build.
scale=release.get('scale',1);test=release.get('test',0)
if type(scale) is not int or scale not in(1,100) or type(test) is not int or not 0<=test<=99 or (test and scale==1):raise ValueError('Invalid scale/test in firmware_version.json')
if scale==100:version_code=(version_code*100) if not test else (version_code-1)*100+test
if test:build_version+='-test'+str(test)
label=channel+' '+version+'.'+str(build)+(' TEST '+str(test) if test else '')
pin=json.loads((root/'meshcore_dependency.json').read_text())
mesh_commit=subprocess.check_output(['git','rev-parse','HEAD'],cwd=root/'meshcore-base',text=True).strip()
if mesh_commit!=pin['commit']:raise ValueError('MeshCore checkout differs from pinned release')
mesh_header=(root/'meshcore-base/examples/companion_radio/MyMesh.h').read_text()
mesh_version=re.search(r'^#define FIRMWARE_VERSION "v([^"\n]+)"',mesh_header,re.M).group(1)
mesh_text='#pragma once\n#define MM_MESHCORE_VERSION "'+mesh_version+'"\n#define MM_MESHCORE_COMMIT "'+mesh_commit+'"\n'
mesh_out=root/'src/mesh/version.h'
if not mesh_out.exists() or mesh_out.read_text()!=mesh_text:mesh_out.write_text(mesh_text)

try:commit=subprocess.check_output(['git','rev-parse','--short','HEAD'],cwd=root,stderr=subprocess.DEVNULL,text=True).strip()
except subprocess.CalledProcessError:commit='uncommitted'
hash_=hashlib.sha256()
hash_.update(release_path.read_bytes())
for p in sorted((root/'src').rglob('*')):
 if p.is_file() and p.name!='build_info.h':hash_.update(p.read_bytes())
text='#pragma once\n#define MM_VERSION "'+version+'"\n#define MM_BUILD_NUMBER '+str(build)+'\n#define MM_VERSION_CODE '+str(version_code)+'\n#define MM_BUILD_VERSION "'+build_version+'"\n#define MM_RELEASE_LABEL "'+label+'"\n#define MM_GIT_COMMIT "'+commit+'"\n#define MM_SOURCE_HASH "'+hash_.hexdigest()[:12]+'"\n'
p=root/'src/ui/build_info.h'
if not p.exists() or p.read_text()!=text:p.write_text(text)
