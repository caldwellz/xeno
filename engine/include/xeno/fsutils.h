/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#ifndef _XENO_FSUTILS_H_
#define _XENO_FSUTILS_H_

#include <stdint.h>
#include <SDL2/SDL_rwops.h>

void XENO_concatBasePath(const char* path, char** target);
uint32_t XENO_readFile(const char* inFilename, char** outData);
int XENO_initFilesystem(const char *argv0, const char** readPaths, size_t nReadPaths);
SDL_RWops* XENO_openSDLBuffer(const char* inFilename);
void XENO_closeSDLBuffer(SDL_RWops* rw);

#endif //_XENO_FSUTILS_H_
