// Native GStreamer decoder -> game's own OpenGL texture. No Windows MF/DXVA queue.
// IPlayer ABI: ReDIVA/movie_play_lib/movie_play_lib.hpp; texture API: AFTModsShared.
// GstMiniObject/GstBuffer public 1.x layout (pts at byte 72); checked by decoder probe.
#include <dlfcn.h>
#include <vector>
#include <algorithm>

namespace NativeMovie {
struct API {
    void *core=nullptr,*app=nullptr;
    int (*init)(int*,char***,void**);
    void* (*parse)(const char*,void**);
    int (*state)(void*,int);
    int (*getState)(void*,int*,int*,uint64_t);
    void* (*getByName)(void*,const char*);
    void (*unref)(void*);
    void (*miniUnref)(void*);
    void* (*buffer)(void*);
    void* (*caps)(void*);
    void* (*structure)(void*,unsigned);
    int (*getInt)(void*,const char*,int*);
    size_t (*bufferSize)(void*);
    size_t (*extract)(void*,size_t,void*,size_t);
    int (*duration)(void*,int,int64_t*);
    int (*seek)(void*,int,int,int64_t);
    void* (*sample)(void*,uint64_t);
    void* (*preroll)(void*,uint64_t);
    bool ready=false;
    bool load(){
        if(ready)return true;
        NSString *root=@(getenv("PDAFT_WINE_ROOT")?:"");
        NSString *lib=[root stringByAppendingPathComponent:@"lib/GStreamer.framework/Versions/1.0/lib"];
        core=dlopen([[lib stringByAppendingPathComponent:@"libgstreamer-1.0.0.dylib"]fileSystemRepresentation],RTLD_NOW|RTLD_GLOBAL);
        app=dlopen([[lib stringByAppendingPathComponent:@"libgstapp-1.0.0.dylib"]fileSystemRepresentation],RTLD_NOW|RTLD_GLOBAL);
        if(!core||!app){if(logfile){fprintf(logfile,"Native movie loader: %s\n",dlerror());fflush(logfile);}return false;}
#define GET(field,lib,symbol) field=(decltype(field))dlsym(lib,symbol);if(!field)return false
        GET(init,core,"gst_init_check");GET(parse,core,"gst_parse_launch");
        GET(state,core,"gst_element_set_state");GET(getState,core,"gst_element_get_state");
        GET(getByName,core,"gst_bin_get_by_name");GET(unref,core,"gst_object_unref");
        GET(miniUnref,core,"gst_mini_object_unref");GET(buffer,core,"gst_sample_get_buffer");
        GET(caps,core,"gst_sample_get_caps");GET(structure,core,"gst_caps_get_structure");
        GET(getInt,core,"gst_structure_get_int");GET(bufferSize,core,"gst_buffer_get_size");
        GET(extract,core,"gst_buffer_extract");GET(duration,core,"gst_element_query_duration");
        GET(seek,core,"gst_element_seek_simple");GET(sample,app,"gst_app_sink_try_pull_sample");
        GET(preroll,app,"gst_app_sink_try_pull_preroll");
#undef GET
        ready=init(nullptr,nullptr,nullptr);return ready;
    }
} api;

struct GameTexture {int32_t refs;uint32_t id,flags;int16_t width,height;uint32_t glid,target,format;int32_t mip,size;};
static_assert(sizeof(GameTexture)==36,"AFT texture ABI");
typedef GameTexture* (__attribute__((ms_abi)) *LoadTexture)(uint32_t,uint32_t,int32_t,int32_t,int32_t,const void**,bool);
typedef void (__attribute__((ms_abi)) *ReleaseTexture)(GameTexture*);
struct Player;
struct Interop {void **vtable;uint32_t refs=1;Player *player;};
static void *playerVtable[19],*interopVtable[4];

struct Player {
    void **vtable=playerVtable;
    uint32_t refs=1;
    Interop interop;
    void *pipeline=nullptr,*sink=nullptr,*pending=nullptr,*clock=nullptr;
    GameTexture *texture=nullptr;
    std::vector<uint8_t> pixels;
    int width=0,height=0,status=0;
    double length=0,start=0,offset=0;
    bool playing=false;
    uint64_t uploads=0;
    Player(){interop.vtable=interopVtable;interop.player=this;}
    double now(){return (double)mach_continuous_time()*clock_scale.numer/clock_scale.denom/1e9;}
    double position(){
        if(clock){typedef int64_t (__attribute__((ms_abi)) *ClockTime)(void*);auto v=*(void***)clock;return std::max(0.0,((ClockTime)v[6])(clock)/1e7);}
        return offset+(playing?now()-start:0);
    }
    void close(){
        if(pipeline)api.state(pipeline,1); // GST_STATE_NULL joins decoder-owned tasks.
        if(pending){api.miniUnref(pending);pending=nullptr;}
        if(sink){api.unref(sink);sink=nullptr;}
        if(pipeline){api.unref(pipeline);pipeline=nullptr;}
        if(texture){((ReleaseTexture)0x14069DA70)(texture);texture=nullptr;}
        pixels.clear();playing=false;status=0;length=0;uploads=0;
    }
    void dimensions(void *s){
        void *caps=api.caps(s);if(!caps)return;void *st=api.structure(caps,0);
        api.getInt(st,"width",&width);api.getInt(st,"height",&height);
    }
    int open(NSString *path){
        close();if(!api.load()){status=6;return int32_t(0x80004005);}
        path=[path stringByReplacingOccurrencesOfString:@"\\" withString:@"/"];
        if([path hasPrefix:@"Z:/"]||[path hasPrefix:@"z:/"])path=[path substringFromIndex:2];
        else if([path hasPrefix:@"C:/PDAFT/"])path=[@(getenv("PDAFT_GAME_ROOT")?:"") stringByAppendingPathComponent:[path substringFromIndex:9]];
        if(!path.isAbsolutePath)path=[NSFileManager.defaultManager.currentDirectoryPath stringByAppendingPathComponent:path];
        if(![NSFileManager.defaultManager fileExistsAtPath:path] && [path.pathExtension.lowercaseString isEqual:@"mp4"]){
            NSString *wmv=[path.stringByDeletingPathExtension stringByAppendingPathExtension:@"wmv"];
            if([NSFileManager.defaultManager fileExistsAtPath:wmv])path=wmv;
        }
        NSString *uri=[NSURL fileURLWithPath:path].absoluteString;
        NSString *description=[NSString stringWithFormat:@"uridecodebin uri=\"%@\" ! videoconvert ! video/x-raw,format=BGRA ! appsink name=pdaft_sink sync=false max-buffers=3 drop=false",uri];
        pipeline=api.parse(description.UTF8String,nullptr);
        if(!pipeline){status=6;return int32_t(0x80004005);}
        sink=api.getByName(pipeline,"pdaft_sink");
        if(!sink||api.state(pipeline,3)==0){close();status=6;return int32_t(0x80004005);}
        api.getState(pipeline,nullptr,nullptr,5*1000000000ULL);
        pending=api.preroll(sink,1000000000ULL);
        if(!pending){if(logfile){fprintf(logfile,"Native movie preroll failed: %s\n",path.UTF8String);fflush(logfile);}close();status=6;return int32_t(0x80004005);}
        dimensions(pending);int64_t ns=0;api.duration(pipeline,3,&ns);length=ns/1e9;
        status=2;offset=0;
        if(logfile){fprintf(logfile,"Native movie open: %s %dx%d duration=%.3f\n",path.UTF8String,width,height,length);fflush(logfile);}
        return 0;
    }
    GameTexture *frame(){
        if(!pipeline||width<=0||height<=0||!CGLGetCurrentContext())return texture;
        double target=position();
        // The appsink backpressures decoding. Consume only frames due on the game clock.
        for(unsigned i=0;i<32;i++){
            if(!pending)pending=api.sample(sink,0);
            if(!pending)break;
            void *b=api.buffer(pending);uint64_t pts=0;memcpy(&pts,(uint8_t*)b+72,8);
            if(pts!=UINT64_MAX && pts/1e9>target+.005 && texture)break;
            size_t size=api.bufferSize(b),required=(size_t)width*height*4;
            if(size==required){
                pixels.resize(size);api.extract(b,0,pixels.data(),size);
                if(!texture){static uint32_t counter=0x4000;texture=((LoadTexture)0x14069B8E0)(0x2F000000|counter++,GL_RGBA8,width,height,0,nullptr,false);}
                if(texture){
                    GLint oldTexture,alignment,rowLength,unpackBuffer;glGetIntegerv(GL_TEXTURE_BINDING_2D,&oldTexture);
                    glGetIntegerv(GL_PIXEL_UNPACK_BUFFER_BINDING,&unpackBuffer);glBindBuffer(GL_PIXEL_UNPACK_BUFFER,0);
                    glGetIntegerv(GL_UNPACK_ALIGNMENT,&alignment);glGetIntegerv(GL_UNPACK_ROW_LENGTH,&rowLength);
                    glBindTexture(GL_TEXTURE_2D,texture->glid);glPixelStorei(GL_UNPACK_ALIGNMENT,1);glPixelStorei(GL_UNPACK_ROW_LENGTH,0);
                    glTexSubImage2D(GL_TEXTURE_2D,0,0,0,width,height,GL_BGRA,GL_UNSIGNED_BYTE,pixels.data());
                    glPixelStorei(GL_UNPACK_ALIGNMENT,alignment);glPixelStorei(GL_UNPACK_ROW_LENGTH,rowLength);glBindTexture(GL_TEXTURE_2D,oldTexture);glBindBuffer(GL_PIXEL_UNPACK_BUFFER,unpackBuffer);
                    uploads++;
                    if(logfile&&(uploads==1||uploads%1800==0)){fprintf(logfile,"Native movie frame: uploads=%llu pts=%.3f game=%.3f texture=%u\n",uploads,pts/1e9,target,texture->glid);fflush(logfile);}
                }
            }
            api.miniUnref(pending);pending=nullptr;
        }
        return texture;
    }
};

#define MS __attribute__((ms_abi))
static int32_t MS query(Player *p,const void*,void **out){if(!out)return int32_t(0x80004003);*out=p;p->refs++;return 0;}
static uint32_t MS retain(Player *p){return ++p->refs;}
static uint32_t MS release(Player *p){uint32_t n=--p->refs;if(!n){p->close();if(p->clock){typedef uint32_t (__attribute__((ms_abi)) *F)(void*);((F)(*(void***)p->clock)[2])(p->clock);}delete p;}return n;}
static int32_t MS openW(Player *p,const uint16_t *path){@autoreleasepool{size_t n=0;while(path[n])n++;return p->open([[NSString alloc]initWithCharacters:path length:n]);}}
static int32_t MS openA(Player *p,const char *path){@autoreleasepool{return p->open(@(path));}}
static int32_t MS close(Player *p){p->close();return 0;}
static int32_t MS setClock(Player *p,void *clock){typedef uint32_t (__attribute__((ms_abi)) *F)(void*);if(clock)((F)(*(void***)clock)[1])(clock);if(p->clock)((F)(*(void***)p->clock)[2])(p->clock);p->clock=clock;return 0;}
static int32_t MS getStatus(Player *p){if(p->status==4&&p->length>0&&p->position()>=p->length+.1)return 6;return p->status;}
static int32_t MS play(Player *p){if(p->pipeline&&!p->playing){p->start=p->now();p->playing=true;p->status=4;api.state(p->pipeline,4);}return 0;}
static int32_t MS pause(Player *p){p->offset=p->position();p->playing=false;if(p->pipeline)api.state(p->pipeline,3);return 0;}
static double MS duration(Player *p){return p->length;}
static double MS position(Player *p){return p->position();}
static int32_t MS seek(Player *p,double pos){if(p->pending){api.miniUnref(p->pending);p->pending=nullptr;}p->offset=pos;p->start=p->now();return p->pipeline&&api.seek(p->pipeline,3,3,int64_t(pos*1e9))?0:int32_t(0x80004005);}
static int32_t MS getVolumes(Player*,float *out){if(out)for(int i=0;i<6;i++)out[i]=0;return 0;}
static int32_t MS setVolumes(Player*,const void*){return 0;}
struct VideoInfo {int32_t width,height,rawWidth,rawHeight;float fps,aspect;};
static int32_t MS info(Player *p,VideoInfo *v){if(!v)return int32_t(0x80004003);*v={p->width,p->height,p->width,p->height,60,1};return 0;}
static int32_t MS d3d(Player*,void*,void **out){if(out)*out=nullptr;return int32_t(0x80004001);}
static int32_t MS getOGL(Player *p,void **out){if(!out)return int32_t(0x80004003);p->interop.refs++;*out=&p->interop;return 0;}
static int32_t MS interopQuery(Interop *p,const void*,void **out){if(!out)return int32_t(0x80004003);*out=p;p->refs++;return 0;}
static uint32_t MS interopRetain(Interop *p){return ++p->refs;}
static uint32_t MS interopRelease(Interop *p){return --p->refs;}
static GameTexture* MS getGL(Interop *p){return p->player->frame();}
static int32_t MS create(Player **out){if(!out)return int32_t(0x80004003);*out=new Player;return 0;}
#undef MS

static void install(){
    static bool installed=false;if(installed||!getenv("PDAFT_NATIVE_MOVIES"))return;
    uint8_t *entry=(uint8_t*)0x1404255c0;
    const uint8_t expected[16]={0x40,0x56,0x48,0x83,0xec,0x20,0x83,0x3d,0xb3,0xc4,0xd7,0,0,0x48,0x8b,0xf1};
    uint8_t actual[16];mach_vm_size_t copied=0;
    if(mach_vm_read_overwrite(mach_task_self(),(mach_vm_address_t)entry,16,(mach_vm_address_t)actual,&copied)!=KERN_SUCCESS||copied!=16||memcmp(actual,expected,16))return;
    void *v[19]={(void*)query,(void*)retain,(void*)release,(void*)openW,(void*)openA,(void*)close,(void*)setClock,(void*)getStatus,(void*)play,(void*)pause,(void*)duration,(void*)position,(void*)seek,(void*)getVolumes,(void*)setVolumes,(void*)info,(void*)d3d,(void*)d3d,(void*)getOGL};
    memcpy(playerVtable,v,sizeof(v));void *iv[4]={(void*)interopQuery,(void*)interopRetain,(void*)interopRelease,(void*)getGL};memcpy(interopVtable,iv,sizeof(iv));
    if(mach_vm_protect(mach_task_self(),(mach_vm_address_t)entry,16,FALSE,VM_PROT_READ|VM_PROT_WRITE)!=KERN_SUCCESS)return;
    uint8_t jump[16]={0xff,0x25,0,0,0,0};uint64_t address=(uint64_t)create;memcpy(jump+6,&address,8);jump[14]=jump[15]=0x90;memcpy(entry,jump,16);
    mach_vm_protect(mach_task_self(),(mach_vm_address_t)entry,16,FALSE,VM_PROT_READ|VM_PROT_EXECUTE);
    installed=true;if(logfile){fprintf(logfile,"Native movie IPlayer factory installed (game-clock synchronized)\n");fflush(logfile);}
}
}
