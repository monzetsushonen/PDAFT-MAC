// Opt-in CPU wall timings of DivaGL debug groups. These include driver waits;
// they are not GPU timestamps. Fixed storage avoids allocating in draw calls.
namespace RenderProfiler {
struct Metric { char name[96]; double total=0,maximum=0; unsigned calls=0; };
struct Scope { uint64_t begin; int metric; };
static thread_local Metric metrics[256];
static thread_local Scope stack[128];
static thread_local unsigned used=0,depth=0,overflow=0,frames=0;
static thread_local double rootMs=0;
static bool enabled(){static bool value=getenv("PDAFT_RENDER_PROFILE")!=nullptr;return value;}
static double elapsed(uint64_t begin){return (mach_continuous_time()-begin)*clock_scale.numer/(double)clock_scale.denom/1e6;}
static void push(GLsizei length,const char *name){
    if(!enabled())return;
    if(depth>=128){++overflow;return;}
    int id=-1;
    if(name){size_t n=length<0?strlen(name):(size_t)length;n=std::min(n,sizeof(metrics[0].name)-1);
        for(unsigned i=0;i<used;i++)if(strlen(metrics[i].name)==n&&!memcmp(metrics[i].name,name,n)){id=i;break;}
        if(id<0&&used<256){id=used++;memcpy(metrics[id].name,name,n);metrics[id].name[n]=0;}
    }
    stack[depth++]={mach_continuous_time(),id};
}
static void pop(){
    if(!enabled())return;
    if(overflow){--overflow;return;}
    if(!depth)return;
    Scope s=stack[--depth];double ms=elapsed(s.begin);
    if(!depth)rootMs+=ms;
    if(s.metric>=0){Metric &m=metrics[s.metric];m.total+=ms;m.maximum=std::max(m.maximum,ms);++m.calls;}
}
static int call_id(const char *name){
    for(unsigned i=0;i<used;i++)if(!strcmp(metrics[i].name,name))return i;
    if(used==256)return -1;unsigned id=used++;strncpy(metrics[id].name,name,sizeof(metrics[id].name)-1);return id;
}
struct CallScope {
    uint64_t begin=0;int id=-1;
    explicit CallScope(const char *name){if(enabled()){id=call_id(name);begin=mach_continuous_time();}}
    ~CallScope(){if(id>=0){double ms=elapsed(begin);Metric &m=metrics[id];m.total+=ms;m.maximum=std::max(m.maximum,ms);++m.calls;}}
};
static void frame(){
    if(!enabled()||!logfile||!control||++frames<120)return;
    fprintf(logfile,"Render CPU scopes: state=%d substate=%d pv=%d frames=%u root_avg_ms=%.3f depth=%u\n",read_game_int(0x140EDA810),read_game_int(0x140EDA82C),read_game_int(0x1418054C4),frames,rootMs/frames,depth);
    for(unsigned i=0;i<used;i++){Metric &m=metrics[i];if(m.calls&&(m.total/frames>.15||m.maximum>5))fprintf(logfile,"  scope=%s avg_frame_ms=%.3f max_ms=%.3f calls=%u\n",m.name,m.total/frames,m.maximum,m.calls);m.total=m.maximum=0;m.calls=0;}
    fflush(logfile);frames=0;rootMs=0;
}
}
