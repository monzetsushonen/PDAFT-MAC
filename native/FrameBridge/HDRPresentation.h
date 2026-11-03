// SDR source -> extended linear sRGB. This does not recover clipped scene radiance.
#import <QuartzCore/CAMetalLayer.h>
#include <cmath>

static const char *PDAFTHDRSource=R"MSL(
#include <metal_stdlib>
using namespace metal;
struct Vertex {float4 position [[position]];};
vertex Vertex hdrVertex(uint id [[vertex_id]]) {
    float2 p=float2((id<<1)&2,id&2);
    return {float4(p*2.0-1.0,0,1)};
}
float3 decodeSRGB(float3 s) {
    return select(s/12.92,pow((s+0.055)/1.055,float3(2.4)),s>0.04045);
}
float4 sceneAndUI(texture2d<float> mask,float2 position,uint2 outputSize) {
    constexpr sampler linearSampler(coord::normalized,address::clamp_to_edge,filter::linear);
    float2 uv=position/float2(outputSize);uv.y=1.0-uv.y;
    return mask.sample(linearSampler,uv);
}
fragment float4 hdrFragment(Vertex v [[stage_in]],texture2d<float> image [[texture(0)]],
                            constant float2 &parameters [[buffer(0)]],texture2d<float> mask [[texture(1)]]) {
    float3 rgb=decodeSRGB(clamp(image.read(uint2(v.position.xy)).rgb,0.0,1.0));
    float4 layers=sceneAndUI(mask,v.position.xy,uint2(image.get_width(),image.get_height()));
    float3 scene=decodeSRGB(layers.rgb);
    float bright=max(scene.r,max(scene.g,scene.b));
    float shoulder=smoothstep(0.55,1.0,bright);
    float gain=(parameters.x-1.0)*parameters.y*shoulder*shoulder;
    float transmittance=1.0-clamp(layers.a,0.0,1.0);
    // Extend only the underlying scene radiance; retain the original SDR UI color.
    return float4(rgb+scene*gain*transmittance,1);
}
kernel void hdrStatistics(texture2d<float,access::read> image [[texture(0)]],
                          device atomic_uint *statistics [[buffer(0)]],texture2d<float> mask [[texture(1)]],uint2 p [[thread_position_in_grid]]) {
    if(p.x>=64 || p.y>=36)return;
    uint2 xy=min(uint2((float2(p)+0.5)*float2(image.get_width(),image.get_height())/float2(64,36)),
                 uint2(image.get_width()-1,image.get_height()-1));
    float3 rgb=image.read(xy).rgb;
    float peak=max(rgb.r,max(rgb.g,rgb.b));
    atomic_fetch_max_explicit(statistics,as_type<uint>(peak),memory_order_relaxed);
    if(sceneAndUI(mask,float2(xy)+0.5,uint2(image.get_width(),image.get_height())).a>0.999){
        atomic_fetch_max_explicit(statistics+2,as_type<uint>(peak),memory_order_relaxed);
        atomic_fetch_add_explicit(statistics+3,1u,memory_order_relaxed);
    }
    if(peak>1.001)atomic_fetch_add_explicit(statistics+1,1u,memory_order_relaxed);
}
)MSL";

@interface PDAFTHDRPresentation : NSObject
- (instancetype)initWithDevice:(id<MTLDevice>)device error:(NSError**)error;
- (id<MTLBuffer>)encodeInput:(id<MTLTexture>)input drawable:(id<MTLTexture>)drawable
             commandBuffer:(id<MTLCommandBuffer>)command headroom:(float)headroom
                  strength:(float)strength measure:(BOOL)measure uiMask:(id<MTLTexture>)mask;
@end
@implementation PDAFTHDRPresentation {
    id<MTLRenderPipelineState> _render;
    id<MTLComputePipelineState> _statistics;
    id<MTLDevice> _device;
    id<MTLTexture> _fallbackMask;
}
- (instancetype)initWithDevice:(id<MTLDevice>)device error:(NSError**)error {
    if(!(self=[super init]))return nil;
    _device=device;
    MTLTextureDescriptor *fallback=[MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatRGBA8Unorm width:1 height:1 mipmapped:NO];
    fallback.storageMode=MTLStorageModeShared;fallback.usage=MTLTextureUsageShaderRead;
    _fallbackMask=[device newTextureWithDescriptor:fallback];const uint8_t white[4]={255,255,255,255};
    [_fallbackMask replaceRegion:MTLRegionMake2D(0,0,1,1) mipmapLevel:0 withBytes:white bytesPerRow:4];
    id<MTLLibrary> library=[device newLibraryWithSource:@(PDAFTHDRSource) options:nil error:error];
    if(!library)return nil;
    MTLRenderPipelineDescriptor *d=[MTLRenderPipelineDescriptor new];
    d.label=@"PDAFT SDR highlight expansion to EDR";
    d.vertexFunction=[library newFunctionWithName:@"hdrVertex"];
    d.fragmentFunction=[library newFunctionWithName:@"hdrFragment"];
    d.colorAttachments[0].pixelFormat=MTLPixelFormatRGBA16Float;
    _render=[device newRenderPipelineStateWithDescriptor:d error:error];
    if(!_render)return nil;
    _statistics=[device newComputePipelineStateWithFunction:[library newFunctionWithName:@"hdrStatistics"] error:error];
    return _statistics?self:nil;
}
- (id<MTLBuffer>)encodeInput:(id<MTLTexture>)input drawable:(id<MTLTexture>)drawable
             commandBuffer:(id<MTLCommandBuffer>)command headroom:(float)headroom
                  strength:(float)strength measure:(BOOL)measure uiMask:(id<MTLTexture>)mask {
    MTLRenderPassDescriptor *pass=[MTLRenderPassDescriptor renderPassDescriptor];
    pass.colorAttachments[0].texture=drawable;
    pass.colorAttachments[0].loadAction=MTLLoadActionDontCare;
    pass.colorAttachments[0].storeAction=MTLStoreActionStore;
    id<MTLRenderCommandEncoder> encoder=[command renderCommandEncoderWithDescriptor:pass];
    float parameters[2]={std::fmax(1.f,std::fmin(4.f,headroom)),std::fmax(0.f,std::fmin(1.f,strength))};
    [encoder setRenderPipelineState:_render];[encoder setFragmentTexture:input atIndex:0];[encoder setFragmentTexture:mask?:_fallbackMask atIndex:1];
    [encoder setFragmentBytes:parameters length:sizeof(parameters) atIndex:0];
    [encoder drawPrimitives:MTLPrimitiveTypeTriangle vertexStart:0 vertexCount:3];[encoder endEncoding];
    if(!measure)return nil;
    id<MTLBuffer> stats=[_device newBufferWithLength:16 options:MTLResourceStorageModeShared];
    memset(stats.contents,0,16);
    id<MTLComputeCommandEncoder> compute=[command computeCommandEncoder];
    [compute setComputePipelineState:_statistics];[compute setTexture:drawable atIndex:0];[compute setTexture:mask?:_fallbackMask atIndex:1];
    [compute setBuffer:stats offset:0 atIndex:0];
    [compute dispatchThreads:MTLSizeMake(64,36,1) threadsPerThreadgroup:MTLSizeMake(8,8,1)];[compute endEncoding];
    return stats;
}
@end
