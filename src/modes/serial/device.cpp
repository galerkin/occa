#include "occa/modes/serial/device.hpp"
#include "occa/base.hpp"

namespace occa {
  namespace serial {
    device::device(){
      uvaEnabled_ = false;
      bytesAllocated = 0;

      getEnvironmentVariables();

      sys::addSharedBinaryFlagsTo(compiler, compilerFlags);
    }

    device::device(const device &d){
      *this = d;
    }

    device& device::operator = (const device &d){
      uvaEnabled_    = d.uvaEnabled_;
      uvaMap         = d.uvaMap;
      uvaDirtyMemory = d.uvaDirtyMemory;

      compiler      = d.compiler;
      compilerFlags = d.compilerFlags;

      bytesAllocated = d.bytesAllocated;

      return *this;
    }

    void* device::getContextHandle(){
      return NULL;
    }

    void device::setup(argInfoMap &aim){
      properties = aim;

      vendor = sys::compilerVendor(compiler);

      sys::addSharedBinaryFlagsTo(vendor, compilerFlags);
    }

    // [REFACTOR]
    void device::addOccaHeadersToInfo(kernelInfo &info_){
    }

    std::string device::getInfoSalt(const kernelInfo &info_){
      std::stringstream salt;

      salt << "Serial"
           << info_.salt()
           << parserVersion
           << compilerEnvScript
           << compiler
           << compilerFlags;

      return salt.str();
    }

    void device::getEnvironmentVariables(){
      if (properties.has("compiler")) {
        compiler = properties["compiler"];
      }
      else if (env::var("OCCA_CXX").size()) {
        compiler = env::var("OCCA_CXX");
      }
      else if (env::var("CXX").size()) {
        compiler = env::var("CXX");
      }
      else{
#if (OCCA_OS & (LINUX_OS | OSX_OS))
        compiler = "g++";
#else
        compiler = "cl.exe";
#endif
      }

      if (properties.has("compilerFlags")) {
        compilerFlags = properties["compilerFlags"];
      }
      else if (env::var("OCCA_CXXFLAGS").size()) {
        compilerFlags = env::var("OCCA_CXXFLAGS");
      }
      else if (env::var("CXXFLAGS").size()) {
        compilerFlags = env::var("CXXFLAGS");
      }
      else{
#if (OCCA_OS & (LINUX_OS | OSX_OS))
        compilerFlags = "-g";
#else
#  if OCCA_DEBUG_ENABLED
        compilerFlags = " /Od";
#  else
        compilerFlags = " /O2";
#  endif
#endif
      }

      if (properties.has("compilerEnvScript")) {
        compilerEnvScript = properties["compilerEnvScript"];
      }
      else {
#if (OCCA_OS == WINDOWS_OS)
        std::string byteness;

        if(sizeof(void*) == 4)
          byteness = "x86 ";
        else if(sizeof(void*) == 8)
          byteness = "amd64";
        else
          OCCA_CHECK(false, "sizeof(void*) is not equal to 4 or 8");

#  if   (OCCA_VS_VERSION == 1800)
        // MSVC++ 12.0 - Visual Studio 2013
        char *visualStudioTools = getenv("VS120COMNTOOLS");
#  elif (OCCA_VS_VERSION == 1700)
        // MSVC++ 11.0 - Visual Studio 2012
        char *visualStudioTools = getenv("VS110COMNTOOLS");
#  else (OCCA_VS_VERSION < 1700)
        // MSVC++ 10.0 - Visual Studio 2010
        char *visualStudioTools = getenv("VS100COMNTOOLS");
#  endif

        if(visualStudioTools != NULL){
          compilerEnvScript = "\"" + std::string(visualStudioTools) + "..\\..\\VC\\vcvarsall.bat\" " + byteness;
        }
        else{
          std::cout << "WARNING: Visual Studio environment variable not found -> compiler environment (vcvarsall.bat) maybe not correctly setup." << std::endl;
        }
#endif
      }
    }

    void device::appendAvailableDevices(std::vector<occa::device> &dList){
      dList.push_back(occa::device(this));
    }

    // [REFACTOR]
    // void device::setCompiler(const std::string &compiler_){
    //   compiler = compiler_;
    //   vendor = sys::compilerVendor(compiler);
    //   sys::addSharedBinaryFlagsTo(vendor, compilerFlags);
    // }

    // void device::setCompilerEnvScript(const std::string &compilerEnvScript_){
    //   compilerEnvScript = compilerEnvScript_;
    // }

    // void device::setCompilerFlags(const std::string &compilerFlags_){
    //   compilerFlags = compilerFlags_;
    //   sys::addSharedBinaryFlagsTo(vendor, compilerFlags);
    // }

    void device::flush(){}

    void device::finish(){}

    bool device::fakesUva(){
      return false;
    }

    void device::waitFor(streamTag tag){}

    stream_t device::createStream(){
      return NULL;
    }

    void device::freeStream(stream_t s){}

    stream_t device::wrapStream(void *handle_){
      return NULL;
    }

    streamTag device::tagStream(){
      streamTag ret;

      ret.tagTime = currentTime();

      return ret;
    }

    double device::timeBetween(const streamTag &startTag, const streamTag &endTag){
      return (endTag.tagTime - startTag.tagTime);
    }

    std::string device::fixBinaryName(const std::string &filename){
#if (OCCA_OS & (LINUX_OS | OSX_OS))
      return filename;
#else
      return (filename + ".dll");
#endif
    }

    kernel_v* device::buildKernelFromSource(const std::string &filename,
                                                      const std::string &functionName,
                                                      const kernelInfo &info_){
      kernel_v *k = new kernel_t<Serial>;
      k->dHandle = this;

      k->buildFromSource(filename, functionName, info_);

      return k;
    }

    kernel_v* device::buildKernelFromBinary(const std::string &filename,
                                                      const std::string &functionName){
      kernel_v *k = new kernel_t<Serial>;
      k->dHandle = this;
      k->buildFromBinary(filename, functionName);
      return k;
    }

    void device::cacheKernelInLibrary(const std::string &filename,
                                                const std::string &functionName,
                                                const kernelInfo &info_){
#if 0
      //---[ Creating shared library ]----
      kernel tmpK = occa::device(this).buildKernelFromSource(filename, functionName, info_);
      tmpK.free();

      kernelInfo info = info_;

      addOccaHeadersToInfo(info);

      std::string cachedBinary = getCachedName(filename, getInfoSalt(info));

#if (OCCA_OS & WINDOWS_OS)
      // Windows requires .dll extension
      cachedBinary = cachedBinary + ".dll";
#endif
      //==================================

      library::infoID_t infoID;

      infoID.modelID    = modelID_;
      infoID.kernelName = functionName;

      library::infoHeader_t &header = library::headerMap[infoID];

      header.fileID = -1;
      header.mode   = Serial;

      const std::string flatDevID = getIdentifier().flattenFlagMap();

      header.flagsOffset = library::addToScratchPad(flatDevID);
      header.flagsBytes  = flatDevID.size();

      header.contentOffset = library::addToScratchPad(cachedBinary);
      header.contentBytes  = cachedBinary.size();

      header.kernelNameOffset = library::addToScratchPad(functionName);
      header.kernelNameBytes  = functionName.size();
#endif
    }

    kernel_v* device::loadKernelFromLibrary(const char *cache,
                                                      const std::string &functionName){
#if 0
      kernel_v *k = new kernel_t<Serial>;
      k->dHandle = this;
      k->loadFromLibrary(cache, functionName);
      return k;
#endif
      return NULL;
    }

    memory_v* device::wrapMemory(void *handle_,
                                           const uintptr_t bytes){
      memory_v *mem = new memory_t<Serial>;

      mem->dHandle = this;
      mem->size    = bytes;
      mem->handle  = handle_;

      mem->memInfo |= memFlag::isAWrapper;

      return mem;
    }

    memory_v* device::wrapTexture(void *handle_,
                                            const int dim, const occa::dim &dims,
                                            occa::formatType type, const int permissions){
      memory_v *mem = new memory_t<Serial>;

      mem->dHandle = this;
      mem->size    = ((dim == 1) ? dims.x : (dims.x * dims.y)) * type.bytes();

      mem->memInfo |= (memFlag::isATexture |
                       memFlag::isAWrapper);

      mem->textureInfo.dim  = dim;

      mem->textureInfo.w = dims.x;
      mem->textureInfo.h = dims.y;
      mem->textureInfo.d = dims.z;

      mem->textureInfo.arg = handle_;

      mem->handle = &(mem->textureInfo);

      return mem;
    }

    memory_v* device::malloc(const uintptr_t bytes,
                                       void *src){
      memory_v *mem = new memory_t<Serial>;

      mem->dHandle = this;
      mem->size    = bytes;

      mem->handle = sys::malloc(bytes);

      if(src != NULL)
        ::memcpy(mem->handle, src, bytes);

      return mem;
    }

    memory_v* device::textureAlloc(const int dim, const occa::dim &dims,
                                             void *src,
                                             occa::formatType type, const int permissions){
      memory_v *mem = new memory_t<Serial>;

      mem->dHandle = this;
      mem->size    = ((dim == 1) ? dims.x : (dims.x * dims.y)) * type.bytes();

      mem->memInfo |= memFlag::isATexture;

      mem->textureInfo.dim  = dim;

      mem->textureInfo.w = dims.x;
      mem->textureInfo.h = dims.y;
      mem->textureInfo.d = dims.z;

      mem->handle = sys::malloc(mem->size);

      ::memcpy(mem->textureInfo.arg, src, mem->size);

      mem->handle = &(mem->textureInfo);

      return mem;
    }

    memory_v* device::mappedAlloc(const uintptr_t bytes,
                                            void *src){
      memory_v *mem = malloc(bytes, src);

      mem->mappedPtr = mem->handle;

      return mem;
    }

    uintptr_t device::memorySize(){
      return sys::installedRAM();
    }

    void device::free(){}

    int device::simdWidth(){
      simdWidth_ = OCCA_SIMD_WIDTH;
      return OCCA_SIMD_WIDTH;
    }
  }
}