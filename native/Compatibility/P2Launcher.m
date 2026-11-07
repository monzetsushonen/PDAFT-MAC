#import <AppKit/AppKit.h>
#import <CommonCrypto/CommonDigest.h>
#import <sys/file.h>
#import <fcntl.h>
#import <signal.h>
#import <errno.h>

static NSURL *root(void){return NSBundle.mainBundle.bundleURL.URLByResolvingSymlinksInPath.URLByDeletingLastPathComponent;}
static NSDictionary *plan(void){
 NSString*r=root().path,*p1=[[r stringByDeletingLastPathComponent] stringByAppendingPathComponent:@"20261107-p1/runtime/MetalSharp-Wine.app/Contents/MacOS"];
 return @{@"root":r,@"runtime":[p1 stringByAppendingPathComponent:@"wine"],@"server":[p1 stringByAppendingPathComponent:@"wineserver"],@"prefix":[r stringByAppendingPathComponent:@"game-prefix"],@"overlay":[r stringByAppendingPathComponent:@"game"],@"profile":[r stringByAppendingPathComponent:@"isolation.sb"],@"logs":[r stringByAppendingPathComponent:@"logs"],@"cache":[r stringByAppendingPathComponent:@"cache/shaders"],@"core":@"Original x86_64 PDAFT / native ARM64 Wine + FEX",@"videoMV":@"disabled: ASF demuxer unavailable",@"HDR":@NO,@"MetalFX":@NO,@"formalScoreSaver":@NO,@"defaultAutoPlay":@YES};
}
static NSString *hashFile(NSString*path){
 NSFileHandle*f=[NSFileHandle fileHandleForReadingAtPath:path];if(!f)return nil;
 CC_SHA256_CTX ctx;CC_SHA256_Init(&ctx);for(;;){NSData*d=[f readDataOfLength:1048576];if(!d.length)break;CC_SHA256_Update(&ctx,d.bytes,(CC_LONG)d.length);}[f closeFile];unsigned char bytes[CC_SHA256_DIGEST_LENGTH];CC_SHA256_Final(bytes,&ctx);NSMutableString*s=[NSMutableString new];for(int i=0;i<CC_SHA256_DIGEST_LENGTH;i++)[s appendFormat:@"%02x",bytes[i]];return s;
}
static NSString *preflight(void){
 NSDictionary*p=plan();NSFileManager*f=NSFileManager.defaultManager;
 for(NSString*k in @[@"runtime",@"server",@"prefix",@"overlay",@"profile",@"cache",@"logs"])if(![f fileExistsAtPath:p[k]])return [@"缺少测试路径：" stringByAppendingString:k];
 NSDictionary*pins=@{@"diva.exe":@"8bad363bafa079c5974daba8a4cf2e2edf91f227686cbf99b45d44786d2de5af",@"plugins/TLAC.dva":@"9b713927e729e14d3f8265eb5fff1d6b91af71168723b4706b4ee381192f4707",@"plugins/PDAFTAutoPlay.dva":@"7ba7919ad19f4a5932ea5b417333da2fb52fa53c522fd8d3f1b49ca9853edbde"};
 for(NSString*n in pins)if(![hashFile([p[@"overlay"] stringByAppendingPathComponent:n]) isEqual:pins[n]])return [@"测试版本校验失败：" stringByAppendingString:n];
 NSString*c=[NSString stringWithContentsOfFile:[p[@"overlay"] stringByAppendingPathComponent:@"plugins/components.ini"] encoding:NSUTF8StringEncoding error:nil];
 c=[[[c componentsSeparatedByCharactersInSet:NSCharacterSet.whitespaceAndNewlineCharacterSet] componentsJoinedByString:@""] lowercaseString];if(![c containsString:@"score_saver=false"])return @"正式成绩保存必须保持关闭。";
 return nil;
}
static BOOL otherGameRunning(void){
 NSString*dir=plan()[@"logs"];for(NSString*n in [NSFileManager.defaultManager contentsOfDirectoryAtPath:dir error:nil]){
  if(![n hasPrefix:@"game-"]||![n hasSuffix:@"-process.json"])continue;
  NSDictionary*d=[NSJSONSerialization JSONObjectWithData:[NSData dataWithContentsOfFile:[dir stringByAppendingPathComponent:n]] options:0 error:nil];
  int pid=[d[@"pid"] intValue];if(d[@"exit"]||pid<=0)continue;if(kill(pid,0)==0||errno==EPERM)return YES;
 }return NO;
}
@interface Launcher:NSObject<NSApplicationDelegate>
@end
@implementation Launcher{
 NSWindow*_window;NSTextField*_status;NSButton*_start,*_stop,*_autoPlay;NSTask*_game;NSFileHandle*_log;int _appLock,_sessionLock;NSString*_report,*_recordPath;NSMutableDictionary*_record;NSTimer*_timer;unsigned long long _lastPolls;NSUInteger _stalledTicks;
}
-(NSDictionary*)environment{
 NSDictionary*p=plan();NSMutableDictionary*e=[NSProcessInfo.processInfo.environment mutableCopy];for(NSString*k in @[@"DYLD_INSERT_LIBRARIES",@"DYLD_FRAMEWORK_PATH",@"DYLD_LIBRARY_PATH",@"PDAFT_NATIVE_PROFILER",@"FEX_SMCCHECKS",@"FEX_TSOENABLED",@"OPENGL_BACKEND",@"WINEMETALGL",@"WINEMETALGL_STATS"]) [e removeObjectForKey:k];
 [e addEntriesFromDictionary:@{@"WINEPREFIX":p[@"prefix"],@"TMPDIR":[p[@"root"] stringByAppendingString:@"/tmp/"],@"WINEDEBUG":@"-all,err+all",@"WINEDLLOVERRIDES":@"dinput8=n,b;mscoree,mshtml,winegstreamer=",@"WINEMETALGL_PIPELINE_CACHE":@"0",@"WINEMETALGL_SHADER_CACHE":p[@"cache"],@"WINEMETALGL_LOG":@"1",@"WINEMETALGL_LOG_FILE":[_report stringByAppendingString:@".metal.log"],@"METALSHARP_CONFINE_CURSOR":@"0",@"PDAFT_P2_AUTOPLAY":_autoPlay.state==NSControlStateValueOn?@"1":@"0",@"PDAFT_P2_REPORT":[@"Z:" stringByAppendingString:[_report stringByReplacingOccurrencesOfString:@"/" withString:@"\\"]]}];return e;
}
-(void)saveRecord{NSData*d=[NSJSONSerialization dataWithJSONObject:_record options:NSJSONWritingPrettyPrinted error:nil];[d writeToFile:_recordPath atomically:YES];}
-(void)applicationDidFinishLaunching:(NSNotification*)note{
 (void)note;_appLock=open([[root() URLByAppendingPathComponent:@"launcher.lock"].path fileSystemRepresentation],O_CREAT|O_RDWR,0600);_sessionLock=-1;
 if(_appLock<0||flock(_appLock,LOCK_EX|LOCK_NB)){NSAlert*a=[NSAlert new];a.messageText=@"P2 测试启动器已在运行";[a runModal];[NSApp terminate:nil];return;}
 NSMenu*bar=[NSMenu new];NSMenuItem*item=[NSMenuItem new];[bar addItem:item];NSMenu*menu=[NSMenu new];[menu addItemWithTitle:@"退出" action:@selector(terminate:) keyEquivalent:@"q"];item.submenu=menu;NSApp.mainMenu=bar;
 _window=[[NSWindow alloc]initWithContentRect:NSMakeRect(0,0,660,340) styleMask:NSWindowStyleMaskTitled|NSWindowStyleMaskClosable|NSWindowStyleMaskMiniaturizable backing:NSBackingStoreBuffered defer:NO];_window.title=@"PDAFT · P2 整曲兼容测试";_window.releasedWhenClosed=NO;
 NSTextField*title=[NSTextField labelWithString:@"原版 PDAFT · 离线整曲测试"];title.font=[NSFont boldSystemFontOfSize:21];title.frame=NSMakeRect(24,284,612,30);[_window.contentView addSubview:title];
 NSTextField*detail=[NSTextField wrappingLabelWithString:@"先选择 EASY 测试 3D MV，保持游戏在前台直至结算。切出窗口会暂停，返回后按回车继续。判定和得分由原版处理。视频 MV 暂关闭，HDR / MetalFX 关闭，正式成绩不保存。"];detail.frame=NSMakeRect(24,209,612,63);[_window.contentView addSubview:detail];
 _autoPlay=[NSButton checkboxWithTitle:@"离线 AutoPlay（测试版，HOLD 与复杂滑条仍待验收）" target:nil action:nil];_autoPlay.frame=NSMakeRect(24,168,612,28);_autoPlay.state=NSControlStateValueOn;[_window.contentView addSubview:_autoPlay];
 _status=[NSTextField wrappingLabelWithString:@"准备就绪。启动后按回车进入选曲，再手动选择歌曲开始。"];_status.frame=NSMakeRect(24,76,612,78);[_window.contentView addSubview:_status];
 _start=[NSButton buttonWithTitle:@"启动游戏" target:self action:@selector(start:)];_start.frame=NSMakeRect(24,24,130,34);[_window.contentView addSubview:_start];
 _stop=[NSButton buttonWithTitle:@"停止本次测试" target:self action:@selector(stop:)];_stop.frame=NSMakeRect(166,24,150,34);_stop.enabled=NO;[_window.contentView addSubview:_stop];
 NSButton*logs=[NSButton buttonWithTitle:@"查看日志" target:self action:@selector(logs:)];logs.frame=NSMakeRect(328,24,130,34);[_window.contentView addSubview:logs];
 [_window center];[_window makeKeyAndOrderFront:nil];[NSApp activateIgnoringOtherApps:YES];
}
-(void)releaseSession{if(_sessionLock>=0){flock(_sessionLock,LOCK_UN);close(_sessionLock);_sessionLock=-1;}}
-(void)start:(id)sender{
 (void)sender;if(_game.running)return;NSString*error=preflight();if(error){_status.stringValue=error;return;}
 _sessionLock=open([[root() URLByAppendingPathComponent:@"session.lock"].path fileSystemRepresentation],O_CREAT|O_RDWR,0600);
 if(_sessionLock<0||flock(_sessionLock,LOCK_EX|LOCK_NB)||otherGameRunning()){[self releaseSession];_status.stringValue=@"检测到已有 P2 测试进程，请先结束当前测试再启动。";return;}
 NSDictionary*p=plan();NSString*stamp=[NSString stringWithFormat:@"%.0f",NSDate.date.timeIntervalSince1970*1000];_report=[p[@"logs"] stringByAppendingPathComponent:[NSString stringWithFormat:@"session-app-%@.jsonl",stamp]];
 NSString*log=[p[@"logs"] stringByAppendingPathComponent:[NSString stringWithFormat:@"game-app-%@.log",stamp]];_recordPath=[p[@"logs"] stringByAppendingPathComponent:[NSString stringWithFormat:@"game-app-%@-process.json",stamp]];
 if(![NSFileManager.defaultManager createFileAtPath:log contents:nil attributes:nil]||!(_log=[NSFileHandle fileHandleForWritingAtPath:log])){[self releaseSession];_status.stringValue=@"无法创建测试日志。";return;}
 NSTask*t=[NSTask new];t.executableURL=[NSURL fileURLWithPath:@"/usr/bin/sandbox-exec"];t.arguments=@[@"-f",p[@"profile"],p[@"runtime"],@"C:\\PDAFT-P2\\diva.exe",@"--launch",@"-w"];t.currentDirectoryURL=[NSURL fileURLWithPath:p[@"overlay"]];t.environment=self.environment;t.standardOutput=_log;t.standardError=_log;
 __weak Launcher*weak=self;t.terminationHandler=^(NSTask*done){dispatch_async(dispatch_get_main_queue(),^{Launcher*s=weak;if(!s||s->_game!=done)return;[s->_timer invalidate];s->_timer=nil;[s->_log closeFile];s->_log=nil;s->_record[@"exit"]=@(done.terminationStatus);s->_record[@"ended"]=@(NSDate.date.timeIntervalSince1970);[s saveRecord];s->_game=nil;[s releaseSession];s->_start.enabled=YES;s->_stop.enabled=NO;s->_autoPlay.enabled=YES;s->_status.stringValue=[NSString stringWithFormat:@"游戏进程已结束（%d）。日志已保留。",done.terminationStatus];});};
 _game=t;NSError*launchError=nil;if(![t launchAndReturnError:&launchError]){[_log closeFile];_log=nil;_game=nil;[self releaseSession];_status.stringValue=[@"启动失败：" stringByAppendingString:launchError.localizedDescription];return;}
 _record=[@{@"mode":@"app",@"pid":@(t.processIdentifier),@"started":@(NSDate.date.timeIntervalSince1970),@"prefix":p[@"prefix"],@"report":_report,@"autoplay":@(_autoPlay.state==NSControlStateValueOn)} mutableCopy];[self saveRecord];
 _start.enabled=NO;_stop.enabled=YES;_autoPlay.enabled=NO;_lastPolls=0;_stalledTicks=0;_status.stringValue=@"游戏已启动。请切到游戏窗口，按回车进入选曲并开始 EASY。";
 _timer=[NSTimer scheduledTimerWithTimeInterval:1 target:self selector:@selector(updateTelemetry:) userInfo:nil repeats:YES];
}
-(void)updateTelemetry:(NSTimer*)timer{
 (void)timer;NSFileHandle*f=[NSFileHandle fileHandleForReadingAtPath:_report];if(!f)return;unsigned long long end=[f seekToEndOfFile];[f seekToFileOffset:end>16384?end-16384:0];NSData*data=[f readDataToEndOfFile];[f closeFile];NSString*s=[[NSString alloc]initWithData:data encoding:NSUTF8StringEncoding];
 for(NSString*line in [[s componentsSeparatedByString:@"\n"] reverseObjectEnumerator]){NSData*d=[line dataUsingEncoding:NSUTF8StringEncoding];NSDictionary*r=d?[NSJSONSerialization JSONObjectWithData:d options:0 error:nil]:nil;if(!r[@"state"])continue;
  int state=[r[@"state"] intValue],sub=[r[@"substate"] intValue];NSString*name=@"启动 / 其他状态";if(state==2&&sub==12)name=@"选曲";else if(state==2&&sub==13)name=@"演奏";else if(state==2&&sub==15)name=@"结算";else if(state==2&&sub==14)name=@"下一首 / 返回选曲";else if(state==1)name=@"标题 / 演示";
  unsigned long long polls=[r[@"polls"] unsignedLongLongValue];_stalledTicks=polls==_lastPolls?_stalledTicks+1:0;_lastPolls=polls;
  NSString*hint=_stalledTicks>=2?@"输入轮询已停止：可能失焦或暂停，请切回游戏；如有暂停菜单，按回车继续。":@"保持游戏在前台直到结算；按键计数不代表帧率。";
  _status.stringValue=[NSString stringWithFormat:@"状态：%@（%d / %d）\nAutoPlay：%@；按键候选观测：%@\n%@",name,state,sub,[r[@"autoplay"] boolValue]?@"开":@"关",r[@"pulses"]?:@0,hint];break;
 }
}
-(void)stop:(id)sender{
 (void)sender;if(!_game.running)return;_record[@"stopRequested"]=@YES;[self saveRecord];[_game terminate];NSTask*t=[NSTask new];t.executableURL=[NSURL fileURLWithPath:plan()[@"server"]];t.arguments=@[@"-k"];t.environment=self.environment;t.standardOutput=[NSFileHandle fileHandleWithNullDevice];t.standardError=[NSFileHandle fileHandleWithNullDevice];[t launchAndReturnError:nil];_status.stringValue=@"正在停止 P2 测试环境…";
}
-(void)logs:(id)sender{(void)sender;[NSWorkspace.sharedWorkspace openURL:[NSURL fileURLWithPath:plan()[@"logs"]]];}
-(BOOL)applicationShouldTerminateAfterLastWindowClosed:(NSApplication*)app{(void)app;return YES;}
-(NSApplicationTerminateReply)applicationShouldTerminate:(NSApplication*)app{(void)app;if(_game.running)[self stop:nil];return NSTerminateNow;}
@end
int main(int argc,char**argv){@autoreleasepool{
 if(argc==2&&(!strcmp(argv[1],"--print-plan")||!strcmp(argv[1],"--verify"))){NSMutableDictionary*d=[plan() mutableCopy];if(!strcmp(argv[1],"--verify"))d[@"verificationError"]=preflight()?:NSNull.null;NSData*j=[NSJSONSerialization dataWithJSONObject:d options:NSJSONWritingPrettyPrinted error:nil];fwrite(j.bytes,1,j.length,stdout);return d[@"verificationError"]&&d[@"verificationError"]!=NSNull.null?1:0;}
 [NSApplication sharedApplication];[NSApp setActivationPolicy:NSApplicationActivationPolicyRegular];Launcher*d=[Launcher new];NSApp.delegate=d;[NSApp run];return 0;
}}
