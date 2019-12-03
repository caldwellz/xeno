/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#include <xeno/platform.h>
#include <xeno/fsutils.h>
#include <xeno/imageutils.h>

#ifdef XENO_PLATFORM_NXDK
#include <hal/xbox.h>
#include <hal/video.h>
#include <windows.h>
#endif

#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include <SDL2/SDL.h>
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

  const char* mounts[] = {"override", "resource.zip"};
  if (!XENO_initFilesystem(argv0, mounts, 2)) {
    debugPrint("initFilesystem failed! PhysFS error msg: %s\n", PHYSFS_getErrorByCode(PHYSFS_getLastErrorCode()));
    debugSleep(3000);
    return 1;
  }

  window = SDL_CreateWindow(APP_TITLE,
    SDL_WINDOWPOS_UNDEFINED,
    SDL_WINDOWPOS_UNDEFINED,
    SCREEN_WIDTH, SCREEN_HEIGHT,
    SDL_WINDOW_SHOWN);
  if(window == NULL)
  {
      debugPrint( "Window could not be created!\n");
      SDL_Quit();
      return 1;
  }

  /* Create the renderer */
  renderer = SDL_CreateRenderer(window, -1, 0);
  if (!renderer) {
      SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Couldn't create renderer.\n");
      SDL_Quit();
      return 1;
  }

  SDL_SetRenderDrawColor(renderer, 0, 0, 0, 0xFF);
  SDL_RenderClear(renderer);

  // Load image
  sprite = XENO_LoadBMPTexture(renderer, "stone.bmp");

  /* Main render loop */
  int done = 0;
  SDL_Event event;
  SDL_Rect position;
  position.x = 30;
  position.y = 50;
  SDL_QueryTexture(sprite, NULL, NULL, &position.w, &position.h);
  while (!done) {
      /* Check for events */
      while (SDL_PollEvent(&event)) {
          switch (event.type) {
          case SDL_WINDOWEVENT:
              switch (event.window.event) {
              case SDL_WINDOWEVENT_EXPOSED:
//                  SDL_SetRenderDrawColor(renderer, 0xA0, 0xA0, 0xA0, 0xFF);
//                  SDL_RenderClear(renderer);
                  break;
              }
              break;
          case SDL_QUIT:
              done = 1;
              break;
          default:
              break;
          }
      }
      SDL_RenderClear(renderer);
      SDL_RenderCopy(renderer, sprite, NULL, &position);
      SDL_RenderPresent(renderer);
  }

debugPrint("main: end of code\n");
debugSleep(3000);
  SDL_Quit();
  PHYSFS_deinit(); // TODO: crashes on Xbox
debugPrint("main: finished PHYSFS_deinit()");
  debugSleep(3000);
  return 0;
}
