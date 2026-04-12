// (‑●‑●)> dual licensed under the WTFPL v2 and MIT licenses
//   without any warranty.
//   by Gregory Pakosz (@gpakosz)
// https://github.com/gpakosz/whereami

// in case you want to #include "whereami.c" in a larger compilation unit
#if !defined(WHEREAMI_H)
#include <whereami.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif

#if defined(__linux__) || defined(__CYGWIN__)
#undef _DEFAULT_SOURCE
#define _DEFAULT_SOURCE
#elif defined(__APPLE__)
#undef _DARWIN_C_SOURCE
#define _DARWIN_C_SOURCE
#define _DARWIN_BETTER_REALPATH
#elif defined(__sun)
#undef __EXTENSIONS__
#define __EXTENSIONS__
#endif

#if !defined(WAI_MALLOC) || !defined(WAI_FREE) || !defined(WAI_REALLOC)
#include <stdlib.h>
#endif

#if !defined(WAI_MALLOC)
#define WAI_MALLOC(size) malloc(size)
#endif

#if !defined(WAI_FREE)
#define WAI_FREE(p) free(p)
#endif

#if !defined(WAI_REALLOC)
#define WAI_REALLOC(p, size) realloc(p, size)
#endif

#ifndef WAI_NOINLINE
#if defined(_MSC_VER)
#define WAI_NOINLINE __declspec(noinline)
#elif defined(__GNUC__)
#define WAI_NOINLINE __attribute__((noinline))
#else
#error unsupported compiler
#endif
#endif

#if defined(_MSC_VER)
#define WAI_RETURN_ADDRESS() _ReturnAddress()
#elif defined(__GNUC__)
#define WAI_RETURN_ADDRESS() __builtin_extract_return_addr(__builtin_return_address(0))
#else
#error unsupported compiler
#endif

#if defined(_WIN32)

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#if defined(_MSC_VER)
#pragma warning(push, 3)
#endif
#include <windows.h>
#include <intrin.h>
#if defined(_MSC_VER)
#pragma warning(pop)
#endif
#if defined(_MSC_VER) && (_MSC_VER >= 1900)
#include <stdbool.h>
#elif defined(__has_include)
#if __has_include(<stdbool.h>)
#include <stdbool.h>
#else
#define bool int
#define false 0
#define true 1
#endif
#else
#define bool int
#define false 0
#define true 1
#endif
#define WAI_DWORD_MAX (~(DWORD)0)

// Maximum extended-length Windows path is 32767 characters, plus the terminal NUL
// https://learn.microsoft.com/en-us/windows/win32/fileio/maximum-file-path-limitation
#define WAI_WINDOWS_MAX_PATH 32768

static int WAI_PREFIX(getModulePath_)(HMODULE module, char* out, int capacity, int* dirname_length)
{
  wchar_t buffer1[MAX_PATH];
  wchar_t buffer2[MAX_PATH];
  wchar_t* path1 = NULL;
  wchar_t* path2 = NULL;
  int length = -1;
  bool ok;

  for (ok = false; !ok; ok = true)
  {
    DWORD size;
    int length_, length__;

    size = GetModuleFileNameW(module, buffer1, sizeof(buffer1) / sizeof(buffer1[0]));

    if (size == 0)
      break;
    else if (size == (DWORD)(sizeof(buffer1) / sizeof(buffer1[0])))
    {
      DWORD size_ = size;
      do
      {
        wchar_t* path1_;

        // prevent later integer overflow when size_ is large
        if (size_ > WAI_DWORD_MAX / (sizeof(wchar_t) * 2))
          break;

        path1_ = (wchar_t*)WAI_REALLOC(path1, sizeof(wchar_t) * size_ * 2);
        if (!path1_)
          break;
        size_ *= 2;
        path1 = path1_;
        size = GetModuleFileNameW(module, path1, size_);
      }
      while (size == size_);

      if (size == 0 || size == size_)
        break;
    }
    else
      path1 = buffer1;

    if (path1 != buffer1)
    {
      // + 1 so _wfullpath() has room for the NUL terminator
      path2 = (wchar_t*)WAI_MALLOC(sizeof(wchar_t) * (size + 1));
      if (!path2)
        break;
    }
    else
      path2 = buffer2;

    if (!_wfullpath(path2, path1, path2 != buffer2 ? size + 1 : sizeof(buffer2) / sizeof(buffer2[0])))
    {
      wchar_t* result = NULL;
      size_t size_ = (size_t)(size < MAX_PATH ? MAX_PATH : size);

      // _wfullpath() also fails for reasons that growing the buffer can't cure
      // stop at the Windows path limit instead of growing until the allocator gives up
      while (!result && size_ < WAI_WINDOWS_MAX_PATH)
      {
        wchar_t* path2_;

        size_ *= 2;
        if (size_ > WAI_WINDOWS_MAX_PATH)
          size_ = WAI_WINDOWS_MAX_PATH;

        path2_ = (wchar_t*)WAI_REALLOC(path2 != buffer2 ? path2 : NULL, sizeof(wchar_t) * size_);
        if (!path2_)
          break;
        path2 = path2_;

        result = _wfullpath(path2, path1, size_);
      }

      if (!result)
        break;
    }

    length_ = (int)wcslen(path2);
    length__ = WideCharToMultiByte(CP_UTF8, 0, path2, length_ , out, capacity, NULL, NULL);

    if (length__ == 0)
      length__ = WideCharToMultiByte(CP_UTF8, 0, path2, length_, NULL, 0, NULL, NULL);
    if (length__ == 0)
      break;

    if (length__ <= capacity && dirname_length)
    {
      int i;

      for (i = length__ - 1; i >= 0; --i)
      {
        if (out[i] == '\\')
        {
          *dirname_length = i;
          break;
        }
      }
    }

    length = length__;
  }

  if (path1 != buffer1)
    WAI_FREE(path1);
  if (path2 != buffer2)
    WAI_FREE(path2);

  return ok ? length : -1;
}

WAI_NOINLINE WAI_FUNCSPEC
int WAI_PREFIX(getExecutablePath)(char* out, int capacity, int* dirname_length)
{
  if (dirname_length)
    *dirname_length = -1;

  return WAI_PREFIX(getModulePath_)(NULL, out, capacity, dirname_length);
}

WAI_NOINLINE WAI_FUNCSPEC
int WAI_PREFIX(getModulePath)(char* out, int capacity, int* dirname_length)
{
  HMODULE module;
  int length = -1;

  if (dirname_length)
    *dirname_length = -1;

#if defined(_MSC_VER)
#pragma warning(push)
#pragma warning(disable: 4054)
#endif
  if (GetModuleHandleEx(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, (LPCTSTR)WAI_RETURN_ADDRESS(), &module))
#if defined(_MSC_VER)
#pragma warning(pop)
#endif
  {
    length = WAI_PREFIX(getModulePath_)(module, out, capacity, dirname_length);
  }

  return length;
}

#elif defined(__linux__) || defined(__CYGWIN__) || (defined(WAI_USE_PROC_SELF_EXE) && !defined(__sun))

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#if defined(__linux__)
#include <linux/limits.h>
#else
#include <limits.h>
#endif
#if defined(__CYGWIN__)
#include <unistd.h>
#endif
#ifndef __STDC_FORMAT_MACROS
#define __STDC_FORMAT_MACROS
#endif
#include <inttypes.h>
#include <stdbool.h>

#if !defined(WAI_PROC_SELF_EXE)
#define WAI_PROC_SELF_EXE "/proc/self/exe"
#endif

WAI_FUNCSPEC
int WAI_PREFIX(getExecutablePath)(char* out, int capacity, int* dirname_length)
{
  char buffer[PATH_MAX];
#if defined(__CYGWIN__)
  char* with_exe = NULL;
#endif
  char* resolved = NULL;
  int length = -1;
  bool ok;

  if (dirname_length)
    *dirname_length = -1;

  for (ok = false; !ok; ok = true)
  {
    resolved = realpath(WAI_PROC_SELF_EXE, buffer);
    if (!resolved)
      break;

    length = (int)strlen(resolved);
    #if defined(__CYGWIN__)
    if (length + sizeof(".exe") <= sizeof(buffer))
    {
      memcpy(buffer + length, ".exe", sizeof(".exe"));
      with_exe = buffer;
    }
    else
    {
      // the .exe probe is a refinement
      with_exe = (char*)WAI_MALLOC(length + sizeof(".exe"));
      if (with_exe)
      {
        memcpy(with_exe, buffer, length);
        memcpy(with_exe + length, ".exe", sizeof(".exe"));
      }
    }

    if (with_exe && access(with_exe, X_OK) == 0)
    {
      resolved = with_exe;
      length += sizeof(".exe") - 1;
    }
    #endif

    if (length <= capacity)
    {
      memcpy(out, resolved, length);

      if (dirname_length)
      {
        int i;

        for (i = length - 1; i >= 0; --i)
        {
          if (out[i] == '/')
          {
            *dirname_length = i;
            break;
          }
        }
      }
    }
  }

#if defined(__CYGWIN__)
  if (with_exe != buffer)
    WAI_FREE(with_exe);
#endif

  return ok ? length : -1;
}

#if !defined(WAI_PROC_SELF_MAPS_RETRY)
#define WAI_PROC_SELF_MAPS_RETRY 5
#endif

#if !defined(WAI_PROC_SELF_MAPS)
#define WAI_PROC_SELF_MAPS "/proc/self/maps"
#endif

#if !defined(WAI_STRINGIZE)
#define WAI_STRINGIZE(s) WAI_STRINGIZE_(s)
#define WAI_STRINGIZE_(s) #s
#endif

#if defined(__ANDROID__) || defined(ANDROID)
#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>
#endif

WAI_NOINLINE WAI_FUNCSPEC
int WAI_PREFIX(getModulePath)(char* out, int capacity, int* dirname_length)
{
  int length = -1;
  FILE* maps = NULL;

  if (dirname_length)
    *dirname_length = -1;

  for (int r = 0; r < WAI_PROC_SELF_MAPS_RETRY; ++r)
  {
    maps = fopen(WAI_PROC_SELF_MAPS, "r");
    if (!maps)
      break;

    for (;;)
    {
      char buffer[128 + PATH_MAX];
      uintptr_t low, high;
      char perms[5];
      uint64_t offset;
      uint32_t major, minor, inode;
      char path[PATH_MAX + 1];

      if (!fgets(buffer, sizeof(buffer), maps))
        break;

      if (sscanf(buffer, "%" SCNxPTR "-%" SCNxPTR " %4s %" SCNx64 " %" SCNx32 ":%" SCNx32 " %" SCNu32 " %" WAI_STRINGIZE(PATH_MAX) "[^\n]\n", &low, &high, perms, &offset, &major, &minor, &inode, path) == 8)
      {
        void* _addr = WAI_RETURN_ADDRESS();
        uintptr_t addr = (uintptr_t)_addr;
        if (low <= addr && addr < high)
        {
          char* resolved;

          resolved = realpath(path, buffer);
          if (!resolved)
            break;

          length = (int)strlen(resolved);
#if defined(__ANDROID__) || defined(ANDROID)
          if (length > 4
              // `offset` is where the local file header search starts from
              // below, it must leave room for one (`offset - 30`)
              && offset >= 30
              &&buffer[length - 1] == 'k'
              &&buffer[length - 2] == 'p'
              &&buffer[length - 3] == 'a'
              &&buffer[length - 4] == '.')
          {
            int fd = open(resolved, O_RDONLY);
            if (fd == -1)
            {
              length = -1; // retry
              break;
            }

            char* begin = (char*)mmap(0, offset, PROT_READ, MAP_SHARED, fd, 0);
            if (begin == MAP_FAILED)
            {
              close(fd);
              length = -1; // retry
              break;
            }

            char* p = begin + offset - 30; // minimum size of local file header
            while (p >= begin) // scan backwards
            {
              const uint32_t signature = 0x04034b50UL;
              if (memcmp(p, &signature, sizeof(signature)) == 0) // local file header signature found
              {
                // ensure we can safely read the length field
                if (p + 26 + sizeof(uint16_t) <= begin + offset)
                {
                  uint16_t length_;
                  memcpy(&length_, p + 26, sizeof(length_));

                  // ensure we can safely read the filename
                  if (p + 30 + length_ <= begin + offset && length + 2 + length_ < (int)sizeof(buffer))
                  {
                    memcpy(&buffer[length], "!/", 2);
                    memcpy(&buffer[length + 2], p + 30, length_);
                    length += 2 + length_;
                  }
                }

                break;
              }

              --p;
            }

            munmap(begin, offset);
            close(fd);
          }
#endif
          if (length <= capacity)
          {
            memcpy(out, resolved, length);

            if (dirname_length)
            {
              int i;

              for (i = length - 1; i >= 0; --i)
              {
                if (out[i] == '/')
                {
                  *dirname_length = i;
                  break;
                }
              }
            }
          }

          break;
        }
      }
    }

    fclose(maps);
    maps = NULL;

    if (length != -1)
      break;
  }

  return length;
}

#elif defined(__sun)

#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <stdbool.h>
#include <dlfcn.h>

#if !defined(WAI_PROC_SELF_EXE)
#define WAI_PROC_SELF_EXE "/proc/self/path/a.out"
#endif

WAI_FUNCSPEC
int WAI_PREFIX(getExecutablePath)(char* out, int capacity, int* dirname_length)
{
  char buffer[PATH_MAX];
  char* resolved = NULL;
  int length = -1;
  bool ok;

  if (dirname_length)
    *dirname_length = -1;

  for (ok = false; !ok; ok = true)
  {
    resolved = realpath(WAI_PROC_SELF_EXE, buffer);
    if (!resolved)
      break;

    length = (int)strlen(resolved);
    if (length <= capacity)
    {
      memcpy(out, resolved, length);

      if (dirname_length)
      {
        int i;

        for (i = length - 1; i >= 0; --i)
        {
          if (out[i] == '/')
          {
            *dirname_length = i;
            break;
          }
        }
      }
    }
  }

  return ok ? length : -1;
}

WAI_NOINLINE WAI_FUNCSPEC
int WAI_PREFIX(getModulePath)(char* out, int capacity, int* dirname_length)
{
  char buffer[PATH_MAX];
  char* resolved = NULL;
  int length = -1;

  if (dirname_length)
    *dirname_length = -1;

  for(;;)
  {
    Dl_info info;

    if (dladdr(WAI_RETURN_ADDRESS(), &info))
    {
      resolved = realpath(info.dli_fname, buffer);
      if (!resolved)
        break;

      length = (int)strlen(resolved);
      if (length <= capacity)
      {
        memcpy(out, resolved, length);

        if (dirname_length)
        {
          int i;

          for (i = length - 1; i >= 0; --i)
          {
            if (out[i] == '/')
            {
              *dirname_length = i;
              break;
            }
          }
        }
      }
    }

    break;
  }

  return length;
}

#elif defined(__APPLE__)

#include <mach-o/dyld.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <dlfcn.h>
#include <stdbool.h>

WAI_FUNCSPEC
int WAI_PREFIX(getExecutablePath)(char* out, int capacity, int* dirname_length)
{
  char buffer1[PATH_MAX];
  char buffer2[PATH_MAX];
  char* path = buffer1;
  char* resolved = NULL;
  int length = -1;
  bool ok;

  if (dirname_length)
    *dirname_length = -1;

  for (ok = false; !ok; ok = true)
  {
    uint32_t size = (uint32_t)sizeof(buffer1);
    if (_NSGetExecutablePath(path, &size) == -1)
    {
      path = (char*)WAI_MALLOC(size);
      if (!path || (_NSGetExecutablePath(path, &size) == -1))
        break;
    }

    resolved = realpath(path, buffer2);
    if (!resolved)
      break;

    length = (int)strlen(resolved);
    if (length <= capacity)
    {
      memcpy(out, resolved, length);

      if (dirname_length)
      {
        int i;

        for (i = length - 1; i >= 0; --i)
        {
          if (out[i] == '/')
          {
            *dirname_length = i;
            break;
          }
        }
      }
    }
  }

  if (path != buffer1)
    WAI_FREE(path);

  return ok ? length : -1;
}

WAI_NOINLINE WAI_FUNCSPEC
int WAI_PREFIX(getModulePath)(char* out, int capacity, int* dirname_length)
{
  char buffer[PATH_MAX];
  char* resolved = NULL;
  int length = -1;

  if (dirname_length)
    *dirname_length = -1;

  for(;;)
  {
    Dl_info info;

    if (dladdr(WAI_RETURN_ADDRESS(), &info))
    {
      resolved = realpath(info.dli_fname, buffer);
      if (!resolved)
        break;

      length = (int)strlen(resolved);
      if (length <= capacity)
      {
        memcpy(out, resolved, length);

        if (dirname_length)
        {
          int i;

          for (i = length - 1; i >= 0; --i)
          {
            if (out[i] == '/')
            {
              *dirname_length = i;
              break;
            }
          }
        }
      }
    }

    break;
  }

  return length;
}

#elif defined(__QNXNTO__)

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dlfcn.h>
#include <stdbool.h>

#if !defined(WAI_PROC_SELF_EXE)
#define WAI_PROC_SELF_EXE "/proc/self/exefile"
#endif

WAI_FUNCSPEC
int WAI_PREFIX(getExecutablePath)(char* out, int capacity, int* dirname_length)
{
  char buffer1[PATH_MAX];
  char buffer2[PATH_MAX];
  char* resolved = NULL;
  FILE* self_exe = NULL;
  int length = -1;
  bool ok;

  if (dirname_length)
    *dirname_length = -1;

  for (ok = false; !ok; ok = true)
  {
    self_exe = fopen(WAI_PROC_SELF_EXE, "r");
    if (!self_exe)
      break;

    if (!fgets(buffer1, sizeof(buffer1), self_exe))
      break;

    resolved = realpath(buffer1, buffer2);
    if (!resolved)
      break;

    length = (int)strlen(resolved);
    if (length <= capacity)
    {
      memcpy(out, resolved, length);

      if (dirname_length)
      {
        int i;

        for (i = length - 1; i >= 0; --i)
        {
          if (out[i] == '/')
          {
            *dirname_length = i;
            break;
          }
        }
      }
    }
  }

  if (self_exe)
    fclose(self_exe);

  return ok ? length : -1;
}

WAI_NOINLINE WAI_FUNCSPEC
int WAI_PREFIX(getModulePath)(char* out, int capacity, int* dirname_length)
{
  char buffer[PATH_MAX];
  char* resolved = NULL;
  int length = -1;

  if (dirname_length)
    *dirname_length = -1;

  for(;;)
  {
    Dl_info info;

    if (dladdr(WAI_RETURN_ADDRESS(), &info))
    {
      resolved = realpath(info.dli_fname, buffer);
      if (!resolved)
        break;

      length = (int)strlen(resolved);
      if (length <= capacity)
      {
        memcpy(out, resolved, length);

        if (dirname_length)
        {
          int i;

          for (i = length - 1; i >= 0; --i)
          {
            if (out[i] == '/')
            {
              *dirname_length = i;
              break;
            }
          }
        }
      }
    }

    break;
  }

  return length;
}

#elif defined(__DragonFly__) || defined(__FreeBSD__) || \
      defined(__FreeBSD_kernel__) || defined(__NetBSD__) || defined(__OpenBSD__)

#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/sysctl.h>
#include <dlfcn.h>
#include <stdbool.h>

#if defined(__OpenBSD__)

#include <unistd.h>

WAI_FUNCSPEC
int WAI_PREFIX(getExecutablePath)(char* out, int capacity, int* dirname_length)
{
  char buffer1[4096];
  char buffer2[PATH_MAX];
  char buffer3[PATH_MAX];
  char** argv = (char**)buffer1;
  char* resolved = NULL;
  int length = -1;
  bool ok;

  if (dirname_length)
    *dirname_length = -1;

  for (ok = false; !ok; ok = true)
  {
    int mib[4] = { CTL_KERN, KERN_PROC_ARGS, getpid(), KERN_PROC_ARGV };
    size_t size;

    if (sysctl(mib, 4, NULL, &size, NULL, 0) != 0)
        break;

    if (size > sizeof(buffer1))
    {
      argv = (char**)WAI_MALLOC(size);
      if (!argv)
        break;
    }

    if (sysctl(mib, 4, argv, &size, NULL, 0) != 0)
        break;

    if (strchr(argv[0], '/'))
    {
      resolved = realpath(argv[0], buffer2);
      if (!resolved)
        break;
    }
    else
    {
      const char* PATH = getenv("PATH");
      if (!PATH)
        break;

      size_t argv0_length = strlen(argv[0]);

      const char* begin = PATH;
      while (1)
      {
        const char* separator = strchr(begin, ':');
        const char* end = separator ? separator : begin + strlen(begin);

        if (end - begin > 0)
        {
          if (*(end -1) == '/')
            --end;

          if (((end - begin) + 1 + argv0_length + 1) <= sizeof(buffer2))
          {
            memcpy(buffer2, begin, end - begin);
            buffer2[end - begin] = '/';
            memcpy(buffer2 + (end - begin) + 1, argv[0], argv0_length + 1);

            resolved = realpath(buffer2, buffer3);
            if (resolved)
              break;
          }
        }

        if (!separator)
          break;

        begin = ++separator;
      }

      if (!resolved)
        break;
    }

    length = (int)strlen(resolved);
    if (length <= capacity)
    {
      memcpy(out, resolved, length);

      if (dirname_length)
      {
        int i;

        for (i = length - 1; i >= 0; --i)
        {
          if (out[i] == '/')
          {
            *dirname_length = i;
            break;
          }
        }
      }
    }
  }

  if (argv != (char**)buffer1)
    WAI_FREE(argv);

  return ok ? length : -1;
}

#else

WAI_FUNCSPEC
int WAI_PREFIX(getExecutablePath)(char* out, int capacity, int* dirname_length)
{
  char buffer1[PATH_MAX];
  char buffer2[PATH_MAX];
  char* path = buffer1;
  char* resolved = NULL;
  int length = -1;
  bool ok;

  if (dirname_length)
    *dirname_length = -1;

  for (ok = false; !ok; ok = true)
  {
#if defined(__NetBSD__)
    int mib[4] = { CTL_KERN, KERN_PROC_ARGS, -1, KERN_PROC_PATHNAME };
#else
    int mib[4] = { CTL_KERN, KERN_PROC, KERN_PROC_PATHNAME, -1 };
#endif
    size_t size = sizeof(buffer1);

    if (sysctl(mib, 4, path, &size, NULL, 0) != 0)
        break;

    resolved = realpath(path, buffer2);
    if (!resolved)
      break;

    length = (int)strlen(resolved);
    if (length <= capacity)
    {
      memcpy(out, resolved, length);

      if (dirname_length)
      {
        int i;

        for (i = length - 1; i >= 0; --i)
        {
          if (out[i] == '/')
          {
            *dirname_length = i;
            break;
          }
        }
      }
    }
  }

  return ok ? length : -1;
}

#endif

WAI_NOINLINE WAI_FUNCSPEC
int WAI_PREFIX(getModulePath)(char* out, int capacity, int* dirname_length)
{
  char buffer[PATH_MAX];
  char* resolved = NULL;
  int length = -1;

  if (dirname_length)
    *dirname_length = -1;

  for(;;)
  {
    Dl_info info;

    if (dladdr(WAI_RETURN_ADDRESS(), &info))
    {
      resolved = realpath(info.dli_fname, buffer);
      if (!resolved)
        break;

      length = (int)strlen(resolved);
      if (length <= capacity)
      {
        memcpy(out, resolved, length);

        if (dirname_length)
        {
          int i;

          for (i = length - 1; i >= 0; --i)
          {
            if (out[i] == '/')
            {
              *dirname_length = i;
              break;
            }
          }
        }
      }
    }

    break;
  }

  return length;
}

#else

#error unsupported platform

#endif

#ifdef __cplusplus
}
#endif
