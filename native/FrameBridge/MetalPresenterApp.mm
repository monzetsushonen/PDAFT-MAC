#import <AppKit/AppKit.h>
#import <MetalKit/MetalKit.h>
#import <IOSurface/IOSurface.h>
#import "../MetalFX/PDAFTSpatialUpscaler.h"
#include "FrameProtocol.h"
#include "HDRPresentation.h"
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>
#include <atomic>
#include <sys/mount.h>
#include <mach/mach_time.h>
#include <notify.h>

static NSString *labPath;
static NSString *controlPath;
// Remember the backing image so a later reboot does not lose the working disk.
static NSData *diskImageTask(NSArray<NSString*> *arguments) {
    NSTask *task=[NSTask new];task.executableURL=[NSURL fileURLWithPath:@"/usr/bin/hdiutil"];task.arguments=arguments;
    NSPipe *pipe=[NSPipe pipe];task.standardOutput=pipe;task.standardError=[NSFileHandle fileHandleWithNullDevice];
    if(![task launchAndReturnError:nil])return nil;
    NSData *data=[pipe.fileHandleForReading readDataToEndOfFile];[task waitUntilExit];return task.terminationStatus==0?data:nil;
}
static FILE *report;
static PDAFTFrameControl *sharedControl;

@interface GameView : MTKView
@end
@implementation GameView
- (BOOL)acceptsFirstResponder{return YES;}
- (int)virtualKey:(NSEvent*)e{
    switch(e.keyCode){case 36:return 13;case 123:return 37;case 124:return 39;case 125:return 40;case 126:return 38;case 49:return 32;default:break;}
    NSString *s=e.charactersIgnoringModifiers.uppercaseString;
    if(s.length==1 && [s characterAtIndex:0]<128)return [s characterAtIndex:0];return -1;
}
- (void)keyDown:(NSEvent*)e{int k=[self virtualKey:e];if(sharedControl && k>=0){sharedControl->key_events++;__atomic_store_n(&sharedControl->pending_taps[k],1,__ATOMIC_RELEASE);__atomic_store_n(&sharedControl->keys[k],1,__ATOMIC_RELEASE);}}
- (void)keyUp:(NSEvent*)e{int k=[self virtualKey:e];if(sharedControl && k>=0)__atomic_store_n(&sharedControl->keys[k],0,__ATOMIC_RELEASE);}
- (BOOL)acceptsFirstMouse:(NSEvent*)e{(void)e;return YES;}
- (void)mousePosition:(NSEvent*)e{if(!sharedControl)return;NSPoint p=[self convertPoint:e.locationInWindow fromView:nil];sharedControl->mouse_x=p.x/self.bounds.size.width*1280;sharedControl->mouse_y=(1-p.y/self.bounds.size.height)*720;sharedControl->mouse_enabled=1;}
- (void)mouseDown:(NSEvent*)e{[self.window makeFirstResponder:self];[self mousePosition:e];if(sharedControl){__atomic_store_n(&sharedControl->pending_taps[1],1,__ATOMIC_RELEASE);__atomic_store_n(&sharedControl->keys[1],1,__ATOMIC_RELEASE);}}
- (void)mouseUp:(NSEvent*)e{[self mousePosition:e];if(sharedControl)__atomic_store_n(&sharedControl->keys[1],0,__ATOMIC_RELEASE);}
@end

@interface Presenter : NSObject <NSApplicationDelegate,MTKViewDelegate,NSWindowDelegate>
@end
@implementation Presenter {
    NSWindow *_window;
    MTKView *_view;
    PDAFTFrameControl *_control;
    id<MTLCommandQueue> _queue;
    PDAFTSpatialUpscaler *_scaler;
    id<MTLTexture> _inputs[PDAFT_FRAME_SLOTS],_hdrMasks[PDAFT_FRAME_SLOTS],_output;
    std::atomic<uint64_t> _presented;
    uint64_t _submitted,_previousPresented;
    double _lastLog;
    NSTask *_gameTask;
    id _activity;
    NSMenu *_languageMenu;
    BOOL _restartAfterLanguage;
    NSUInteger _outputWidth,_outputHeight;
    NSDictionary *_launchOptions;
    NSWindow *_launchWindow;
    PDAFTHDRPresentation *_hdr;
    BOOL _hdrRequested,_hdrBypass;
    NSMenuItem *_hdrToggleMenuItem;
    double _lastHDRMeasure;
    uint64_t _previousFrameReadyNs,_staleFrames;
    int _frameNotifyToken;
}
#include "LaunchSettings.h"
- (void)applicationDidFinishLaunching:(NSNotification *)notification {
    (void)notification;
    setenv("MTL_HUD_ENABLED","1",0);setenv("MTL_HUD_LOG_ENABLED","1",0);
    _activity=[NSProcessInfo.processInfo beginActivityWithOptions:NSActivityUserInitiated|NSActivityLatencyCritical reason:@"PDAFT game presentation"];
    labPath=[NSUserDefaults.standardUserDefaults stringForKey:@"PDAFTWorkDirectory"];
    if(labPath && ![[NSFileManager defaultManager]fileExistsAtPath:labPath]){
        NSString *image=[NSUserDefaults.standardUserDefaults stringForKey:@"PDAFTWorkImage"];
        if(image && [[NSFileManager defaultManager]fileExistsAtPath:image])diskImageTask(@[@"attach",@"-nobrowse",image]);
    }
    if(!labPath || ![[NSFileManager defaultManager]fileExistsAtPath:labPath]){
        NSOpenPanel *panel=[NSOpenPanel openPanel];panel.canChooseDirectories=YES;panel.canChooseFiles=NO;
        panel.canCreateDirectories=YES;panel.directoryURL=[NSURL fileURLWithPath:@"/Volumes"];
        panel.message=@"选择外置 APFS 盘上的运行目录。Wine 前缀、配置和日志保存在此处，游戏资源保留原位。";
        if([panel runModal]!=NSModalResponseOK){[NSApp terminate:nil];return;}
        labPath=[panel.URL.path stringByAppendingPathComponent:@"PDAFT Support"];
    }
    struct statfs filesystem;
    NSString *parent=[labPath stringByDeletingLastPathComponent];
    if(![labPath hasPrefix:@"/Volumes/"] || statfs(parent.fileSystemRepresentation,&filesystem) || strcmp(filesystem.f_fstypename,"apfs")){
        NSAlert *alert=[NSAlert new];alert.messageText=@"需要外置 APFS 运行目录";
        alert.informativeText=@"游戏资源可以位于 exFAT 盘；Wine 运行目录需要 APFS 以支持符号链接。请接回测试盘后重试。";
        [alert runModal];[NSUserDefaults.standardUserDefaults removeObjectForKey:@"PDAFTWorkDirectory"];[NSApp terminate:nil];return;
    }
    [NSUserDefaults.standardUserDefaults setObject:labPath forKey:@"PDAFTWorkDirectory"];
    NSData *images=diskImageTask(@[@"info",@"-plist"]);
    NSDictionary *inventory=images?[NSPropertyListSerialization propertyListWithData:images options:0 format:nil error:nil]:nil;
    [NSUserDefaults.standardUserDefaults removeObjectForKey:@"PDAFTWorkImage"];
    for(NSDictionary *image in inventory[@"images"]){
        for(NSDictionary *entity in image[@"system-entities"]){
            NSString *mount=entity[@"mount-point"];
            if(mount && [labPath hasPrefix:[mount stringByAppendingString:@"/"]] && image[@"image-path"])
                [NSUserDefaults.standardUserDefaults setObject:image[@"image-path"] forKey:@"PDAFTWorkImage"];
        }
    }
    NSString *bridge=[labPath stringByAppendingPathComponent:@"bridge"];
    NSError *folderError=nil;
    if(![[NSFileManager defaultManager]createDirectoryAtPath:bridge withIntermediateDirectories:YES attributes:nil error:&folderError]){[NSApp terminate:nil];return;}
    if(![self showLaunchSettings:YES]){[NSApp terminate:nil];return;}
    report=fopen([[bridge stringByAppendingPathComponent:@"presenter.jsonl"] fileSystemRepresentation],"a");
    controlPath=[bridge stringByAppendingPathComponent:@"control.bin"];
    int fd=open(controlPath.fileSystemRepresentation,O_CREAT|O_TRUNC|O_RDWR,0600);
    if(fd<0 || ftruncate(fd,PDAFT_FRAME_MAP_SIZE)){[NSApp terminate:nil];return;}
    _control=(PDAFTFrameControl*)mmap(NULL,PDAFT_FRAME_MAP_SIZE,PROT_READ|PROT_WRITE,MAP_SHARED,fd,0);close(fd);
    if(_control==MAP_FAILED){[NSApp terminate:nil];return;}
    memset(_control,0,PDAFT_FRAME_MAP_SIZE);_control->version=2;
    sharedControl=_control;_control->auto_play=[_launchOptions[@"autoplay"]boolValue];_control->input_active=1;_control->hdr_mv_only=[_launchOptions[@"hdr"]boolValue];
    __atomic_store_n(&_control->magic,PDAFT_FRAME_MAGIC,__ATOMIC_RELEASE);
    id<MTLDevice> device=MTLCreateSystemDefaultDevice();_queue=[device newCommandQueue];
    _window=[[NSWindow alloc]initWithContentRect:NSMakeRect(0,0,960,540)
        styleMask:NSWindowStyleMaskTitled|NSWindowStyleMaskClosable|NSWindowStyleMaskMiniaturizable|NSWindowStyleMaskResizable
        backing:NSBackingStoreBuffered defer:NO];
    _window.title=[NSString stringWithFormat:@"PDAFT MetalFX — %lup · waiting for game frames",(unsigned long)_outputHeight];
    // During reference testing, keep the native output above Wine's auxiliary window.
    _window.level=NSFloatingWindowLevel;
    [_window center];
    _view=[[GameView alloc]initWithFrame:_window.contentView.bounds device:device];
    _view.autoresizingMask=NSViewWidthSizable|NSViewHeightSizable;
    if(![self configureDynamicRange]){[NSApp terminate:nil];return;}
    _view.autoResizeDrawable=NO;_view.drawableSize=CGSizeMake(_outputWidth,_outputHeight);
    _view.framebufferOnly=NO;_view.preferredFramesPerSecond=120;
    _view.delegate=self;_window.contentView=_view;
    // Two unrelated 60Hz clocks can miss each other and display only 30–45fps.
    // Render after the producer publishes a completed surface instead of polling.
    _frameNotifyToken=-1;__weak Presenter *weakPresenter=self;
    NSString *frameNotification=[NSString stringWithFormat:@"local.pdaft.frame.%d",getpid()];
    uint32_t notifyStatus=notify_register_dispatch(frameNotification.UTF8String,&_frameNotifyToken,dispatch_get_main_queue(),^(int token){
        (void)token;Presenter *presenter=weakPresenter;if(presenter)[presenter->_view draw];
    });
    if(notifyStatus==NOTIFY_STATUS_OK){_view.paused=YES;_view.enableSetNeedsDisplay=NO;}
    if(report){fprintf(report,"{\"event\":\"frame_schedule\",\"producer_notifications\":%s,\"fallback_poll_hz\":120}\n",notifyStatus==NOTIFY_STATUS_OK?"true":"false");fflush(report);}
    NSMenu *menu=[NSMenu new];NSMenuItem *root=[NSMenuItem new];[menu addItem:root];
    NSMenu *app=[NSMenu new];[app addItemWithTitle:@"Quit PDAFT MetalFX" action:@selector(terminate:) keyEquivalent:@"q"];
    [app insertItemWithTitle:@"Start Game" action:@selector(startGame:) keyEquivalent:@"r" atIndex:0];
    [app insertItemWithTitle:@"Stop Game" action:@selector(stopGame:) keyEquivalent:@"." atIndex:1];
    [app insertItemWithTitle:@"Choose Game Resources…" action:@selector(chooseResources:) keyEquivalent:@"o" atIndex:2];
    [app addItemWithTitle:@"选择运行依赖目录…" action:@selector(chooseComponents:) keyEquivalent:@""];
    [app addItemWithTitle:@"启动设置… / Launch Settings" action:@selector(chooseLaunchSettings:) keyEquivalent:@","];
    [app addItemWithTitle:@"Choose UI Skin… (Experimental)" action:@selector(chooseSkin:) keyEquivalent:@""];
    [app addItemWithTitle:@"Default UI Skin" action:@selector(defaultSkin:) keyEquivalent:@""];
    NSMenuItem *languages=[NSMenuItem new];languages.title=@"语言 / Language";
    _languageMenu=[NSMenu new];
    NSString *selected=[NSString stringWithContentsOfFile:[labPath stringByAppendingPathComponent:@"bridge/language.txt"] encoding:NSUTF8StringEncoding error:nil];
    if(!selected)selected=@"en";
    for(NSArray *pair in @[@[@"zh-CN",@"简体中文（文字、歌名与歌词）"],@[@"en",@"English"],@[@"ja",@"日本語"]]){
        NSMenuItem *item=[_languageMenu addItemWithTitle:pair[1] action:@selector(switchLanguage:) keyEquivalent:@""];
        item.representedObject=pair[0];item.state=[selected isEqual:pair[0]]?NSControlStateValueOn:NSControlStateValueOff;
    }
    languages.submenu=_languageMenu;[app addItem:languages];
    NSMenuItem *automatic=[app insertItemWithTitle:@"AutoPlay" action:@selector(toggleAuto:) keyEquivalent:@"" atIndex:2];automatic.state=_control->auto_play?NSControlStateValueOn:NSControlStateValueOff;
    _hdrToggleMenuItem=[app addItemWithTitle:@"MV HDR 增强（对照开关）" action:@selector(toggleHDRComparison:) keyEquivalent:@"h"];
    _hdrToggleMenuItem.keyEquivalentModifierMask=NSEventModifierFlagCommand|NSEventModifierFlagShift;
    _hdrToggleMenuItem.enabled=_hdrRequested;_hdrToggleMenuItem.state=_hdrRequested?NSControlStateValueOn:NSControlStateValueOff;
    [app addItemWithTitle:@"导入中文字体…" action:@selector(importChineseFont:) keyEquivalent:@""];
    [app addItemWithTitle:@"恢复思源黑体" action:@selector(defaultChineseFont:) keyEquivalent:@""];
    root.submenu=app;NSApp.mainMenu=menu;
    [_window makeKeyAndOrderFront:nil];[NSApp activateIgnoringOtherApps:YES];
    [_window makeFirstResponder:_view];
    if(report){fprintf(report,"{\"event\":\"start\",\"pid\":%d,\"output\":[%lu,%lu],\"architecture\":\"arm64\"}\n",getpid(),(unsigned long)_outputWidth,(unsigned long)_outputHeight);fflush(report);}
    [self startGame:nil];
}
- (void)toggleHDRComparison:(NSMenuItem*)sender {
    _hdrBypass=!_hdrBypass;sender.state=_hdrBypass?NSControlStateValueOff:NSControlStateValueOn;
    if(report){fprintf(report,"{\"event\":\"hdr_comparison\",\"enhancement_enabled\":%s}\n",_hdrBypass?"false":"true");fflush(report);}
}
- (void)switchLanguage:(NSMenuItem*)sender {
    NSString *mode=sender.representedObject;
    if(![@[@"zh-CN",@"en",@"ja"]containsObject:mode])return;
    NSError *error=nil;
    if(![mode writeToFile:[labPath stringByAppendingPathComponent:@"bridge/language.txt"] atomically:YES encoding:NSUTF8StringEncoding error:&error]){
        NSAlert *alert=[NSAlert new];alert.messageText=@"无法保存语言设置";alert.informativeText=error.localizedDescription;[alert runModal];return;
    }
    for(NSMenuItem *item in _languageMenu.itemArray)item.state=[item.representedObject isEqual:mode]?NSControlStateValueOn:NSControlStateValueOff;
    if(_gameTask.running){_restartAfterLanguage=YES;[self stopGame:nil];}else [self startGame:nil];
}
- (void)restartForFontChange {
    if(_gameTask.running){_restartAfterLanguage=YES;[self stopGame:nil];}
    else [self startGame:nil];
}
- (void)importChineseFont:(id)sender {
    (void)sender;
    NSOpenPanel *panel=[NSOpenPanel openPanel];panel.canChooseDirectories=NO;panel.canChooseFiles=YES;
    panel.allowsMultipleSelection=NO;panel.allowedFileTypes=@[@"ttf",@"otf"];
    panel.message=@"选择包含简体中文字形的 TTF / OTF 字体。导入后重新生成独立字库并重启游戏。";
    if([panel runModal]!=NSModalResponseOK)return;
    NSData *data=[NSData dataWithContentsOfURL:panel.URL];
    CGDataProviderRef provider=data?CGDataProviderCreateWithCFData((__bridge CFDataRef)data):NULL;
    CGFontRef face=provider?CGFontCreateWithDataProvider(provider):NULL;
    if(provider)CGDataProviderRelease(provider);
    if(!face){NSAlert *a=[NSAlert new];a.messageText=@"无法读取该字体";[a runModal];return;}
    CGFontRelease(face);
    NSString *directory=[labPath stringByAppendingPathComponent:@"bridge/fonts"];
    [[NSFileManager defaultManager]createDirectoryAtPath:directory withIntermediateDirectories:YES attributes:nil error:nil];
    // Content-addressed import keeps a running renderer's font immutable until restart.
    NSString *name=[NSString stringWithFormat:@"%@.%@",NSUUID.UUID.UUIDString,panel.URL.pathExtension.lowercaseString];
    NSString *path=[directory stringByAppendingPathComponent:name];NSError *error=nil;
    if(![data writeToFile:path options:NSDataWritingAtomic error:&error] ||
       ![path writeToFile:[labPath stringByAppendingPathComponent:@"bridge/font.path"] atomically:YES encoding:NSUTF8StringEncoding error:&error]){
        NSAlert *a=[NSAlert new];a.messageText=@"字体导入失败";a.informativeText=error.localizedDescription;[a runModal];return;
    }
    [self restartForFontChange];
}
- (void)defaultChineseFont:(id)sender {
    (void)sender;[[NSFileManager defaultManager]removeItemAtPath:[labPath stringByAppendingPathComponent:@"bridge/font.path"] error:nil];
    [self restartForFontChange];
}
- (void)chooseResources:(id)sender{
    (void)sender;
    if(_gameTask.running){NSAlert *a=[NSAlert new];a.messageText=@"请先停止游戏再更换资源目录";[a runModal];return;}
    NSOpenPanel *panel=[NSOpenPanel openPanel];panel.canChooseDirectories=YES;panel.canChooseFiles=NO;
    panel.allowsMultipleSelection=NO;panel.message=@"选择已解压并合并补丁的游戏目录，包含 diva.exe、rom、firm、mdata。ZIP / 7z 请先解压。目录中的 MDATA 会启用。";
    if([panel runModal]==NSModalResponseOK){
        NSURL *url=panel.URL;
        if(![[NSFileManager defaultManager]fileExistsAtPath:[url.path stringByAppendingPathComponent:@"diva.exe"]] || ![[NSFileManager defaultManager]fileExistsAtPath:[url.path stringByAppendingPathComponent:@"rom"]]){
            NSAlert *a=[NSAlert new];a.messageText=@"该目录缺少 diva.exe";[a runModal];return;
        }
        [url.path writeToFile:[labPath stringByAppendingPathComponent:@"bridge/resources.path"] atomically:YES encoding:NSUTF8StringEncoding error:nil];
    }
}
- (void)chooseComponents:(id)sender {
    (void)sender;if(_gameTask.running)return;
    NSOpenPanel *p=[NSOpenPanel openPanel];p.canChooseDirectories=YES;p.canChooseFiles=NO;
    p.message=@"选择运行依赖目录：Bootstrap（PD Loader / DivaGL）、Wine、vc_redist.x64.exe。DMG 不包含游戏或补丁；压缩包请先解压。";
    if([p runModal]!=NSModalResponseOK)return;
    for(NSString *file in @[@"Bootstrap/plugins/config.ini",@"Bootstrap/plugins/TLAC.dva",@"Bootstrap/plugins/DivaGL.dva",@"Bootstrap/dinput8.dll",@"Wine/bin/wine64",@"vc_redist.x64.exe"]){
        if(![[NSFileManager defaultManager]fileExistsAtPath:[p.URL.path stringByAppendingPathComponent:file]]){
            NSAlert *a=[NSAlert new];a.messageText=@"运行依赖不完整";a.informativeText=[@"缺少：" stringByAppendingString:file];[a runModal];return;
        }
    }
    [p.URL.path writeToFile:[labPath stringByAppendingPathComponent:@"bridge/components.path"] atomically:YES encoding:NSUTF8StringEncoding error:nil];
}
- (void)chooseSkin:(id)sender {
    (void)sender;if(_gameTask.running){NSAlert *a=[NSAlert new];a.messageText=@"请先停止游戏再切换皮肤";[a runModal];return;}
    NSAlert *warning=[NSAlert new];warning.messageText=@"皮肤兼容性测试";
    warning.informativeText=@"这批皮肤仍在适配。White Rainbow 已复现结算阶段崩溃；建议使用默认皮肤。继续只适合临时画面测试。";
    [warning addButtonWithTitle:@"取消"];[warning addButtonWithTitle:@"继续测试"];
    if([warning runModal]!=NSAlertSecondButtonReturn)return;
    NSString *source=[NSString stringWithContentsOfFile:[labPath stringByAppendingPathComponent:@"bridge/resources.path"] encoding:NSUTF8StringEncoding error:nil];
    NSOpenPanel *panel=[NSOpenPanel openPanel];panel.canChooseDirectories=YES;panel.canChooseFiles=NO;
    if(source)panel.directoryURL=[NSURL fileURLWithPath:[source stringByAppendingPathComponent:@"_SKINS"]];
    panel.message=@"选择一套皮肤目录；同一时间只能应用一套 UI 皮肤。下次启动游戏生效。";
    if([panel runModal]==NSModalResponseOK){
        NSString *skin=[panel.URL.path stringByAppendingPathComponent:@"mdata/MZKN"];
        if(![[NSFileManager defaultManager]fileExistsAtPath:[skin stringByAppendingPathComponent:@"info.txt"]]){NSAlert *a=[NSAlert new];a.messageText=@"该目录不是支持的 MZKN 皮肤";[a runModal];return;}
        [skin writeToFile:[labPath stringByAppendingPathComponent:@"bridge/skin.path"] atomically:YES encoding:NSUTF8StringEncoding error:nil];
    }
}
- (void)defaultSkin:(id)sender {
    (void)sender;if(_gameTask.running)return;
    [[NSFileManager defaultManager]removeItemAtPath:[labPath stringByAppendingPathComponent:@"bridge/skin.path"] error:nil];
}
- (void)startGame:(id)sender{
    (void)sender;if(_gameTask.running)return;
    NSString *resourcesFile=[labPath stringByAppendingPathComponent:@"bridge/resources.path"];
    if(![[NSFileManager defaultManager]fileExistsAtPath:resourcesFile])[self chooseResources:nil];
    if(![[NSFileManager defaultManager]fileExistsAtPath:resourcesFile])return;
    NSString *componentsFile=[labPath stringByAppendingPathComponent:@"bridge/components.path"];
    if(![[NSFileManager defaultManager]fileExistsAtPath:componentsFile])[self chooseComponents:nil];
    if(![[NSFileManager defaultManager]fileExistsAtPath:componentsFile])return;
    NSString *script=[NSBundle.mainBundle.resourcePath stringByAppendingPathComponent:@"launch-game.sh"];
    if(![[NSFileManager defaultManager]fileExistsAtPath:script])return;
    if(_gameTask){
        id<MTLCommandBuffer> barrier=[_queue commandBuffer];[barrier commit];[barrier waitUntilCompleted];
        uint32_t automatic=_control->auto_play;
        _scaler=nil;_output=nil;for(unsigned i=0;i<PDAFT_FRAME_SLOTS;i++){_inputs[i]=nil;_hdrMasks[i]=nil;}
        memset(_control,0,PDAFT_FRAME_MAP_SIZE);_control->version=2;_control->auto_play=automatic;_control->input_active=1;_control->hdr_mv_only=[_launchOptions[@"hdr"]boolValue];
        __atomic_store_n(&_control->magic,PDAFT_FRAME_MAGIC,__ATOMIC_RELEASE);
        _presented.store(0);_submitted=_previousPresented=0;_lastLog=0;_previousFrameReadyNs=0;_staleFrames=0;
        _window.title=[NSString stringWithFormat:@"PDAFT MetalFX — %lup · waiting for game frames",(unsigned long)_outputHeight];
    }
    _gameTask=[NSTask new];_gameTask.executableURL=[NSURL fileURLWithPath:@"/bin/zsh"];_gameTask.arguments=@[script];
    NSMutableDictionary *environment=[NSProcessInfo.processInfo.environment mutableCopy];
    environment[@"PDAFT_FRAME_NOTIFY_NAME"]=[NSString stringWithFormat:@"local.pdaft.frame.%d",getpid()];
    environment[@"PDAFT_HDR_MV_ONLY"]=[_launchOptions[@"hdr"]boolValue]?@"1":@"0";environment[@"PDAFT_WORK_ROOT"]=labPath;environment[@"PDAFT_METAL_HUD"]=[_launchOptions[@"hud"]boolValue]?@"1":@"0";_gameTask.environment=environment;
    NSString *launchLog=[labPath stringByAppendingPathComponent:@"bridge/launcher.log"];
    [[NSFileManager defaultManager]createFileAtPath:launchLog contents:nil attributes:nil];
    NSFileHandle *logHandle=[NSFileHandle fileHandleForWritingAtPath:launchLog];
    _gameTask.standardOutput=logHandle;_gameTask.standardError=logHandle;
    __weak Presenter *weakSelf=self;
    _gameTask.terminationHandler=^(NSTask *t){
        if(report){fprintf(report,"{\"event\":\"game_exit\",\"status\":%d}\n",t.terminationStatus);fflush(report);}
        dispatch_async(dispatch_get_main_queue(),^{
            Presenter *strongSelf=weakSelf;
            if(strongSelf && t==strongSelf->_gameTask && strongSelf->_restartAfterLanguage){strongSelf->_restartAfterLanguage=NO;[strongSelf startGame:nil];}
        });
        if(t.terminationStatus!=0 && t.terminationStatus!=15)dispatch_async(dispatch_get_main_queue(),^{
            NSAlert *a=[NSAlert new];a.messageText=@"游戏未能启动或意外退出";
            a.informativeText=[@"诊断日志：" stringByAppendingString:launchLog];[a runModal];
        });
    };
    NSError *error=nil;
    if(![_gameTask launchAndReturnError:&error] && report){fprintf(report,"Game launch failed: %s\n",error.localizedDescription.UTF8String);fflush(report);}
}
- (void)stopGame:(id)sender{
    (void)sender;if(_control)_control->stop_requested=1;
    _window.title=@"PDAFT MetalFX — stopping game";
    NSTask *task=_gameTask;
    dispatch_after(dispatch_time(DISPATCH_TIME_NOW,NSEC_PER_SEC),dispatch_get_main_queue(),^{
        // Wine can remain in its exit loop; only terminate this app's own test child.
        if(task.running)[task terminate];
        if(task==self->_gameTask)self->_window.title=@"PDAFT MetalFX — game stopped";
    });
}
- (void)toggleAuto:(NSMenuItem*)sender{_control->auto_play=!_control->auto_play;sender.state=_control->auto_play?NSControlStateValueOn:NSControlStateValueOff;}
- (NSApplicationTerminateReply)applicationShouldTerminate:(NSApplication*)sender{
    if(_frameNotifyToken>=0)notify_cancel(_frameNotifyToken);
    (void)sender;[self stopGame:nil];
    if(_gameTask.running)[_gameTask terminate];return NSTerminateNow;
}
- (BOOL)applicationShouldTerminateAfterLastWindowClosed:(NSApplication *)sender {(void)sender;return YES;}
- (BOOL)prepare {
    if(!_control->width || !_control->height || !_control->slots[2].surface_id)return NO;
    NSError *error=nil;
    _scaler=[[PDAFTSpatialUpscaler alloc]initWithDevice:_view.device inputWidth:_control->width inputHeight:_control->height
        outputWidth:_outputWidth outputHeight:_outputHeight pixelFormat:MTLPixelFormatBGRA8Unorm
        colorMode:MTLFXSpatialScalerColorProcessingModePerceptual error:&error];
    if(!_scaler){if(report){fprintf(report,"{\"event\":\"error\",\"message\":\"%s\"}\n",error.localizedDescription.UTF8String);fflush(report);}_control->stopped=1;return NO;}
    MTLTextureDescriptor *d=[MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatBGRA8Unorm width:_control->width height:_control->height mipmapped:NO];
    d.storageMode=MTLStorageModeShared;d.usage=_scaler.inputUsage;
    for(unsigned i=0;i<PDAFT_FRAME_SLOTS;i++){
        IOSurfaceRef surface=IOSurfaceLookup(_control->slots[i].surface_id);
        if(!surface){_control->stopped=1;return NO;}
        _inputs[i]=[_view.device newTextureWithDescriptor:d iosurface:surface plane:0];CFRelease(surface);
        if(!_inputs[i]){_control->stopped=1;return NO;}
        if(_hdrRequested && _control->ui_mask_surface_ids[i]){
            IOSurfaceRef mask=IOSurfaceLookup(_control->ui_mask_surface_ids[i]);
            if(mask){_hdrMasks[i]=[_view.device newTextureWithDescriptor:d iosurface:mask plane:0];CFRelease(mask);}
        }
    }
    d.width=_outputWidth;d.height=_outputHeight;d.storageMode=MTLStorageModePrivate;d.usage=_scaler.outputUsage|MTLTextureUsageShaderRead;
    _output=[_view.device newTextureWithDescriptor:d];
    if(!_output){_control->stopped=1;return NO;}
    _window.title=[NSString stringWithFormat:@"PDAFT MetalFX — %up → %lup · live game%@",_control->height,(unsigned long)_outputHeight,_hdrRequested?@" · MV HDR":@""];
    if(report){fprintf(report,"{\"event\":\"shared_surfaces_imported\",\"input\":[%u,%u],\"output\":[%lu,%lu],\"cpu_pixel_readback\":false}\n",_control->width,_control->height,(unsigned long)_outputWidth,(unsigned long)_outputHeight);fflush(report);}
    return YES;
}
- (void)drawInMTKView:(MTKView *)view {
    if(!_control || _control->stopped)return;
    if(!_scaler && ![self prepare])return;
    int selected=-1;uint64_t oldest=0;
    for(unsigned i=0;i<PDAFT_FRAME_SLOTS;i++)if(__atomic_load_n(&_control->slots[i].state,__ATOMIC_ACQUIRE)==1 && _control->slots[i].sequence>oldest){selected=i;oldest=_control->slots[i].sequence;}
    if(selected<0)return;
    uint32_t expected=1;
    if(!__atomic_compare_exchange_n(&_control->slots[selected].state,&expected,2,false,__ATOMIC_ACQUIRE,__ATOMIC_RELAXED))return;
    // Prioritize current input response after a stall instead of displaying an old queue.
    for(unsigned i=0;i<PDAFT_FRAME_SLOTS;i++)if((int)i!=selected && _control->slots[i].sequence<oldest){
        uint32_t ready=1;if(__atomic_compare_exchange_n(&_control->slots[i].state,&ready,0,false,__ATOMIC_ACQ_REL,__ATOMIC_RELAXED))_staleFrames++;
    }
    PDAFTFrameSlot *slot=&_control->slots[selected];
    mach_timebase_info_data_t timebase;mach_timebase_info(&timebase);
    double ageMs=((double)mach_continuous_time()*timebase.numer/timebase.denom-slot->time_ns)/1e6;
    double gameGapMs=_previousFrameReadyNs?(slot->time_ns-_previousFrameReadyNs)/1e6:0;
    _previousFrameReadyNs=slot->time_ns;
    if(report && (ageMs>35 || gameGapMs>35)){
        fprintf(report,"{\"event\":\"frame_stall\",\"game_gap_ms\":%.3f,\"queue_age_ms\":%.3f,\"state\":%d,\"substate\":%d,\"stale_frames_discarded\":%llu}\n",gameGapMs,ageMs,slot->game_state,slot->game_substate,(unsigned long long)_staleFrames);fflush(report);
    }
    id<CAMetalDrawable> drawable=view.currentDrawable;
    if(!drawable){__atomic_store_n(&slot->state,0,__ATOMIC_RELEASE);return;}
    NSError *error=nil;id<MTLCommandBuffer> command=[_queue commandBuffer];
    command.label=@"PDAFT real game IOSurface → MetalFX → drawable";
    if(![_scaler encodeInput:_inputs[selected] output:_output commandBuffer:command error:&error]){_control->stopped=1;return;}
    double hdrNow=NSProcessInfo.processInfo.systemUptime;
    NSScreen *screen=_window.screen?:NSScreen.mainScreen;
    float headroom=screen?screen.maximumExtendedDynamicRangeColorComponentValue:1.f;
    BOOL mvActive=slot->game_state==2 && slot->game_substate==13;
    BOOL maskValid=_hdrMasks[selected] && _control->ui_mask_sequence[selected]==slot->sequence;
    float strength=mvActive&&maskValid&&!_hdrBypass?[_launchOptions[@"hdrStrength"]floatValue]:0;
    BOOL measure=_hdrRequested && hdrNow-_lastHDRMeasure>=5;
    id<MTLBuffer> hdrStatistics=nil;
    if(_hdrRequested){
        hdrStatistics=[_hdr encodeInput:_output drawable:drawable.texture commandBuffer:command
                              headroom:headroom strength:strength measure:measure uiMask:_hdrMasks[selected]];
        if(measure)_lastHDRMeasure=hdrNow;
    }else{
        id<MTLBlitCommandEncoder> blit=[command blitCommandEncoder];
        [blit copyFromTexture:_output sourceSlice:0 sourceLevel:0 sourceOrigin:MTLOriginMake(0,0,0) sourceSize:MTLSizeMake(_outputWidth,_outputHeight,1)
            toTexture:drawable.texture destinationSlice:0 destinationLevel:0 destinationOrigin:MTLOriginMake(0,0,0)];[blit endEncoding];
    }
    [drawable addPresentedHandler:^(id<MTLDrawable> d){(void)d;self->_presented.fetch_add(1);}];
    [command addCompletedHandler:^(id<MTLCommandBuffer> c){
        if(c.status==MTLCommandBufferStatusError && report){fprintf(report,"{\"event\":\"gpu_error\",\"code\":%ld}\n",(long)c.error.code);fflush(report);}
        if(hdrStatistics && c.status==MTLCommandBufferStatusCompleted && report){
            float peak=0;memcpy(&peak,hdrStatistics.contents,4);
            uint32_t above=((uint32_t*)hdrStatistics.contents)[1];float uiPeak=0;memcpy(&uiPeak,(uint32_t*)hdrStatistics.contents+2,4);
            fprintf(report,"{\"event\":\"hdr_output\",\"source\":\"SDR\",\"format\":\"RGBA16Float\",\"color_space\":\"extendedLinearSRGB\",\"headroom\":%.4f,\"strength\":%.3f,\"sampled_peak\":%.4f,\"above_sdr_samples\":%u,\"sample_count\":2304,\"mv_active\":%s,\"ui_mask_valid\":%s,\"ui_peak\":%.4f,\"ui_samples\":%u}\n",headroom,strength,peak,above,mvActive?"true":"false",maskValid?"true":"false",uiPeak,((uint32_t*)hdrStatistics.contents)[3]);fflush(report);
        }
        __atomic_store_n(&slot->state,0,__ATOMIC_RELEASE);
    }];
    [command presentDrawable:drawable];[command commit];_submitted++;
    double now=NSProcessInfo.processInfo.systemUptime;
    if(now-_lastLog>=1){
        uint64_t shown=_presented.load();
        if(report){fprintf(report,"{\"event\":\"frames\",\"sequence\":%llu,\"submitted\":%llu,\"presented\":%llu,\"fps\":%.3f,\"producer_dropped\":%llu,\"game_state\":%d,\"game_substate\":%d,\"input_hooked\":%u,\"autoplay_pulses\":%llu}\n",
            (unsigned long long)oldest,(unsigned long long)_submitted,(unsigned long long)shown,
            _lastLog?(shown-_previousPresented)/(now-_lastLog):0,(unsigned long long)_control->dropped,slot->game_state,slot->game_substate,_control->input_hooked,(unsigned long long)_control->autoplay_pulses);fflush(report);}
        _previousPresented=shown;_lastLog=now;
        if(report){fprintf(report,"{\"event\":\"input\",\"key_events\":%llu,\"polls\":%llu,\"manual_seen\":%llu,\"focus_calls\":%llu,\"expected_window\":%llu,\"returned_window\":%llu}\n",(unsigned long long)_control->key_events,(unsigned long long)_control->key_polls,(unsigned long long)_control->manual_seen,(unsigned long long)_control->foreground_calls,(unsigned long long)_control->expected_window,(unsigned long long)_control->returned_window);fflush(report);}
    }
}
- (void)mtkView:(MTKView *)view drawableSizeWillChange:(CGSize)size{(void)view;(void)size;}
@end
int main(){@autoreleasepool{NSApplication *app=NSApplication.sharedApplication;Presenter *p=[Presenter new];app.delegate=p;[app run];}return 0;}
