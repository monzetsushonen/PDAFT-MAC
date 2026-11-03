#!/usr/bin/env python3
from pathlib import Path
import subprocess,plistlib,argparse,shutil,os
p=Path(__file__).resolve().parents[1]
a=argparse.ArgumentParser();a.add_argument('--output',type=Path,required=True);v=a.parse_args();app=v.output/'PDAFT MAC.app';r=app/'Contents/Resources';f=app/'Contents/Frameworks';m=app/'Contents/MacOS'
if app.exists():raise SystemExit('Output already exists')
for d in (r,f,m):d.mkdir(parents=True)
def build(*args):subprocess.run(['xcrun','clang++','-std=c++17','-fobjc-arc','-O2','-mmacosx-version-min=13.0',*map(str,args)],check=True)
build('-arch','arm64','-dynamiclib',p/'native/MetalFX/PDAFTSpatialUpscaler.mm','-framework','Foundation','-framework','Metal','-framework','MetalFX','-install_name','@rpath/libPDAFTMetalFX.dylib','-o',f/'libPDAFTMetalFX.dylib')
build('-arch','x86_64','-dynamiclib',p/'native/FrameBridge/gl_surface_producer.mm','-framework','Foundation','-framework','OpenGL','-framework','IOSurface','-o',r/'libPDAFTFrameProducer.dylib')
build('-arch','arm64',p/'native/FrameBridge/MetalPresenterApp.mm','-L'+str(f),'-lPDAFTMetalFX','-Wl,-rpath,@executable_path/../Frameworks',*[x for framework in ('AppKit','MetalKit','Metal','MetalFX','IOSurface','QuartzCore') for x in ('-framework',framework)],'-o',m/'PDAFTMetalFX')
build('-arch','arm64',p/'native/Localization/PDAFTLocalizer.mm','-framework','AppKit','-framework','CoreText','-lz','-o',r/'PDAFTLocalizer')
for file in (p/'tools/app').iterdir():
 dest=r/('launch-game.sh' if file.name=='launch-game-app.sh' else file.name);shutil.copyfile(file,dest);dest.chmod(0o755)
(app/'Contents/Info.plist').write_bytes(plistlib.dumps(dict(CFBundleIdentifier='io.github.monzetsushonen.pdaftmac',CFBundleName='PDAFT MAC',CFBundleExecutable='PDAFTMetalFX',CFBundleShortVersionString='0.2.4',CFBundleVersion='24',CFBundlePackageType='APPL',LSMinimumSystemVersion='13.0',NSHighResolutionCapable=True)))
for file in (p/'README.md',p/'THIRD-PARTY.md',p/'LICENSE'):shutil.copyfile(file,r/file.name)
for file in app.rglob('._*'):file.unlink()
for file in (f/'libPDAFTMetalFX.dylib',r/'libPDAFTFrameProducer.dylib',r/'PDAFTLocalizer'):subprocess.run(['codesign','--force','--sign','-',str(file)],check=True)
for file in app.rglob('._*'):file.unlink()
subprocess.run(['codesign','--force','--sign','-',str(app)],check=True)
subprocess.run(['codesign','--verify','--strict',str(app)],check=True)
print(app)
