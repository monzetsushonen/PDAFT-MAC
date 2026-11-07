/* Offline test-input adapter for the pinned PDAFT/TLAC binaries.
 * No game judgment/score/timeline code is replaced. No desktop input API.
 * Scope v3: edge-scheduled tap/chance/slide inputs; HOLD bonus completeness is unverified.
 */
typedef unsigned char U8;typedef unsigned short U16;typedef unsigned int U32;typedef unsigned long long U64;typedef long long I64;typedef void* HANDLE;
int _fltused=0;
#define API __declspec(dllimport)
API HANDLE GetModuleHandleA(const char*);API U32 GetEnvironmentVariableA(const char*,char*,U32);API int VirtualProtect(void*,U64,U32,U32*);API HANDLE CreateThread(void*,U64,U32(*)(void*),void*,U32,U32*);API int CloseHandle(HANDLE);API void Sleep(U32);API int DisableThreadLibraryCalls(HANDLE);API HANDLE GetCurrentProcess(void);API int ReadProcessMemory(HANDLE,const void*,void*,U64,U64*);API int QueryPerformanceCounter(I64*);API int QueryPerformanceFrequency(I64*);API HANDLE CreateFileA(const char*,U32,U32,void*,U32,U32,HANDLE);API int WriteFile(HANDLE,const void*,U32,U32*,void*);API int FlushFileBuffers(HANDLE);API U32 GetCurrentProcessId(void);
void*_InterlockedExchangePointer(void*volatile*,void*);
#pragma intrinsic(_InterlockedExchangePointer)
static short(*originalKey)(int);static U8 keys[256];static volatile U32 enabled,hooked,unexpectedTypes,activeNotes;static volatile U64 polls,pulses;static HANDLE report;static I64 frequency,start;static int previousState=-1,previousSub=-1;static U32 resultRecorded;static U64 pollDelta;
#include "P2InputScheduler.h"
static P2Scheduler scheduler;static volatile U64 lateWindows;static volatile U32 schedulerAdvanced,observedHits[9];static int lastHit[64],wasPlaying;static volatile U32 tapWorst,slideWorst,lastWorstType;
static U32 u32(const U8*p){return (U32)p[0]|((U32)p[1]<<8)|((U32)p[2]<<16)|((U32)p[3]<<24);}
static int equal(const char*a,const char*b){while(*a&&*b){if(*a++!=*b++)return 0;}return *a==*b;}
static U64 milliseconds(void){I64 now;QueryPerformanceCounter(&now);return (U64)((now-start)*1000/frequency);}
static void text(char*b,U32*n,const char*s){while(*s&&*n<2046)b[(*n)++]=*s++;}
static void number(char*b,U32*n,U64 v){char a[24];U32 z=0;do{a[z++]=(char)('0'+v%10);v/=10;}while(v&&z<24);while(z&&*n<2046)b[(*n)++]=a[--z];}
static void field(char*b,U32*n,const char*s,U64 v){text(b,n,s);number(b,n,v);}
static void writeLine(char*b,U32 n){if(!report||report==(HANDLE)-1)return;U32 written;b[n++]='\n';WriteFile(report,b,n,&written,0);}
static void event(const char*name){char b[2048];U32 n=0;text(b,&n,"{\"event\":\"");text(b,&n,name);text(b,&n,"\"");field(b,&n,",\"ms\":",milliseconds());field(b,&n,",\"pid\":",GetCurrentProcessId());text(b,&n,"}");writeLine(b,n);}
static int safeRead(U64 address,void*out,U64 count){U64 got=0;return address && ReadProcessMemory(GetCurrentProcess(),(void*)address,out,count,&got)&&got==count;}
static short testKeys(int key){
 if(key==0){
  ++polls;
  int state=*(volatile int*)0x140EDA810ULL,sub=*(volatile int*)0x140EDA82CULL;
  int playing=enabled && state==2 && sub==13;P2Note notes[64];
  for(U32 i=0;i<64;++i){
   if(playing){const U8*t=(const U8*)(0x140D0B688ULL+(U64)i*0x4a8);
    notes[i].type=*(const int*)(t+0x14);notes[i].hit=*(const int*)(t+0x45c);notes[i].left=*(const float*)(t+0x18);
    if(!wasPlaying)lastHit[i]=notes[i].hit;
    else if(lastHit[i]==21 && notes[i].hit>=0 && notes[i].hit<=8){++observedHits[notes[i].hit];if(notes[i].hit==8){lastWorstType=notes[i].type;if(notes[i].type==12||notes[i].type==13||notes[i].type==15||notes[i].type==16||notes[i].type>=23)++slideWorst;else ++tapWorst;}}
    lastHit[i]=notes[i].hit;
   }else{notes[i].type=0;notes[i].hit=21;notes[i].left=0;}
  }
  if(playing&&!wasPlaying){for(int i=0;i<9;++i)observedHits[i]=0;tapWorst=slideWorst=lastWorstType=0;}
  P2Frame frame=p2Schedule(&scheduler,notes,playing,keys);pulses+=frame.attempted;unexpectedTypes+=frame.unsupported;lateWindows+=frame.late;schedulerAdvanced=frame.advanced;
  U32 count=0;for(int i=0;i<256;++i)count+=keys[i]!=0;activeNotes=count;wasPlaying=playing;
 }
 short value=originalKey?originalKey(key):0;if(enabled&&key>=0&&key<256&&keys[key])value|=(short)0x8001;return value;
}
static int install(HANDLE module){
 U8*b=module;if(!b||b[0]!='M'||b[1]!='Z')return 0;U32 pe=u32(b+60);if(pe>0x100000||u32(b+pe)!=0x4550)return 0;
 U32 size=u32(b+pe+24+56),rva=u32(b+pe+24+120);if(!rva||rva>size-20)return 0;
 for(U32 d=0;d<512 && rva+d*20<size-20;++d){U8*desc=b+rva+d*20;U32 lookup=u32(desc),name=u32(desc+12),iat=u32(desc+16);if(!name)break;if(!lookup)continue;
  if(lookup>=size||iat>=size)return 0;
  for(U32 i=0;i<2048 && lookup+(U64)(i+1)*8<=size && iat+(U64)(i+1)*8<=size;++i){U64 entry=((U64*)(b+lookup))[i];if(!entry)break;if(entry>>63)continue;if(entry+2+32>=size)return 0;
   if(equal((char*)b+entry+2,"GetAsyncKeyState")){void*volatile*slot=(void*volatile*)(b+iat+i*8);U32 protection=0,ignored=0;if(!VirtualProtect((void*)slot,8,4,&protection))return 0;originalKey=(void*)*slot;_InterlockedExchangePointer(slot,(void*)testKeys);VirtualProtect((void*)slot,8,protection,&ignored);return 1;}
  }
 }return 0;
}
static void snapshot(void){
 int state=-1,sub=-1;safeRead(0x140EDA810ULL,&state,4);safeRead(0x140EDA82CULL,&sub,4);
 char b[2048];U32 n=0;int changed=state!=previousState||sub!=previousSub;
 text(b,&n,changed?"{\"event\":\"state\"":"{\"event\":\"tick\"");field(b,&n,",\"ms\":",milliseconds());field(b,&n,",\"state\":",(U32)state);field(b,&n,",\"substate\":",(U32)sub);field(b,&n,",\"hooked\":",hooked);field(b,&n,",\"autoplay\":",enabled);field(b,&n,",\"polls\":",polls);field(b,&n,",\"pollsSinceTick\":",polls-pollDelta);pollDelta=polls;field(b,&n,",\"pulses\":",pulses);field(b,&n,",\"activeTargets\":",activeNotes);field(b,&n,",\"unsupportedTargets\":",unexpectedTypes);field(b,&n,",\"schedulerVersion\":",3);field(b,&n,",\"targetTimesAdvanced\":",schedulerAdvanced);field(b,&n,",\"lateWindows\":",lateWindows);field(b,&n,",\"observedCool\":",observedHits[0]);field(b,&n,",\"observedFine\":",observedHits[1]);field(b,&n,",\"observedSafe\":",observedHits[2]);field(b,&n,",\"observedSad\":",observedHits[3]);field(b,&n,",\"observedWorst\":",observedHits[8]);field(b,&n,",\"observedTapWorst\":",tapWorst);field(b,&n,",\"observedSlideWorst\":",slideWorst);field(b,&n,",\"lastWorstType\":",lastWorstType);field(b,&n,",\"sliderHeldMask\":",(keys['Q']?1:0)|(keys['O']?2:0));text(b,&n,"}");writeLine(b,n);
 previousState=state;previousSub=sub;
 if(state==2&&sub==13)resultRecorded=0;
 if(state==2&&sub==15&&!resultRecorded){
  U64 base=0;if(safeRead(0x14CC93830ULL+0x100,&base,8)&&base){U8 data[0x19c];if(safeRead(base,data,sizeof(data))){
   n=0;text(b,&n,"{\"event\":\"native_result\"");field(b,&n,",\"ms\":",milliseconds());field(b,&n,",\"pv\":",u32(data+0x2c));field(b,&n,",\"difficulty\":",u32(data+0x34));field(b,&n,",\"ex\":",u32(data+0x44));field(b,&n,",\"score\":",u32(data+0x18c));field(b,&n,",\"rawPercent\":",u32(data+0x190));field(b,&n,",\"combo\":",u32(data+0x180));field(b,&n,",\"holdScore\":",u32(data+0x188));field(b,&n,",\"slideScore\":",u32(data+0x194));
   for(int i=0;i<5;++i){const char*names[]={",\"cool\":",",\"fine\":",",\"safe\":",",\"sad\":",",\"worst\":"};field(b,&n,names[i],u32(data+0x158+i*4));}text(b,&n,"}");writeLine(b,n);resultRecorded=1;FlushFileBuffers(report);
  }}
 }
}
static U32 worker(void*unused){
 (void)unused;char path[1024],flag[16];if(!GetEnvironmentVariableA("PDAFT_P2_REPORT",path,sizeof(path)))return 1;
 QueryPerformanceFrequency(&frequency);QueryPerformanceCounter(&start);if(frequency<=0)return 2;
 report=CreateFileA(path,0x40000000U,5,0,2,0x80,0);if(report==(HANDLE)-1)return 3;
 enabled=GetEnvironmentVariableA("PDAFT_P2_AUTOPLAY",flag,sizeof(flag))&&flag[0]=='1';event("test_adapter_loaded");event("edge_scheduler_v3");
 if(*(U16*)0x140000000ULL!=0x5a4d){event("image_guard_failed");return 4;}
 for(int i=0;i<3000&&!hooked;++i){HANDLE tlac=GetModuleHandleA("TLAC.dva");if(tlac)hooked=install(tlac);if(!hooked)Sleep(10);}
 event(hooked?"input_adapter_installed":"input_adapter_failed");if(!hooked)return 5;
 for(;;){snapshot();Sleep(250);}
}
int DllMain(HANDLE module,U32 reason,void*reserved){(void)reserved;if(reason==1){DisableThreadLibraryCalls(module);HANDLE thread=CreateThread(0,0,worker,0,0,0);if(thread)CloseHandle(thread);}return 1;}
