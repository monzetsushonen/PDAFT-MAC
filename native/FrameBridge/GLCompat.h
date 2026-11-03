// AFT's legacy ARB buffer entry points can be NULL in Apple's core profile.
// GL 1.5 core buffer functions have the same signatures as ARB_vertex_buffer_object.
// Table addresses are verified against AFTMods/DivaGL/wrap.cpp for PDAFT 7.10.
static void install_game_gl_aliases(){
    static bool complete=false;if(complete)return;
    struct Pair{uint64_t alias,core;const char *name;};
    static const Pair pairs[]={
        {0x00000001411A4B10ULL,0x00000001411A41E0ULL,"glBindBuffer"},
        {0x00000001411A4B20ULL,0x00000001411A41F0ULL,"glGenBuffers"},
        {0x00000001411A4B18ULL,0x00000001411A41E8ULL,"glDeleteBuffers"},
        {0x00000001411A4B30ULL,0x00000001411A4200ULL,"glBufferData"},
        {0x00000001411A4B38ULL,0x00000001411A4208ULL,"glBufferSubData"},
        {0x00000001411A4B48ULL,0x00000001411A4218ULL,"glMapBuffer"},
        {0x00000001411A4B50ULL,0x00000001411A4220ULL,"glUnmapBuffer"},
        {0x00000001411A4B58ULL,0x00000001411A4228ULL,"glGetBufferParameteriv"},
        {0x00000001411A4B60ULL,0x00000001411A4230ULL,"glGetBufferPointerv"},
        {0x00000001411A4B40ULL,0x00000001411A4210ULL,"glGetBufferSubData"},
        {0x00000001411A4B28ULL,0x00000001411A41F8ULL,"glIsBuffer"},
    };
    unsigned available=0,filled=0;
    for(const Pair &p:pairs){
        uint64_t alias=0,core=0;mach_vm_size_t size=0;
        if(mach_vm_read_overwrite(mach_task_self(),p.core,8,(mach_vm_address_t)&core,&size)!=KERN_SUCCESS||size!=8||core<0x10000)continue;
        available++;
        if(mach_vm_read_overwrite(mach_task_self(),p.alias,8,(mach_vm_address_t)&alias,&size)!=KERN_SUCCESS||size!=8||alias)continue;
        if(mach_vm_protect(mach_task_self(),p.alias,8,FALSE,VM_PROT_READ|VM_PROT_WRITE)!=KERN_SUCCESS)continue;
        uint64_t expected=0;
        if(__atomic_compare_exchange_n((uint64_t*)p.alias,&expected,core,false,__ATOMIC_RELEASE,__ATOMIC_RELAXED)){
            filled++;if(logfile)fprintf(logfile,"GL compatibility: %sARB -> core\n",p.name);
        }
    }
    if(filled&&logfile)fflush(logfile);
    complete=available==sizeof(pairs)/sizeof(pairs[0]);
}
