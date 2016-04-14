#if (OCCA_OS & LINUX_OS)
#  include <unistd.h>
#  include <sys/types.h>
#  include <sys/dir.h>
#elif (OCCA_OS & OSX_OS)
#  include <sys/types.h>
#  include <sys/dir.h>
#else
#  ifndef NOMINMAX
#    define NOMINMAX     // NBN: clear min/max macros
#  endif
#  include <windows.h>
#  include <string>
#  include <direct.h>    // NBN: rmdir _rmdir
#endif

#include <fstream>
#include <stddef.h>

#include "occa/tools/env.hpp"
#include "occa/tools/io.hpp"

namespace occa {
  strToBoolMap_t fileLocks;

  // Kernel Caching
  namespace kc {
    std::string sourceFile = "source.occa";
    std::string binaryFile = "binary";
  }

  std::string getOnlyFilename(const std::string &filename) {
    std::string dir = getFileDirectory(filename);

    if (dir.size() < filename.size())
      return filename.substr(dir.size());

    return "";
  }

  std::string getPlainFilename(const std::string &filename) {
    std::string ext = getFileExtension(filename);
    std::string dir = getFileDirectory(filename);

    int start = (int) dir.size();
    int end = (int) filename.size();

    // For the [/] character
    if (0 < start)
      ++start;

    // For the [.ext] extension
    if (0 < ext.size())
      end -= (ext.size() - 1);

    return filename.substr(start, end - start);
  }

  std::string getFileDirectory(const std::string &filename) {
    const int chars = (int) filename.size();
    const char *c   = filename.c_str();

    int lastSlash = 0;

#if (OCCA_OS & (LINUX_OS | OSX_OS))
    for(int i = 0; i < chars; ++i)
      if (c[i] == '/')
        lastSlash = i;
#else
    for(int i = 0; i < chars; ++i)
      if ((c[i] == '/') || (c[i] == '\\'))
        lastSlash = i;
#endif

    if (lastSlash || (c[0] == '/'))
      ++lastSlash;

    return filename.substr(0, lastSlash);
  }

  std::string getFileExtension(const std::string &filename) {
    const char *c = filename.c_str();
    const char *i = NULL;

    while(*c != '\0') {
      if (*c == '.')
        i = c;

      ++c;
    }

    if (i != NULL)
      return filename.substr(i - filename.c_str() + 1);

    return "";
  }

  std::string compressFilename(const std::string &filename) {
    if (filename.find(env::OCCA_CACHE_DIR) != 0)
      return filename;

    const std::string libPath = env::OCCA_CACHE_DIR + "libraries/";
    const std::string kerPath = env::OCCA_CACHE_DIR + "kernels/";

    if (filename.find(libPath) == 0) {
      std::string libName = getLibraryName(filename);
      std::string theRest = filename.substr(libPath.size() + libName.size() + 1);

      return ("[" + libName + "]/" + theRest);
    }
    else if (filename.find(kerPath) == 0) {
      return filename.substr(kerPath.size());
    }

    return filename;
  }

  // NBN: handle binary mode and EOL chars on Windows
  std::string readFile(const std::string &filename, const bool readingBinary) {
    FILE *fp = NULL;

    if (!readingBinary) {
      fp = fopen(filename.c_str(), "r");
    }
    else{
      fp = fopen(filename.c_str(), "rb");
    }

    OCCA_CHECK(fp != 0,
               "Failed to open [" << compressFilename(filename) << "]");

    struct stat statbuf;
    stat(filename.c_str(), &statbuf);

    const size_t nchars = statbuf.st_size;

    char *buffer = (char*) calloc(nchars + 1, sizeof(char));
    size_t nread = fread(buffer, sizeof(char), nchars, fp);

    fclose(fp);
    buffer[nread] = '\0';

    std::string contents(buffer, nread);

    free(buffer);

    return contents;
  }

  void writeToFile(const std::string &filename,
                   const std::string &content) {

    sys::mkpath(getFileDirectory(filename));

    FILE *fp = fopen(filename.c_str(), "w");

    OCCA_CHECK(fp != 0,
               "Failed to open [" << compressFilename(filename) << "]");

    fputs(content.c_str(), fp);

    fclose(fp);
  }

  std::string getFileLock(const std::string &hash, const int depth) {
    std::string ret = (env::OCCA_CACHE_DIR + "locks/" + hash);

    ret += '_';
    ret += (char) ('0' + depth);

    return ret;
  }

  void clearLocks() {
    strToBoolMapIterator it = fileLocks.begin();
    while (it != fileLocks.end()) {
      releaseHash(it->first);
      ++it;
    }
    fileLocks.clear();
  }

  bool haveHash(const std::string &hash, const int depth) {
    std::string lockDir = getFileLock(hash, depth);

    sys::mkpath(env::OCCA_CACHE_DIR + "locks/");

    int mkdirStatus = sys::mkdir(lockDir);

    if (mkdirStatus && (errno == EEXIST))
      return false;

    fileLocks[lockDir] = true;

    return true;
  }

  void waitForHash(const std::string &hash, const int depth) {
    struct stat buffer;

    std::string lockDir   = getFileLock(hash, depth);
    const char *c_lockDir = lockDir.c_str();

    while(stat(c_lockDir, &buffer) == 0)
      ; // Do Nothing
  }

  void releaseHash(const std::string &hash, const int depth) {
    releaseHashLock(getFileLock(hash, depth));
  }

  void releaseHashLock(const std::string &lockDir) {
    sys::rmdir(lockDir);
    fileLocks.erase(lockDir);
  }

  bool fileNeedsParser(const std::string &filename) {
    std::string ext = getFileExtension(filename);

    return ((ext == "okl") ||
            (ext == "ofl") ||
            (ext == "cl") ||
            (ext == "cu"));
  }

  parsedKernelInfo parseFileForFunction(const std::string &deviceMode,
                                        const std::string &filename,
                                        const std::string &parsedFile,
                                        const std::string &functionName,
                                        const kernelInfo &info) {

    parser fileParser;

    const std::string extension = getFileExtension(filename);

    flags_t parserFlags = info.getParserFlags();

    parserFlags["mode"]     = deviceMode;
    parserFlags["language"] = ((extension != "ofl") ? "C" : "Fortran");

    if ((extension == "oak") ||
       (extension == "oaf")) {

      parserFlags["magic"] = "enabled";
    }

    std::string parsedContent = fileParser.parseFile(info.header,
                                                     filename,
                                                     parserFlags);

    if (!sys::fileExists(parsedFile)) {
      sys::mkpath(getFileDirectory(parsedFile));

      std::ofstream fs;
      fs.open(parsedFile.c_str());

      fs << parsedContent;

      fs.close();
    }

    kernelInfoIterator kIt = fileParser.kernelInfoMap.find(functionName);

    if (kIt != fileParser.kernelInfoMap.end())
      return (kIt->second)->makeParsedKernelInfo();

    OCCA_CHECK(false,
               "Could not find function ["
               << functionName << "] in file ["
               << compressFilename(filename    ) << "]");

    return parsedKernelInfo();
  }

  std::string removeSlashes(const std::string &str) {
    std::string ret = str;
    const size_t chars = str.size();

    for(size_t i = 0; i < chars; ++i) {
      if (ret[i] == '/')
        ret[i] = '_';
    }

    return ret;
  }

  std::string getOccaScriptFile(const std::string &filename) {
    return readFile(env::OCCA_DIR + "/scripts/" + filename);
  }

  void setupOccaHeaders(const kernelInfo &info) {
    cacheFile(sys::getFilename("[occa]/primitives.hpp"),
              readFile(env::OCCA_DIR + "/include/occa/defines/vector.hpp"),
              "vectorDefines");

    // [REFACTOR]
    // std::string mode = modeToStr(info.mode);
    // cacheFile(info.getModeHeaderFilename(),
    //           readFile(env::OCCA_DIR + "/include/occa/defines/" + mode + ".hpp"),
    //           mode + "Defines");
  }

  void cacheFile(const std::string &filename,
                 std::string source,
                 const std::string &hash) {

    cacheFile(filename, source.c_str(), hash, false);
  }

  void cacheFile(const std::string &filename,
                 const char *source,
                 const std::string &hash,
                 const bool deleteSource) {
    if(!haveHash(hash)){
      waitForHash(hash);
    } else {
      if (!sys::fileExists(filename)) {
        sys::mkpath(getFileDirectory(filename));

        std::ofstream fs2;
        fs2.open(filename.c_str());
        fs2 << source;
        fs2.close();
      }
      releaseHash(hash);
    }
    if (deleteSource)
      delete [] source;
  }

  void createSourceFileFrom(const std::string &filename,
                            const std::string &hashDir,
                            const kernelInfo &info) {

    const std::string sourceFile = hashDir + kc::sourceFile;

    if (sys::fileExists(sourceFile))
      return;

    sys::mkpath(hashDir);

    setupOccaHeaders(info);

    std::ofstream fs;
    fs.open(sourceFile.c_str());

    fs << "#include \"" << info.getModeHeaderFilename() << "\"\n"
       << "#include \"" << sys::getFilename("[occa]/primitives.hpp") << "\"\n";

    // [REFACTOR]
    // if (info.mode & (Serial | OpenMP | Pthreads | CUDA)) {
    //   fs << "#if defined(OCCA_IN_KERNEL) && !OCCA_IN_KERNEL\n"
    //      << "using namespace occa;\n"
    //      << "#endif\n";
    // }

    fs << info.header
       << readFile(filename);

    fs.close();
  }
}
