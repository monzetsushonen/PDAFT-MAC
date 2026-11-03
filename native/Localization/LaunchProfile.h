// Apply validated startup effect switches to the owned Windows profile only.
static void applyLaunchProfile(NSMutableString *config){
    const char *path=getenv("PDAFT_LAUNCH_SETTINGS");if(!path||!*path)return;
    NSDictionary *profile=[NSJSONSerialization JSONObjectWithData:[NSData dataWithContentsOfFile:@(path)] options:0 error:nil];
    if(![profile isKindOfClass:NSDictionary.class])throw std::runtime_error("Invalid launch settings");
    NSDictionary *effects=profile[@"effects"];
    if(![effects isKindOfClass:NSDictionary.class])throw std::runtime_error("Invalid effects profile");
    NSDictionary *keys=@{@"reflections":@"Reflections",@"shadows":@"Shadows",@"dof":@"DOF",@"glare":@"Glare"};
    NSMutableArray *lines=[[config componentsSeparatedByString:@"\n"]mutableCopy];BOOL graphics=NO;
    NSMutableSet *updated=[NSMutableSet new];
    for(NSUInteger i=0;i<lines.count;i++){
        NSString *line=lines[i],*trim=[line stringByTrimmingCharactersInSet:NSCharacterSet.whitespaceAndNewlineCharacterSet];
        if([trim hasPrefix:@"["]){graphics=[trim isEqual:@"[Graphics]"];continue;}
        if(!graphics)continue;
        NSRange eq=[line rangeOfString:@"="];if(eq.location==NSNotFound)continue;
        NSString *key=[[line substringToIndex:eq.location]stringByTrimmingCharactersInSet:NSCharacterSet.whitespaceCharacterSet];
        for(NSString *setting in keys)if([keys[setting]isEqual:key]){
            if(![effects[setting]isKindOfClass:NSNumber.class])throw std::runtime_error("Missing effect switch");
            lines[i]=[NSString stringWithFormat:@"%@ = %d",key,[effects[setting]boolValue]?1:0];[updated addObject:setting];
        }
    }
    if(getenv("PDAFT_NATIVE_FRAME_PACING")){
        BOOL inGraphics=NO;
        for(NSUInteger i=0;i<lines.count;i++){
            NSString *trim=[lines[i] stringByTrimmingCharactersInSet:NSCharacterSet.whitespaceAndNewlineCharacterSet];
            if([trim hasPrefix:@"["]){inGraphics=[trim isEqual:@"[Graphics]"];continue;}
            if(inGraphics&&[trim hasPrefix:@"FPS.Limit ="])lines[i]=@"FPS.Limit = 0";
        }
        printf("Frame pacing: native mach_wait_until 60Hz; Windows Render limiter bypassed\n");
    }
    if(updated.count!=4)throw std::runtime_error("Owned graphics profile lacks expected effect keys");
    [config setString:[lines componentsJoinedByString:@"\n"]];
    printf("Launch effects: reflections=%d shadows=%d dof=%d glare=%d; MetalFX output=%ldx%ld; HDR=%s (presentation only)\n",[effects[@"reflections"]boolValue],[effects[@"shadows"]boolValue],[effects[@"dof"]boolValue],[effects[@"glare"]boolValue],(long)[profile[@"outputWidth"]integerValue],(long)[profile[@"outputHeight"]integerValue],[profile[@"hdr"]boolValue]?"SDR-to-EDR":"off");
}
