#pragma once

// TODO: Add android flags
#if __ANDROID_API__ >= 26
#include <android/hardware_buffer.h>
#endif
#include <exception>
#include <functional>
#include <memory>
#include <string>

#if defined(SK_GRAPHITE)
#include "DawnContext.h"
#else
#include "OpenGLContext.h"
#endif

#include "AHardwareBufferUtils.h"
#include "JniPlatformContext.h"
#include "MainThreadDispatcher.h"
#include "RNSkAndroidVideo.h"
#include "RNSkPlatformContext.h"

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdocumentation"

#include "include/ports/SkFontMgr_android.h"

#pragma clang diagnostic pop

namespace RNSkia {
namespace jsi = facebook::jsi;

class RNSkAndroidPlatformContext : public RNSkPlatformContext {
public:
  RNSkAndroidPlatformContext(
      JniPlatformContext *jniPlatformContext, jsi::Runtime *runtime,
      std::shared_ptr<facebook::react::CallInvoker> jsCallInvoker)
      : RNSkPlatformContext(runtime, jsCallInvoker,
                            jniPlatformContext->getPixelDensity()),
        _jniPlatformContext(jniPlatformContext) {}

  ~RNSkAndroidPlatformContext() {}

  void performStreamOperation(
      const std::string &sourceUri,
      const std::function<void(std::unique_ptr<SkStreamAsset>)> &op) override {
    _jniPlatformContext->performStreamOperation(sourceUri, op);
  }

  void raiseError(const std::exception &err) override {
    _jniPlatformContext->raiseError(err);
  }

  sk_sp<SkSurface> makeOffscreenSurface(int width, int height) override {
#if defined(SK_GRAPHITE)
    return DawnContext::getInstance().MakeOffscreen(width, height);
#else
    return OpenGLContext::getInstance().MakeOffscreen(width, height);
#endif
  }

  std::shared_ptr<WindowContext>
  makeContextFromNativeSurface(void *surface, int width, int height) override {
#if defined(SK_GRAPHITE)
    return DawnContext::getInstance().MakeWindow(surface, width, height);
#else
    auto aWindow = reinterpret_cast<ANativeWindow *>(surface);
    return OpenGLContext::getInstance().MakeWindow(aWindow, width, height);
#endif
  }

  sk_sp<SkImage> makeImageFromNativeBuffer(void *buffer) override {
#if defined(SK_GRAPHITE)
    return DawnContext::getInstance().MakeImageFromBuffer(buffer);
#else
    return OpenGLContext::getInstance().MakeImageFromBuffer(buffer);
#endif
  }

  sk_sp<SkImage> makeImageFromNativeTexture(void *nativeTexture, int width, int height) {
    auto textureId = reinterpret_cast<uint64_t>(nativeTexture);
    OpenGLContext::getInstance().makeCurrent();
    if (glIsTexture(textureId) == GL_FALSE) {
      throw new std::runtime_error("Not valid texture");
    }


    GLuint fbo;
    glGenFramebuffers(1, &fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);

    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, textureId, 0);
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
      glBindFramebuffer(GL_FRAMEBUFFER, 0);
      throw new std::runtime_error("Not frame buffer");
    }
    std::vector<GLubyte> pixels(width * height * 4); // Pour RGBA
    glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
    glBindTexture(GL_TEXTURE_2D, 0);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glDeleteFramebuffers(1, &fbo);

    SkImageInfo imageInfo = SkImageInfo::Make(
        width, height,             // Dimensions
        kRGBA_8888_SkColorType,    // Format des pixels (RGBA 8 bits par canal)
        kUnpremul_SkAlphaType,     // Alpha non prémultiplié
        SkColorSpace::MakeSRGB()   // Espace colorimétrique (sRGB)
    );

    // Crée un objet SkData à partir des pixels
    sk_sp<SkData> skData = SkData::MakeWithCopy(pixels.data(), pixels.size());

    // Crée une SkImage à partir des données
    return SkImages::RasterFromData(imageInfo, skData, width * 4);



    /*GrGLTextureInfo textureInfo;
    textureInfo.fTarget = GL_TEXTURE_2D;
    textureInfo.fID = textureId;
    textureInfo.fFormat = GR_GL_RGBA8;

    GrBackendTexture backendTexture = GrBackendTextures::MakeGL(
        width, height, skgpu::Mipmapped::kNo, textureInfo
    );
    return SkImages::BorrowTextureFrom(
        OpenGLContext::getInstance().getDirectContext(), backendTexture, kTopLeft_GrSurfaceOrigin,
        kRGBA_8888_SkColorType, kPremul_SkAlphaType, nullptr);*/
  }

  std::shared_ptr<RNSkVideo> createVideo(const std::string &url) override {
    auto jniVideo = _jniPlatformContext->createVideo(url);
    return std::make_shared<RNSkAndroidVideo>(jniVideo);
  }

  void releaseNativeBuffer(uint64_t pointer) override {
#if __ANDROID_API__ >= 26
    AHardwareBuffer *buffer = reinterpret_cast<AHardwareBuffer *>(pointer);
    AHardwareBuffer_release(buffer);
#endif
  }

  uint64_t makeNativeBuffer(sk_sp<SkImage> image) override {
#if __ANDROID_API__ >= 26
    auto bytesPerPixel = image->imageInfo().bytesPerPixel();
    int bytesPerRow = image->width() * bytesPerPixel;
    auto buf = SkData::MakeUninitialized(image->width() * image->height() *
                                         bytesPerPixel);
    SkImageInfo info =
        SkImageInfo::Make(image->width(), image->height(), image->colorType(),
                          image->alphaType());
    image->readPixels(nullptr, info, const_cast<void *>(buf->data()),
                      bytesPerRow, 0, 0);
    const void *pixelData = buf->data();

    // Define the buffer description
    AHardwareBuffer_Desc desc = {};
    // TODO: use image info here
    desc.width = image->width();
    desc.height = image->height();
    desc.layers = 1; // Single image layer
    desc.format = GetBufferFormatFromSkColorType(
        image->colorType()); // Assuming the image
                             // is in this format
    desc.usage = AHARDWAREBUFFER_USAGE_CPU_READ_OFTEN |
                 AHARDWAREBUFFER_USAGE_CPU_WRITE_OFTEN |
                 AHARDWAREBUFFER_USAGE_GPU_COLOR_OUTPUT |
                 AHARDWAREBUFFER_USAGE_GPU_SAMPLED_IMAGE;
    desc.stride = bytesPerRow; // Stride in pixels, not in bytes

    // Allocate the buffer
    AHardwareBuffer *buffer = nullptr;
    if (AHardwareBuffer_allocate(&desc, &buffer) != 0) {
      // Handle allocation failure
      return 0;
    }

    // Map the buffer to gain access to its memory
    void *mappedBuffer = nullptr;
    AHardwareBuffer_lock(buffer, AHARDWAREBUFFER_USAGE_CPU_WRITE_OFTEN, -1,
                         nullptr, &mappedBuffer);
    if (mappedBuffer == nullptr) {
      // Handle mapping failure
      AHardwareBuffer_release(buffer);
      return 0;
    }

    // Copy the image data to the buffer
    memcpy(mappedBuffer, pixelData, desc.height * bytesPerRow);

    // Unmap the buffer
    AHardwareBuffer_unlock(buffer, nullptr);

    // Return the buffer pointer as a uint64_t. It's the caller's responsibility
    // to manage this buffer.
    return reinterpret_cast<uint64_t>(buffer);
#else
    return 0;
#endif
  }

  uint64_t getImageBackendTexture(sk_sp<SkImage> image) {
    GrBackendTexture texture;
    if (!SkImages::GetBackendTextureFromImage(image, &texture, true)) {
      return -1;
    }
    GrGLTextureInfo textureInfo;
    if (!GrBackendTextures::GetGLTextureInfo(texture, &textureInfo)) {
      return -1;
    }
    if (glIsTexture(textureInfo.fID) == GL_FALSE) {
      return -1;
    }


    OpenGLContext::getInstance().makeCurrent();

    /*GLuint texId;
    glGenTextures(1, &texId);

    // Crée un framebuffer
    GLuint fbo;
    glGenFramebuffers(1, &fbo);

    // Bind le framebuffer
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);

    // Attache la texture source au framebuffer
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, textureInfo.fID, 0);

    // Vérifie que le framebuffer est complet
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
      glBindFramebuffer(GL_FRAMEBUFFER, 0);
      glDeleteFramebuffers(1, &fbo);
      throw std::runtime_error("Framebuffer is not complete!");
    }

    // Bind la texture cible
    glBindTexture(GL_TEXTURE_2D, texId);

    // Copier le contenu du framebuffer dans la texture cible
    glCopyTexImage2D(
        GL_TEXTURE_2D, 0, GL_RGBA, // Cible, niveau de mipmap, format interne
        0, 0, image->width(), image->height(),       // Position et dimensions
        0                          // Bordure (toujours 0 en OpenGL ES)
    );

    glBindTexture(GL_TEXTURE_2D, 0);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glDeleteFramebuffers(1, &fbo)
    return texId;*/


   /* GLuint fbo;
    glGenFramebuffers(1, &fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);

    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, textureInfo.fID, 0);
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
      glBindFramebuffer(GL_FRAMEBUFFER, 0);
      return -1;
    }
    auto  width= image->width();
    auto  height= image->height();
    std::vector<GLubyte> pixels(width * height * 4);
    glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
    glBindTexture(GL_TEXTURE_2D, 0);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glDeleteFramebuffers(1, &fbo);*/
   glFinish();



    /*glBindTexture(GL_TEXTURE_2D, textureInfo.fID);
    std::vector<GLubyte> data(width * height * 4, 255); // Remplissage blanc
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, data.data());
    glBindTexture(GL_TEXTURE_2D, 0);*/

    return textureInfo.fID;
  }

#if !defined(SK_GRAPHITE)
  GrDirectContext *getDirectContext() override {
    return OpenGLContext::getInstance().getDirectContext();
  }
#endif

  sk_sp<SkFontMgr> createFontMgr() override {
    return SkFontMgr_New_Android(nullptr);
  }

  void runOnMainThread(std::function<void()> task) override {
    MainThreadDispatcher::getInstance().post(std::move(task));
  }

  sk_sp<SkImage> takeScreenshotFromViewTag(size_t tag) override {
    return _jniPlatformContext->takeScreenshotFromViewTag(tag);
  }

private:
  JniPlatformContext *_jniPlatformContext;
};

} // namespace RNSkia
