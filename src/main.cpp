/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#include <xeno/platform.h>
#include <xeno/fsutils.h>
#include <xeno/imageutils.h>

#include <SDL2/SDL.h>
#include <physfs.h>
#include <tinyxml2.h>
#include <irrlicht.h>
using namespace irr;

#ifdef XENO_PLATFORM_NXDK
#include <hal/xbox.h>
#include <hal/video.h>
#include <windows.h>
#endif

#include <string.h>
#include <stdlib.h>
#include <assert.h>

//Screen dimension constants
#ifdef XENO_PLATFORM_NXDK
extern "C" {
  const extern int SCREEN_WIDTH;
  const extern int SCREEN_HEIGHT;
}
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
/*
  SDL_Window *window;
  SDL_Renderer *renderer;
  SDL_Texture *sprite;

  SDL_LogSetPriority(SDL_LOG_CATEGORY_APPLICATION, SDL_LOG_PRIORITY_INFO);

  if (SDL_Init(SDL_INIT_VIDEO) < 0) {
    SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Couldn't initialize SDL.\n");
    return 1;
  }

  if (!SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "1"))
    SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Couldn't set scale sampling quality.\n");
*/
  const char* mounts[] = {"override", "resource.zip"};
  if (!XENO_initFilesystem(argv0, mounts, 2)) {
    debugPrint("initFilesystem failed! PhysFS error msg: %s\n", PHYSFS_getErrorByCode(PHYSFS_getLastErrorCode()));
    debugSleep(3000);
    return 1;
  }
debugPrint("Mounted filesystems\n");

  // Test Irrlicht renderer
  IrrlichtDevice *device = createDevice(video::EDT_BURNINGSVIDEO, core::dimension2d<u32>(SCREEN_WIDTH, SCREEN_HEIGHT), 32, false);
  if (device) {
    debugPrint("main: IrrlichtDevice created\n");
    video::IVideoDriver* driver = device->getVideoDriver();
    scene::ISceneManager* smgr = device->getSceneManager();
    debugPrint("main: device pointers grabbed\n");

    if (smgr->addCameraSceneNode(0, core::vector3df(0,-20,0), core::vector3df(0,0,0)))
      debugPrint("main: camera created\n");
    else
      debugPrint("main: failed to create camera\n");
    scene::IMeshSceneNode* cube = smgr->addCubeSceneNode(5.0);
    if (cube) {
      debugPrint("main: cube created\n");
      scene::ISceneNodeAnimator* anim = smgr->createRotationAnimator(core::vector3df(0.8f, 0, 0.8f));
      if (anim) {
        cube->addAnimator(anim);
        debugPrint("main: cube animator attached\n");
        anim->drop();

        // Begin main loop
        debugSleep(2000);
        u32 frames=0;
        while(device->run()) {
          driver->beginScene(true, true, video::SColor(0,255,255,255));
          smgr->drawAll();
          driver->endScene();

//          if (++frames==50) {
//            (s32)driver->getFPS();
//            frames=0;
//          }
        }
      }
      else
        debugPrint("main: failed to create animation\n");
    }
    else
      debugPrint("main: failed to create cube\n");
  }
  else
    debugPrint("main: failed to create IrrlichtDevice\n");

debugPrint("main: end of code\n");
debugSleep(3000);
  device->drop();
//  SDL_Quit();
  PHYSFS_deinit(); // TODO: crashes on Xbox
debugPrint("main: finished PHYSFS_deinit()\n");
  debugSleep(3000);
  return 0;
}
