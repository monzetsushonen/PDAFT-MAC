#include <stdio.h>
#include <math.h>
#include "P2InputScheduler.h"
static P2Scheduler s;static P2Note notes[64];static unsigned char keys[256];static int checks;
#define CHECK(x) do{++checks;if(!(x)){fprintf(stderr,"FAIL line %d: %s\n",__LINE__,#x);return 1;}}while(0)
static void clean(void){p2Reset(&s);for(int i=0;i<64;++i){notes[i].type=0;notes[i].hit=21;notes[i].left=0;}}
static int tapped(int primary,int secondary,const unsigned char*before,const unsigned char*now){return(now[primary]&&!before[primary])||(now[secondary]&&!before[secondary]);}
int main(void){P2Frame f;unsigned char before[256];
 clean();notes[0]=(P2Note){0,21,.020f};f=p2Schedule(&s,notes,1,keys);CHECK(f.attempted==1&&keys['W']);
 notes[0].left=.003f;f=p2Schedule(&s,notes,1,keys);CHECK(f.attempted==0&&!keys['W']&&!keys['I']);
 notes[0].left=-.014f;f=p2Schedule(&s,notes,1,keys);CHECK(f.attempted==0);
 notes[0].left=1.0f;p2Schedule(&s,notes,1,keys);notes[0].left=.015f;f=p2Schedule(&s,notes,1,keys);CHECK(f.attempted==1);
 clean();notes[0]=(P2Note){2,21,.020f};p2Schedule(&s,notes,1,keys);for(int i=0;i<256;++i)before[i]=keys[i];
 notes[0].hit=0;notes[0].left=.003f;notes[1]=(P2Note){2,21,.018f};f=p2Schedule(&s,notes,1,keys);
 CHECK(f.attempted==1&&keys['K']&&!keys['S']);CHECK(tapped('S','K',before,keys));
 clean();notes[0]=(P2Note){0,21,.100f};p2Schedule(&s,notes,1,keys);notes[0].left=.018f;p2Schedule(&s,notes,1,keys);
 notes[1]=(P2Note){1,21,.019f};p2Schedule(&s,notes,1,keys);/* become observed */
 f=p2Schedule(&s,notes,1,keys);CHECK(!f.advanced&&!f.attempted&&!keys['D']);
 notes[2]=(P2Note){3,21,.014f};f=p2Schedule(&s,notes,0,keys);CHECK(!f.attempted&&!keys['A']);
 f=p2Schedule(&s,notes,1,keys);CHECK(f.attempted>0&&keys['A']);
 clean();notes[0]=(P2Note){0,21,.019f};notes[1]=(P2Note){1,21,.018f};notes[2]=(P2Note){3,21,.017f};f=p2Schedule(&s,notes,1,keys);CHECK(f.attempted==3&&keys['W']&&keys['D']&&keys['A']);
 clean();notes[0]=(P2Note){0,21,NAN};notes[1]=(P2Note){1,21,INFINITY};f=p2Schedule(&s,notes,1,keys);CHECK(!f.attempted&&!keys['W']&&!keys['D']);
 clean();notes[0]=(P2Note){0,21,.060f};p2Schedule(&s,notes,1,keys);notes[0].left=-.080f;f=p2Schedule(&s,notes,1,keys);CHECK(f.late==1&&!f.attempted);
 clean();notes[0]=(P2Note){11,21,.010f};f=p2Schedule(&s,notes,1,keys);CHECK(f.unsupported==1&&!f.attempted);
 clean();notes[0]=(P2Note){0,0,.010f};f=p2Schedule(&s,notes,1,keys);CHECK(!f.attempted);
 clean();f=p2Schedule(&s,notes,1,keys);CHECK(!f.advanced&&!f.attempted);
 /* Slider emulator queries AnyDown before AnyTapped. A key must persist
  * into another poll to establish contact and motion after lastDownId is set. */
 clean();notes[0]=(P2Note){13,21,.020f};p2Schedule(&s,notes,1,keys);CHECK(keys['O']);
 notes[0].hit=0;notes[0].left=.003f;f=p2Schedule(&s,notes,1,keys);CHECK(keys['O']&&!f.attempted);
 notes[0].left=-.014f;p2Schedule(&s,notes,1,keys);CHECK(keys['O']);
 notes[0].left=-.031f;p2Schedule(&s,notes,1,keys);CHECK(!keys['O']);
 clean();notes[0]=(P2Note){16,21,.020f};notes[1]=(P2Note){16,21,.100f};p2Schedule(&s,notes,1,keys);
 notes[0].hit=5;
 for(int i=0;i<7;++i){notes[0].left-=.006f;notes[1].left-=.006f;p2Schedule(&s,notes,1,keys);CHECK(keys['O']);}
 f=p2Schedule(&s,notes,1,keys);CHECK(!f.advanced&&!keys['O']);
 notes[1].left-=.006f;p2Schedule(&s,notes,1,keys);CHECK(keys['O']);
 notes[1].hit=5;notes[1].left-=.006f;p2Schedule(&s,notes,1,keys);CHECK(!keys['O']);
 clean();notes[0]=(P2Note){12,21,.020f};notes[1]=(P2Note){13,21,.020f};p2Schedule(&s,notes,1,keys);CHECK(keys['Q']&&keys['O']);
 p2Schedule(&s,notes,0,keys);CHECK(!keys['Q']&&!keys['O']);
 printf("PASS %d scheduler regressions: tap/repress/chord, paused timeline, slot reuse, invalid/late input, slider contact persistence and long-slide continuation/release. HOLD bonus and real chart coverage still require game acceptance.\n",checks);return 0;
}
