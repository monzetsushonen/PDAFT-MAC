// Measure game-side frame intervals, including stalls hidden by average FPS.
#include <algorithm>
static void report_frame_timing(uint64_t now){
    static uint64_t previous=0;
    static int state=-1,substate=-1,pv=-1;
    static double samples[120];static unsigned count=0;
    int current=read_game_int(0x140EDA810),sub=read_game_int(0x140EDA82C),song=read_game_int(0x1418054C4);
    if(current!=state || sub!=substate || song!=pv){count=0;previous=0;state=current;substate=sub;pv=song;}
    if(previous)samples[count++]=(now-previous)/1e6;
    previous=now;if(count<120)return;
    double total=0;unsigned over=0;for(double value:samples){total+=value;if(value>25)++over;}
    std::sort(samples,samples+120);
    if(logfile){fprintf(logfile,"Frame timing: state=%d substate=%d pv=%d frames=120 fps=%.3f p50_ms=%.3f p95_ms=%.3f p99_ms=%.3f max_ms=%.3f over25ms=%u\n",state,substate,pv,120000.0/total,samples[59],samples[113],samples[118],samples[119],over);fflush(logfile);}
    count=0;
}
