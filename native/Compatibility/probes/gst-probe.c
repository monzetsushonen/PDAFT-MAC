#include <stdio.h>
#include <dlfcn.h>
#include <stdlib.h>
int main(int argc,char**argv){
 if(argc!=2)return 2;
 char file[4096];snprintf(file,sizeof(file),"%s/libgstreamer-1.0.0.dylib",argv[1]);
 void*lib=dlopen(file,RTLD_NOW|RTLD_GLOBAL);if(!lib){fprintf(stderr,"%s\n",dlerror());return 2;}
 int(*init)(int*,char***,void**)=dlsym(lib,"gst_init_check");void*(*factory)(const char*)=dlsym(lib,"gst_element_factory_find");void(*unref)(void*)=dlsym(lib,"gst_object_unref");
 if(!init||!factory||!unref||!init(0,0,0))return 2;
 const char*names[]={"asfdemux","avdemux_asf","avdemux_asf_o","avdec_wmv3","avdec_vc1","decodebin","typefind","fakesink"};
 for(unsigned i=0;i<sizeof(names)/sizeof(names[0]);++i){void*f=factory(names[i]);printf("factory %-16s %s\n",names[i],f?"present":"ABSENT");if(f)unref(f);}
 return 0;
}
