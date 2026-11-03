// Version-specific reference input bridge. Original PE SHA is checked at launch.
// Offsets derive from PD Loader TLAC's public 7.10 TargetState/Constants definitions.
typedef short (__attribute__((ms_abi)) *KeyStateFn)(int);
typedef void * (__attribute__((ms_abi)) *ForegroundFn)(void);
static KeyStateFn originalKeyState;
static ForegroundFn originalForeground;
static void *gameWindow;
static void **gameWindowStorage;
typedef void *(__attribute__((ms_abi)) *FindWindowFn)(const char*,const char*);
static FindWindowFn findWindow;
static ForegroundFn activeWindow;
struct WinPoint{int32_t x,y;};
typedef int(__attribute__((ms_abi)) *CursorFn)(WinPoint*);
typedef int(__attribute__((ms_abi)) *ClientPointFn)(void*,WinPoint*);
static CursorFn originalCursor;
static ClientPointFn clientToScreen;
typedef intptr_t(__attribute__((ms_abi)) *MessageFn)(void*,uint32_t,uintptr_t,intptr_t);
static MessageFn sendMessage;
static bool notifiedActive;
static uint8_t automaticKeys[256],manualLatches[256];

static void *pe_export(void *image,const char *name){
    if(!image)return NULL;uint8_t *b=(uint8_t*)image;
    if(*(uint16_t*)b!=0x5a4d)return NULL;
    uint32_t pe=*(uint32_t*)(b+0x3c),rva=*(uint32_t*)(b+pe+24+112),size=*(uint32_t*)(b+pe+24+116);
    uint8_t *e=b+rva;uint32_t count=*(uint32_t*)(e+24);
    uint32_t *names=(uint32_t*)(b+*(uint32_t*)(e+32)),*functions=(uint32_t*)(b+*(uint32_t*)(e+28));
    uint16_t *ordinals=(uint16_t*)(b+*(uint32_t*)(e+36));
    for(uint32_t i=0;i<count;i++)if(!strcmp((char*)b+names[i],name)){
        uint32_t f=functions[ordinals[i]];if(f>=rva && f<rva+size)return NULL;return b+f;
    }
    return NULL;
}
static uint64_t *pe_import(void *image,const char *name){
    uint8_t *b=(uint8_t*)image;uint32_t pe=*(uint32_t*)(b+0x3c),rva=*(uint32_t*)(b+pe+24+120);
    for(uint32_t *desc=(uint32_t*)(b+rva);desc[3];desc+=5){
        if(!desc[0])continue;
        uint64_t *lookup=(uint64_t*)(b+desc[0]),*iat=(uint64_t*)(b+desc[4]);
        for(unsigned i=0;lookup[i];i++)if(!(lookup[i]>>63) && !strcmp((char*)b+lookup[i]+2,name))return &iat[i];
    }return NULL;
}
static short __attribute__((ms_abi)) virtual_keys(int key){
    if(!control || key<0 || key>255)return originalKeyState?originalKeyState(key):0;
    if(key==0){
        control->key_polls++;
        for(unsigned i=0;i<256;i++)manualLatches[i]=__atomic_exchange_n(&control->pending_taps[i],0,__ATOMIC_ACQ_REL);
        memset(automaticKeys,0,sizeof automaticKeys);
        if(control->auto_play && *(int32_t*)0x140EDA810==2 && *(int32_t*)0x140EDA82C==13){
            const char map[4]={'W','D','S','A'};
            for(unsigned i=0;i<64;i++){
                uint8_t *t=(uint8_t*)(0x140D0B688ULL+i*0x4a8);
                int type=*(int32_t*)(t+0x14),hit=*(int32_t*)(t+0x45c);float left=*(float*)(t+0x18);
                // Runtime snapshot: offset 0x450 is 1 on active notes, not a removed flag.
                // Validity follows TLAC's actual inspector: pending hit + nonzero time.
                if(hit!=21 || !isfinite(left) || left==0.0f || left>0.023f || left<-.040f)continue;
                int button=-1;
                if(type>=0 && type<=7)button=map[type%4];
                else if(type>=18 && type<=21)button=map[type-18];
                else if(type==12 || type==15 || type==23)button='Q';
                else if(type==13 || type==16 || type==24)button='O';
                if(button>=0){automaticKeys[button]=1;control->autoplay_pulses++;}
            }
        }
    }
    if(manualLatches[key] || __atomic_load_n(&control->keys[key],__ATOMIC_RELAXED)){control->manual_seen++;return (short)0x8001;}
    if(automaticKeys[key])return (short)0x8001;
    if(control->input_active || control->auto_play)return 0;
    return originalKeyState?originalKeyState(key):0;
}
static void * __attribute__((ms_abi)) virtual_foreground(void){
    if(control)control->foreground_calls++;
    if(gameWindowStorage && *gameWindowStorage)gameWindow=*gameWindowStorage;
    if(!gameWindow && findWindow){gameWindow=findWindow("GLUT",NULL);if(!gameWindow)gameWindow=findWindow("FREEGLUT",NULL);}
    if(!gameWindow && activeWindow)gameWindow=activeWindow();
    if(control){control->expected_window=(uint64_t)(gameWindowStorage?*gameWindowStorage:NULL);control->returned_window=(uint64_t)gameWindow;}
    if(control && (control->input_active || control->auto_play) && gameWindow)return gameWindow;
    return originalForeground?originalForeground():NULL;
}
static int __attribute__((ms_abi)) virtual_cursor(WinPoint *p){
    if(control && control->mouse_enabled && gameWindow){
        p->x=control->mouse_x;p->y=control->mouse_y;if(clientToScreen)clientToScreen(gameWindow,p);return 1;
    }return originalCursor?originalCursor(p):0;
}
static bool patch_import(void *module,const char *name,void *replacement,void **original){
    uint64_t *entry=pe_import(module,name);if(!entry)return false;
    *original=(void*)*entry;
    if(mach_vm_protect(mach_task_self(),(mach_vm_address_t)entry,8,FALSE,VM_PROT_READ|VM_PROT_WRITE)!=KERN_SUCCESS)return false;
    __atomic_store_n(entry,(uint64_t)replacement,__ATOMIC_RELEASE);return true;
}
static void install_game_input(void){
    typedef void *(__attribute__((ms_abi)) *ModuleFn)(const char*);
    typedef void *(__attribute__((ms_abi)) *ProcFn)(void*,const char*);
    // Stable Wine 7.7 kernelbase mapping; verify signature before invoking anything.
    if(read_game_int(0x7b000000)<0 || (*(uint16_t*)0x7b000000)!=0x5a4d)return;
    ModuleFn getModule=(ModuleFn)pe_export((void*)0x7b000000,"GetModuleHandleA");
    ProcFn getProc=(ProcFn)pe_export((void*)0x7b000000,"GetProcAddress");
    if(!getModule || !getProc)return;
    void *tlac=getModule("TLAC.dva"),*gl=getModule("opengl32.dll"),*user=getModule("user32.dll");
    if(!tlac || !gl || !user)return;
    // PD Loader becac44e: resolve the HWND storage from its actual
    // WindowFromDC call / RIP-relative store, rather than guessing a title.
    uint8_t *site=(uint8_t*)tlac+0x15c07;
    if(site[0]==0xff && site[1]==0x15 && site[6]==0x48 && site[7]==0x89 && site[8]==0x05){
        int32_t displacement;memcpy(&displacement,site+9,4);
        gameWindowStorage=(void**)(site+13+displacement);gameWindow=*gameWindowStorage;
    }
    findWindow=(FindWindowFn)getProc(user,"FindWindowA");activeWindow=(ForegroundFn)getProc(user,"GetActiveWindow");
    clientToScreen=(ClientPointFn)getProc(user,"ClientToScreen");
    sendMessage=(MessageFn)getProc(user,"SendMessageW");
    typedef void *(__attribute__((ms_abi)) *DCFn)(void);
    typedef void *(__attribute__((ms_abi)) *WndFn)(void*);
    DCFn getDC=(DCFn)getProc(gl,"wglGetCurrentDC");WndFn window=(WndFn)getProc(user,"WindowFromDC");
    if(!gameWindow && getDC && window)gameWindow=window(getDC());
    if(!gameWindow && findWindow){gameWindow=findWindow("GLUT",NULL);if(!gameWindow)gameWindow=findWindow("FREEGLUT",NULL);}
    bool keys=patch_import(tlac,"GetAsyncKeyState",(void*)virtual_keys,(void**)&originalKeyState);
    bool focus=patch_import(tlac,"GetForegroundWindow",(void*)virtual_foreground,(void**)&originalForeground);
    patch_import(tlac,"GetCursorPos",(void*)virtual_cursor,(void**)&originalCursor);
    PlayerDataGuard::install(tlac);
    control->input_hooked=keys && focus;
    if(logfile){fprintf(logfile,"App input/AutoPlay hook keys=%d focus=%d window=%p\n",keys,focus,gameWindow);fflush(logfile);}
}
static void update_game_activation(void){
    if(gameWindowStorage && *gameWindowStorage)gameWindow=*gameWindowStorage;
    if(!notifiedActive && gameWindow && sendMessage){
        // Forward this app's game-session activation to its own child window.
        // This does not activate another desktop app or generate OS-wide input.
        sendMessage(gameWindow,0x1c,1,0);sendMessage(gameWindow,0x06,1,0);sendMessage(gameWindow,0x07,0,0);
        notifiedActive=true;
    }
    static uint32_t last;
    uint64_t ptr=0;mach_vm_size_t got=0;
    if(mach_vm_read_overwrite(mach_task_self(),0x140EDA330,8,(mach_vm_address_t)&ptr,&got)==KERN_SUCCESS && ptr){
        uint32_t down=read_game_int(ptr+0x20);
        if(down!=last && logfile){fprintf(logfile,"Game JVS down=%x tapped=%x\n",down,read_game_int(ptr));fflush(logfile);}last=down;
    }
}
