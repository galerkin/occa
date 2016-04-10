#ifndef OCCA_SERIAL_DEVICE_HEADER
#define OCCA_SERIAL_DEVICE_HEADER

#include "occa/device.hpp"

namespace occa {
  namespace serial {
    //---[ Data Structs ]---------------
    struct SerialKernelData_t {
      void *dlHandle;
      handleFunction_t handle;

      void *vArgs[2*OCCA_MAX_ARGS];
    };
    //==================================

    class device : public occa::device_v {
      int vendor;

      std::string compiler, compilerFlags, compilerEnvScript;

      device();
      device(const device &k);
      device& operator = (const device &k);
      void free();

      void* getContextHandle();

      void setup(argInfoMap &aim);

      void addOccaHeadersToInfo(kernelInfo &info_);

      std::string getInfoSalt(const kernelInfo &info_);

      void getEnvironmentVariables();

      void appendAvailableDevices(std::vector<occa::device> &dList);

      void flush();
      void finish();

      //  |---[ Stream ]----------------
      stream createStream();
      void freeStream(stream s);

      streamTag tagStream();
      void waitFor(streamTag tag);
      double timeBetween(const streamTag &startTag, const streamTag &endTag);

      stream wrapStream(void *handle_);
      //  |=============================

      //  |---[ Kernel ]----------------
      std::string fixBinaryName(const std::string &filename);

      kernel_v* buildKernelFromSource(const std::string &filename,
                                      const std::string &functionName,
                                      const kernelInfo &info_);

      kernel_v* buildKernelFromBinary(const std::string &filename,
                                      const std::string &functionName);
      //  |=============================

      //  |---[ Kernel ]----------------
      memory_v* wrapMemory(void *handle_,
                           const uintptr_t bytes);

      memory_v* malloc(const uintptr_t bytes,
                       void *src);

      memory_v* mappedAlloc(const uintptr_t bytes,
                            void *src);

      uintptr_t memorySize();
      //  |=============================
    };
  }
}

#endif