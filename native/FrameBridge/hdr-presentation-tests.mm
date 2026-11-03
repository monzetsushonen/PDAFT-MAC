#import <Foundation/Foundation.h>
#import <Metal/Metal.h>
#include "HDRPresentation.h"
#include <cassert>
#include <initializer_list>
int main(){@autoreleasepool{
    id<MTLDevice> device=MTLCreateSystemDefaultDevice();if(!device){fprintf(stderr,"No GPU\n");return 2;}
    NSError *error=nil;PDAFTHDRPresentation *hdr=[[PDAFTHDRPresentation alloc]initWithDevice:device error:&error];
    if(!hdr){fprintf(stderr,"%s\n",error.localizedDescription.UTF8String);return 3;}
    MTLTextureDescriptor *desc=[MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatRGBA8Unorm width:4 height:1 mipmapped:NO];
    desc.storageMode=MTLStorageModeShared;desc.usage=MTLTextureUsageShaderRead;
    id<MTLTexture> input=[device newTextureWithDescriptor:desc];
    uint8_t pixels[16]={0,0,0,255,128,128,128,255,255,255,255,255,255,0,0,255};
    [input replaceRegion:MTLRegionMake2D(0,0,4,1) mipmapLevel:0 withBytes:pixels bytesPerRow:16];
    desc.pixelFormat=MTLPixelFormatRGBA16Float;desc.usage=MTLTextureUsageRenderTarget|MTLTextureUsageShaderRead;
    id<MTLTexture> output=[device newTextureWithDescriptor:desc];id<MTLCommandQueue> queue=[device newCommandQueue];
    MTLTextureDescriptor *maskDesc=[MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatRGBA8Unorm width:4 height:1 mipmapped:NO];
    maskDesc.storageMode=MTLStorageModeShared;maskDesc.usage=MTLTextureUsageShaderRead;
    id<MTLTexture> mask=[device newTextureWithDescriptor:maskDesc];uint8_t clear[16];memcpy(clear,pixels,16);for(int i=0;i<4;i++)clear[4*i+3]=0;
    [mask replaceRegion:MTLRegionMake2D(0,0,4,1) mipmapLevel:0 withBytes:clear bytesPerRow:16];
    const float cases[][3]={{2,.7,1.7},{1,.7,1},{16,1,4},{2,0,1},{0,1,1}};
    for(auto &item:cases){
        id<MTLCommandBuffer> command=[queue commandBuffer];
        id<MTLBuffer> stats=[hdr encodeInput:input drawable:output commandBuffer:command headroom:item[0] strength:item[1] measure:YES uiMask:mask];
        [command commit];[command waitUntilCompleted];
        if(command.status==MTLCommandBufferStatusError){fprintf(stderr,"%s\n",command.error.localizedDescription.UTF8String);return 4;}
        _Float16 data[16];[output getBytes:data bytesPerRow:32 fromRegion:MTLRegionMake2D(0,0,4,1) mipmapLevel:0];
        float peak=0;memcpy(&peak,stats.contents,4);
        assert(fabs((float)data[0])<.0001);
        assert(fabs((float)data[4]-.21586)<.002);
        assert(fabs((float)data[8]-item[2])<.003);
        assert(fabs((float)data[12]-item[2])<.003);
        assert(fabs((float)data[13])<.0001);
        assert(fabs(peak-item[2])<.003);
        printf("headroom=%.1f strength=%.1f black=%.4f mid=%.4f white=%.4f peak=%.4f above=%u PASS\n",item[0],item[1],(float)data[0],(float)data[4],(float)data[8],peak,((uint32_t*)stats.contents)[1]);
    }
    uint8_t covered[16];memcpy(covered,pixels,16);[mask replaceRegion:MTLRegionMake2D(0,0,4,1) mipmapLevel:0 withBytes:covered bytesPerRow:16];
    for(int missing=0;missing<2;missing++){
        id<MTLCommandBuffer> command=[queue commandBuffer];
        id<MTLBuffer> stats=[hdr encodeInput:input drawable:output commandBuffer:command headroom:4 strength:1 measure:YES uiMask:missing?nil:mask];
        [command commit];[command waitUntilCompleted];assert(command.status==MTLCommandBufferStatusCompleted);
        float peak=0;memcpy(&peak,stats.contents,4);assert(fabs(peak-1)<.001);
        assert(((uint32_t*)stats.contents)[1]==0);assert(((uint32_t*)stats.contents)[3]==2304);
        printf("UI protected / missing-mask=%d peak=%.4f PASS\n",missing,peak);
    }
    // A moving translucent overlay must vary continuously, not toggle HDR on/off.
    float previous=5;
    for(int alpha:{0,64,128,192,255}){
        for(int i=0;i<4;i++)covered[4*i+3]=alpha;
        [mask replaceRegion:MTLRegionMake2D(0,0,4,1) mipmapLevel:0 withBytes:covered bytesPerRow:16];
        id<MTLCommandBuffer> command=[queue commandBuffer];
        [hdr encodeInput:input drawable:output commandBuffer:command headroom:4 strength:1 measure:NO uiMask:mask];
        [command commit];[command waitUntilCompleted];assert(command.status==MTLCommandBufferStatusCompleted);
        _Float16 data[16];[output getBytes:data bytesPerRow:32 fromRegion:MTLRegionMake2D(0,0,4,1) mipmapLevel:0];
        float white=(float)data[8],expected=1+3*(1-alpha/255.f);
        assert(fabs(white-expected)<.004);assert(white<=previous);previous=white;
        printf("UI alpha=%d scene peak=%.4f continuous composition PASS\n",alpha,white);
    }

}}
