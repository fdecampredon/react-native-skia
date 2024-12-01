#include "MetalContext.h"

#import <MetalKit/MetalKit.h>

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdocumentation"

#import <include/gpu/ganesh/GrBackendSurface.h>
#import <include/gpu/ganesh/SkImageGanesh.h>
#import <include/gpu/ganesh/mtl/GrMtlBackendContext.h>
#import <include/gpu/ganesh/mtl/GrMtlBackendSurface.h>
#import <include/gpu/ganesh/mtl/GrMtlDirectContext.h>
#import <include/gpu/ganesh/mtl/GrMtlTypes.h>
#import <include/gpu/ganesh/mtl/SkSurfaceMetal.h>

#pragma clang diagnostic pop

MetalContext::MetalContext() {
  _device = MTLCreateSystemDefaultDevice();
  _context.commandQueue =
      id<MTLCommandQueue>(CFRetain((GrMTLHandle)[_device newCommandQueue]));

  GrMtlBackendContext backendContext = {};
  backendContext.fDevice.reset((__bridge void *)_device);
  backendContext.fQueue.reset((__bridge void *)_context.commandQueue);
  GrContextOptions grContextOptions; // set different options here.

  // Create the Skia Direct Context
  _context.skContext = GrDirectContexts::MakeMetal(backendContext);
  if (_context.skContext == nullptr) {
    RNSkia::RNSkLogger::logToConsole("Couldn't create a Skia Metal Context");
  }
}

MetalContext::~MetalContext() {
  if (_context.commandQueue != nullptr) {
    CFRelease((GrMTLHandle)_context.commandQueue);
  }
}


id<MTLTexture> MetalContext::copyTextureForShare(id<MTLTexture> sourceTexture) {


  NSDictionary *ioSurfaceProperties = @{
      (__bridge NSString *)kIOSurfaceWidth: @(512),
      (__bridge NSString *)kIOSurfaceHeight: @(512),
      (__bridge NSString *)kIOSurfacePixelFormat: @(MTLPixelFormatBGRA8Unorm),
      (__bridge NSString *)kIOSurfaceBytesPerElement: @(4)
  };

  IOSurfaceRef ioSurface = IOSurfaceCreate((__bridge CFDictionaryRef)ioSurfaceProperties);

  MTLTextureDescriptor* descriptor = [MTLTextureDescriptor
      texture2DDescriptorWithPixelFormat:sourceTexture.pixelFormat
                                   width:sourceTexture.width
                                  height:sourceTexture.height
                               mipmapped:NO];
  descriptor.storageMode = MTLStorageModeShared;
  descriptor.pixelFormat = sourceTexture.pixelFormat;
  id<MTLTexture> shareableTexture =
      [_device newTextureWithDescriptor:descriptor];

  id<MTLCommandBuffer> commandBuffer = [_context.commandQueue commandBufferWithUnretainedReferences];
  id<MTLBlitCommandEncoder> blitEncoder = [commandBuffer blitCommandEncoder];
  [blitEncoder
        copyFromTexture:sourceTexture
            sourceSlice:0
            sourceLevel:0
           sourceOrigin:MTLOriginMake(0, 0, 0)
             sourceSize:MTLSizeMake(sourceTexture.width, sourceTexture.height, 1)
              toTexture:shareableTexture
       destinationSlice:0
       destinationLevel:0
      destinationOrigin:MTLOriginMake(0, 0, 0)];

  [blitEncoder endEncoding];
  [commandBuffer commit];
  [commandBuffer waitUntilCompleted];
  
  return shareableTexture;
}
