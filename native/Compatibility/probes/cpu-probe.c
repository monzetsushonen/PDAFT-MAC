/* Original standalone Windows x64 compatibility probe, no game code or data. */
typedef unsigned int U32; typedef unsigned long long U64;
typedef long long I64; typedef void *HANDLE;
int _fltused=0;
#define API __declspec(dllimport)
API HANDLE GetStdHandle(U32);
API int WriteFile(HANDLE,const void*,U32,U32*,void*);
API void ExitProcess(U32);
API U32 GetLastError(void);
API U32 GetCurrentProcessId(void);
API HANDLE GetCurrentProcess(void);
API int QueryPerformanceCounter(I64*);
API int QueryPerformanceFrequency(I64*);
API void Sleep(U32);
API U32 TlsAlloc(void);
API int TlsSetValue(U32,void*);
API void *TlsGetValue(U32);
API int TlsFree(U32);
API HANDLE CreateThread(void*,U64,U32(*)(void*),void*,U32,U32*);
API U32 WaitForMultipleObjects(U32,const HANDLE*,int,U32);
API int CloseHandle(HANDLE);
API void *VirtualAlloc(void*,U64,U32,U32);
API int VirtualFree(void*,U64,U32);
API int FlushInstructionCache(HANDLE,const void*,U64);
API void RaiseException(U32,U32,U32,const U64*);
long long _InterlockedIncrement64(volatile long long*);
#pragma intrinsic(_InterlockedIncrement64)
static U32 tls; static volatile I64 increments; static volatile U32 threadErrors;
static void line(const char*s){U32 n=0,w;while(s[n])++n;WriteFile(GetStdHandle((U32)-11),s,n,&w,0);}
static void hex(const char *label,U64 value){char s[19];const char*d="0123456789abcdef";s[0]='0';s[1]='x';for(int i=0;i<16;++i)s[i+2]=d[(value>>((15-i)*4))&15];s[18]='\n';U32 w;line(label);WriteFile(GetStdHandle((U32)-11),s,19,&w,0);}
static void check(int ok,const char*what){if(!ok){line("FAIL ");line(what);line("\n");hex("winError=",GetLastError());ExitProcess(1);}}
static U32 worker(void*arg){U64 id=(U64)arg; if(!TlsSetValue(tls,arg))++threadErrors;for(U32 j=0;j<100000;++j){_InterlockedIncrement64(&increments);if((U64)TlsGetValue(tls)!=id)++threadErrors;}return 0;}
void mainCRTStartup(void){
 line("PDAFT_P1_CPU_PROBE PE=x86_64 original-source-v1\n");hex("pid=",GetCurrentProcessId());
 volatile U64 seed=0x123456789abcdef0ULL; U64 state=seed;
 for(U32 i=0;i<100000;++i){state^=state<<13;state^=state>>7;state^=state<<17;}
 hex("integerChecksum=",state);check(state==0x092ab39ea1159461ULL,"integer checksum");
 volatile double a=1.5,b=2.25;double c=a*b+a;check(c==4.875,"SSE2 scalar double");
 tls=TlsAlloc();check(tls!=0xffffffffU,"TlsAlloc");HANDLE threads[4];
 for(U64 i=0;i<4;++i){threads[i]=CreateThread(0,0,worker,(void*)(i+1),0,0);check(threads[i]!=0,"CreateThread");}
 check(WaitForMultipleObjects(4,threads,1,15000)==0,"thread wait");
 check(increments==400000 && threadErrors==0,"atomic/thread-local isolation");
 for(int i=0;i<4;++i)CloseHandle(threads[i]);TlsFree(tls);
 I64 frequency,start,end;check(QueryPerformanceFrequency(&frequency)&&frequency>0,"QPC frequency");QueryPerformanceCounter(&start);Sleep(20);QueryPerformanceCounter(&end);check(end>start,"QPC monotonic");
 int caught=0;__try{RaiseException(0xE0425044U,0,0,0);}__except(1){caught=1;}check(caught,"SEH catch");
 unsigned char *code=VirtualAlloc(0,4096,0x3000,0x40);check(code!=0,"own JIT allocation");
 code[0]=0xB8;code[1]=7;code[2]=code[3]=code[4]=0;code[5]=0xC3;
 check(FlushInstructionCache(GetCurrentProcess(),code,6),"flush initial");
 int(*fn)(void)=(void*)code;check(fn()==7,"own generated x64 code");code[1]=11;
 check(FlushInstructionCache(GetCurrentProcess(),code,6),"flush modified");check(fn()==11,"x64 modified-code invalidation");VirtualFree(code,0,0x8000);
 line("PASS integer/fp64/threads/atomic/TLS/QPC/SEH/own-code-invalidation\n");
 line("OBSERVE_PROCESS_FOR_15_SECONDS\n");Sleep(15000);ExitProcess(0);
}
