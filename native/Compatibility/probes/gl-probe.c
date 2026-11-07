/* Standalone Windows x64 WGL/GL rendering probe, no PDAFT implementation. */
typedef unsigned int U32;typedef unsigned short U16;typedef unsigned char U8;typedef unsigned long long U64;typedef long long I64;typedef void *HANDLE;
int _fltused=0;
#define API __declspec(dllimport)
API HANDLE GetStdHandle(U32);API int WriteFile(HANDLE,const void*,U32,U32*,void*);API void ExitProcess(U32);API U32 GetLastError(void);API HANDLE LoadLibraryA(const char*);API void *GetProcAddress(HANDLE,const char*);API HANDLE GetModuleHandleA(const char*);API void Sleep(U32);API U32 GetCurrentProcessId(void);
static void line(const char*s){U32 n=0,w;while(s[n])++n;WriteFile(GetStdHandle((U32)-11),s,n,&w,0);}
static void check(int v,const char*s){if(!v){line("FAIL ");line(s);line("\n");ExitProcess(1);}}
static int contains(const char*s,const char*n){for(;*s;++s){const char*a=s,*b=n;while(*a&&*b&&*a==*b){++a;++b;}if(!*b)return 1;}return 0;}
static void *load(HANDLE m,const char*n){void*p=GetProcAddress(m,n);check(p!=0,n);return p;}
typedef I64(*WNDPROC)(HANDLE,U32,U64,I64);
struct WC{U32 size,style;WNDPROC proc;int clsExtra,wndExtra;HANDLE instance,icon,cursor,background;const char*menu,*name;HANDLE smallIcon;};
struct PFD{U16 size,version;U32 flags;U8 type,color,red,redShift,green,greenShift,blue,blueShift,alpha,alphaShift,accum,accumR,accumG,accumB,accumA,depth,stencil,aux,layer,reserved;U32 layerMask,visibleMask,damageMask;};
_Static_assert(sizeof(struct WC)==80,"WNDCLASSEXA layout");_Static_assert(sizeof(struct PFD)==40,"PFD layout");
typedef void*(*WGLPROC)(const char*);static WGLPROC wglproc;
static void *glproc(const char*n){void*p=wglproc(n);check(p && p!=(void*)1 && p!=(void*)2 && p!=(void*)3 && p!=(void*)-1,n);return p;}
void mainCRTStartup(void){
 line("PDAFT_P1_GL_PROBE PE=x86_64 original-source-v1\n");
 HANDLE user=LoadLibraryA("user32.dll"),gdi=LoadLibraryA("gdi32.dll"),gl=LoadLibraryA("opengl32.dll");check(user&&gdi&&gl,"Windows graphics libraries");
 U16(*registerClass)(const struct WC*)=load(user,"RegisterClassExA");
 HANDLE(*createWindow)(U32,const char*,const char*,U32,int,int,int,int,HANDLE,HANDLE,HANDLE,void*)=load(user,"CreateWindowExA");
 HANDLE(*getDC)(HANDLE)=load(user,"GetDC");int(*destroyWindow)(HANDLE)=load(user,"DestroyWindow");
 struct WC wc={0};wc.size=sizeof(wc);wc.style=0x20;wc.proc=load(user,"DefWindowProcA");wc.instance=GetModuleHandleA(0);wc.name="PDAFT-P1-hidden-GL-probe";
 check(registerClass(&wc)!=0,"RegisterClassExA");HANDLE window=createWindow(0,wc.name,wc.name,0x80000000U,0,0,64,64,0,0,wc.instance,0);check(window!=0,"hidden Win32 window");HANDLE dc=getDC(window);check(dc!=0,"GetDC");
 int(*choose)(HANDLE,const struct PFD*)=load(gdi,"ChoosePixelFormat");int(*set)(HANDLE,int,const struct PFD*)=load(gdi,"SetPixelFormat");
 struct PFD pfd={0};pfd.size=sizeof(pfd);pfd.version=1;pfd.flags=0x25;pfd.color=32;pfd.depth=24;pfd.stencil=8;
 int pf=choose(dc,&pfd);check(pf>0 && set(dc,pf,&pfd),"pixel format");
 HANDLE(*createContext)(HANDLE)=load(gl,"wglCreateContext");int(*makeCurrent)(HANDLE,HANDLE)=load(gl,"wglMakeCurrent");int(*deleteContext)(HANDLE)=load(gl,"wglDeleteContext");wglproc=load(gl,"wglGetProcAddress");
 HANDLE initial=createContext(dc);check(initial&&makeCurrent(dc,initial),"WGL base context");
 HANDLE(*createAttribs)(HANDLE,HANDLE,const int*)=glproc("wglCreateContextAttribsARB");int attributes[]={0x2091,4,0x2092,1,0x9126,1,0};
 HANDLE context=createAttribs(dc,0,attributes);check(context!=0,"WGL4.1 core context");check(makeCurrent(dc,context),"core current");deleteContext(initial);
 const char*(*getString)(U32)=load(gl,"glGetString");U32(*getError)(void)=load(gl,"glGetError");void(*viewport)(int,int,int,int)=load(gl,"glViewport");void(*clearColor)(float,float,float,float)=load(gl,"glClearColor");void(*clear)(U32)=load(gl,"glClear");void(*readPixels)(int,int,int,int,U32,U32,void*)=load(gl,"glReadPixels");void(*finish)(void)=load(gl,"glFinish");
 line("vendor=");line(getString(0x1F00));line("\nrenderer=");line(getString(0x1F01));line("\nversion=");line(getString(0x1F02));line("\n");
 U32(*shader)(U32)=glproc("glCreateShader");void(*source)(U32,int,const char*const*,const int*)=glproc("glShaderSource");void(*compile)(U32)=glproc("glCompileShader");void(*shaderiv)(U32,U32,int*)=glproc("glGetShaderiv");
 U32(*program)(void)=glproc("glCreateProgram");void(*attach)(U32,U32)=glproc("glAttachShader");void(*link)(U32)=glproc("glLinkProgram");void(*programiv)(U32,U32,int*)=glproc("glGetProgramiv");void(*use)(U32)=glproc("glUseProgram");void(*genVAO)(int,U32*)=glproc("glGenVertexArrays");void(*bindVAO)(U32)=glproc("glBindVertexArray");
 void(*drawArrays)(U32,int,int)=load(gl,"glDrawArrays");
 const char*vs="#version 330 core\nvoid main(){vec2 p[3]=vec2[3](vec2(-1,-1),vec2(3,-1),vec2(-1,3));gl_Position=vec4(p[gl_VertexID],0,1);}";
 const char*fs="#version 330 core\nout vec4 color;void main(){color=vec4(0.25,0.5,0.75,1.0);}";
 U32 v=shader(0x8B31),f=shader(0x8B30);source(v,1,&vs,0);source(f,1,&fs,0);compile(v);compile(f);int ok=0;shaderiv(v,0x8B81,&ok);check(ok,"vertex GLSL330");shaderiv(f,0x8B81,&ok);check(ok,"fragment GLSL330");U32 prog=program();attach(prog,v);attach(prog,f);link(prog);programiv(prog,0x8B82,&ok);check(ok,"GLSL link");use(prog);U32 vao;genVAO(1,&vao);bindVAO(vao);
 viewport(0,0,64,64);clearColor(1,0,0,1);clear(0x4000);drawArrays(4,0,3);finish();U8 pixel[4]={0};readPixels(32,32,1,1,0x1908,0x1401,pixel);check(getError()==0,"GL draw/read error");
 check(pixel[0]>=63&&pixel[0]<=65&&pixel[1]>=127&&pixel[1]<=129&&pixel[2]>=190&&pixel[2]<=192&&pixel[3]==255,"shader pixel expected64/128/191/255");
 check(contains(getString(0x1F01),"Metal")||contains(getString(0x1F00),"Metal"),"reported Metal candidate (module evidence also required)");
 line("PASS WGL4.1/GLSL330/full-triangle/readback64-128-191-255; hidden window; no FPS claim\nOBSERVE_PROCESS_FOR_15_SECONDS\n");Sleep(15000);makeCurrent(0,0);deleteContext(context);destroyWindow(window);ExitProcess(0);
}
