// PD Loader becac44e TLAC SHA256:
// 9b713927e729e14d3f8265eb5fff1d6b91af71168723b4706b4ee381192f4707
// Guard the proven Update vtable entry. Keep ApplyCustomData each frame, but
// avoid repeatedly changing executable-page protection for identical patches.
namespace PlayerDataGuard {
using Method=void (__attribute__((ms_abi)) *)(void*);
static Method originalUpdate,applyCustomData;
struct Snapshot {
    uint64_t pvPatch;
    uint32_t pv,selector;
    uint8_t menu,card,workaround,refresh;
};
static Snapshot last={};
static void *lastObject;
static unsigned settle;
static uint64_t full,skipped;
static Snapshot snapshot(void *object){
    auto bytes=(uint8_t*)object;void *custom=*(void**)(bytes+0x18);
    return {*(volatile uint64_t*)0x1405CBBA3,
            *(volatile uint32_t*)0x1418054C4,*(volatile uint32_t*)0x1411A9790,
            uint8_t(*(volatile uint8_t*)0x14CC5E270==32),custom?*((uint8_t*)custom+0x7B):uint8_t(0),
            bytes[0x26],*(volatile uint8_t*)0x1405BC8E6};
}
static bool equal(const Snapshot&a,const Snapshot&b){
    return a.pvPatch==b.pvPatch && a.pv==b.pv && a.selector==b.selector && a.menu==b.menu
        && a.card==b.card && a.workaround==b.workaround && a.refresh==b.refresh;
}
static void __attribute__((ms_abi)) update(void *object){
    applyCustomData(object);
    Snapshot current=snapshot(object);
    if(object!=lastObject || !equal(current,last))settle=2;
    if(settle){--settle;originalUpdate(object);++full;}
    else ++skipped;
    last=snapshot(object);lastObject=object;
    if(logfile && (full+skipped)%600==0){fprintf(logfile,"PlayerData guard: pv=%u full=%llu redundant_patch_updates_skipped=%llu\n",last.pv,full,skipped);fflush(logfile);}
}
static void install(void *module){
    static bool installed=false;if(installed || !getenv("PDAFT_NATIVE_FRAME_PACING"))return;
    uint8_t *base=(uint8_t*)module;
    const uint8_t expected[]={0x40,0x55,0x53,0x48,0x8B,0xEC,0x48,0x83,0xEC,0x38,0x48,0x8B,0x05,0x8F,0x89,0x02,0x00};
    auto entry=(Method*)(base+0x2BC58);
    if(memcmp(base+0xF680,expected,sizeof(expected)) || *entry!=(Method)(base+0xF680))return;
    // The original Update's first call resolves exactly to ApplyCustomData.
    if(base[0xF69B]!=0xE8)return;int32_t relative;memcpy(&relative,base+0xF69C,4);
    if(base+0xF6A0+relative!=base+0x11BD0)return;
    originalUpdate=*entry;applyCustomData=(Method)(base+0x11BD0);
    mach_vm_address_t region=(mach_vm_address_t)entry;mach_vm_size_t size=0;
    vm_region_basic_info_data_64_t info={};mach_msg_type_number_t count=VM_REGION_BASIC_INFO_COUNT_64;mach_port_t object=MACH_PORT_NULL;
    if(mach_vm_region(mach_task_self(),&region,&size,VM_REGION_BASIC_INFO_64,(vm_region_info_t)&info,&count,&object)!=KERN_SUCCESS)return;
    if(object!=MACH_PORT_NULL)mach_port_deallocate(mach_task_self(),object);
    if(mach_vm_protect(mach_task_self(),(mach_vm_address_t)entry,8,FALSE,info.protection|VM_PROT_WRITE)!=KERN_SUCCESS)return;
    __atomic_store_n(entry,(Method)update,__ATOMIC_RELEASE);
    mach_vm_protect(mach_task_self(),(mach_vm_address_t)entry,8,FALSE,info.protection);
    installed=true;if(logfile){fprintf(logfile,"PlayerData redundant-code-patch guard installed (TLAC becac44e)\n");fflush(logfile);}
}
}
