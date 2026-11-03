#import "PDAFTSpatialUpscaler.h"
#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <vector>

static void require(bool ok, NSString *why) {
    if (!ok) throw std::runtime_error(why.UTF8String ?: "MetalFX test failed");
}

int main() {
    @autoreleasepool { try {
        id<MTLDevice> device = MTLCreateSystemDefaultDevice();
        require(device != nil, @"No Metal GPU available.");
        id<MTLCommandQueue> queue = [device newCommandQueue];
        require(queue != nil, @"No command queue.");
        NSMutableArray *cases = [NSMutableArray new];
        const uint8_t colors[4][4] = {{32,64,96,255}, {180,64,24,255},
                                      {48,160,80,255}, {200,200,200,255}};
        constexpr NSUInteger iw = 1280, ih = 720;
        std::vector<uint8_t> pattern(iw * ih * 4);
        for (NSUInteger y = 0; y < ih; y++) for (NSUInteger x = 0; x < iw; x++)
            std::copy(colors[(y >= ih/2)*2 + (x >= iw/2)],
                      colors[(y >= ih/2)*2 + (x >= iw/2)] + 4, &pattern[(y*iw+x)*4]);
        for (NSUInteger ow : {1920u, 2560u, 3840u}) { @autoreleasepool {
            NSUInteger oh = ow * 9 / 16;
            NSError *error = nil;
            PDAFTSpatialUpscaler *scaler = [[PDAFTSpatialUpscaler alloc] initWithDevice:device
                inputWidth:iw inputHeight:ih outputWidth:ow outputHeight:oh
                pixelFormat:MTLPixelFormatRGBA8Unorm colorMode:MTLFXSpatialScalerColorProcessingModePerceptual error:&error];
            require(scaler != nil, error.localizedDescription);
            MTLTextureDescriptor *d = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatRGBA8Unorm
                width:iw height:ih mipmapped:NO];
            d.storageMode = MTLStorageModeShared; d.usage = scaler.inputUsage;
            id<MTLTexture> input = [device newTextureWithDescriptor:d];
            d.width = ow; d.height = oh; d.storageMode = MTLStorageModePrivate;
            d.usage = scaler.outputUsage | MTLTextureUsageShaderRead;
            id<MTLTexture> output = [device newTextureWithDescriptor:d];
            require(input != nil && output != nil, @"Texture allocation failed.");
            input.label = @"Test pattern: full composed frame";
            output.label = @"MetalFX full-frame output";
            [input replaceRegion:MTLRegionMake2D(0,0,iw,ih) mipmapLevel:0 withBytes:pattern.data() bytesPerRow:iw*4];
            std::vector<double> times;
            for (unsigned frame = 0; frame < 23; frame++) { @autoreleasepool {
                id<MTLCommandBuffer> command = [queue commandBuffer];
                command.label = @"MetalFX subsystem test (not game FPS)";
                require([scaler encodeInput:input output:output commandBuffer:command error:&error], error.localizedDescription);
                [command commit]; [command waitUntilCompleted];
                require(command.status == MTLCommandBufferStatusCompleted, command.error.localizedDescription);
                if (frame >= 3) times.push_back((command.GPUEndTime - command.GPUStartTime) * 1000.0);
            }}
            NSUInteger row = (ow * 4 + 255) & ~NSUInteger(255);
            id<MTLBuffer> readback = [device newBufferWithLength:row * oh options:MTLResourceStorageModeShared];
            require(readback != nil, @"Readback allocation failed.");
            id<MTLCommandBuffer> copy = [queue commandBuffer];
            id<MTLBlitCommandEncoder> blit = [copy blitCommandEncoder];
            [blit copyFromTexture:output sourceSlice:0 sourceLevel:0 sourceOrigin:MTLOriginMake(0,0,0)
                sourceSize:MTLSizeMake(ow,oh,1) toBuffer:readback destinationOffset:0
                destinationBytesPerRow:row destinationBytesPerImage:row*oh];
            [blit endEncoding]; [copy commit]; [copy waitUntilCompleted];
            require(copy.status == MTLCommandBufferStatusCompleted, copy.error.localizedDescription);
            unsigned maxError = 0;
            for (NSUInteger q = 0; q < 4; q++) {
                NSUInteger x = ow * ((q % 2) * 2 + 1) / 4, y = oh * ((q / 2) * 2 + 1) / 4;
                const uint8_t *p = (const uint8_t *)readback.contents + y*row + x*4;
                for (unsigned c = 0; c < 4; c++) maxError = std::max(maxError, (unsigned)std::abs(int(p[c])-int(colors[q][c])));
            }
            require(maxError <= 3, @"MetalFX color/orientation check failed.");
            // A wrong-size frame must fail before any GPU encoding occurs.
            id<MTLCommandBuffer> invalid = [queue commandBuffer];
            require(![scaler encodeInput:output output:output commandBuffer:invalid error:&error], @"Mismatched input was accepted.");
            std::sort(times.begin(), times.end());
            [cases addObject:@{@"input":@[@(iw),@(ih)], @"output":@[@(ow),@(oh)],
                @"scale":@((double)ow/iw), @"gpu_ms_p50":@(times[times.size()/2]),
                @"gpu_ms_p95":@(times[(times.size()-1)*95/100]), @"color_max_error":@(maxError),
                @"wrong_size_rejected":@YES, @"passed":@YES}];
        }}
        NSDictionary *report = @{@"device":device.name, @"architecture":@"arm64",
            @"implementation":@"Apple MetalFX spatial scaler; full-frame color texture",
            @"scope":@"Subsystem GPU tests using generated color quadrants. No game renderer or presentation is connected.",
            @"game_integrated":@NO, @"hdr_verified":@NO, @"cases":cases};
        NSData *json = [NSJSONSerialization dataWithJSONObject:report options:NSJSONWritingPrettyPrinted error:nil];
        fwrite(json.bytes, 1, json.length, stdout); fputc('\n',stdout);
        return 0;
    } catch (const std::exception &e) { fprintf(stderr, "%s\n", e.what()); return 1; }}
}
