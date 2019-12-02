/*
 * Windows support routines for PhysicsFS.
 *
 * Please see the file LICENSE.txt in the source's root directory.
 *
 *  This file written by Ryan C. Gordon, and made sane by Gregory S. Read.
 */

#define __PHYSICSFS_INTERNAL__
#include "physfs_platforms.h"

#if PHYSFS_PLATFORM_NXDK

/* Forcibly disable UNICODE macro, since we manage this ourselves. */
#ifdef UNICODE
#undef UNICODE
#endif

#if defined(_MSC_VER) && !defined(_CRT_SECURE_NO_WARNINGS)
#define _CRT_SECURE_NO_WARNINGS 1
#endif

#define WIN32_LEAN_AND_MEAN 1
#include <windows.h>
#include <xboxkrnl/xboxkrnl.h>
#include <xboxrt/libc_extensions/stat.h>
#include <hal/fileio.h>
#include <hal/debug.h>

#if !defined(PHYSFS_NO_CDROM_SUPPORT)
#include <dbt.h>
#endif

#include <errno.h>
#include <ctype.h>
#include <time.h>

#ifdef allocator  /* apparently Windows 10 SDK conflicts here. */
#undef allocator
#endif

#include "physfs_internal.h"

/*
 * Users without the platform SDK don't have this defined.  The original docs
 *  for SetFilePointer() just said to compare with 0xFFFFFFFF, so this should
 *  work as desired.
 */
#define PHYSFS_INVALID_SET_FILE_POINTER  0xFFFFFFFF

/* just in case... */
#define PHYSFS_INVALID_FILE_ATTRIBUTES   0xFFFFFFFF

/* Not defined before the Vista SDK. */
#define PHYSFS_FILE_ATTRIBUTE_REPARSE_POINT 0x400
#define PHYSFS_IO_REPARSE_TAG_SYMLINK    0xA000000C


#define UTF8_TO_UNICODE_STACK(w_assignto, str) { \
    if (str == NULL) \
        w_assignto = NULL; \
    else { \
        const size_t len = (PHYSFS_uint64) ((strlen(str) + 1) * 2); \
        w_assignto = (char *) __PHYSFS_smallAlloc(len); \
        if (w_assignto != NULL) \
            PHYSFS_utf8ToUtf16(str, (PHYSFS_uint16 *) w_assignto, len); \
    } \
} \

/* Note this counts chars, not codepoints! */
static PHYSFS_uint64 wStrLen(const char *wstr)
{
    PHYSFS_uint64 len = 0;
    while (*(wstr++))
        len++;
    return len;
} /* wStrLen */

static char *unicodeToUtf8Heap(const char *w_str)
{
    char *retval = NULL;
    if (w_str != NULL)
    {
        void *ptr = NULL;
        const PHYSFS_uint64 len = (wStrLen(w_str) * 4) + 1;
        retval = allocator.Malloc(len);
        BAIL_IF(!retval, PHYSFS_ERR_OUT_OF_MEMORY, NULL);
        PHYSFS_utf8FromUtf16((const PHYSFS_uint16 *) w_str, retval, len);
        ptr = allocator.Realloc(retval, strlen(retval) + 1); /* shrink. */
        if (ptr != NULL)
            retval = (char *) ptr;
    } /* if */
    return retval;
} /* unicodeToUtf8Heap */


/* Some older APIs aren't in WinRT (only the "Ex" version, etc).
   Since non-WinRT might not have the "Ex" version, we tapdance to use
   the perfectly-fine-and-available-even-on-Win95 API on non-WinRT targets. */

static inline BOOL winInitializeCriticalSection(LPCRITICAL_SECTION lpcs)
{
    #ifdef PHYSFS_PLATFORM_WINRT
    return InitializeCriticalSectionEx(lpcs, 2000, 0);
    #else
    InitializeCriticalSection(lpcs);
    return TRUE;
    #endif
} /* winInitializeCriticalSection */

static inline HANDLE winCreateFileW(const char *wfname, const DWORD mode,
                                    const DWORD creation)
{
    const DWORD share = FILE_SHARE_READ | FILE_SHARE_WRITE;
    return CreateFileA(wfname, mode, share, NULL, creation,
                       FILE_ATTRIBUTE_NORMAL, NULL);
} /* winCreateFileW */

static BOOL winSetFilePointer(HANDLE h, const PHYSFS_sint64 pos,
                              PHYSFS_sint64 *_newpos, const DWORD whence)
{
    #ifdef PHYSFS_PLATFORM_WINRT
    LARGE_INTEGER lipos;
    LARGE_INTEGER linewpos;
    BOOL rc;
    lipos.QuadPart = (LONGLONG) pos;
    rc = SetFilePointerEx(h, lipos, &linewpos, whence);
    if (_newpos)
        *_newpos = (PHYSFS_sint64) linewpos.QuadPart;
    return rc;
    #else
    const LONG low = (LONG) (pos & 0xFFFFFFFF);
    LONG high = (LONG) ((pos >> 32) & 0xFFFFFFFF);
    const DWORD rc = SetFilePointer(h, low, &high, whence);
    /* 0xFFFFFFFF could be valid, so you have to check GetLastError too! */
    if (_newpos)
        *_newpos = ((PHYSFS_sint64) rc) | (((PHYSFS_sint64) high) << 32);
    if ((rc == PHYSFS_INVALID_SET_FILE_POINTER) && (GetLastError() != NO_ERROR))
        return FALSE;
    return TRUE;
    #endif
} /* winSetFilePointer */

static PHYSFS_sint64 winGetFileSize(HANDLE h)
{
    #ifdef PHYSFS_PLATFORM_WINRT
    FILE_STANDARD_INFO info;
    const BOOL rc = GetFileInformationByHandleEx(h, FileStandardInfo,
                                                 &info, sizeof (info));
    return rc ? (PHYSFS_sint64) info.EndOfFile.QuadPart : -1;
    #else
    DWORD high = 0;
    const DWORD rc = GetFileSize(h, &high);
    if ((rc == PHYSFS_INVALID_SET_FILE_POINTER) && (GetLastError() != NO_ERROR))
        return -1;
    return (PHYSFS_sint64) ((((PHYSFS_uint64) high) << 32) | rc);
    #endif
} /* winGetFileSize */


static PHYSFS_ErrorCode errcodeFromWinApiError(const DWORD err)
{
    /*
     * win32 error codes are sort of a tricky thing; Microsoft intentionally
     *  doesn't list which ones a given API might trigger, there are several
     *  with overlapping and unclear meanings...and there's 16 thousand of
     *  them in Windows 7. It looks like the ones we care about are in the
     *  first 500, but I can't say this list is perfect; we might miss
     *  important values or misinterpret others.
     *
     * Don't treat this list as anything other than a work in progress.
     */
    switch (err)
    {
        case ERROR_SUCCESS: return PHYSFS_ERR_OK;
        case ERROR_ACCESS_DENIED: return PHYSFS_ERR_PERMISSION;
        case ERROR_NETWORK_ACCESS_DENIED: return PHYSFS_ERR_PERMISSION;
        case ERROR_NOT_READY: return PHYSFS_ERR_IO;
        case ERROR_CRC: return PHYSFS_ERR_IO;
        case ERROR_SEEK: return PHYSFS_ERR_IO;
        case ERROR_SECTOR_NOT_FOUND: return PHYSFS_ERR_IO;
        case ERROR_NOT_DOS_DISK: return PHYSFS_ERR_IO;
        case ERROR_WRITE_FAULT: return PHYSFS_ERR_IO;
        case ERROR_READ_FAULT: return PHYSFS_ERR_IO;
        case ERROR_DEV_NOT_EXIST: return PHYSFS_ERR_IO;
        case ERROR_BUFFER_OVERFLOW: return PHYSFS_ERR_BAD_FILENAME;
        case ERROR_INVALID_NAME: return PHYSFS_ERR_BAD_FILENAME;
        case ERROR_BAD_PATHNAME: return PHYSFS_ERR_BAD_FILENAME;
        case ERROR_DIRECTORY: return PHYSFS_ERR_BAD_FILENAME;
        case ERROR_FILE_NOT_FOUND: return PHYSFS_ERR_NOT_FOUND;
        case ERROR_PATH_NOT_FOUND: return PHYSFS_ERR_NOT_FOUND;
        case ERROR_DELETE_PENDING: return PHYSFS_ERR_NOT_FOUND;
        case ERROR_INVALID_DRIVE: return PHYSFS_ERR_NOT_FOUND;
        case ERROR_HANDLE_DISK_FULL: return PHYSFS_ERR_NO_SPACE;
        case ERROR_DISK_FULL: return PHYSFS_ERR_NO_SPACE;
        case ERROR_WRITE_PROTECT: return PHYSFS_ERR_READ_ONLY;
        case ERROR_LOCK_VIOLATION: return PHYSFS_ERR_BUSY;
        case ERROR_SHARING_VIOLATION: return PHYSFS_ERR_BUSY;
        case ERROR_CURRENT_DIRECTORY: return PHYSFS_ERR_BUSY;
        case ERROR_DRIVE_LOCKED: return PHYSFS_ERR_BUSY;
        case ERROR_PATH_BUSY: return PHYSFS_ERR_BUSY;
        case ERROR_BUSY: return PHYSFS_ERR_BUSY;
        case ERROR_NOT_ENOUGH_MEMORY: return PHYSFS_ERR_OUT_OF_MEMORY;
        case ERROR_OUTOFMEMORY: return PHYSFS_ERR_OUT_OF_MEMORY;
        case ERROR_DIR_NOT_EMPTY: return PHYSFS_ERR_DIR_NOT_EMPTY;
        default: return PHYSFS_ERR_OS_ERROR;
    } /* switch */
} /* errcodeFromWinApiError */

static inline PHYSFS_ErrorCode errcodeFromWinApi(void)
{
    return errcodeFromWinApiError(GetLastError());
} /* errcodeFromWinApi */


static PHYSFS_ErrorCode errcodeFromErrnoError(const int err)
{
    switch (err)
    {
        case 0: return PHYSFS_ERR_OK;
        case EACCES: return PHYSFS_ERR_PERMISSION;
        case EPERM: return PHYSFS_ERR_PERMISSION;
//        case EDQUOT: return PHYSFS_ERR_NO_SPACE;
        case EIO: return PHYSFS_ERR_IO;
        case ELOOP: return PHYSFS_ERR_SYMLINK_LOOP;
        case EMLINK: return PHYSFS_ERR_NO_SPACE;
        case ENAMETOOLONG: return PHYSFS_ERR_BAD_FILENAME;
        case ENOENT: return PHYSFS_ERR_NOT_FOUND;
        case ENOSPC: return PHYSFS_ERR_NO_SPACE;
        case ENOTDIR: return PHYSFS_ERR_NOT_FOUND;
        case EISDIR: return PHYSFS_ERR_NOT_A_FILE;
        case EROFS: return PHYSFS_ERR_READ_ONLY;
        case ETXTBSY: return PHYSFS_ERR_BUSY;
        case EBUSY: return PHYSFS_ERR_BUSY;
        case ENOMEM: return PHYSFS_ERR_OUT_OF_MEMORY;
        case ENOTEMPTY: return PHYSFS_ERR_DIR_NOT_EMPTY;
        default: return PHYSFS_ERR_OS_ERROR;
    } /* switch */
} /* errcodeFromErrnoError */


static inline PHYSFS_ErrorCode errcodeFromErrno(void)
{
    return errcodeFromErrnoError(errno);
} /* errcodeFromErrno */


#if defined(PHYSFS_NO_CDROM_SUPPORT)
#define detectAvailableCDs(cb, data)
#define deinitCDThread()
#else
static HANDLE detectCDThreadHandle = NULL;
static HWND detectCDHwnd = NULL;
static volatile DWORD drivesWithMediaBitmap = 0;

typedef BOOL (WINAPI *fnSTEM)(DWORD, LPDWORD b);

static DWORD pollDiscDrives(void)
{
    /* Try to use SetThreadErrorMode(), which showed up in Windows 7. */
    HANDLE lib = LoadLibraryA("kernel32.dll");
    fnSTEM stem = NULL;
    char drive[4] = { 'x', ':', '\\', '\0' };
    DWORD oldErrorMode = 0;
    DWORD drives = 0;
    DWORD i;

    if (lib)
        stem = (fnSTEM) GetProcAddress(lib, "SetThreadErrorMode");

    if (stem)
        stem(SEM_FAILCRITICALERRORS, &oldErrorMode);
    else
        oldErrorMode = SetErrorMode(SEM_FAILCRITICALERRORS);
    
    /* Do detection. This may block if a disc is spinning up. */
    for (i = 'A'; i <= 'Z'; i++)
    {
        DWORD tmp = 0;
        drive[0] = (char) i;
        if (GetDriveTypeA(drive) != DRIVE_CDROM)
            continue;

        /* If this function succeeds, there's media in the drive */
        if (GetVolumeInformationA(drive, NULL, 0, NULL, NULL, &tmp, NULL, 0))
            drives |= (1 << (i - 'A'));
    } /* for */

    if (stem)
        stem(oldErrorMode, NULL);
    else
        SetErrorMode(oldErrorMode);

    if (lib)
        FreeLibrary(lib);

    return drives;
} /* pollDiscDrives */


static LRESULT CALLBACK detectCDWndProc(HWND hwnd, UINT msg,
                                        WPARAM wp, LPARAM lparam)
{
    PDEV_BROADCAST_HDR lpdb = (PDEV_BROADCAST_HDR) lparam;
    PDEV_BROADCAST_VOLUME lpdbv = (PDEV_BROADCAST_VOLUME) lparam;
    const int removed = (wp == DBT_DEVICEREMOVECOMPLETE);

    if (msg == WM_DESTROY)
        return 0;
    else if ((msg != WM_DEVICECHANGE) ||
             ((wp != DBT_DEVICEARRIVAL) && (wp != DBT_DEVICEREMOVECOMPLETE)) ||
             (lpdb->dbch_devicetype != DBT_DEVTYP_VOLUME) ||
             ((lpdbv->dbcv_flags & DBTF_MEDIA) == 0))
    {
        return DefWindowProcW(hwnd, msg, wp, lparam);
    } /* else if */

    if (removed)
        drivesWithMediaBitmap &= ~lpdbv->dbcv_unitmask;
    else
        drivesWithMediaBitmap |= lpdbv->dbcv_unitmask;

    return TRUE;
} /* detectCDWndProc */


static DWORD WINAPI detectCDThread(LPVOID arg)
{
    HANDLE initialDiscDetectionComplete = *((HANDLE *) arg);
    const char *classname = "PhysicsFSDetectCDCatcher";
    const char *winname = "PhysicsFSDetectCDMsgWindow";
    HINSTANCE hInstance = GetModuleHandleW(NULL);
    ATOM class_atom = 0;
    WNDCLASSEXA wce;
    MSG msg;

    memset(&wce, '\0', sizeof (wce));
    wce.cbSize = sizeof (wce);
    wce.lpfnWndProc = detectCDWndProc;
    wce.lpszClassName = classname;
    wce.hInstance = hInstance;
    class_atom = RegisterClassExA(&wce);
    if (class_atom == 0)
    {
        SetEvent(initialDiscDetectionComplete);  /* let main thread go on. */
        return 0;
    } /* if */

    detectCDHwnd = CreateWindowExA(0, classname, winname, WS_OVERLAPPEDWINDOW,
                        CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
                        CW_USEDEFAULT, HWND_DESKTOP, NULL, hInstance, NULL);

    if (detectCDHwnd == NULL)
    {
        SetEvent(initialDiscDetectionComplete);  /* let main thread go on. */
        UnregisterClassA(classname, hInstance);
        return 0;
    } /* if */

    /* We'll get events when discs come and go from now on. */

    /* Do initial detection, possibly blocking awhile... */
    drivesWithMediaBitmap = pollDiscDrives();

    SetEvent(initialDiscDetectionComplete);  /* let main thread go on. */

    do
    {
        const BOOL rc = GetMessageW(&msg, detectCDHwnd, 0, 0);
        if ((rc == 0) || (rc == -1))
            break;  /* don't care if WM_QUIT or error break this loop. */
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    } while (1);

    /* we've been asked to quit. */
    DestroyWindow(detectCDHwnd);
    UnregisterClassA(classname, hInstance);
    return 0;
} /* detectCDThread */

static void detectAvailableCDs(PHYSFS_StringCallback cb, void *data)
{
    char drive_str[4] = { 'x', ':', '\\', '\0' };
    DWORD drives = 0;
    DWORD i;

    /*
     * If you poll a drive while a user is inserting a disc, the OS will
     *  block this thread until the drive has spun up. So we swallow the risk
     *  once for initial detection, and spin a thread that will get device
     *  events thereafter, for apps that use this interface to poll for
     *  disc insertion.
     */
    if (!detectCDThreadHandle)
    {
        HANDLE initialDetectDone = CreateEvent(NULL, TRUE, FALSE, NULL);
        if (!initialDetectDone)
            return;  /* oh well. */

        detectCDThreadHandle = CreateThread(NULL, 0, detectCDThread,
                                            &initialDetectDone, 0, NULL);
        if (detectCDThreadHandle)
            WaitForSingleObject(initialDetectDone, INFINITE);
        CloseHandle(initialDetectDone);

        if (!detectCDThreadHandle)
            return;  /* oh well. */
    } /* if */

    drives = drivesWithMediaBitmap; /* whatever the thread has seen, we take. */
    for (i = 'A'; i <= 'Z'; i++)
    {
        if (drives & (1 << (i - 'A')))
        {
            drive_str[0] = (char) i;
            cb(data, drive_str);
        } /* if */
    } /* for */
} /* detectAvailableCDs */

static void deinitCDThread(void)
{
    if (detectCDThreadHandle)
    {
        if (detectCDHwnd)
            PostMessageW(detectCDHwnd, WM_QUIT, 0, 0);
        CloseHandle(detectCDThreadHandle);
        detectCDThreadHandle = NULL;
        drivesWithMediaBitmap = 0;
    } /* if */
} /* deinitCDThread */
#endif


void __PHYSFS_platformDetectAvailableCDs(PHYSFS_StringCallback cb, void *data)
{
    detectAvailableCDs(cb, data);
} /* __PHYSFS_platformDetectAvailableCDs */

char *__PHYSFS_platformCalcBaseDir(const char *argv0)
{
    //return getCurrentDirString();
    return "T:\\";
} /* __PHYSFS_platformCalcBaseDir */


char *__PHYSFS_platformCalcUserDir(void)
{
  return "E:\\";
} /* __PHYSFS_platformCalcUserDir */


char *__PHYSFS_platformCalcPrefDir(const char *org, const char *app)
{
  static char* dir = NULL;
  static size_t oldOrgLen = 0;
  static size_t oldAppLen = 0;

  if (org && app) {
    size_t orgLen = strlen(org);
    size_t appLen = strlen(app);
    char* base = __PHYSFS_platformCalcUserDir();
    size_t baseLen = strlen(base);

    if ((appLen > oldAppLen) || (orgLen > oldOrgLen)) {
      dir = allocator.Realloc(dir, baseLen + orgLen + appLen + 3);
      if (!dir)
        return NULL;
      oldAppLen = appLen;
    }

    strcpy(dir, base); // Assuming it already ends in a pathSep; TODO add one and adjust Realloc above if not
    strcat(dir, org);
    dir[baseLen+orgLen] = '\\';
    dir[baseLen+orgLen+1] = '\0';
    strcat(dir, app);
    dir[baseLen+orgLen+1+appLen] = '\\';
    dir[baseLen+orgLen+1+appLen+1] = '\0';
  }

  return dir;
} /* __PHYSFS_platformCalcPrefDir */


int __PHYSFS_platformInit(void)
{
    // See http://xboxdevwiki.net/Hard_Drive for partition objects and drive letters
    // Userdata partition
    OBJECT_STRING MountPath = RTL_CONSTANT_STRING("\\??\\E:");
    OBJECT_STRING DrivePath = RTL_CONSTANT_STRING("\\Device\\Harddisk0\\Partition1\\");

    NTSTATUS status = IoCreateSymbolicLink(&MountPath, &DrivePath);
    if (!NT_SUCCESS(status)) {
        debugPrint("PHYSFS_platformInit: Failed to mount '%s' at '%s' with status 0x%08x\n", DrivePath.Buffer, MountPath.Buffer, (unsigned int) status);
        BAIL(PHYSFS_ERR_IO, NULL);
    }
//    else
//      debugPrint("PHYSFS_platformInit: Mounted '%s' at '%s'\n", DrivePath.Buffer, MountPath.Buffer);

    // Mount the base dir (usually the disk or wherever you're running the game from) as T:
    MountPath.Buffer = "\\??\\T:";
    DrivePath.Buffer = getCurrentDirString();
    DrivePath.MaximumLength = DrivePath.Length = strlen(DrivePath.Buffer);

    status = IoCreateSymbolicLink(&MountPath, &DrivePath);
    if (!NT_SUCCESS(status)) {
        debugPrint("PHYSFS_platformInit: Failed to mount '%s' at '%s' with status 0x%08x\n", DrivePath.Buffer, MountPath.Buffer, (unsigned int) status);
        BAIL(PHYSFS_ERR_IO, NULL);
    }
//    else
//      debugPrint("PHYSFS_platformInit: Mounted '%s' at '%s'\n", DrivePath.Buffer, MountPath.Buffer);

    return 1;
} /* __PHYSFS_platformInit */


void __PHYSFS_platformDeinit(void)
{
    deinitCDThread();
} /* __PHYSFS_platformDeinit */


void *__PHYSFS_platformGetThreadID(void)
{
    return ( (void *) ((size_t) GetCurrentThreadId()) );
} /* __PHYSFS_platformGetThreadID */


PHYSFS_EnumerateCallbackResult __PHYSFS_platformEnumerate(const char *dirname,
                               PHYSFS_EnumerateCallback callback,
                               const char *origdir, void *callbackdata)
{
    PHYSFS_EnumerateCallbackResult retval = PHYSFS_ENUM_OK;
    HANDLE dir = INVALID_HANDLE_VALUE;
    WIN32_FIND_DATAA entw;
    size_t len = strlen(dirname);
    char *searchPath = NULL;
    char *wSearchPath = NULL;

    /* Allocate a new string for path, maybe '\\', "*", and NULL terminator */
    searchPath = (char *) __PHYSFS_smallAlloc(len + 3);
    BAIL_IF(!searchPath, PHYSFS_ERR_OUT_OF_MEMORY, PHYSFS_ENUM_ERROR);

    /* Copy current dirname */
    strcpy(searchPath, dirname);

    /* if there's no '\\' at the end of the path, stick one in there. */
    if (searchPath[len - 1] != '\\')
    {
        searchPath[len++] = '\\';
        searchPath[len] = '\0';
    } /* if */

    /* Append the "*" to the end of the string */
    strcat(searchPath, "*");

    //UTF8_TO_UNICODE_STACK(wSearchPath, searchPath);
    //__PHYSFS_smallFree(searchPath);
    //BAIL_IF_ERRPASS(!wSearchPath, PHYSFS_ENUM_ERROR);
    wSearchPath = searchPath;

    dir = FindFirstFile(wSearchPath, &entw);
    __PHYSFS_smallFree(wSearchPath);
    BAIL_IF(dir==INVALID_HANDLE_VALUE, errcodeFromWinApi(), PHYSFS_ENUM_ERROR);

    do
    {
        const char *fn = entw.cFileName;
        char *utf8;

        if (fn[0] == '.')  /* ignore "." and ".." */
        {
            if ((fn[1] == '\0') || ((fn[1] == '.') && (fn[2] == '\0')))
                continue;
        } /* if */

        utf8 = unicodeToUtf8Heap(fn);
        if (utf8 == NULL)
            retval = -1;
        else
        {
            retval = callback(callbackdata, origdir, utf8);
            allocator.Free(utf8);
            if (retval == PHYSFS_ENUM_ERROR)
                PHYSFS_setErrorCode(PHYSFS_ERR_APP_CALLBACK);
        } /* else */
    } while ((retval == PHYSFS_ENUM_OK) && (FindNextFile(dir, &entw) != 0));

    FindClose(dir);

    return retval;
} /* __PHYSFS_platformEnumerate */


int __PHYSFS_platformMkDir(const char *path)
{
    char *wpath;
    DWORD rc;
    //UTF8_TO_UNICODE_STACK(wpath, path);
    rc = CreateDirectory(path, NULL);
    //__PHYSFS_smallFree(wpath);
    BAIL_IF(rc == 0, errcodeFromWinApi(), 0);
    return 1;
} /* __PHYSFS_platformMkDir */


static HANDLE doOpen(const char *fname, DWORD mode, DWORD creation)
{
    HANDLE fileh;
    char *wfname;

// Converts the fname to just a drive letter, which obviously doesn't work. WTF?
//    UTF8_TO_UNICODE_STACK(wfname, fname);
//    BAIL_IF(!wfname, PHYSFS_ERR_OUT_OF_MEMORY, NULL);
    wfname = fname; // But using it directly works fine
    fileh = winCreateFileW(wfname, mode, creation);
//    __PHYSFS_smallFree(wfname);

    if (fileh == INVALID_HANDLE_VALUE)
        BAIL(errcodeFromWinApi(), INVALID_HANDLE_VALUE);

    return fileh;
} /* doOpen */


void *__PHYSFS_platformOpenRead(const char *filename)
{
    HANDLE h = doOpen(filename, GENERIC_READ, OPEN_EXISTING);
    return (h == INVALID_HANDLE_VALUE) ? NULL : (void *) h;
} /* __PHYSFS_platformOpenRead */


void *__PHYSFS_platformOpenWrite(const char *filename)
{
    HANDLE h = doOpen(filename, GENERIC_WRITE, CREATE_ALWAYS);
    return (h == INVALID_HANDLE_VALUE) ? NULL : (void *) h;
} /* __PHYSFS_platformOpenWrite */


void *__PHYSFS_platformOpenAppend(const char *filename)
{
    HANDLE h = doOpen(filename, GENERIC_WRITE, OPEN_ALWAYS);
    BAIL_IF_ERRPASS(h == INVALID_HANDLE_VALUE, NULL);

    if (!winSetFilePointer(h, 0, NULL, FILE_END))
    {
        const PHYSFS_ErrorCode err = errcodeFromWinApi();
        CloseHandle(h);
        BAIL(err, NULL);
    } /* if */

    return (void *) h;
} /* __PHYSFS_platformOpenAppend */


PHYSFS_sint64 __PHYSFS_platformRead(void *opaque, void *buf, PHYSFS_uint64 len)
{
    HANDLE h = (HANDLE) opaque;
    PHYSFS_sint64 totalRead = 0;

    if (!__PHYSFS_ui64FitsAddressSpace(len))
        BAIL(PHYSFS_ERR_INVALID_ARGUMENT, -1);

    while (len > 0)
    {
        const DWORD thislen = (len > 0xFFFFFFFF) ? 0xFFFFFFFF : (DWORD) len;
        DWORD numRead = 0;
        if (!ReadFile(h, buf, thislen, &numRead, NULL))
            BAIL(errcodeFromWinApi(), -1);
        len -= (PHYSFS_uint64) numRead;
        totalRead += (PHYSFS_sint64) numRead;
        if (numRead != thislen)
            break;
    } /* while */

    return totalRead;
} /* __PHYSFS_platformRead */


PHYSFS_sint64 __PHYSFS_platformWrite(void *opaque, const void *buffer,
                                     PHYSFS_uint64 len)
{
    HANDLE h = (HANDLE) opaque;
    PHYSFS_sint64 totalWritten = 0;

    if (!__PHYSFS_ui64FitsAddressSpace(len))
        BAIL(PHYSFS_ERR_INVALID_ARGUMENT, -1);

    while (len > 0)
    {
        const DWORD thislen = (len > 0xFFFFFFFF) ? 0xFFFFFFFF : (DWORD) len;
        DWORD numWritten = 0;
        if (!WriteFile(h, buffer, thislen, &numWritten, NULL))
            BAIL(errcodeFromWinApi(), -1);
        len -= (PHYSFS_uint64) numWritten;
        totalWritten += (PHYSFS_sint64) numWritten;
        if (numWritten != thislen)
            break;
    } /* while */

    return totalWritten;
} /* __PHYSFS_platformWrite */


int __PHYSFS_platformSeek(void *opaque, PHYSFS_uint64 pos)
{
    HANDLE h = (HANDLE) opaque;
    const PHYSFS_sint64 spos = (PHYSFS_sint64) pos;
    BAIL_IF(!winSetFilePointer(h,spos,NULL,FILE_BEGIN), errcodeFromWinApi(), 0);
    return 1;  /* No error occured */
} /* __PHYSFS_platformSeek */


PHYSFS_sint64 __PHYSFS_platformTell(void *opaque)
{
    HANDLE h = (HANDLE) opaque;
    PHYSFS_sint64 pos = 0;
    BAIL_IF(!winSetFilePointer(h,0,&pos,FILE_CURRENT), errcodeFromWinApi(), -1);
    return pos;
} /* __PHYSFS_platformTell */


PHYSFS_sint64 __PHYSFS_platformFileLength(void *opaque)
{
    HANDLE h = (HANDLE) opaque;
    const PHYSFS_sint64 retval = winGetFileSize(h);
    BAIL_IF(retval < 0, errcodeFromWinApi(), -1);
    return retval;
} /* __PHYSFS_platformFileLength */


int __PHYSFS_platformFlush(void *opaque)
{
    //HANDLE h = (HANDLE) opaque;
    //BAIL_IF(!FlushFileBuffers(h), errcodeFromWinApi(), 0);
    return PHYSFS_ERR_OK;
} /* __PHYSFS_platformFlush */


void __PHYSFS_platformClose(void *opaque)
{
    HANDLE h = (HANDLE) opaque;
    (void) CloseHandle(h); /* ignore errors. You should have flushed! */
} /* __PHYSFS_platformClose */


int __PHYSFS_platformDelete(const char *path)
{
    return (RemoveDirectory(path) ? PHYSFS_ERR_OK : errcodeFromWinApiError(DeleteFile(path)));
} /* __PHYSFS_platformDelete */


void *__PHYSFS_platformCreateMutex(void)
{
    LPCRITICAL_SECTION lpcs;
    lpcs = (LPCRITICAL_SECTION) allocator.Malloc(sizeof (CRITICAL_SECTION));
    BAIL_IF(!lpcs, PHYSFS_ERR_OUT_OF_MEMORY, NULL);

    if (!winInitializeCriticalSection(lpcs))
    {
        allocator.Free(lpcs);
        BAIL(errcodeFromWinApi(), NULL);
    } /* if */

    return lpcs;
} /* __PHYSFS_platformCreateMutex */


void __PHYSFS_platformDestroyMutex(void *mutex)
{
    DeleteCriticalSection((LPCRITICAL_SECTION) mutex);
    allocator.Free(mutex);
} /* __PHYSFS_platformDestroyMutex */


int __PHYSFS_platformGrabMutex(void *mutex)
{
    EnterCriticalSection((LPCRITICAL_SECTION) mutex);
    return 1;
} /* __PHYSFS_platformGrabMutex */


void __PHYSFS_platformReleaseMutex(void *mutex)
{
    LeaveCriticalSection((LPCRITICAL_SECTION) mutex);
} /* __PHYSFS_platformReleaseMutex */


/* check for symlinks. These exist in NTFS 3.1 (WinXP), even though
   they aren't really available to userspace before Vista. I wonder
   what would happen if you put an NTFS disk with a symlink on it
   into an XP machine, though; would this flag get set?
   NTFS symlinks are a form of "reparse point" (junction, volume mount,
   etc), so if the REPARSE_POINT attribute is set, check for the symlink
   tag thereafter. This assumes you already read in the file attributes. */
static int isSymlink(const char *wpath, const DWORD attr)
{
    WIN32_FIND_DATAA w32dw;
    HANDLE h;

    if ((attr & PHYSFS_FILE_ATTRIBUTE_REPARSE_POINT) == 0)
        return 0;  /* not a reparse point? Definitely not a symlink. */

    h = FindFirstFile(wpath, &w32dw);
    if (h == INVALID_HANDLE_VALUE)
        return 0;  /* ...maybe the file just vanished...? */

    FindClose(h);
    return (w32dw.dwReserved0 == PHYSFS_IO_REPARSE_TAG_SYMLINK);
} /* isSymlink */


int __PHYSFS_platformStat(const char *filename, PHYSFS_Stat *st, const int follow)
{
    // stat() / fstat() implementations provided by xboxrt/libc_extensions/stat.c
    struct stat statbuf;
    const int rc = stat(filename, &statbuf);
//    assert (rc == 0);
    BAIL_IF(rc == -1, errcodeFromErrno(), 0);
    if (S_ISREG(statbuf.st_mode))
    {
        st->filetype = PHYSFS_FILETYPE_REGULAR;
        st->filesize = statbuf.st_size;
    } /* if */

    else if(S_ISDIR(statbuf.st_mode))
    {
        st->filetype = PHYSFS_FILETYPE_DIRECTORY;
        st->filesize = 0;
    } /* else if */

    else if(S_ISLNK(statbuf.st_mode))
    {
        st->filetype = PHYSFS_FILETYPE_SYMLINK;
        st->filesize = 0;
    } /* else if */

    else
    {
        st->filetype = PHYSFS_FILETYPE_OTHER;
        st->filesize = statbuf.st_size;
    } /* else */

    st->modtime = statbuf.st_mtime;
    st->createtime = statbuf.st_ctime;
    st->accesstime = statbuf.st_atime;

    st->readonly = !(statbuf.st_mode & S_IWRITE);
    return 1;
} /* __PHYSFS_platformStat */

#endif  /* PHYSFS_PLATFORM_NXDK */

/* end of physfs_platform_windows.c ... */


