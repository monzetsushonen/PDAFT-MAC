// Captures the game's composed back buffer into a GPU-shared IOSurface ring.
// No CPU pixel readback. glFinish is the initial correctness synchronization.
#define GL_SILENCE_DEPRECATION
#import <Foundation/Foundation.h>
#import <IOSurface/IOSurface.h>
#import <OpenGL/OpenGL.h>
#import <OpenGL/gl3.h>
#include <mach/mach.h>
#include <mach/mach_vm.h>
#include <mach/mach_time.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>
#include <pthread.h>
#include <notify.h>
#include <cmath>
#include <string>
#include "FrameProtocol.h"

static PDAFTFrameControl *control;
static IOSurfaceRef surfaces[PDAFT_FRAME_SLOTS];
static GLuint textures[PDAFT_FRAME_SLOTS], fbos[PDAFT_FRAME_SLOTS];
static CGLContextObj owner;
static id gameActivity;
static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
static FILE *logfile;
static mach_timebase_info_data_t clock_scale;
static int32_t read_game_int(uint64_t address);
#include "PlayerDataGuard.h"
#include "GameInput.h"
#include "NativeMovie.h"
#include "GLCompat.h"
#include <algorithm>
#include "RenderProfiler.h"
#include "UICoverage.h"
#include "FrameTiming.h"

static bool initialize(CGLContextObj context, int width, int height) {
    const char *path = getenv("PDAFT_FRAME_CONTROL");
    if (!path) return false;
    int fd = open(path, O_RDWR);
    if (fd < 0) return false;
    control = (PDAFTFrameControl *)mmap(NULL, PDAFT_FRAME_MAP_SIZE, PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0);
    close(fd);
    if (control == MAP_FAILED) { control = NULL; return false; }
    if (__atomic_load_n(&control->magic, __ATOMIC_ACQUIRE) != PDAFT_FRAME_MAGIC || control->producer_pid) {
        munmap(control, PDAFT_FRAME_MAP_SIZE); control = NULL; return false;
    }
    const char *log = getenv("PDAFT_BRIDGE_LOG");
    if (log) logfile = fopen(log, "a");
    control->producer_pid = getpid(); control->width = width; control->height = height;
    mach_timebase_info(&clock_scale);
    GLint oldTexture, oldDraw;
    glGetIntegerv(GL_TEXTURE_BINDING_RECTANGLE, &oldTexture);
    glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &oldDraw);
    bool ok = true;
    for (unsigned i=0; i<PDAFT_FRAME_SLOTS; i++) {
        NSDictionary *properties = @{(id)kIOSurfaceWidth:@(width), (id)kIOSurfaceHeight:@(height),
            (id)kIOSurfaceBytesPerElement:@4, (id)kIOSurfaceBytesPerRow:@((width*4+63)&~63),
            (id)kIOSurfacePixelFormat:@(0x42475241), (id)kIOSurfaceIsGlobal:@YES};
        surfaces[i] = IOSurfaceCreate((__bridge CFDictionaryRef)properties);
        if (!surfaces[i]) { ok=false; break; }
        glGenTextures(1, &textures[i]); glBindTexture(GL_TEXTURE_RECTANGLE, textures[i]);
        CGLError error = CGLTexImageIOSurface2D(context, GL_TEXTURE_RECTANGLE, GL_RGBA8,
            width, height, GL_BGRA, GL_UNSIGNED_INT_8_8_8_8_REV, surfaces[i], 0);
        if (error) { if(logfile)fprintf(logfile,"IOSurface CGL import failed: %d\n",error); ok=false; break; }
        glGenFramebuffers(1, &fbos[i]); glBindFramebuffer(GL_DRAW_FRAMEBUFFER, fbos[i]);
        glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_RECTANGLE, textures[i], 0);
        if (glCheckFramebufferStatus(GL_DRAW_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) { ok=false; break; }
        control->slots[i].surface_id = IOSurfaceGetID(surfaces[i]);
        if(control->hdr_mv_only){
            uiSurfaces[i]=IOSurfaceCreate((__bridge CFDictionaryRef)properties);
            if(!uiSurfaces[i]){ok=false;break;}
            glGenTextures(1,&uiTextures[i]);glBindTexture(GL_TEXTURE_RECTANGLE,uiTextures[i]);
            if(CGLTexImageIOSurface2D(context,GL_TEXTURE_RECTANGLE,GL_RGBA8,width,height,GL_BGRA,GL_UNSIGNED_INT_8_8_8_8_REV,uiSurfaces[i],0)){ok=false;break;}
            control->ui_mask_surface_ids[i]=IOSurfaceGetID(uiSurfaces[i]);
            glGenFramebuffers(1,&uiCaptureFBOs[i]);glBindFramebuffer(GL_DRAW_FRAMEBUFFER,uiCaptureFBOs[i]);
            glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_RECTANGLE,uiTextures[i],0);
            if(glCheckFramebufferStatus(GL_DRAW_FRAMEBUFFER)!=GL_FRAMEBUFFER_COMPLETE){ok=false;break;}
        }
    }
    glBindTexture(GL_TEXTURE_RECTANGLE, oldTexture); glBindFramebuffer(GL_DRAW_FRAMEBUFFER, oldDraw);
    if (!ok) { control->stopped=1; if(logfile){fprintf(logfile,"Bridge initialization failed\n");fflush(logfile);} return false; }
    owner=context;
    gameActivity=[NSProcessInfo.processInfo beginActivityWithOptions:NSActivityUserInitiatedAllowingIdleSystemSleep|NSActivityLatencyCritical reason:@"PDAFT game rendering behind native presenter"];
    if(getenv("PDAFT_NATIVE_FRAME_PACING")){
        GLint swapInterval=0;CGLSetParameter(context,kCGLCPSwapInterval,&swapInterval);
        pthread_set_qos_class_self_np(QOS_CLASS_USER_INTERACTIVE,0);
    }
    install_game_gl_aliases();
    NativeMovie::install();
    install_game_input();
    if(logfile){fprintf(logfile,"GPU IOSurface producer ready pid=%d %dx%d renderer=%s\n",getpid(),width,height,glGetString(GL_RENDERER));fflush(logfile);}
    return true;
}

static int32_t read_game_int(uint64_t address) {
    int32_t value=-1; mach_vm_size_t copied=0;
    if(mach_vm_read_overwrite(mach_task_self(),address,4,(mach_vm_address_t)&value,&copied)!=KERN_SUCCESS || copied!=4)return -1;
    return value;
}
static CGLError bridge_flush(CGLContextObj context) {
    @autoreleasepool {
        static uint64_t previousSwap=0;
        if(control && owner==context && !control->stopped){
            uint64_t now=mach_continuous_time()*clock_scale.numer/clock_scale.denom;
            report_frame_timing(now);
            RenderProfiler::frame();
            double gap=previousSwap?(now-previousSwap)/1e6:0;previousSwap=now;
            if(gap>35 && logfile){fprintf(logfile,"Producer frame stall: gap_ms=%.3f state=%d substate=%d\n",gap,read_game_int(0x140EDA810),read_game_int(0x140EDA82C));fflush(logfile);}
        }
        if (CGLGetCurrentContext()==context && getenv("PDAFT_FRAME_CONTROL")) {
            pthread_mutex_lock(&mutex);
            GLint vp[4]; glGetIntegerv(GL_VIEWPORT,vp);
            if (!control && vp[2]>0 && vp[3]>0) initialize(context,vp[2],vp[3]);
            if (owner==context && control && !control->stopped && vp[2]==(int)control->width && vp[3]==(int)control->height) {
                if(control->stop_requested)*(volatile bool*)0x140EDA6B0=true;
                install_game_gl_aliases();
                NativeMovie::install();
                            update_game_activation();
                int selected=uiSlot;
                for(unsigned i=0;selected<0 && i<PDAFT_FRAME_SLOTS;i++){
                    uint32_t expected=0;
                    if(__atomic_compare_exchange_n(&control->slots[i].state,&expected,2,false,__ATOMIC_ACQUIRE,__ATOMIC_RELAXED)){selected=i;break;}
                }
                if(selected>=0){
                    GLint readFbo,drawFbo,readBuffer,drawBuffer;
                    glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING,&readFbo);glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING,&drawFbo);
                    glGetIntegerv(GL_READ_BUFFER,&readBuffer);glGetIntegerv(GL_DRAW_BUFFER,&drawBuffer);
                    GLboolean scissor=glIsEnabled(GL_SCISSOR_TEST), srgb=glIsEnabled(GL_FRAMEBUFFER_SRGB);
                    glDisable(GL_SCISSOR_TEST);glDisable(GL_FRAMEBUFFER_SRGB);
                    glBindFramebuffer(GL_READ_FRAMEBUFFER,0);glReadBuffer(GL_BACK);
                    glBindFramebuffer(GL_DRAW_FRAMEBUFFER,fbos[selected]);glDrawBuffer(GL_COLOR_ATTACHMENT0);
                    // GL's bottom-left becomes Metal's top-left in the shared surface.
                    glBlitFramebuffer(vp[0],vp[1]+vp[3],vp[0]+vp[2],vp[1],0,0,vp[2],vp[3],GL_COLOR_BUFFER_BIT,GL_NEAREST);
                    uint64_t fenceStart=mach_continuous_time();glFinish();
                    double fenceMs=(mach_continuous_time()-fenceStart)*clock_scale.numer/(double)clock_scale.denom/1e6;
                    if(fenceMs>8 && logfile){fprintf(logfile,"Producer GPU fence stall: %.3f ms\n",fenceMs);fflush(logfile);}
                    glBindFramebuffer(GL_READ_FRAMEBUFFER,readFbo);glReadBuffer(readBuffer);
                    glBindFramebuffer(GL_DRAW_FRAMEBUFFER,drawFbo);glDrawBuffer(drawBuffer);
                    if(scissor)glEnable(GL_SCISSOR_TEST);if(srgb)glEnable(GL_FRAMEBUFFER_SRGB);
                    PDAFTFrameSlot *slot=&control->slots[selected];
                    slot->sequence=++control->produced;
                    slot->time_ns=mach_continuous_time()*clock_scale.numer/clock_scale.denom;
                    slot->game_state=read_game_int(0x140EDA810);slot->game_substate=read_game_int(0x140EDA82C);
                    control->ui_mask_sequence[selected]=(selected==uiSlot && uiFinished)?slot->sequence:0;
                    uiSlot=-1;uiFinished=false;
                    __atomic_store_n(&slot->state,1,__ATOMIC_RELEASE);
                    if(const char *notification=getenv("PDAFT_FRAME_NOTIFY_NAME"))notify_post(notification);
                }else control->dropped++;
            }
            pthread_mutex_unlock(&mutex);
        }
    }
    if(control && owner==context && !control->stopped && getenv("PDAFT_NATIVE_FRAME_PACING")){
        // One monotonic native clock paces the completed game frame. Avoid the
        // Windows sleep_until + polling limiter and a second hidden-window VSync.
        static uint64_t deadline=0;
        uint64_t now=mach_absolute_time();
        uint64_t period=(uint64_t)(1e9/60.0*clock_scale.denom/clock_scale.numer);
        if(!deadline)deadline=now;
        if(now<deadline)mach_wait_until(deadline);
        uint64_t actual=mach_absolute_time();deadline+=period;
        if(actual>deadline+period)deadline=actual+period;
    }
    // The native Metal window is the sole presenter. The game back buffer is
    // already GPU-complete and copied into the shared ring; presenting Wine's
    // covered auxiliary window would introduce a second display scheduler.
    if(control && owner==context && !control->stopped && getenv("PDAFT_NATIVE_PRESENT_ONLY"))return kCGLNoError;
    return CGLFlushDrawable(context);
}
__attribute__((used)) static struct { const void *replacement,*original; }
interposition __attribute__((section("__DATA,__interpose"))) = {(const void*)bridge_flush,(const void*)CGLFlushDrawable};

// DivaGL's shared get_max_alpha helper uses ALPHA_TEST_DEF in shaders which
// do not declare that variant. Apple's GLSL preprocessor rejects the undefined
// expression, dropping the entire eye/water/refraction program. Preserve all
// declared variants and supply the disabled default only at the shared helper.
static void bridge_shader_source(GLuint shader, GLsizei count, const GLchar *const *strings, const GLint *lengths) {
    if(count<=0 || !strings){glShaderSource(shader,count,strings,lengths);return;}
    std::string source;
    for(GLsizei i=0;i<count;i++)if(strings[i])source.append(strings[i],lengths&&lengths[i]>=0?(size_t)lengths[i]:strlen(strings[i]));
    uiShaders.erase(shader);bool changed=false;
    const std::string helper="float get_max_alpha(float alpha)";
    size_t position=source.find(helper);
    if(source.find("#version 410 core")!=std::string::npos && position!=std::string::npos){
        source.insert(position,"#ifndef ALPHA_TEST_DEF\n#define ALPHA_TEST_DEF 0\n#endif\n");changed=true;
    }
    // Every single-color DivaGL UI shader participates, including custom note
    // and effect shaders. Multi-target 3D shaders retain their original outputs.
    GLint type=0;glGetShaderiv(shader,GL_SHADER_TYPE,&type);
    size_t main=source.find("void main()");
    bool singleColor=source.find("out vec4 result;")!=std::string::npos
        && source.find("layout(location = 1) out")==std::string::npos
        && source.find("layout(location=1) out")==std::string::npos;
    if(type==GL_FRAGMENT_SHADER && singleColor && main!=std::string::npos
        && (source.find("// sprite.frag")!=std::string::npos || source.find("// font.frag")!=std::string::npos)){
        source.replace(main,11,"void pdaft_original_main()");
        source.insert(main,"layout(location = 1) out vec4 pdaft_ui_coverage;\n");
        source.append("\nvoid main(){pdaft_original_main();pdaft_ui_coverage=vec4(0.0,0.0,0.0,clamp(result.a,0.0,1.0));}\n");
        uiShaders.insert(shader);changed=true;
    }
    if(changed){const GLchar *text=source.c_str();GLint length=(GLint)source.size();glShaderSource(shader,1,&text,&length);return;}
    glShaderSource(shader,count,strings,lengths);
}
__attribute__((used)) static struct { const void *replacement,*original; }
shader_interposition __attribute__((section("__DATA,__interpose"))) = {(const void*)bridge_shader_source,(const void*)glShaderSource};
