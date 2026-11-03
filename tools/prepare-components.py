#!/usr/bin/env python3
# Creates a LOCAL dependency directory, never part of the public DMG/repository.
from pathlib import Path
import argparse,shutil,zipfile,configparser,re,hashlib
parser=argparse.ArgumentParser()
for name in ('pd-loader','divagl','wine','vc-redist','output'):parser.add_argument('--'+name,type=Path,required=True)
args=parser.parse_args()
for file,expected in ((args.pd_loader,'113a6f6c5bc658184bc324986a5f1bf03868329d42a27e9bc115ecdf5314bf45'),(args.divagl,'e913fb873f14b0ded08dd801b9525b12ed00497d373363a1b3a23e9189efe003')):
 if hashlib.sha256(file.read_bytes()).hexdigest()!=expected:raise SystemExit('Unsupported dependency checksum: '+file.name)
if not (args.wine/'bin/wine64').is_file() or not args.vc_redist.is_file():raise SystemExit('Missing Wine or VC runtime')
resources=args.output
resources.mkdir(parents=True,exist_ok=False)
b=resources/'Bootstrap';b.mkdir()
with zipfile.ZipFile(args.pd_loader) as z:
 for n in z.namelist():
  if n.startswith('plugins/') and not n.endswith('/'):
   dest=b/n;dest.parent.mkdir(parents=True,exist_ok=True);dest.write_bytes(z.read(n))
 (b/'dinput8.dll').write_bytes(z.read('dnsapi.dll'))
shutil.copyfile(args.divagl,b/'plugins/DivaGL.dva')
cfg=configparser.ConfigParser(interpolation=None,strict=False,comment_prefixes=('#',';'));cfg.optionxform=str;cfg.read(b/'plugins/config_template.bin',encoding='utf-8-sig')
changes={'GPU':{'Model':'0'},'Launcher':{'Skip':'1','No_GPU_Dialog':'1','Use_divahook_bat':'0'},'Patches':{'No_Movies':'0','Auto_Databank':'1','Prevent_Data_Deletion':'1','Custom_Patches':'1','Quick_Start':'2','Card':'0'},'Graphics':{'FPS.Limit':'60','FPS.Limit.LightMode':'1','TAA':'0','MLAA':'0','DOF':'0','Reflections':'1','Shadows':'0','Glare':'0','2D':'0'},'Resolution':{'Display':'0','Width':'1280','Height':'720','RefreshRate':'60','r.Enable':'1','r.Width':'1280','r.Height':'720'},'plugins':{'Launcher.dva':'0','Novidia.dva':'0','ShaderPatch.dva':'0','DivaMovie.dva':'0','DivaWig.dva':'0','DSCRemote.dva':'0','DivaGL.dva':'1','TLAC.dva':'1','Render.dva':'1','Patches.dva':'1','DivaSound.dva':'1','custom_freeplay_text_example.p':'0'}}
for section,items in changes.items():
 if not cfg.has_section(section):cfg.add_section(section)
 cfg[section].update(items)
with (b/'plugins/config.ini').open('w') as f:cfg.write(f)
for name in ('components','keyconfig','playerdata','DivaSound'):
 s=(b/('plugins/'+name+'_template.bin')).read_text(encoding='utf-8-sig')
 edits={'components':{'score_saver':'false','player_data_manager':'true','debug_component':'false','fast_loader':'true','pause':'true'},'playerdata':{'player_name':'GUEST','use_card':'false','module_card_workaround':'true'}}.get(name,{})
 for k,v in edits.items():s=re.sub(r'(?m)^'+re.escape(k)+r'\s*=.*$',k+'='+v,s)
 (b/('plugins/'+name+'.ini')).write_text(s)
(b/'plugins/DivaGL.ini').write_text('[experimentals]\nshared_storage_uniform_buffer=1\ntask_movie_player_no_interop=0\ntexture_skinning_buffer=0\n')
(resources/'Wine').symlink_to(args.wine.resolve(),target_is_directory=True)
(resources/'vc_redist.x64.exe').symlink_to(args.vc_redist.resolve())
print(resources)
