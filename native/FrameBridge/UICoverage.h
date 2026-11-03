// DivaGL v0.11 sprite pass emits an extra coverage target. Original color/blend
// attachment 0 remains untouched. Missing markers/shaders fail closed to SDR.
#include <unordered_set>
#import <OpenGL/gl3ext.h>
static IOSurfaceRef uiSurfaces[PDAFT_FRAME_SLOTS];
static GLuint uiTextures[PDAFT_FRAME_SLOTS],uiCaptureFBOs[PDAFT_FRAME_SLOTS];
static thread_local std::unordered_set<GLuint> uiLoggedPrograms;
static thread_local std::unordered_set<GLuint> uiShaders,uiPrograms,uiWarnedPrograms;
static thread_local int uiSlot=-1,uiDepth=0;
static thread_local GLuint uiFBO=0,uiBoundDrawFBO=0,uiCurrentProgram=0;
static thread_local GLint uiOldBuffers[2];
static thread_local bool uiAttached=false,uiValid=false,uiFinished=false;
static thread_local uint64_t uiFrames=0;

static void ui_begin(){
    if(!control || !control->hdr_mv_only || owner!=CGLGetCurrentContext() || uiSlot>=0)return;
    if(read_game_int(0x140EDA810)!=2 || read_game_int(0x140EDA82C)!=13)return;
    for(unsigned i=0;i<PDAFT_FRAME_SLOTS;i++){
        uint32_t expected=0;
        if(__atomic_compare_exchange_n(&control->slots[i].state,&expected,2,false,__ATOMIC_ACQUIRE,__ATOMIC_RELAXED)){
            uiSlot=i;control->ui_mask_sequence[i]=0;break;
        }
    }
    if(uiSlot<0)return;
    uiDepth=1;uiFBO=0;uiAttached=false;uiFinished=false;uiValid=true;
    GLint readFB,drawFB,readBuffer,drawBuffer;GLboolean colors[4];
    glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING,&readFB);glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING,&drawFB);
    glGetIntegerv(GL_READ_BUFFER,&readBuffer);glGetIntegerv(GL_DRAW_BUFFER,&drawBuffer);glGetBooleanv(GL_COLOR_WRITEMASK,colors);
    uiBoundDrawFBO=drawFB;GLint program=0;glGetIntegerv(GL_CURRENT_PROGRAM,&program);uiCurrentProgram=program;
    GLboolean scissor=glIsEnabled(GL_SCISSOR_TEST),srgb=glIsEnabled(GL_FRAMEBUFFER_SRGB);
    glDisable(GL_SCISSOR_TEST);glDisable(GL_FRAMEBUFFER_SRGB);
    glBindFramebuffer(GL_READ_FRAMEBUFFER,0);glReadBuffer(GL_BACK);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER,uiCaptureFBOs[uiSlot]);glDrawBuffer(GL_COLOR_ATTACHMENT0);
    glBlitFramebuffer(0,0,control->width,control->height,0,0,control->width,control->height,GL_COLOR_BUFFER_BIT,GL_NEAREST);
    glColorMask(GL_FALSE,GL_FALSE,GL_FALSE,GL_TRUE);const GLfloat zero[4]={0,0,0,0};glClearBufferfv(GL_COLOR,0,zero);
    glColorMask(colors[0],colors[1],colors[2],colors[3]);
    glBindFramebuffer(GL_READ_FRAMEBUFFER,readFB);glReadBuffer(readBuffer);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER,drawFB);glDrawBuffer(drawBuffer);
    if(scissor)glEnable(GL_SCISSOR_TEST);if(srgb)glEnable(GL_FRAMEBUFFER_SRGB);
}
static void ui_attach(GLenum target,GLuint framebuffer){
    if(!uiDepth || uiSlot<0 || uiAttached || !framebuffer || (target!=GL_DRAW_FRAMEBUFFER && target!=GL_FRAMEBUFFER))return;
    GLint existing=0;glGetFramebufferAttachmentParameteriv(GL_DRAW_FRAMEBUFFER,GL_COLOR_ATTACHMENT1,GL_FRAMEBUFFER_ATTACHMENT_OBJECT_NAME,&existing);
    if(existing || !uiTextures[uiSlot]){uiValid=false;return;}
    glGetIntegerv(GL_DRAW_BUFFER0,uiOldBuffers);glGetIntegerv(GL_DRAW_BUFFER1,uiOldBuffers+1);
    glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER,GL_COLOR_ATTACHMENT1,GL_TEXTURE_RECTANGLE,uiTextures[uiSlot],0);
    GLenum buffers[]={GL_COLOR_ATTACHMENT0,GL_COLOR_ATTACHMENT1};glDrawBuffers(2,buffers);
    if(glCheckFramebufferStatus(GL_DRAW_FRAMEBUFFER)!=GL_FRAMEBUFFER_COMPLETE){
        glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER,GL_COLOR_ATTACHMENT1,GL_TEXTURE_RECTANGLE,0,0);
        GLenum restore[]={(GLenum)uiOldBuffers[0],(GLenum)uiOldBuffers[1]};glDrawBuffers(2,restore);uiValid=false;return;
    }
    uiFBO=framebuffer;uiAttached=true;
    glColorMaski(1,GL_TRUE,GL_TRUE,GL_TRUE,GL_TRUE);
    glEnablei(GL_BLEND,1);glBlendEquationSeparatei(1,GL_FUNC_ADD,GL_FUNC_ADD);
    // Keep the pre-UI scene RGB and accumulate UI alpha as source-over.
    glBlendFuncSeparatei(1,GL_ZERO,GL_ONE,GL_ONE,GL_ONE_MINUS_SRC_ALPHA);
}
static bool ui_target(){
    if(!uiDepth || !uiAttached)return false;
    return uiBoundDrawFBO==uiFBO;
}
static void ui_check_program(){
    if(!uiValid || !ui_target())return;
    GLuint program=uiCurrentProgram;
    if(logfile && uiLoggedPrograms.insert(program).second){
        GLsizei n=0;GLuint shaders[8];glGetAttachedShaders(program,8,&n,shaders);
        for(GLsizei i=0;i<n;i++){GLint type=0;glGetShaderiv(shaders[i],GL_SHADER_TYPE,&type);if(type==GL_FRAGMENT_SHADER){char source[128]={0};glGetShaderSource(shaders[i],127,nullptr,source);fprintf(logfile,"UI program %d source: %.127s\n",program,source);}}
        fflush(logfile);
    }
    if(!uiPrograms.count(program) && glGetFragDataLocation(program,"pdaft_ui_coverage")==1)uiPrograms.insert(program);
    if(!uiPrograms.count(program)){
        uiValid=false;
        if(logfile && uiWarnedPrograms.insert(program).second){
            GLsizei n=0;GLuint shaders[8];glGetAttachedShaders(program,8,&n,shaders);
            fprintf(logfile,"UI coverage: unrecognized sprite program=%d; frame remains SDR\n",program);
            for(GLsizei i=0;i<n;i++){GLint type=0;glGetShaderiv(shaders[i],GL_SHADER_TYPE,&type);if(type==GL_FRAGMENT_SHADER){char source[160]={0};glGetShaderSource(shaders[i],159,nullptr,source);fprintf(logfile,"UI coverage shader header: %.159s\n",source);}}
            fflush(logfile);
        }
    }
}
static void ui_end(){
    if(uiSlot<0)return;
    if(uiAttached){
        GLint previous=0;glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING,&previous);
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER,uiFBO);
        glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER,GL_COLOR_ATTACHMENT1,GL_TEXTURE_RECTANGLE,0,0);
        GLenum restore[]={(GLenum)uiOldBuffers[0],(GLenum)uiOldBuffers[1]};glDrawBuffers(2,restore);
        glDisablei(GL_BLEND,1);glBlendEquationSeparatei(1,GL_FUNC_ADD,GL_FUNC_ADD);
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER,previous);
    }
    uiFinished=uiAttached&&uiValid;
    if(logfile && (++uiFrames==1 || uiFrames%600==0)){fprintf(logfile,"UI coverage frame=%llu valid=%d sprite_programs=%zu\n",uiFrames,uiFinished,uiPrograms.size());fflush(logfile);}
}
static void bridge_push_marker(GLsizei length,const char *marker){
    RenderProfiler::push(length,marker);
    if(uiDepth)uiDepth++;
    else if(marker && ((length==11 && !memcmp(marker,"pass_sprite",11)) || (length<0 && !strcmp(marker,"pass_sprite"))))ui_begin();
    glPushGroupMarkerEXT(length,marker);
}
static void bridge_pop_marker(){if(uiDepth && --uiDepth==0)ui_end();glPopGroupMarkerEXT();RenderProfiler::pop();}
static void bridge_bind_framebuffer(GLenum target,GLuint framebuffer){glBindFramebuffer(target,framebuffer);if(target==GL_DRAW_FRAMEBUFFER||target==GL_FRAMEBUFFER)uiBoundDrawFBO=framebuffer;ui_attach(target,framebuffer);}
static void bridge_use_program(GLuint program){glUseProgram(program);uiCurrentProgram=program;}
static void bridge_disable(GLenum cap){glDisable(cap);if(cap==GL_BLEND && ui_target())glEnablei(GL_BLEND,1);}
static void bridge_blend_equation(GLenum rgb,GLenum alpha){glBlendEquationSeparate(rgb,alpha);if(ui_target())glBlendEquationSeparatei(1,GL_FUNC_ADD,GL_FUNC_ADD);}
static void bridge_blend_function(GLenum sr,GLenum dr,GLenum sa,GLenum da){glBlendFuncSeparate(sr,dr,sa,da);if(ui_target())glBlendFuncSeparatei(1,GL_ZERO,GL_ONE,GL_ONE,GL_ONE_MINUS_SRC_ALPHA);}
static void bridge_link_program(GLuint program){
    RenderProfiler::CallScope timing("GL:glLinkProgram");glLinkProgram(program);GLsizei n=0;GLuint shaders[8];glGetAttachedShaders(program,8,&n,shaders);
    for(GLsizei i=0;i<n;i++)if(uiShaders.count(shaders[i])){uiPrograms.insert(program);return;}
    uiPrograms.erase(program);
}
static void bridge_delete_shader(GLuint shader){uiShaders.erase(shader);glDeleteShader(shader);}
static void bridge_delete_program(GLuint program){uiPrograms.erase(program);glDeleteProgram(program);}
static void bridge_draw_arrays(GLenum mode,GLint first,GLsizei count){ui_check_program();RenderProfiler::CallScope timing("GL:glDrawArrays");glDrawArrays(mode,first,count);}
static void bridge_draw_range(GLenum mode,GLuint start,GLuint end,GLsizei count,GLenum type,const void *indices){ui_check_program();RenderProfiler::CallScope timing("GL:glDrawRangeElements");glDrawRangeElements(mode,start,end,count,type,indices);}
static void bridge_buffer_data(GLenum target,GLsizeiptr size,const void *data,GLenum usage){RenderProfiler::CallScope timing("GL:glBufferData");glBufferData(target,size,data,usage);}
static void bridge_buffer_subdata(GLenum target,GLintptr offset,GLsizeiptr size,const void *data){RenderProfiler::CallScope timing("GL:glBufferSubData");glBufferSubData(target,offset,size,data);}
static void *bridge_map_buffer_range(GLenum target,GLintptr offset,GLsizeiptr length,GLbitfield access){RenderProfiler::CallScope timing("GL:glMapBufferRange");return glMapBufferRange(target,offset,length,access);}
static GLboolean bridge_unmap_buffer(GLenum target){RenderProfiler::CallScope timing("GL:glUnmapBuffer");return glUnmapBuffer(target);}
static void bridge_draw_elements(GLenum mode,GLsizei count,GLenum type,const void *indices){ui_check_program();RenderProfiler::CallScope timing("GL:glDrawElements");glDrawElements(mode,count,type,indices);}
static void bridge_compile_shader(GLuint shader){RenderProfiler::CallScope timing("GL:glCompileShader");glCompileShader(shader);}
#define UI_INTERPOSE(replacement,original) \
    __attribute__((used)) static struct {const void *r,*o;} ui_interpose_##replacement \
    __attribute__((section("__DATA,__interpose")))={(const void*)replacement,(const void*)original}
UI_INTERPOSE(bridge_push_marker,glPushGroupMarkerEXT);
UI_INTERPOSE(bridge_pop_marker,glPopGroupMarkerEXT);
UI_INTERPOSE(bridge_bind_framebuffer,glBindFramebuffer);
UI_INTERPOSE(bridge_use_program,glUseProgram);
UI_INTERPOSE(bridge_disable,glDisable);
UI_INTERPOSE(bridge_blend_equation,glBlendEquationSeparate);
UI_INTERPOSE(bridge_blend_function,glBlendFuncSeparate);
UI_INTERPOSE(bridge_link_program,glLinkProgram);
UI_INTERPOSE(bridge_delete_shader,glDeleteShader);
UI_INTERPOSE(bridge_delete_program,glDeleteProgram);
UI_INTERPOSE(bridge_draw_arrays,glDrawArrays);
UI_INTERPOSE(bridge_draw_range,glDrawRangeElements);
UI_INTERPOSE(bridge_buffer_data,glBufferData);
UI_INTERPOSE(bridge_buffer_subdata,glBufferSubData);
UI_INTERPOSE(bridge_map_buffer_range,glMapBufferRange);
UI_INTERPOSE(bridge_unmap_buffer,glUnmapBuffer);
UI_INTERPOSE(bridge_draw_elements,glDrawElements);
UI_INTERPOSE(bridge_compile_shader,glCompileShader);
#undef UI_INTERPOSE
