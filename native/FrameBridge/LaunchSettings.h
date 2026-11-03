// Startup choices are saved before any game process is created.
- (BOOL)showLaunchSettings:(BOOL)first {
    NSDictionary *saved=[NSUserDefaults.standardUserDefaults dictionaryForKey:@"PDAFTLaunchOptions"]?:@{};
    NSString *language=[NSString stringWithContentsOfFile:[labPath stringByAppendingPathComponent:@"bridge/language.txt"] encoding:NSUTF8StringEncoding error:nil]?:saved[@"language"]?:@"en";
    NSPopUpButton *languages=[[NSPopUpButton alloc]initWithFrame:NSMakeRect(0,0,290,26) pullsDown:NO];
    [languages addItemsWithTitles:@[@"简体中文",@"English",@"日本語"]];
    NSArray *languageCodes=@[@"zh-CN",@"en",@"ja"];
    NSUInteger languageIndex=[languageCodes indexOfObject:language];[languages selectItemAtIndex:languageIndex==NSNotFound?0:languageIndex];
    NSPopUpButton *resolution=[[NSPopUpButton alloc]initWithFrame:NSMakeRect(0,0,290,26) pullsDown:NO];
    [resolution addItemsWithTitles:@[@"1920 × 1080 — 1080p",@"2560 × 1440 — 1440p",@"3840 × 2160 — 2160p"]];
    NSArray *heights=@[@1080,@1440,@2160],*widths=@[@1920,@2560,@3840];
    NSUInteger resolutionIndex=[heights indexOfObject:saved[@"outputHeight"]];[resolution selectItemAtIndex:resolutionIndex==NSNotFound?0:resolutionIndex];
    NSDictionary *effects=saved[@"effects"]?:@{@"reflections":@YES,@"shadows":@NO,@"dof":@NO,@"glare":@NO};
    NSMutableDictionary<NSString*,NSButton*> *effectButtons=[NSMutableDictionary new];
    NSMutableArray<NSView*> *effectViews=[NSMutableArray new];
    for(NSArray *pair in @[@[@"reflections",@"反射 / Reflections"],@[@"shadows",@"阴影 / Shadows"],@[@"dof",@"景深 / Depth of field"],@[@"glare",@"光晕 / Glare"]]){
        NSButton *b=[NSButton checkboxWithTitle:pair[1] target:nil action:nil];b.state=[effects[pair[0]]boolValue]?NSControlStateValueOn:NSControlStateValueOff;
        effectButtons[pair[0]]=b;[effectViews addObject:b];
    }
    NSStackView *effectsView=[NSStackView stackViewWithViews:effectViews];effectsView.orientation=NSUserInterfaceLayoutOrientationVertical;effectsView.alignment=NSLayoutAttributeLeading;effectsView.spacing=7;
    BOOL hdrCapable=NO;for(NSScreen *screen in NSScreen.screens)if(screen.maximumPotentialExtendedDynamicRangeColorComponentValue>1.0)hdrCapable=YES;
    NSButton *hdr=[NSButton checkboxWithTitle:hdrCapable?@"HDR（仅 MV · 实验性）":@"HDR（当前屏幕不支持）" target:nil action:nil];
    hdr.enabled=hdrCapable;hdr.state=hdrCapable&&[saved[@"hdr"]boolValue]?NSControlStateValueOn:NSControlStateValueOff;
    NSSlider *hdrStrength=[NSSlider sliderWithValue:saved[@"hdrStrength"]?[saved[@"hdrStrength"]doubleValue]:0.7 minValue:0 maxValue:1 target:nil action:nil];
    hdrStrength.enabled=hdrCapable;hdrStrength.toolTip=@"只增强 MV 亮部；UI 保持 SDR；无法恢复原画面丢失的高光细节";
    NSStackView *strengthRow=[NSStackView stackViewWithViews:@[[NSTextField labelWithString:@"增强强度"],hdrStrength]];
    NSStackView *hdrView=[NSStackView stackViewWithViews:@[hdr,strengthRow]];hdrView.orientation=NSUserInterfaceLayoutOrientationVertical;hdrView.alignment=NSLayoutAttributeLeading;hdrView.spacing=7;
    NSButton *automatic=[NSButton checkboxWithTitle:@"AutoPlay" target:nil action:nil];automatic.state=saved[@"autoplay"]?[saved[@"autoplay"]boolValue]:NSControlStateValueOn;
    NSStackView *tests=[NSStackView stackViewWithViews:@[automatic]];tests.orientation=NSUserInterfaceLayoutOrientationVertical;tests.alignment=NSLayoutAttributeLeading;tests.spacing=7;
    NSGridView *grid=[NSGridView gridViewWithViews:@[
        @[[NSTextField labelWithString:@"语言 / Language"],languages],
        @[[NSTextField labelWithString:@"输出分辨率 / Output"],resolution],
        @[[NSTextField labelWithString:@"画面特效 / Effects"],effectsView],
        @[[NSTextField labelWithString:@"动态范围 / Range"],hdrView],
        @[[NSTextField labelWithString:@"测试 / Testing"],tests]
    ]];
    grid.rowSpacing=16;grid.columnSpacing=20;grid.xPlacement=NSGridCellPlacementLeading;
    [grid columnAtIndex:0].width=155;[grid columnAtIndex:1].width=300;
    for(NSInteger i=0;i<grid.numberOfRows;i++)[grid rowAtIndex:i].yPlacement=NSGridCellPlacementTop;
    const CGFloat rowHeights[]={26,26,112,65,26};
    for(NSInteger i=0;i<grid.numberOfRows;i++)[grid rowAtIndex:i].height=rowHeights[i];
    grid.frame=NSMakeRect(28,160,544,319);
    _launchWindow=[[NSWindow alloc]initWithContentRect:NSMakeRect(0,0,600,620) styleMask:NSWindowStyleMaskTitled|NSWindowStyleMaskClosable backing:NSBackingStoreBuffered defer:NO];
    _launchWindow.title=@"PDAFT · 启动设置";_launchWindow.releasedWhenClosed=NO;_launchWindow.delegate=self;_launchWindow.level=NSModalPanelWindowLevel;
    NSTextField *heading=[NSTextField labelWithString:@"PDAFT · 启动设置 / Launch settings"];
    heading.font=[NSFont boldSystemFontOfSize:20];heading.frame=NSMakeRect(28,562,544,30);
    NSTextField *description=[NSTextField wrappingLabelWithString:@"选择后启动游戏。画面整体使用 MetalFX 超分，游戏内部渲染为 720p；设置会自动记住。"];
    description.frame=NSMakeRect(28,505,544,42);description.textColor=NSColor.secondaryLabelColor;
    NSButton *start=[NSButton buttonWithTitle:@"启动游戏 / Start" target:self action:@selector(acceptLaunchSettings:)];start.frame=NSMakeRect(410,28,162,32);start.keyEquivalent=@"\r";
    NSButton *cancel=[NSButton buttonWithTitle:first?@"退出 / Quit":@"取消 / Cancel" target:self action:@selector(cancelLaunchSettings:)];cancel.frame=NSMakeRect(278,28,120,32);cancel.keyEquivalent=@"\e";
    for(NSView *v in @[heading,description,grid,cancel,start])[_launchWindow.contentView addSubview:v];
    [_launchWindow center];[_launchWindow makeKeyAndOrderFront:nil];[NSApp activateIgnoringOtherApps:YES];
    NSInteger response=[NSApp runModalForWindow:_launchWindow];[_launchWindow orderOut:nil];_launchWindow.delegate=nil;_launchWindow=nil;
    if(response!=NSModalResponseOK)return NO;
    NSMutableDictionary *chosenEffects=[NSMutableDictionary new];
    for(NSString *key in effectButtons)chosenEffects[key]=@(effectButtons[key].state==NSControlStateValueOn);
    NSDictionary *options=@{@"language":languageCodes[languages.indexOfSelectedItem],
        @"outputWidth":widths[resolution.indexOfSelectedItem],@"outputHeight":heights[resolution.indexOfSelectedItem],
        @"effects":chosenEffects,@"autoplay":@(automatic.state==NSControlStateValueOn),
        @"hud":@YES,@"hdr":@(hdr.state==NSControlStateValueOn),@"hdrStrength":@(hdrStrength.doubleValue),@"internalWidth":@1280,@"internalHeight":@720};
    NSError *error=nil;
    NSData *data=[NSJSONSerialization dataWithJSONObject:options options:NSJSONWritingPrettyPrinted error:&error];
    BOOL written=data&&[data writeToFile:[labPath stringByAppendingPathComponent:@"bridge/launch-settings.json"] options:NSDataWritingAtomic error:&error];
    written=written&&[options[@"language"]writeToFile:[labPath stringByAppendingPathComponent:@"bridge/language.txt"] atomically:YES encoding:NSUTF8StringEncoding error:&error];
    if(!written){NSAlert *a=[NSAlert new];a.messageText=@"无法保存启动设置";a.informativeText=error.localizedDescription?:@"运行目录不可写";[a runModal];return NO;}
    [NSUserDefaults.standardUserDefaults setObject:options forKey:@"PDAFTLaunchOptions"];
    _launchOptions=options;_outputWidth=[options[@"outputWidth"]unsignedIntegerValue];_outputHeight=[options[@"outputHeight"]unsignedIntegerValue];
    setenv("MTL_HUD_ENABLED",[options[@"hud"]boolValue]?"1":"0",1);setenv("MTL_HUD_LOG_ENABLED",[options[@"hud"]boolValue]?"1":"0",1);
    return YES;
}
- (void)chooseLaunchSettings:(id)sender {
    (void)sender;
    if(_gameTask.running){NSAlert *a=[NSAlert new];a.messageText=@"请先停止游戏再更改启动设置";[a runModal];return;}
    if(![self showLaunchSettings:NO])return;
    if(![self configureDynamicRange])return;
    _view.drawableSize=CGSizeMake(_outputWidth,_outputHeight);
    _control->auto_play=[_launchOptions[@"autoplay"]boolValue];
    for(NSMenuItem *item in _languageMenu.itemArray)item.state=[item.representedObject isEqual:_launchOptions[@"language"]]?NSControlStateValueOn:NSControlStateValueOff;
    for(NSMenuItem *item in NSApp.mainMenu.itemArray.firstObject.submenu.itemArray)
        if(item.action==@selector(toggleAuto:))item.state=_control->auto_play?NSControlStateValueOn:NSControlStateValueOff;
    [self startGame:nil];
}

- (void)acceptLaunchSettings:(id)sender{(void)sender;[NSApp stopModalWithCode:NSModalResponseOK];}
- (void)cancelLaunchSettings:(id)sender{(void)sender;[NSApp stopModalWithCode:NSModalResponseCancel];}
- (BOOL)windowShouldClose:(NSWindow*)window{if(window==_launchWindow){[NSApp stopModalWithCode:NSModalResponseCancel];return NO;}return YES;}

- (BOOL)configureDynamicRange {
    if(_queue){id<MTLCommandBuffer> barrier=[_queue commandBuffer];[barrier commit];[barrier waitUntilCompleted];}
    BOOL requested=[_launchOptions[@"hdr"]boolValue];
    NSError *error=nil;
    PDAFTHDRPresentation *hdr=requested?[[PDAFTHDRPresentation alloc]initWithDevice:_view.device error:&error]:nil;
    if(requested&&!hdr){NSAlert *a=[NSAlert new];a.messageText=@"HDR 输出管线创建失败";a.informativeText=error.localizedDescription?:@"无法创建浮点输出";[a runModal];return NO;}
    _hdr=hdr;_hdrRequested=requested;_hdrBypass=NO;_hdrToggleMenuItem.enabled=requested;_hdrToggleMenuItem.state=requested?NSControlStateValueOn:NSControlStateValueOff;_lastHDRMeasure=0;
    _view.colorPixelFormat=requested?MTLPixelFormatRGBA16Float:MTLPixelFormatBGRA8Unorm;
    CGColorSpaceRef color=CGColorSpaceCreateWithName(requested?kCGColorSpaceExtendedLinearSRGB:kCGColorSpaceSRGB);
    _view.colorspace=color;CGColorSpaceRelease(color);
    ((CAMetalLayer*)_view.layer).wantsExtendedDynamicRangeContent=requested;
    if(report){fprintf(report,"{\"event\":\"dynamic_range\",\"hdr_requested\":%s,\"format\":\"%s\",\"source\":\"SDR\"}\n",requested?"true":"false",requested?"RGBA16Float":"BGRA8Unorm");fflush(report);}
    return YES;
}
