#import "PDAFTSpatialUpscaler.h"

static BOOL fail(NSError **error, NSString *message) {
    if (error) *error = [NSError errorWithDomain:@"PDAFT.MetalFX" code:1
        userInfo:@{NSLocalizedDescriptionKey: message}];
    return NO;
}

@implementation PDAFTSpatialUpscaler {
    id<MTLDevice> _device;
    id<MTLFXSpatialScaler> _scaler;
}

- (nullable instancetype)initWithDevice:(id<MTLDevice>)device
                            inputWidth:(NSUInteger)iw inputHeight:(NSUInteger)ih
                           outputWidth:(NSUInteger)ow outputHeight:(NSUInteger)oh
                           pixelFormat:(MTLPixelFormat)format
                             colorMode:(MTLFXSpatialScalerColorProcessingMode)mode
                                 error:(NSError **)error {
    self = [super init];
    if (!self) return nil;
    if (![MTLFXSpatialScalerDescriptor supportsDevice:device]) {
        fail(error, @"This GPU does not support MetalFX spatial upscaling."); return nil;
    }
    if (!iw || !ih || ow < iw || oh < ih || (ow == iw && oh == ih)) {
        fail(error, @"An upscale requires nonzero input and a larger output."); return nil;
    }
    if (iw * (uint64_t)oh != ih * (uint64_t)ow) {
        fail(error, @"Preserve the game frame aspect ratio; letterbox at presentation."); return nil;
    }
    if (format != MTLPixelFormatRGBA8Unorm && format != MTLPixelFormatBGRA8Unorm
        && format != MTLPixelFormatRGBA16Float) {
        fail(error, @"Unsupported frame format for the current MetalFX integration."); return nil;
    }
    if (mode == MTLFXSpatialScalerColorProcessingModeHDR && format != MTLPixelFormatRGBA16Float) {
        fail(error, @"HDR requires the floating-point frame path."); return nil;
    }
    MTLFXSpatialScalerDescriptor *desc = [MTLFXSpatialScalerDescriptor new];
    desc.inputWidth = iw; desc.inputHeight = ih;
    desc.outputWidth = ow; desc.outputHeight = oh;
    desc.colorTextureFormat = desc.outputTextureFormat = format;
    desc.colorProcessingMode = mode;
    _scaler = [desc newSpatialScalerWithDevice:device];
    if (!_scaler) { fail(error, @"MetalFX rejected this resolution/format combination."); return nil; }
    _device = device;
    return self;
}

- (MTLTextureUsage)inputUsage { return _scaler.colorTextureUsage; }
- (MTLTextureUsage)outputUsage { return _scaler.outputTextureUsage; }

- (BOOL)encodeInput:(id<MTLTexture>)input output:(id<MTLTexture>)output
      commandBuffer:(id<MTLCommandBuffer>)command error:(NSError **)error {
    if (input.device != _device || output.device != _device || command.device != _device)
        return fail(error, @"Input, output and command buffer must use the scaler's Metal device.");
    if (input.width != _scaler.inputWidth || input.height != _scaler.inputHeight
        || output.width != _scaler.outputWidth || output.height != _scaler.outputHeight)
        return fail(error, @"Texture dimensions differ from the prepared MetalFX profile.");
    if (input.pixelFormat != _scaler.colorTextureFormat || output.pixelFormat != _scaler.outputTextureFormat)
        return fail(error, @"Frame format differs from the prepared MetalFX profile.");
    if (input.textureType != MTLTextureType2D || output.textureType != MTLTextureType2D
        || input.sampleCount != 1 || output.sampleCount != 1)
        return fail(error, @"Resolve the complete frame to single-sample 2D textures first.");
    if (output.storageMode != MTLStorageModePrivate)
        return fail(error, @"MetalFX output must use private GPU storage.");
    if ((input.usage & self.inputUsage) != self.inputUsage
        || (output.usage & self.outputUsage) != self.outputUsage)
        return fail(error, @"Frame textures lack MetalFX's required usage flags.");
    _scaler.colorTexture = input;
    _scaler.outputTexture = output;
    _scaler.inputContentWidth = input.width;
    _scaler.inputContentHeight = input.height;
    [_scaler encodeToCommandBuffer:command];
    return YES;
}
@end
