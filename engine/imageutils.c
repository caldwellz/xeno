/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#include <xeno/platform.h>
#include <xeno/fsutils.h>
#include <xeno/imageutils.h>
#include <SDL2/SDL.h>

// Modified from original NXDK SDL sample
SDL_Texture * XENO_LoadBMPTexture(SDL_Renderer *renderer, const char *filename) {
  SDL_RWops *buffer = NULL;;
  SDL_Surface *surf = NULL;
  SDL_Texture *tex = NULL;

  // Buffer the image
  buffer = XENO_openSDLBuffer(filename);
  if (buffer == NULL) {
      SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Couldn't load %s: %s", filename, SDL_GetError());
      return 0;
  }

  // Load the image from the buffer
  surf = SDL_LoadBMP_RW(buffer, 0);
  if (surf == NULL) {
      SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Couldn't load %s: %s", filename, SDL_GetError());
      return 0;
  }

  XENO_closeSDLBuffer(buffer);

  /* Set transparent pixel as the pixel at (0,0) */
  // TODO: make this configurable
  SDL_SetColorKey(surf, 1, *(Uint32 *) surf->pixels);

  /* Create texture from the image */
  tex = SDL_CreateTextureFromSurface(renderer, surf);
  if (!tex) {
      SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Couldn't create texture: %s\n", SDL_GetError());
      SDL_FreeSurface(surf);
      return 0;
  }
  SDL_FreeSurface(surf);

  return tex;
}
