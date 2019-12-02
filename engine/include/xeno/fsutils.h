/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#ifndef _XENO_FSUTILS_H_
#define _XENO_FSUTILS_H_

void XENO_concatBasePath(const char* path, char** target);
int XENO_readFile(const char* inFilename, char** outData);
int XENO_initFilesystem(const char *argv0, const char** readPaths, size_t nReadPaths);

#endif //_XENO_FSUTILS_H_
