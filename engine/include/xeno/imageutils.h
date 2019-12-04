/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#ifndef _XENO_IMAGEUTILS_H_
#define _XENO_IMAGEUTILS_H_

#include <SDL2/SDL_render.h>

#ifdef __cplusplus
extern "C" {
#endif

SDL_Texture * XENO_LoadBMPTexture(SDL_Renderer *renderer, const char *filename);

#ifdef __cplusplus
}
#endif
#endif //_XENO_IMAGEUTILS_H_
