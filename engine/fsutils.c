/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#include <xeno/platform.h>
#include <xeno/fsutils.h>
#include <physfs.h>
#include <SDL2/SDL.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#ifdef XENO_PLATFORM_NXDK
  #include <hal/fileio.h>
#endif

void XENO_concatBasePath(const char* path, char** target) {
  // Using assert() instead of if() because we shouldn't
  // ever get NULL values during normal operation.
  assert(path && target);
  assert(PHYSFS_isInit());
  if (*target) {
    free(*target);
    *target = NULL;
  }

  const char* base = PHYSFS_getBaseDir();
  const char* dirSep = PHYSFS_getDirSeparator();
  size_t baseLen = strlen(base);
  size_t pathLen = strlen(path);
  size_t dirSepLen = strlen(dirSep);
  char * buffer = malloc(baseLen + pathLen + dirSepLen + 1);

  assert(buffer); // OOM errors aren't really something we can recover from...
  strcpy(buffer, base);
  buffer[baseLen] = '\0'; // Ensure there's a null terminator for strcat to find

  // Find whether the base ends in a dirSep
  // Otherwise, add a dirSep to the buffer
  size_t fromEnd;
  for (fromEnd = 1; (fromEnd <= dirSepLen) && (dirSep[dirSepLen - fromEnd - 1] == base[baseLen - fromEnd - 1]); ++fromEnd);
  if (fromEnd <= dirSepLen) { // Needs a path separator
    strcat(buffer, dirSep);
    buffer[baseLen] = '\0'; // Ensure there's another null terminator for strcat to find
  }

  strcat(buffer, path);
  *target = buffer;
}


uint32_t XENO_readFile(const char* inFilename, char** outData) {
  assert(PHYSFS_isInit());
  if (inFilename && outData) {
    int status = PHYSFS_exists(inFilename);
//debugClearScreen();
    if (status) {
      PHYSFS_File* myfile = PHYSFS_openRead(inFilename);
      assert(myfile != NULL);
      uint32_t file_size = PHYSFS_fileLength(myfile);

      char* buffer;
      buffer = malloc(file_size + 1);
      if (buffer) {
        uint32_t length_read = PHYSFS_read(myfile, buffer, 1, file_size);
        PHYSFS_close(myfile);

        if (length_read == file_size) {
          if (*outData)
            free(*outData); // TODO: Be careful; this can bite if we're passed a new but uninitialized pointer
          buffer[length_read] = '\0';
          (*outData) = buffer;
          return length_read;
        }
        else {
          free(buffer);
          debugPrint("readFile: File size and read length mismatch\n");
          return 0;
        }
      }
      else {
        debugPrint("readFile: Could not malloc memory\n");
        return 0;
      }
    }
    else {
      debugPrint("readFile: File '%s' not found\n", inFilename);
      return 0;
    }
  }

  return 0;
}


/** Initializes PhysFS filesystem access. */
int XENO_initFilesystem(const char *argv0, const char** readPaths, size_t nReadPaths) {
  int rv = PHYSFS_init(argv0);

  if (rv) {
    char* target = NULL;

#ifdef XENO_PLATFORM_NXDK
    debugPrint("Base directory: '%s' mounted at '%s'\n", getCurrentDirString(), PHYSFS_getBaseDir());
#endif

    if (readPaths && nReadPaths) {
      for (size_t n = 0; n < nReadPaths; ++n) {
        XENO_concatBasePath(readPaths[n], &target);
        rv = PHYSFS_mount(target, "/", 1);
        if (rv) {
          debugPrint("Using read path '%s'\n", target);
        } else {
          free(target);
          return rv;
        }
      }
    }

    if (target)
      free(target);
    target = PHYSFS_getPrefDir("Games", APP_TITLE);
    rv = PHYSFS_setWriteDir(target);
    debugPrint("Using write path '%s'\n", target);

#if 0
    XENO_concatBasePath("data", &target);
    PHYSFS_setWriteDir(target);
    free(target);
#endif
    debugPrint("Filesystem initialized\n");
  }

  return rv;
}


SDL_RWops* XENO_openSDLBuffer(const char* inFilename) {
  char* buffer = NULL;
  uint32_t size = XENO_readFile(inFilename, &buffer);

  if (buffer) {
    if (size) {
      SDL_RWops* rw = SDL_RWFromMem((void*) buffer, size);
      return rw;
    }
    else {
      free(buffer);
      return NULL;
    }
  }
  else
    return NULL;
}


void XENO_closeSDLBuffer(SDL_RWops* rw) {
  if (rw) {
    void* buf = rw->hidden.mem.base;
    SDL_RWclose(rw);
    if (buf)
      free(buf);
  }
}
