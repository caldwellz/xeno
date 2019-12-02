/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#include <xeno/platform.h>
#include <xeno/fsutils.h>

#ifdef XENO_PLATFORM_NXDK
#include <hal/xbox.h>
#include <hal/video.h>
#include <windows.h>
#endif

#include <string.h>
#include <stdlib.h>
#include <assert.h>
//#include <SDL.h>
#include <physfs.h>

//Screen dimension constants
#ifdef XENO_PLATFORM_NXDK
const extern int SCREEN_WIDTH;
const extern int SCREEN_HEIGHT;
#else
const int SCREEN_WIDTH = 640;
const int SCREEN_HEIGHT = 480;
#endif


#ifdef XENO_PLATFORM_NXDK
int main(void) {  
  XVideoSetMode(SCREEN_WIDTH, SCREEN_HEIGHT, 32, REFRESH_DEFAULT);
  char *argv0 = NULL;
#else
int main(int argc, char* argv[]) {
  char *argv0 = argv[0];
#endif

  const char* mounts[] = {"override", "resource.zip"};
  if (!XENO_initFilesystem(argv0, mounts, 2)) {
    debugPrint("initFilesystem failed! PhysFS error msg: %s\n", PHYSFS_getErrorByCode(PHYSFS_getLastErrorCode()));
    debugSleep(3000);
    return 1;
  }

  char* data = NULL;
  unsigned int length = XENO_readFile("ziponlyfile.txt", &data);
  if (length) {
    debugPrint(data);
  }
  else {
    debugPrint("Failed to read ziponlyfile.txt");
    return 1;
  }

  length = XENO_readFile("testfile.txt", &data);
  if (length) {
    debugPrint(data);
  }
  else {
    debugPrint("Failed to read testfile.txt");
    return 1;
  }

debugPrint("main: end of code\n");
debugSleep(3000);
  PHYSFS_deinit(); // TODO: crashes on Xbox
debugPrint("main: finished PHYSFS_deinit()");
  debugSleep(3000);
  return 0;
}
