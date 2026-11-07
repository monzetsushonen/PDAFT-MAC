#pragma once
#include "TestTargetInput.h"
/* Test input scheduling only. No game judgment, clock or score is written.
 * A target is attempted once per slot generation. Alternate bound keys let
 * successive notes produce fresh TLAC Keyboard::IsTapped edges.
 */
typedef struct {int type,hit;float left;} P2Note;
typedef struct {
 P2Note previous[64];unsigned char fired[64],seen[64],previousKeys[256],slideFrames[2],longSlide[2];
} P2Scheduler;
typedef struct {unsigned int attempted,unsupported,advanced,late;} P2Frame;
static void p2Reset(P2Scheduler*s){
 for(int i=0;i<64;++i){s->previous[i].type=0;s->previous[i].hit=0;s->previous[i].left=0;s->fired[i]=s->seen[i]=0;}
 for(int i=0;i<256;++i)s->previousKeys[i]=0;
 for(int i=0;i<2;++i)s->slideFrames[i]=s->longSlide[i]=0;
}
static int p2Group(int key){
 switch(key){case 'W':return 0;case 'D':return 1;case 'S':return 2;case 'A':return 3;case 'Q':return 4;case 'O':return 5;default:return -1;}
}
static P2Frame p2Schedule(P2Scheduler*s,const P2Note*notes,int playing,unsigned char*keys){
 P2Frame out={0,0,0,0};for(int i=0;i<256;++i)keys[i]=0;
 if(!playing){p2Reset(s);return out;}
 for(int i=0;i<64;++i){
  const P2Note*n=notes+i;const P2Note*old=s->previous+i;
  int finite=n->left==n->left&&n->left<1000000.0f&&n->left>-1000000.0f;
  if(finite&&n->left!=old->left)out.advanced=1;
  if(!s->seen[i]||n->type!=old->type||n->hit!=21||old->hit!=21||
     (finite&&n->left>old->left+.010f&&n->left>.023f))s->fired[i]=0;
  if(finite&&s->seen[i]&&!s->fired[i]&&n->hit==21&&old->left>.023f&&n->left<-.040f)++out.late;
 }
 if(out.advanced){
  unsigned char groups[4]={0,0,0,0},longUpcoming[2]={0,0};
  for(int i=0;i<64;++i)if(notes[i].hit==21&&notes[i].left!=0&&notes[i].left<=.13f&&notes[i].left>=-.040f){
   if(notes[i].type==15)longUpcoming[0]=1;
   if(notes[i].type==16)longUpcoming[1]=1;
  }
  for(int i=0;i<64;++i){int key=p2TargetKey(notes[i].type,notes[i].hit,notes[i].left);if(key==-2)++out.unsupported;
   if(key>=0&&!s->fired[i]){int group=p2Group(key);if(group>=0){
    if(group<4)groups[group]=1;
    else{int direction=group-4;s->slideFrames[direction]=3;if(notes[i].type==15||notes[i].type==16)s->longSlide[direction]=1;}
    s->fired[i]=1;++out.attempted;
   }}
  }
  const int pairs[4][2]={{'W','I'},{'D','L'},{'S','K'},{'A','J'}};
  for(int i=0;i<4;++i)if(groups[i]){int a=pairs[i][0],b=pairs[i][1];keys[s->previousKeys[a]?b:a]=1;}
  for(int i=0;i<2;++i){
   if(s->longSlide[i]&&!longUpcoming[i])s->longSlide[i]=0;
   if(s->slideFrames[i]||s->longSlide[i])keys[i?'O':'Q']=1;
   if(s->slideFrames[i])--s->slideFrames[i];
  }
 }
 for(int i=0;i<64;++i){s->previous[i]=notes[i];s->seen[i]=1;}
 for(int i=0;i<256;++i)s->previousKeys[i]=keys[i];
 return out;
}
