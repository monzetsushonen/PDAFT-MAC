#import <Foundation/Foundation.h>
#import <Metal/Metal.h>
#import <MetalFX/MetalFX.h>

NS_ASSUME_NONNULL_BEGIN

// One persistent scaler per resolution/format. Call from a single render queue.
// The caller owns the complete scene + UI input and presentation destination.
@interface PDAFTSpatialUpscaler : NSObject
- (nullable instancetype)initWithDevice:(id<MTLDevice>)device
                            inputWidth:(NSUInteger)inputWidth
                           inputHeight:(NSUInteger)inputHeight
                           outputWidth:(NSUInteger)outputWidth
                          outputHeight:(NSUInteger)outputHeight
                           pixelFormat:(MTLPixelFormat)pixelFormat
                             colorMode:(MTLFXSpatialScalerColorProcessingMode)colorMode
                                 error:(NSError * _Nullable * _Nullable)error;
- (instancetype)init NS_UNAVAILABLE;
@property(nonatomic, readonly) MTLTextureUsage inputUsage;
@property(nonatomic, readonly) MTLTextureUsage outputUsage;
- (BOOL)encodeInput:(id<MTLTexture>)input
             output:(id<MTLTexture>)output
      commandBuffer:(id<MTLCommandBuffer>)commandBuffer
              error:(NSError * _Nullable * _Nullable)error;
@end

NS_ASSUME_NONNULL_END
