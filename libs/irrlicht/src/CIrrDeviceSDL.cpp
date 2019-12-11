// Copyright (C) 2002-2012 Nikolaus Gebhardt
// This file is part of the "Irrlicht Engine".
// For conditions of distribution and use, see copyright notice in irrlicht.h
//
// SDL2 upgrade
// Jonathan Frisch
// Sep 15, 2014

#include "IrrCompileConfig.h"

#ifdef _IRR_COMPILE_WITH_SDL_DEVICE_

#include "CIrrDeviceSDL.h"
#include "IEventReceiver.h"
#include "irrList.h"
#include "os.h"
#include "CTimer.h"
#include "irrString.h"
#include "Keycodes.h"
#include "COSOperator.h"
#include <stdio.h>
#include <stdlib.h>
#include "SIrrCreationParameters.h"
//#include "CEGLManager.h"
#include "SExposedVideoData.h"
#include "ISceneManager.h"
#include "ICameraSceneNode.h"
#include "CLogger.h"

#ifdef NXDK
  #include <hal/xbox.h>
  #include <hal/debug.h>
  #define SDL_Delay(x) XSleep(x)
#else
  #define debugPrint(...)
#endif

#ifdef _MSC_VER
#pragma comment(lib, "SDL2.lib")
#endif // _MSC_VER

#define A_TITLE "Irrlicht on SDL2"

namespace irr
{
	namespace video
	{
		#ifdef _IRR_COMPILE_WITH_DIRECT3D_8_
		//IVideoDriver* createDirectX8Driver(const irr::SIrrlichtCreationParameters &params, io::IFileSystem* io, HWND window);
		#endif

		#ifdef _IRR_COMPILE_WITH_DIRECT3D_9_
		//IVideoDriver* createDirectX9Driver(const irr::SIrrlichtCreationParameters &params, io::IFileSystem* io, HWND window);
		#endif

		#ifdef _IRR_COMPILE_WITH_OPENGL_
		IVideoDriver* createOpenGLDriver(const SIrrlichtCreationParameters &params, io::IFileSystem* io, CIrrDeviceSDL* device);
		#endif

		#ifdef _IRR_COMPILE_WITH_OGLES1_
		IVideoDriver* createOGLES1Driver(const SIrrlichtCreationParameters &params, io::IFileSystem* io, IContextManager* manager);
		#endif

		#ifdef _IRR_COMPILE_WITH_OGLES2_
		IVideoDriver* createOGLES2Driver(const SIrrlichtCreationParameters &params, io::IFileSystem* io, IContextManager* manager);
		#endif
	} // end namespace video
} // end namespace irr

namespace irr
{
    //! constructor
    CIrrDeviceSDL::CIrrDeviceSDL(const SIrrlichtCreationParameters &param)
            : CIrrDeviceStub(param), MouseX(0), MouseY(0), MouseButtonStates(0),
            Width(param.WindowSize.Width), Height(param.WindowSize.Height), Resizable(true),
            WindowHasFocus(false), WindowMinimized(false), window((SDL_Window*)param.WindowId),
            glContext(NULL), renderer(NULL), rendFlags(0), winFlags(SDL_WINDOW_RESIZABLE), filler()
    {
        Uint32 flags = SDL_INIT_VIDEO | SDL_INIT_EVENTS;
        #ifndef _IRR_XBOX_PLATFORM_
        flags |= SDL_INIT_TIMER | SDL_INIT_AUDIO;
        #endif

        #ifdef _DEBUG
        setDebugName("CIrrDeviceSDL");
        #endif

        #if defined(_IRR_COMPILE_WITH_JOYSTICK_EVENTS_)
        flags |= SDL_INIT_GAMECONTROLLER;
        SDL_SetHint(SDL_HINT_JOYSTICK_ALLOW_BACKGROUND_EVENTS, "1");
        SDL_SetHint(SDL_HINT_IDLE_TIMER_DISABLED, "1");
        SDL_SetHint(SDL_HINT_ACCELEROMETER_AS_JOYSTICK, "1");
        #endif // defined
        SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "linear");  // make the scaled rendering look smoother.
        SDL_SetHint(SDL_HINT_MOUSE_RELATIVE_MODE_WARP, "1");

        // Initialize SDL2... everything, because it's easier
        // Except timers and audio, because no support on NXDK; we replace the one used timer function (SDL_Delay) with Sleep()
        if(SDL_Init(flags) == 0)
        {
debugPrint("IrrDeviceSDL: SDL initialized\n");
            // create keymap
            createKeyMap();

            // create window
            if(createWindow())
            {
                SDL_VERSION(&Info.version); // initialize info structure with SDL version info
                #ifdef NXDK
                if (true) {
                #else
                if(SDL_GetWindowWMInfo(window, &Info)) { // the call returns true on success
                #endif
                    // success
                    const char *subsystem = "an unknown system!";
                    switch(Info.subsystem) {
                        case SDL_SYSWM_WINDOWS:
                            subsystem = "Microsoft Windows(TM)";
                            break;
                        case SDL_SYSWM_X11:
                            subsystem = "X Window System";
                            break;
                        #if SDL_VERSION_ATLEAST(2, 0, 3)
                        case SDL_SYSWM_WINRT:
                            subsystem = "WinRT";
                            break;
                        #endif
                        case SDL_SYSWM_DIRECTFB:
                            subsystem = "DirectFB";
                            break;
                        case SDL_SYSWM_COCOA:
                            subsystem = "Apple OS X";
                            break;
                        case SDL_SYSWM_UIKIT:
                            subsystem = "UIKit";
                            break;
                        #if SDL_VERSION_ATLEAST(2, 0, 2)
                        case SDL_SYSWM_WAYLAND:
                            subsystem = "Wayland";
                            break;
                        case SDL_SYSWM_MIR:
                            subsystem = "Mir";
                            break;
                        #endif
                        #if SDL_VERSION_ATLEAST(2, 0, 4)
                        case SDL_SYSWM_ANDROID:
                            subsystem = "Android";
                            break;
                        #endif
                        default:
                            break;
                    }
                    core::stringc sdlversion = "This program is running SDL version ";
                    sdlversion += Info.version.major;
                    sdlversion += ".";
                    sdlversion += Info.version.minor;
                    sdlversion += ".";
                    sdlversion += Info.version.patch;
                    sdlversion += " on ";
                    sdlversion += subsystem;
                    Operator = new COSOperator(sdlversion);
                    os::Printer::Logger->log(sdlversion.c_str(), ELL_INFORMATION);
                }
                else
                {
                    // call failed
                    char log[300];
                    sprintf(log, "Couldn't get window information: %s\n", SDL_GetError());
                    os::Printer::Logger->log(log, ELL_ERROR);
                }

                if(createRenderer())
                {
                    // create cursor control
                    CursorControl = new CCursorControlSDL2(this, window);

                    // create driver
                    createDriver();

                    if(VideoDriver) createGUIAndScene();
                    else
                    {
                        os::Printer::Logger->log("Unable to create video driver!", ELL_ERROR);
                        Close = true;
                    }
                }
                else
                {
                    char log[300];
                    sprintf(log, "Unable to create SDL renderer!  %s\n", SDL_GetError());
                    os::Printer::Logger->log(log, ELL_ERROR);
                    Close = true;
                }
            }
            else
            {
                char log[300];
                sprintf(log, "Unable to get or create SDL window!  %s\n", SDL_GetError());
                os::Printer::Logger->log(log, ELL_ERROR);
                Close = true;
            }
        }
        else
        {
            char log[300];
            sprintf(log, "Unable to initialize SDL!  %s\n", SDL_GetError());
            os::Printer::Logger->log(log, ELL_ERROR);
            debugPrint("IrrDeviceSDL: Unable to initialize SDL!  %s\n", SDL_GetError());
            Close = true;
        }
    }

    //! destructor
    CIrrDeviceSDL::~CIrrDeviceSDL()
    {
        #if defined(_IRR_COMPILE_WITH_JOYSTICK_EVENTS_)
        const u32 numJoysticks = Joysticks.size();
        for(u32 i = 0; i < numJoysticks; ++i) SDL_JoystickClose(Joysticks[i]);
        #endif
        if(glContext) SDL_GL_DeleteContext(glContext);
        if(renderer) SDL_DestroyRenderer(renderer);
        if(window) SDL_DestroyWindow(window);
        SDL_Quit();
    }

    bool CIrrDeviceSDL::createWindow()
    {
        if(!window)
        {
            if(CreationParams.DriverType == video::EDT_NULL) winFlags |= SDL_WINDOW_HIDDEN;
            else winFlags |= SDL_WINDOW_SHOWN;

            if(CreationParams.Fullscreen) winFlags |= SDL_WINDOW_FULLSCREEN_DESKTOP;

            int x = SDL_WINDOWPOS_CENTERED, y = SDL_WINDOWPOS_CENTERED, w = 640, h = 480;
/*            if(CreationParams.WindowPosition.X >= 0 && CreationParams.WindowPosition.Y >= 0)
            {
                x = CreationParams.WindowPosition.X;
                y = CreationParams.WindowPosition.Y;
            }
*/            if(CreationParams.WindowSize.Width > 0 && CreationParams.WindowSize.Height > 0)
            {
                w = CreationParams.WindowSize.Width;
                h = CreationParams.WindowSize.Height;
            }
            /*
            CreationParams.WindowId = window = SDL_CreateWindow(A_TITLE, x, y, w, h, winFlags);
            /*/
            if(CreationParams.DriverType == video::EDT_OPENGL
            /*   || CreationParams.DriverType == video::EDT_OGLES1
               || CreationParams.DriverType == video::EDT_OGLES2 */)
            {
                winFlags |= SDL_WINDOW_OPENGL;
                if(CreationParams.DriverType == video::EDT_OPENGL)
                {
                    os::Printer::Logger->log("EDT_OPENGL", ELL_DEBUG);
                    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_COMPATIBILITY);
                }
/*                else // if(CreationParams.DriverType & (video::EDT_OGLES1 | video::EDT_OGLES2))
                {
                    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES | SDL_GL_CONTEXT_PROFILE_COMPATIBILITY);
                    if(CreationParams.DriverType == video::EDT_OGLES1)
                    {
                        os::Printer::Logger->log("EDT_OGLES1", ELL_DEBUG);
                        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 1);
                        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
                    }
                    else
                    {
                        os::Printer::Logger->log("EDT_OGLES2", ELL_DEBUG);
                        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
                        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
                    }
                }
*/
                if(CreationParams.Bits < 24)
                {
                    SDL_GL_SetAttribute(SDL_GL_RED_SIZE, 4);
                    SDL_GL_SetAttribute(SDL_GL_GREEN_SIZE, 4);
                    SDL_GL_SetAttribute(SDL_GL_BLUE_SIZE, 4);
                    SDL_GL_SetAttribute(SDL_GL_ALPHA_SIZE, CreationParams.WithAlphaChannel ? 1 : 0);
                }
                else
                {
                    SDL_GL_SetAttribute(SDL_GL_RED_SIZE, 8);
                    SDL_GL_SetAttribute(SDL_GL_GREEN_SIZE, 8);
                    SDL_GL_SetAttribute(SDL_GL_BLUE_SIZE, 8);
                    SDL_GL_SetAttribute(SDL_GL_ALPHA_SIZE, CreationParams.WithAlphaChannel ? 8 : 0);
                }

                SDL_GL_SetAttribute(SDL_GL_BUFFER_SIZE, CreationParams.Bits);
                SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, CreationParams.ZBufferBits);

                if(CreationParams.Doublebuffer) SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
                if(CreationParams.Stereobuffer) SDL_GL_SetAttribute(SDL_GL_STEREO, 1);
                if(CreationParams.AntiAlias > 1)
                {
                    SDL_GL_SetAttribute(SDL_GL_MULTISAMPLEBUFFERS, 1);
                    SDL_GL_SetAttribute(SDL_GL_MULTISAMPLESAMPLES, CreationParams.AntiAlias);
                }

                window = SDL_CreateWindow(A_TITLE, x, y, w, h, winFlags);

                if(!window && CreationParams.Stereobuffer)
                {
                    SDL_GL_SetAttribute(SDL_GL_STEREO, 0);
                    window = SDL_CreateWindow(A_TITLE, x, y, w, h, winFlags);
                    if(window) os::Printer::Logger->log("StereoBuffer disabled due to lack of support!", ELL_WARNING);
                }
                if(!window && CreationParams.AntiAlias > 1)
                {
                    while(--CreationParams.AntiAlias > 1)
                    {
                        SDL_GL_SetAttribute(SDL_GL_MULTISAMPLESAMPLES, CreationParams.AntiAlias);
                        window = SDL_CreateWindow(A_TITLE, x, y, w, h, winFlags);
                        if(window) break;
                    }
                    if(!window)
                    {
                        SDL_GL_SetAttribute(SDL_GL_MULTISAMPLEBUFFERS, 0);
                        SDL_GL_SetAttribute(SDL_GL_MULTISAMPLESAMPLES, 0);
                        window = SDL_CreateWindow(A_TITLE, x, y, w, h, winFlags);
                        if(window) os::Printer::Logger->log("AntiAliasing disabled due to lack of support!", ELL_WARNING);
                    }
                }
                if(!window && CreationParams.Doublebuffer)
                {
                    // Try single buffer
                    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 0);
                    window = SDL_CreateWindow(A_TITLE, x, y, w, h, winFlags);
                    if(window) os::Printer::Logger->log("DoubleBuffer disabled due to lack of support!", ELL_WARNING);
                }
            }
            else window = SDL_CreateWindow(A_TITLE, x, y, w, h, winFlags);

            CreationParams.WindowId = window;
            if(!window)
            {
                os::Printer::Logger->log("Could not initialize Window!", ELL_ERROR);
                return false;
            }
        }
        return true;
    }

    bool CIrrDeviceSDL::createRenderer()
    {
        if(Close) return false;
debugPrint("IrrDeviceSDL: Requested driver type '%d'\n", CreationParams.DriverType);
        switch(CreationParams.DriverType)
        {
//            case video::EDT_OGLES1:
//            case video::EDT_OGLES2:
            case video::EDT_OPENGL:
                glContext = SDL_GL_CreateContext(window);
                if(glContext)
                {
                    SDL_GL_SetSwapInterval(CreationParams.Vsync ? 0 : 1);
                    SDL_GL_MakeCurrent(window, glContext);
                    return true;
                }
                return false;
            case video::EDT_BURNINGSVIDEO:
                #ifndef NXDK
                rendFlags |= SDL_RENDERER_ACCELERATED;
                #endif
                break;
            case video::EDT_SOFTWARE:
                rendFlags |= SDL_RENDERER_SOFTWARE;
                break;
            case video::EDT_NULL:
                rendFlags |= SDL_RENDERER_TARGETTEXTURE;
                break;
            default:
                return false;
        }
        if(CreationParams.Vsync) rendFlags |= SDL_RENDERER_PRESENTVSYNC;
debugPrint("IrrDeviceSDL: Switch handled the driver type\n");
        renderer = SDL_CreateRenderer(window, -1, rendFlags);

        if(renderer)
        {
            if(CreationParams.Fullscreen)
            {
                SDL_RenderSetLogicalSize(renderer, CreationParams.WindowSize.Width, CreationParams.WindowSize.Height);
            }
            SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
            SDL_RenderClear(renderer);
            SDL_RenderPresent(renderer);
            return true;
        }
        return false;
    }

    //! create the driver
    void CIrrDeviceSDL::createDriver()
    {
        switch(CreationParams.DriverType)
        {
            case video::EDT_OPENGL:
                #ifdef _IRR_COMPILE_WITH_OPENGL_
                VideoDriver = video::createOpenGLDriver(CreationParams, FileSystem, this);
                #else
                os::Printer::log("No OpenGL support compiled in.", ELL_ERROR);
                #endif
                break;
/*
            case video::EDT_OGLES1:
                #ifdef _IRR_COMPILE_WITH_OGLES1_
                ContextManager = new video::CEGLManager();
                ContextManager->initialize(CreationParams, filler);
                VideoDriver = video::createOGLES1Driver(CreationParams, FileSystem, ContextManager);
                #else
                os::Printer::log("No OGLES1 support compiled in.", ELL_ERROR);
                #endif
                break;

            case video::EDT_OGLES2:
                #ifdef _IRR_COMPILE_WITH_OGLES2_
                ContextManager = new video::CEGLManager();
                ContextManager->initialize(CreationParams, filler);
                VideoDriver = video::createOGLES2Driver(CreationParams, FileSystem, ContextManager);
                #else
                os::Printer::log("No OGLES2 support compiled in.", ELL_ERROR);
                #endif
                break;
*/
            case video::EDT_BURNINGSVIDEO:
                #ifdef _IRR_COMPILE_WITH_BURNINGSVIDEO_
                VideoDriver = video::createBurningVideoDriver(CreationParams, FileSystem, this);
                #else
                os::Printer::log("Burning's video driver was not compiled in.", ELL_ERROR);
                #endif
                break;

            case video::EDT_SOFTWARE:
                #ifdef _IRR_COMPILE_WITH_SOFTWARE_
                VideoDriver = video::createSoftwareDriver(CreationParams.WindowSize, CreationParams.Fullscreen, FileSystem, this);
                #else
                os::Printer::log("No Software driver support compiled in.", ELL_ERROR);
                #endif
                break;

            case video::EDT_NULL:
                VideoDriver = video::createNullDriver(FileSystem, CreationParams.WindowSize);
                break;

            case video::EDT_DIRECT3D8:
//                #ifdef _IRR_COMPILE_WITH_DIRECT3D_8_
//                VideoDriver = video::createDirectX8Driver(CreationParams, FileSystem, HWnd);
//                if(!VideoDriver)
//                {
//                    os::Printer::log("Could not create DIRECT3D8 Driver.", ELL_ERROR);
//                }
//                #else
                os::Printer::log("DIRECT3D8 Driver is not currently setup for SDL2.", ELL_ERROR);
//                #endif // _IRR_COMPILE_WITH_DIRECT3D_8_
                break;

            case video::EDT_DIRECT3D9:
//                #ifdef _IRR_COMPILE_WITH_DIRECT3D_9_
//                VideoDriver = video::createDirectX9Driver(CreationParams, FileSystem, HWnd);
//                if(!VideoDriver)
//                {
//                    os::Printer::log("Could not create DIRECT3D9 Driver.", ELL_ERROR);
//                }
//                #else
                os::Printer::log("DIRECT3D9 Driver is not currently setup for SDL2.", ELL_ERROR);
//                #endif // _IRR_COMPILE_WITH_DIRECT3D_9_
                break;

            default:
                os::Printer::log("Unable to create video driver of unknown type.", ELL_ERROR);
                break;
        }
    }

    //! runs the device. Returns false if device wants to be deleted
    bool CIrrDeviceSDL::run()
    {
        os::Timer::tick();
        SDL_Event SDL_event;
        bool kHandled = false;

        while(!Close && SDL_PollEvent(&SDL_event))
        {
            SEvent irrevent;
            switch(SDL_event.type)
            {
                case SDL_QUIT:
                    {
                        os::Printer::Logger->log("SDL_QUIT", ELL_DEBUG);
                        Close = true;
                        return false;
                    }
                    break;
                case SDL_APP_TERMINATING:
                    {
                        os::Printer::Logger->log("SDL_APP_TERMINATING", ELL_DEBUG);
                        Close = true;
                        return false;
                    }
                    break;
                case SDL_APP_LOWMEMORY:
                    {
                        os::Printer::Logger->log("SDL_APP_LOWMEMORY", ELL_WARNING);
                    }
                    break;
                case SDL_APP_WILLENTERBACKGROUND:
                    {
                        os::Printer::Logger->log("SDL_APP_WILLENTERBACKGROUND", ELL_DEBUG);
                        WindowHasFocus = false;
                        WindowMinimized = true;
                    }
                    break;
                case SDL_APP_DIDENTERBACKGROUND:
                    {
                        os::Printer::Logger->log("SDL_APP_DIDENTERBACKGROUND", ELL_DEBUG);
                        WindowHasFocus = false;
                        WindowMinimized = true;
                    }
                    break;
                case SDL_APP_WILLENTERFOREGROUND:
                    {
                        os::Printer::Logger->log("SDL_APP_WILLENTERFOREGROUND", ELL_DEBUG);
                        WindowHasFocus = true;
                        WindowMinimized = false;
                    }
                    break;
                case SDL_APP_DIDENTERFOREGROUND:
                    {
                        os::Printer::Logger->log("SDL_APP_DIDENTERFOREGROUND", ELL_DEBUG);
                        WindowHasFocus = true;
                        WindowMinimized = false;
                    }
                    break;
                case SDL_WINDOWEVENT:
                    {
                        os::Printer::Logger->log("SDL_WINDOWEVENT", ELL_DEBUG);
                        switch(SDL_event.window.event)
                        {
                            case SDL_WINDOWEVENT_SHOWN:
                                {
                                    char m[300];
                                    sprintf(m, "SDL_WINDOWEVENT_SHOWN::Window %d shown", SDL_event.window.windowID);
                                    os::Printer::Logger->log(m, ELL_DEBUG);
                                    WindowMinimized = false;
                                }
                                break;
                            case SDL_WINDOWEVENT_HIDDEN:
                                {
                                    char m[300];
                                    sprintf(m, "SDL_WINDOWEVENT_HIDDEN::Window %d hidden", SDL_event.window.windowID);
                                    os::Printer::Logger->log(m, ELL_DEBUG);
                                    WindowMinimized = true;
                                }
                                break;
                            case SDL_WINDOWEVENT_EXPOSED:
                                {
                                    char m[300];
                                    sprintf(m, "SDL_WINDOWEVENT_EXPOSED::Window %d exposed", SDL_event.window.windowID);
                                    os::Printer::Logger->log(m, ELL_DEBUG);
                                    WindowMinimized = false;
                                }
                                break;
                            case SDL_WINDOWEVENT_MOVED:
                                {
                                    char m[300];
                                    sprintf(m, "SDL_WINDOWEVENT_MOVED::Window %d moved to %d,%d",
                                            SDL_event.window.windowID, SDL_event.window.data1, SDL_event.window.data2);
                                    os::Printer::Logger->log(m, ELL_DEBUG);
                                }
                                break;
                            case SDL_WINDOWEVENT_RESIZED:
                                {
                                    char m[300];
                                    sprintf(m, "SDL_WINDOWEVENT_RESIZED::Window %d resized to %dx%d",
                                            SDL_event.window.windowID, SDL_event.window.data1, SDL_event.window.data2);
                                    os::Printer::Logger->log(m, ELL_DEBUG);
                                    updateWindowSize();
                                }
                                break;
                            case SDL_WINDOWEVENT_SIZE_CHANGED:
                                {
                                    char m[300];
                                    sprintf(m, "SDL_WINDOWEVENT_SIZE_CHANGED::Window %d size changed to %dx%d",
                                            SDL_event.window.windowID, SDL_event.window.data1, SDL_event.window.data2);
                                    os::Printer::Logger->log(m, ELL_DEBUG);
                                    updateWindowSize();
                                }
                                break;
                            case SDL_WINDOWEVENT_MINIMIZED:
                                {
                                    char m[300];
                                    sprintf(m, "SDL_WINDOWEVENT_MINIMIZED::Window %d minimized", SDL_event.window.windowID);
                                    os::Printer::Logger->log(m, ELL_DEBUG);
                                    WindowMinimized = true;
                                }
                                break;
                            case SDL_WINDOWEVENT_MAXIMIZED:
                                {
                                    char m[300];
                                    sprintf(m, "SDL_WINDOWEVENT_MAXIMIZED::Window %d maximized", SDL_event.window.windowID);
                                    os::Printer::Logger->log(m, ELL_DEBUG);
                                    WindowMinimized = false;
                                }
                                break;
                            case SDL_WINDOWEVENT_RESTORED:
                                {
                                    char m[300];
                                    sprintf(m, "SDL_WINDOWEVENT_RESTORED::Window %d restored", SDL_event.window.windowID);
                                    os::Printer::Logger->log(m, ELL_DEBUG);
                                    WindowMinimized = false;
                                }
                                break;
                            case SDL_WINDOWEVENT_ENTER:
                                {
                                    char m[300];
                                    sprintf(m, "SDL_WINDOWEVENT_ENTER::Mouse entered window %d", SDL_event.window.windowID);
                                    os::Printer::Logger->log(m, ELL_DEBUG);
                                }
                                break;
                            case SDL_WINDOWEVENT_LEAVE:
                                {
                                    char m[300];
                                    sprintf(m, "SDL_WINDOWEVENT_LEAVE::Mouse left window %d", SDL_event.window.windowID);
                                    os::Printer::Logger->log(m, ELL_DEBUG);
                                }
                                break;
                            case SDL_WINDOWEVENT_FOCUS_GAINED:
                                {
                                    char m[300];
                                    sprintf(m, "SDL_WINDOWEVENT_LEAVE::Window %d gained keyboard focus", SDL_event.window.windowID);
                                    os::Printer::Logger->log(m, ELL_DEBUG);
                                    WindowHasFocus = true;
                                }
                                break;
                            case SDL_WINDOWEVENT_FOCUS_LOST:
                                {
                                    char m[300];
                                    sprintf(m, "SDL_WINDOWEVENT_LEAVE::Window %d lost keyboard focus", SDL_event.window.windowID);
                                    os::Printer::Logger->log(m, ELL_DEBUG);
                                    WindowHasFocus = false;
                                }
                                break;
                            case SDL_WINDOWEVENT_CLOSE:
                                {
                                    char m[300];
                                    sprintf(m, "SDL_WINDOWEVENT_CLOSE::Window %d closed", SDL_event.window.windowID);
                                    os::Printer::Logger->log(m, ELL_DEBUG);
                                    Close = true;
                                    return false;
                                }
                                break;
                            default:
                                {
                                    char m[300];
                                    sprintf(m, "Window %d got unknown event %d", SDL_event.window.windowID, SDL_event.window.event);
                                    os::Printer::Logger->log(m, ELL_DEBUG);
                                }
                                break;
                        }
                    }
                    break;
                case SDL_SYSWMEVENT:
                    {
                        os::Printer::Logger->log("SDL_SYSWMEVENT", ELL_DEBUG);
                    }
                    break;
                case SDL_KEYDOWN:
                    {
                        os::Printer::Logger->log("SDL_KEYDOWN", ELL_DEBUG);
                        kHandled = true;

                        SKeyMap key(SDL_event.key.keysym.sym, KEY_UNKNOWN);
                        s32 idx = KeyMap.binary_search(key);

                        if(idx > -1) key.IrrKey = KeyMap[idx].IrrKey;

                        irrevent.EventType = irr::EET_KEY_INPUT_EVENT;
                        irrevent.KeyInput.Char = (wchar_t)SDL_event.key.keysym.sym;
                        irrevent.KeyInput.Key = (EKEY_CODE)key.IrrKey;
                        irrevent.KeyInput.PressedDown = true;
                        irrevent.KeyInput.Shift = (SDL_event.key.keysym.mod & KMOD_SHIFT) != 0;
                        irrevent.KeyInput.Control = (SDL_event.key.keysym.mod & KMOD_CTRL) != 0;

                        postEventFromUser(irrevent);

                    }
                    break;
                case SDL_KEYUP:
                    {
                        os::Printer::Logger->log("SDL_KEYUP", ELL_DEBUG);
                        SKeyMap key(SDL_event.key.keysym.sym, KEY_UNKNOWN);
                        s32 idx = KeyMap.binary_search(key);

                        if(idx > -1) key.IrrKey = KeyMap[idx].IrrKey;

                        irrevent.EventType = irr::EET_KEY_INPUT_EVENT;
                        irrevent.KeyInput.Char = (wchar_t)SDL_event.key.keysym.sym;
                        irrevent.KeyInput.Key = (EKEY_CODE)key.IrrKey;
                        irrevent.KeyInput.PressedDown = false;
                        irrevent.KeyInput.Shift = (SDL_event.key.keysym.mod & KMOD_SHIFT) != 0;
                        irrevent.KeyInput.Control = (SDL_event.key.keysym.mod & KMOD_CTRL) != 0;

                        postEventFromUser(irrevent);

                        wchar_t test = (wchar_t)irrevent.KeyInput.Key;
                        os::Printer::Logger->log(&test, ELL_DEBUG);
                        kHandled = false;
                    }
                    break;
                case SDL_TEXTEDITING:
                    {
                        os::Printer::Logger->log("SDL_TEXTEDITING", ELL_DEBUG);
                    }
                    break;
                case SDL_TEXTINPUT:
                    {
                        os::Printer::Logger->log("SDL_TEXTINPUT", ELL_DEBUG);
                        if(!kHandled)
                        {
                            wchar_t wc = SDL_event.text.text[0];
//                            core::utf8ToWchar(&SDL_event.text.text[0], &wc, 1);

                            SKeyMap key((s32)wc, KEY_UNKNOWN);
                            s32 idx = KeyMap.binary_search(key);

                            if(idx > -1) key.IrrKey = KeyMap[idx].IrrKey;

                            irrevent.EventType = irr::EET_KEY_INPUT_EVENT;
                            irrevent.KeyInput.Char = wc;
                            irrevent.KeyInput.Key = (EKEY_CODE)key.IrrKey;
                            irrevent.KeyInput.PressedDown = true;
                            irrevent.KeyInput.Control = false;
                            irrevent.KeyInput.Shift = false;

                            postEventFromUser(irrevent);
                            irrevent.KeyInput.PressedDown = false;
                            postEventFromUser(irrevent);

                            os::Printer::Logger->log(&irrevent.KeyInput.Char, ELL_DEBUG);
                        }
                    }
                    break;
                case SDL_MOUSEMOTION:
                    {
                        os::Printer::Logger->log("SDL_MOUSEMOTION", ELL_DEBUG);
                        if(SDL_event.motion.which != SDL_TOUCH_MOUSEID)
                        {
                            irrevent.EventType = irr::EET_MOUSE_INPUT_EVENT;
                            irrevent.MouseInput.Event = irr::EMIE_MOUSE_MOVED;
                            MouseX = irrevent.MouseInput.X = SDL_event.motion.x;
                            MouseY = irrevent.MouseInput.Y = SDL_event.motion.y;
                            MouseButtonStates = SDL_event.motion.state & SDL_BUTTON_LMASK ? irr::EMBSM_LEFT : 0;
                            MouseButtonStates |= SDL_event.motion.state & SDL_BUTTON_MMASK ? irr::EMBSM_MIDDLE : 0;
                            MouseButtonStates |= SDL_event.motion.state & SDL_BUTTON_RMASK ? irr::EMBSM_RIGHT : 0;
                            MouseButtonStates |= SDL_event.motion.state & SDL_BUTTON_X1MASK ? irr::EMBSM_EXTRA1 : 0;
                            MouseButtonStates |= SDL_event.motion.state & SDL_BUTTON_X2MASK ? irr:: EMBSM_EXTRA2 : 0;
                            irrevent.MouseInput.ButtonStates = MouseButtonStates;
                            irrevent.MouseInput.Control = (SDL_event.key.keysym.mod & KMOD_CTRL) != 0;
                            irrevent.MouseInput.Shift = (SDL_event.key.keysym.mod & KMOD_SHIFT) != 0;

                            postEventFromUser(irrevent);
                        }
                    }
                    break;
                case SDL_MOUSEBUTTONDOWN:
                    {
                        os::Printer::Logger->log("SDL_MOUSEBUTTONDOWN", ELL_DEBUG);
                        if(SDL_event.button.which != SDL_TOUCH_MOUSEID)
                        {
                            irrevent.EventType = irr::EET_MOUSE_INPUT_EVENT;
                            MouseX = irrevent.MouseInput.X = SDL_event.button.x;
                            MouseY = irrevent.MouseInput.Y = SDL_event.button.y;
                            irrevent.MouseInput.ButtonStates = MouseButtonStates;
                            irrevent.MouseInput.Control = (SDL_event.key.keysym.mod & KMOD_CTRL) != 0;
                            irrevent.MouseInput.Shift = (SDL_event.key.keysym.mod & KMOD_SHIFT) != 0;

                            switch(SDL_event.button.button)
                            {
                                case SDL_BUTTON_LEFT:
                                    os::Printer::Logger->log("SDL_BUTTON_LEFT", ELL_DEBUG);
                                    switch(SDL_event.button.clicks)
                                    {
                                        case 1:
                                            os::Printer::Logger->log("single click", ELL_DEBUG);
                                            irrevent.MouseInput.Event = irr::EMIE_LMOUSE_PRESSED_DOWN;
                                            break;
                                        case 2:
                                            os::Printer::Logger->log("double click", ELL_DEBUG);
                                            irrevent.MouseInput.Event = irr::EMIE_LMOUSE_DOUBLE_CLICK;
                                            break;
                                        case 3:
                                            os::Printer::Logger->log("triple click", ELL_DEBUG);
                                            irrevent.MouseInput.Event = irr::EMIE_LMOUSE_TRIPLE_CLICK;
                                            break;
                                        default:
                                            os::Printer::Logger->log("unhandled click count", ELL_DEBUG);
                                            break;
                                    }
                                    break;
                                case SDL_BUTTON_RIGHT:
                                    os::Printer::Logger->log("SDL_BUTTON_RIGHT", ELL_DEBUG);
                                    switch(SDL_event.button.clicks)
                                    {
                                        case 1:
                                            os::Printer::Logger->log("single click", ELL_DEBUG);
                                            irrevent.MouseInput.Event = irr::EMIE_RMOUSE_PRESSED_DOWN;
                                            break;
                                        case 2:
                                            os::Printer::Logger->log("double click", ELL_DEBUG);
                                            irrevent.MouseInput.Event = irr::EMIE_RMOUSE_DOUBLE_CLICK;
                                            break;
                                        case 3:
                                            os::Printer::Logger->log("triple click", ELL_DEBUG);
                                            irrevent.MouseInput.Event = irr::EMIE_RMOUSE_TRIPLE_CLICK;
                                            break;
                                        default:
                                            os::Printer::Logger->log("unhandled click count", ELL_DEBUG);
                                            break;
                                    }
                                    break;
                                case SDL_BUTTON_MIDDLE:
                                    os::Printer::Logger->log("SDL_BUTTON_MIDDLE", ELL_DEBUG);
                                    switch(SDL_event.button.clicks)
                                    {
                                        case 1:
                                            os::Printer::Logger->log("single click", ELL_DEBUG);
                                            irrevent.MouseInput.Event = irr::EMIE_MMOUSE_PRESSED_DOWN;
                                            break;
                                        case 2:
                                            os::Printer::Logger->log("double click", ELL_DEBUG);
                                            irrevent.MouseInput.Event = irr::EMIE_MMOUSE_DOUBLE_CLICK;
                                            break;
                                        case 3:
                                            os::Printer::Logger->log("triple click", ELL_DEBUG);
                                            irrevent.MouseInput.Event = irr::EMIE_MMOUSE_TRIPLE_CLICK;
                                            break;
                                        default:
                                            os::Printer::Logger->log("unhandled click count", ELL_DEBUG);
                                            break;
                                    }
                                    break;
                                default:
                                    os::Printer::Logger->log("unhandled sdl mouse button down event", ELL_DEBUG);
                                    break;
                            }

                            postEventFromUser(irrevent);
                        }
                    }
                    break;
                case SDL_MOUSEBUTTONUP:
                    {
                        os::Printer::Logger->log("SDL_MOUSEBUTTONUP", ELL_DEBUG);
                        if(SDL_event.button.which != SDL_TOUCH_MOUSEID)
                        {
                            irrevent.EventType = irr::EET_MOUSE_INPUT_EVENT;
                            MouseX = irrevent.MouseInput.X = SDL_event.button.x;
                            MouseY = irrevent.MouseInput.Y = SDL_event.button.y;
                            irrevent.MouseInput.ButtonStates = MouseButtonStates;
                            irrevent.MouseInput.Control = (SDL_event.key.keysym.mod & KMOD_CTRL) != 0;
                            irrevent.MouseInput.Shift = (SDL_event.key.keysym.mod & KMOD_SHIFT) != 0;

                            switch(SDL_event.button.button)
                            {
                                case SDL_BUTTON_LEFT:
                                    os::Printer::Logger->log("SDL_BUTTON_LEFT", ELL_DEBUG);
                                    irrevent.MouseInput.Event = irr::EMIE_LMOUSE_LEFT_UP;
                                    break;
                                case SDL_BUTTON_RIGHT:
                                    os::Printer::Logger->log("SDL_BUTTON_RIGHT", ELL_DEBUG);
                                    irrevent.MouseInput.Event = irr::EMIE_RMOUSE_LEFT_UP;
                                    break;
                                case SDL_BUTTON_MIDDLE:
                                    os::Printer::Logger->log("SDL_BUTTON_MIDDLE", ELL_DEBUG);
                                    irrevent.MouseInput.Event = irr::EMIE_MMOUSE_LEFT_UP;
                                    break;
                                default:
                                    os::Printer::Logger->log("unhandled sdl mouse button up event", ELL_DEBUG);
                                    break;
                            }

                            postEventFromUser(irrevent);
                        }
                    }
                    break;
                case SDL_MOUSEWHEEL:
                    {
                        os::Printer::Logger->log("SDL_MOUSEWHEEL", ELL_DEBUG);
                        if(SDL_event.button.which != SDL_TOUCH_MOUSEID)
                        {
                            irrevent.EventType = irr::EET_MOUSE_INPUT_EVENT;
                            MouseX = irrevent.MouseInput.X = SDL_event.button.x;
                            MouseY = irrevent.MouseInput.Y = SDL_event.button.y;
                            irrevent.MouseInput.ButtonStates = MouseButtonStates;
                            irrevent.MouseInput.Event = irr::EMIE_MOUSE_WHEEL;
                            irrevent.MouseInput.Wheel = (f32)SDL_event.wheel.x;
                            irrevent.MouseInput.Control = (SDL_event.key.keysym.mod & KMOD_CTRL) != 0;
                            irrevent.MouseInput.Shift = (SDL_event.key.keysym.mod & KMOD_SHIFT) != 0;

                            postEventFromUser(irrevent);
                        }
                    }
                    break;
                #ifdef _IRR_COMPILE_WITH_JOYSTICK_EVENTS_
                case SDL_JOYAXISMOTION:
                    {
                        SDL_JoyAxisEvent jaxis = SDL_event.jaxis;
                        char logString[256];
                        sprintf(logString, "SDL_JOYAXISMOTION: axis %d, value %d", jaxis.axis, jaxis.value);
                        os::Printer::Logger->log(logString, ELL_DEBUG);
                    }
                    break;
                case SDL_JOYBALLMOTION:
                    {
                        os::Printer::Logger->log("SDL_JOYBALLMOTION", ELL_DEBUG);
                    }
                    break;
                case SDL_JOYHATMOTION:
                    {
                        os::Printer::Logger->log("SDL_JOYHATMOTION", ELL_DEBUG);
                    }
                    break;
                case SDL_JOYBUTTONDOWN:
                    {
                        os::Printer::Logger->log("SDL_JOYBUTTONDOWN", ELL_DEBUG);
                    }
                    break;
                case SDL_JOYBUTTONUP:
                    {
                        os::Printer::Logger->log("SDL_JOYBUTTONUP", ELL_DEBUG);
                    }
                    break;
                case SDL_JOYDEVICEADDED:
                    {
                        os::Printer::Logger->log("SDL_JOYDEVICEADDED", ELL_DEBUG);
                        int j = SDL_event.jdevice.which;

                        SDL_Joystick* joy = SDL_JoystickOpen(j);
                        if(j < Joysticks.size()) Joysticks.erase(j);
                        Joysticks.insert(joy, j);

                        SJoystickInfo info;
                        info.Joystick = j;
                        info.Axes = SDL_JoystickNumAxes(joy);
                        info.Buttons = SDL_JoystickNumButtons(joy);
                        info.Name = SDL_JoystickName(joy);
                        info.PovHat = (SDL_JoystickNumHats(joy) > 0) ? SJoystickInfo::POV_HAT_PRESENT : SJoystickInfo::POV_HAT_ABSENT;
                        if(joyinfo)
                        {
                            if(j < joyinfo->size()) joyinfo->erase(j);
                            joyinfo->insert(info, j);
                        }

                        char logString[256];
                        sprintf(logString, "Found joystick %d ('%s'): %d axes, %d buttons, %s POV hat",
                                j, info.Name.c_str(), info.Axes, info.Buttons, info.PovHat == SJoystickInfo::POV_HAT_PRESENT ? "with" : "no");
                        os::Printer::log(logString, ELL_INFORMATION);
                    }
                    break;
                case SDL_JOYDEVICEREMOVED:
                    {
                        os::Printer::Logger->log("SDL_JOYDEVICEREMOVED", ELL_DEBUG);
                        int j = SDL_event.jdevice.which;

                        char logString[256];
                        sprintf(logString, "Lost joystick %d ('%s')", j, SDL_JoystickName(Joysticks[j]));
                        os::Printer::log(logString, ELL_INFORMATION);

                        SDL_JoystickClose(Joysticks[SDL_event.jdevice.which]);
                    }
                    break;
                case SDL_CONTROLLERAXISMOTION:
                    {
                        os::Printer::Logger->log("SDL_CONTROLLERAXISMOTION", ELL_DEBUG);
                    }
                    break;
                case SDL_CONTROLLERBUTTONDOWN:
                    {
                        os::Printer::Logger->log("SDL_CONTROLLERBUTTONDOWN", ELL_DEBUG);
                    }
                    break;
                case SDL_CONTROLLERBUTTONUP:
                    {
                        os::Printer::Logger->log("SDL_CONTROLLERBUTTONUP", ELL_DEBUG);
                    }
                    break;
                case SDL_CONTROLLERDEVICEADDED:
                    {
                        os::Printer::Logger->log("SDL_CONTROLLERDEVICEADDED", ELL_DEBUG);
                    }
                    break;
                case SDL_CONTROLLERDEVICEREMOVED:
                    {
                        os::Printer::Logger->log("SDL_CONTROLLERDEVICEREMOVED", ELL_DEBUG);
                    }
                    break;
                case SDL_CONTROLLERDEVICEREMAPPED:
                    {
                        os::Printer::Logger->log("SDL_CONTROLLERDEVICEREMAPPED", ELL_DEBUG);
                    }
                    break;
                #endif // _IRR_COMPILE_WITH_JOYSTICK_EVENTS_
/*                case SDL_FINGERDOWN:
                    {
                        os::Printer::Logger->log("SDL_FINGERDOWN", ELL_DEBUG);
                        irrevent.EventType = irr::EET_TOUCH_INPUT_EVENT;
                        irrevent.TouchInput.ID = SDL_event.tfinger.fingerId;
                        irrevent.TouchInput.X = SDL_event.tfinger.x * (s32)Width;
                        irrevent.TouchInput.Y = SDL_event.tfinger.y * (s32)Height;
                        irrevent.TouchInput.Event = irr::ETIE_PRESSED_DOWN;

                        postEventFromUser(irrevent);
                    }
                    break;
                case SDL_FINGERUP:
                    {
                        os::Printer::Logger->log("SDL_FINGERUP", ELL_DEBUG);
                        irrevent.EventType = irr::EET_TOUCH_INPUT_EVENT;
                        irrevent.TouchInput.ID = SDL_event.tfinger.fingerId;
                        irrevent.TouchInput.X = SDL_event.tfinger.x * (s32)Width;
                        irrevent.TouchInput.Y = SDL_event.tfinger.y * (s32)Height;
                        irrevent.TouchInput.Event = irr::ETIE_LEFT_UP;

                        postEventFromUser(irrevent);
                    }
                    break;
                case SDL_FINGERMOTION:
                    {
                        os::Printer::Logger->log("SDL_FINGERMOTION", ELL_DEBUG);
                        irrevent.EventType = irr::EET_TOUCH_INPUT_EVENT;
                        irrevent.TouchInput.ID = SDL_event.tfinger.fingerId;
                        irrevent.TouchInput.X = SDL_event.tfinger.x * (s32)Width;
                        irrevent.TouchInput.Y = SDL_event.tfinger.y * (s32)Height;
                        irrevent.TouchInput.Event = irr::ETIE_MOVED;

                        postEventFromUser(irrevent);
                    }
                    break;
*/                case SDL_DOLLARGESTURE:
                    {
                        os::Printer::Logger->log("SDL_DOLLARGESTURE", ELL_DEBUG);
                    }
                    break;
                case SDL_DOLLARRECORD:
                    {
                        os::Printer::Logger->log("SDL_DOLLARRECORD", ELL_DEBUG);
                    }
                    break;
                case SDL_MULTIGESTURE:
                    {
                        os::Printer::Logger->log("SDL_MULTIGESTURE", ELL_DEBUG);
                    }
                    break;
                case SDL_CLIPBOARDUPDATE:
                    {
                        os::Printer::Logger->log("SDL_CLIPBOARDUPDATE", ELL_DEBUG);
                    }
                    break;
                case SDL_DROPFILE:
                    {
                        os::Printer::Logger->log("SDL_DROPFILE", ELL_DEBUG);
                    }
                    break;
                case SDL_RENDER_TARGETS_RESET:
                    {
                        os::Printer::Logger->log("SDL_RENDER_TARGETS_RESET", ELL_DEBUG);
                    }
                    break;
                case SDL_USEREVENT:
                    {
                        os::Printer::Logger->log("SDL_USEREVENT", ELL_DEBUG);
                        irrevent.EventType = irr::EET_USER_EVENT;
                        irrevent.UserEvent.UserData1 = *(reinterpret_cast<s32*>(&SDL_event.user.data1));
                        irrevent.UserEvent.UserData2 = *(reinterpret_cast<s32*>(&SDL_event.user.data2));

                        postEventFromUser(irrevent);
                    }
                    break;
                default:
                    {
                        char m[300];
                        sprintf(m, "unknown/unhandled event %d", SDL_event.type);
                        os::Printer::Logger->log(m, ELL_DEBUG);
                    }
                    break;
            } // end switch

        } // end while

        #if defined(_IRR_COMPILE_WITH_JOYSTICK_EVENTS_)
        // TODO: Check if the multiple open/close calls are too expensive, then
        // open/close in the constructor/destructor instead

        // we'll always send joystick input events...
        SEvent joyevent;
        joyevent.EventType = EET_JOYSTICK_INPUT_EVENT;
        for(u32 i = 0; i < Joysticks.size(); ++i)
        {
            SDL_Joystick* joystick = Joysticks[i];
            int j;
            // query all buttons
            const int numButtons = core::min_(SDL_JoystickNumButtons(joystick), 32);
            joyevent.JoystickEvent.ButtonStates = 0;
            for(j = 0; j < numButtons; ++j)
                joyevent.JoystickEvent.ButtonStates |= (SDL_JoystickGetButton(joystick, j)<<j);

            // query all axes, already in correct range
            const int numAxes = core::min_(SDL_JoystickNumAxes(joystick), (int)SEvent::SJoystickEvent::NUMBER_OF_AXES);
            joyevent.JoystickEvent.Axis[SEvent::SJoystickEvent::AXIS_X] = 0;
            joyevent.JoystickEvent.Axis[SEvent::SJoystickEvent::AXIS_Y] = 0;
            joyevent.JoystickEvent.Axis[SEvent::SJoystickEvent::AXIS_Z] = 0;
            joyevent.JoystickEvent.Axis[SEvent::SJoystickEvent::AXIS_R] = 0;
            joyevent.JoystickEvent.Axis[SEvent::SJoystickEvent::AXIS_U] = 0;
            joyevent.JoystickEvent.Axis[SEvent::SJoystickEvent::AXIS_V] = 0;
            for(j = 0; j < numAxes; ++j) joyevent.JoystickEvent.Axis[j] = SDL_JoystickGetAxis(joystick, j);

            // we can only query one hat, SDL only supports 8 directions
            joyevent.JoystickEvent.POV = 65535; // center/none
            if(SDL_JoystickNumHats(joystick) > 0)
            {
                switch(SDL_JoystickGetHat(joystick, 0))
                {
                    case SDL_HAT_UP:
                        joyevent.JoystickEvent.POV = 0;
                        break;
                    case SDL_HAT_RIGHTUP:
                        joyevent.JoystickEvent.POV = 4500;
                        break;
                    case SDL_HAT_RIGHT:
                        joyevent.JoystickEvent.POV = 9000;
                        break;
                    case SDL_HAT_RIGHTDOWN:
                        joyevent.JoystickEvent.POV = 13500;
                        break;
                    case SDL_HAT_DOWN:
                        joyevent.JoystickEvent.POV = 18000;
                        break;
                    case SDL_HAT_LEFTDOWN:
                        joyevent.JoystickEvent.POV = 22500;
                        break;
                    case SDL_HAT_LEFT:
                        joyevent.JoystickEvent.POV = 27000;
                        break;
                    case SDL_HAT_LEFTUP:
                        joyevent.JoystickEvent.POV = 31500;
                        break;
                    case SDL_HAT_CENTERED:
                    default:
                        break;
                }
            }

            // we map the number directly
            joyevent.JoystickEvent.Joystick = static_cast<u8>(i);
            // now post the event
            postEventFromUser(joyevent);
            // and close the joystick
        }
        #endif
        return !Close;
    }

    void CIrrDeviceSDL::updateWindowSize()
    {
        int w, h;
        if(glContext) SDL_GL_GetDrawableSize(window, &w, &h);
        else SDL_GetWindowSize(window, &w, &h);
        if(w > 0) Width = (u32)w;
        if(h > 0) Height = (u32)h;
        VideoDriver->OnResize(core::dimension2du(Width, Height));
        char l[200];
        sprintf(l, "Viewport size is %dx%d", Width, Height);
        os::Printer::Logger->log(l, ELL_DEBUG);

        if(SceneManager)
        {
            scene::ICameraSceneNode* cam = SceneManager->getActiveCamera();
            if(cam)
            {
                cam->setAspectRatio((f32)Width / (f32)Height);

                sprintf(l, "Camera aspect ratio is %.4f", cam->getAspectRatio());
                os::Printer::Logger->log(l, ELL_DEBUG);
            }
        }
    }

    //! Sets a new event receiver to receive events
    void CIrrDeviceSDL::setEventReceiver(IEventReceiver* receiver)
    {
        if(!sdlEventReceiver) sdlEventReceiver = new SDL2EventReceiver();
        sdlEventReceiver->actual = receiver;
        CIrrDeviceStub::setEventReceiver(sdlEventReceiver);
    }

    //! Activate any joysticks, and generate events for them.
    bool CIrrDeviceSDL::activateJoysticks(core::array<SJoystickInfo> &joystickInfo)
    {
        #if defined(_IRR_COMPILE_WITH_JOYSTICK_EVENTS_)
        joystickInfo.clear();

        int joystick = 0;
        for(; joystick < Joysticks.size(); joystick++)
        {
            SJoystickInfo info;

            info.Joystick = joystick;
            info.Axes = SDL_JoystickNumAxes(Joysticks[joystick]);
            info.Buttons = SDL_JoystickNumButtons(Joysticks[joystick]);
            info.Name = SDL_JoystickName(Joysticks[joystick]);
            info.PovHat = (SDL_JoystickNumHats(Joysticks[joystick]) > 0)
                            ? SJoystickInfo::POV_HAT_PRESENT : SJoystickInfo::POV_HAT_ABSENT;

            joystickInfo.push_back(info);

            char logString[256];
            sprintf(logString, "Found joystick %d ('%s'): %d axes, %d buttons, %s POV hat",
                    joystick, info.Name.c_str(), info.Axes, info.Buttons, info.PovHat == SJoystickInfo::POV_HAT_PRESENT ? "with" : "no");
            os::Printer::log(logString, ELL_INFORMATION);
        }
        joyinfo = &joystickInfo;

        return true;
        #else
        return false;
        #endif // _IRR_COMPILE_WITH_JOYSTICK_EVENTS_
    }

    //! pause execution temporarily
    void CIrrDeviceSDL::yield()
    {
        sleep(0, false);
    }

    //! pause execution for a specified time
    void CIrrDeviceSDL::sleep(u32 timeMs, bool pauseTimer)
    {
        const bool wasStopped = Timer ? Timer->isStopped() : true;
        if(pauseTimer && !wasStopped) Timer->stop();
        SDL_Delay(timeMs);
        if(pauseTimer && !wasStopped) Timer->start();
    }
 
    //! sets the caption of the window
    void CIrrDeviceSDL::setWindowCaption(const wchar_t* text)
    {
        core::stringc textc = text;
        SDL_SetWindowTitle(window, textc.c_str());
    }

    //! presents a surface in the client area
    bool CIrrDeviceSDL::present(video::IImage* irrSurface, void* windowId, core::recti* srcClip)
    {
        SDL_Surface *sdlSurface = SDL_CreateRGBSurfaceFrom(irrSurface->lock(),
                irrSurface->getDimension().Width, irrSurface->getDimension().Height,
                irrSurface->getBitsPerPixel(), irrSurface->getPitch(), irrSurface->getRedMask(),
                irrSurface->getGreenMask(), irrSurface->getBlueMask(), irrSurface->getAlphaMask());
        if(!sdlSurface) return false;
        //SDL_SetAlpha(sdlSurface, 0, 0); // what is this doing?
        //SDL_SetColorKey(sdlSurface, 0, 0); // what is this doing?
        sdlSurface->format->BitsPerPixel = irrSurface->getBitsPerPixel();
        sdlSurface->format->BytesPerPixel = irrSurface->getBytesPerPixel();
        switch(irrSurface->getColorFormat())
        {
            case video::ECF_R8G8B8:
            case video::ECF_A8R8G8B8:
                sdlSurface->format->Rloss = 0;
                sdlSurface->format->Gloss = 0;
                sdlSurface->format->Bloss = 0;
                sdlSurface->format->Rshift = 16;
                sdlSurface->format->Gshift = 8;
                sdlSurface->format->Bshift = 0;
                if(irrSurface->getColorFormat() == video::ECF_R8G8B8)
                {
                    sdlSurface->format->Aloss = 8;
                    sdlSurface->format->Ashift = 32;
                }
                else
                {
                    sdlSurface->format->Aloss = 0;
                    sdlSurface->format->Ashift = 24;
                }
                break;
            case video::ECF_R5G6B5:
                sdlSurface->format->Rloss = 3;
                sdlSurface->format->Gloss = 2;
                sdlSurface->format->Bloss = 3;
                sdlSurface->format->Aloss = 8;
                sdlSurface->format->Rshift = 11;
                sdlSurface->format->Gshift = 5;
                sdlSurface->format->Bshift = 0;
                sdlSurface->format->Ashift = 16;
                break;
            case video::ECF_A1R5G5B5:
                sdlSurface->format->Rloss = 3;
                sdlSurface->format->Gloss = 3;
                sdlSurface->format->Bloss = 3;
                sdlSurface->format->Aloss = 7;
                sdlSurface->format->Rshift = 10;
                sdlSurface->format->Gshift = 5;
                sdlSurface->format->Bshift = 0;
                sdlSurface->format->Ashift = 15;
                break;
            default:
                break;
        }

        if(renderer)
        {
            SDL_Texture* sdlTexture = SDL_CreateTextureFromSurface(renderer, sdlSurface);
            if(sdlTexture)
            {
                SDL_UpdateTexture(sdlTexture, NULL, sdlSurface->pixels, sdlSurface->pitch);
                if(srcClip)
                {
                    SDL_Rect sdlsrcClip;
                    sdlsrcClip.x = srcClip->UpperLeftCorner.X;
                    sdlsrcClip.y = srcClip->UpperLeftCorner.Y;
                    sdlsrcClip.w = srcClip->getWidth();
                    sdlsrcClip.h = srcClip->getHeight();
                    SDL_RenderCopy(renderer, sdlTexture, &sdlsrcClip, NULL);
                }
                else SDL_RenderCopy(renderer, sdlTexture, NULL, NULL);
            }
            else
            {
                os::Printer::log("Error creating SDL_Texture: %s", SDL_GetError());
            }
            SDL_RenderPresent(renderer);
            SDL_DestroyTexture(sdlTexture);
        }

        SDL_FreeSurface(sdlSurface);
        irrSurface->unlock();
        return true;//sdlTexture != NULL;
    }

    //! notifies the device that it should close itself
    void CIrrDeviceSDL::closeDevice()
    {
        Close = true;
    }

    //! \return Pointer to a list with all video modes supported
    video::IVideoModeList* CIrrDeviceSDL::getVideoModeList()
    {
        if(!VideoModeList->getVideoModeCount())
        {
            // enumerate video modes.
            SDL_DisplayMode current;
            for(int i = 0; i < SDL_GetNumVideoDisplays(); ++i)
            {
                if(SDL_GetCurrentDisplayMode(i, &current) == 0)
                {
                    VideoModeList->addMode(core::dimension2du(current.w, current.h), SDL_BITSPERPIXEL(current.format));
                }
                else
                {
                    char msg[300];
                    sprintf(msg, "Could not get display mode for video display #%d: %s", i, SDL_GetError());
                    os::Printer::Logger->log(msg, ELL_DEBUG);
                }
            }
        }

        return VideoModeList;
    }

    //! Sets if the window should be resizable in windowed mode.
    void CIrrDeviceSDL::setResizable(bool resize)
    {
        // meh... leave it always resizable - let SDL take care of stuff.
    }

    //! Minimizes window if possible
    void CIrrDeviceSDL::minimizeWindow()
    {
        SDL_MinimizeWindow(window);
        WindowMinimized = true;
        WindowHasFocus = false;
    }

    //! Maximize window
    void CIrrDeviceSDL::maximizeWindow()
    {
        SDL_MaximizeWindow(window);
        WindowMinimized = false;
        WindowHasFocus = true;
    }

    //! Get the position of this window on screen
    core::position2di CIrrDeviceSDL::getWindowPosition()
    {
        int x, y;
        SDL_GetWindowPosition(window, &x, &y);
        return core::position2di(s32(x), s32(y));
    }

    //! Restore original window size
    void CIrrDeviceSDL::restoreWindow()
    {
        SDL_RestoreWindow(window);
        WindowMinimized = false;
    }

    //! returns if window is active. if not, nothing need to be drawn
    bool CIrrDeviceSDL::isWindowActive() const
    {
        return WindowHasFocus || SDL_GetMouseFocus() == window || SDL_GetKeyboardFocus() == window;// (WindowHasFocus && !WindowMinimized);
    }

    //! returns if window has focus.
    bool CIrrDeviceSDL::isWindowFocused() const
    {
        return WindowHasFocus || SDL_GetMouseFocus() == window || SDL_GetKeyboardFocus() == window;
    }

    //! returns if window is minimized.
    bool CIrrDeviceSDL::isWindowMinimized() const
    {
        return WindowMinimized;
    }

    //! Set the current Gamma Value for the Display
    bool CIrrDeviceSDL::setGammaRamp(f32 red, f32 green, f32 blue, f32 brightness, f32 contrast)
    {
        //! TODO -- https://wiki.libsdl.org/SDL_SetWindowGammaRamp
        //! manual...
        return false;
    }

    //! Get the current Gamma Value for the Display
    bool CIrrDeviceSDL::getGammaRamp(f32 &red, f32 &green, f32 &blue, f32 &brightness, f32 &contrast)
    {
        //! TODO -- https://wiki.libsdl.org/SDL_GetWindowGammaRamp
        //! manual...
        return false;
    }

    //! returns color format of the window.
    video::ECOLOR_FORMAT CIrrDeviceSDL::getColorFormat() const
    {
        if(window)
        {
            u32 cfrmt = SDL_GetWindowPixelFormat(window);
            if(SDL_BITSPERPIXEL(cfrmt) < 24)
            {
                if(SDL_ISPIXELFORMAT_ALPHA(cfrmt)) return video::ECF_A1R5G5B5;
                else return video::ECF_R5G6B5;
            }
            else
            {
                if(SDL_ISPIXELFORMAT_ALPHA(cfrmt)) return video::ECF_A8R8G8B8;
                else return video::ECF_R8G8B8;
            }
        }
        return CIrrDeviceStub::getColorFormat();
    }

    void CIrrDeviceSDL::createKeyMap()
    {
        // I don't know if this is the best method  to create
        // the lookuptable, but I'll leave it like that until
        // I find a better version.

        KeyMap.reallocate(105);

        KeyMap.push_back(SKeyMap(SDLK_0, KEY_KEY_0));
        KeyMap.push_back(SKeyMap(SDLK_1, KEY_KEY_1));
        KeyMap.push_back(SKeyMap(SDLK_2, KEY_KEY_2));
        KeyMap.push_back(SKeyMap(SDLK_3, KEY_KEY_3));
        KeyMap.push_back(SKeyMap(SDLK_4, KEY_KEY_4));
        KeyMap.push_back(SKeyMap(SDLK_5, KEY_KEY_5));
        KeyMap.push_back(SKeyMap(SDLK_6, KEY_KEY_6));
        KeyMap.push_back(SKeyMap(SDLK_7, KEY_KEY_7));
        KeyMap.push_back(SKeyMap(SDLK_8, KEY_KEY_8));
        KeyMap.push_back(SKeyMap(SDLK_9, KEY_KEY_9));
        KeyMap.push_back(SKeyMap(SDLK_a, KEY_KEY_A));
        KeyMap.push_back(SKeyMap(SDLK_APPLICATION, KEY_APPS));
        KeyMap.push_back(SKeyMap(SDLK_AUDIOPLAY, KEY_PLAY));
        KeyMap.push_back(SKeyMap(SDLK_b, KEY_KEY_B));
        KeyMap.push_back(SKeyMap(SDLK_BACKSPACE, KEY_BACK));
        KeyMap.push_back(SKeyMap(SDLK_c, KEY_KEY_C));
        KeyMap.push_back(SKeyMap(SDLK_CAPSLOCK, KEY_CAPITAL));
        KeyMap.push_back(SKeyMap(SDLK_CLEAR, KEY_CLEAR));
        KeyMap.push_back(SKeyMap(SDLK_COMMA,  KEY_COMMA));
        KeyMap.push_back(SKeyMap(SDLK_CRSEL,  KEY_CRSEL));
        KeyMap.push_back(SKeyMap(SDLK_d, KEY_KEY_D));
        KeyMap.push_back(SKeyMap(SDLK_DELETE, KEY_DELETE));
        KeyMap.push_back(SKeyMap(SDLK_DOWN, KEY_DOWN));
        KeyMap.push_back(SKeyMap(SDLK_e, KEY_KEY_E));
        KeyMap.push_back(SKeyMap(SDLK_END, KEY_END));
        KeyMap.push_back(SKeyMap(SDLK_ESCAPE, KEY_ESCAPE));
        KeyMap.push_back(SKeyMap(SDLK_EXECUTE, KEY_EXECUT));
        KeyMap.push_back(SKeyMap(SDLK_EXSEL, KEY_EXSEL));
        KeyMap.push_back(SKeyMap(SDLK_f, KEY_KEY_F));
        KeyMap.push_back(SKeyMap(SDLK_F1,  KEY_F1));
        KeyMap.push_back(SKeyMap(SDLK_F2,  KEY_F2));
        KeyMap.push_back(SKeyMap(SDLK_F3,  KEY_F3));
        KeyMap.push_back(SKeyMap(SDLK_F4,  KEY_F4));
        KeyMap.push_back(SKeyMap(SDLK_F5,  KEY_F5));
        KeyMap.push_back(SKeyMap(SDLK_F6,  KEY_F6));
        KeyMap.push_back(SKeyMap(SDLK_F7,  KEY_F7));
        KeyMap.push_back(SKeyMap(SDLK_F8,  KEY_F8));
        KeyMap.push_back(SKeyMap(SDLK_F9,  KEY_F9));
        KeyMap.push_back(SKeyMap(SDLK_F10, KEY_F10));
        KeyMap.push_back(SKeyMap(SDLK_F11, KEY_F11));
        KeyMap.push_back(SKeyMap(SDLK_F12, KEY_F12));
        KeyMap.push_back(SKeyMap(SDLK_F13, KEY_F13));
        KeyMap.push_back(SKeyMap(SDLK_F14, KEY_F14));
        KeyMap.push_back(SKeyMap(SDLK_F15, KEY_F15));
        KeyMap.push_back(SKeyMap(SDLK_F16, KEY_F16));
        KeyMap.push_back(SKeyMap(SDLK_F17, KEY_F17));
        KeyMap.push_back(SKeyMap(SDLK_F18, KEY_F18));
        KeyMap.push_back(SKeyMap(SDLK_F19, KEY_F19));
        KeyMap.push_back(SKeyMap(SDLK_F20, KEY_F20));
        KeyMap.push_back(SKeyMap(SDLK_F21, KEY_F21));
        KeyMap.push_back(SKeyMap(SDLK_F22, KEY_F22));
        KeyMap.push_back(SKeyMap(SDLK_F23, KEY_F23));
        KeyMap.push_back(SKeyMap(SDLK_F24, KEY_F24));
        KeyMap.push_back(SKeyMap(SDLK_g, KEY_KEY_G));
        KeyMap.push_back(SKeyMap(SDLK_h, KEY_KEY_H));
        KeyMap.push_back(SKeyMap(SDLK_HELP, KEY_HELP));
        KeyMap.push_back(SKeyMap(SDLK_HOME, KEY_HOME));
        KeyMap.push_back(SKeyMap(SDLK_i, KEY_KEY_I));
        KeyMap.push_back(SKeyMap(SDLK_INSERT, KEY_INSERT));
        KeyMap.push_back(SKeyMap(SDLK_j, KEY_KEY_J));
        KeyMap.push_back(SKeyMap(SDLK_k, KEY_KEY_K));
        KeyMap.push_back(SKeyMap(SDLK_KP_0, KEY_NUMPAD0));
        KeyMap.push_back(SKeyMap(SDLK_KP_00, KEY_NUMPAD0));
        KeyMap.push_back(SKeyMap(SDLK_KP_000, KEY_NUMPAD0));
        KeyMap.push_back(SKeyMap(SDLK_KP_1, KEY_NUMPAD1));
        KeyMap.push_back(SKeyMap(SDLK_KP_2, KEY_NUMPAD2));
        KeyMap.push_back(SKeyMap(SDLK_KP_3, KEY_NUMPAD3));
        KeyMap.push_back(SKeyMap(SDLK_KP_4, KEY_NUMPAD4));
        KeyMap.push_back(SKeyMap(SDLK_KP_5, KEY_NUMPAD5));
        KeyMap.push_back(SKeyMap(SDLK_KP_6, KEY_NUMPAD6));
        KeyMap.push_back(SKeyMap(SDLK_KP_7, KEY_NUMPAD7));
        KeyMap.push_back(SKeyMap(SDLK_KP_8, KEY_NUMPAD8));
        KeyMap.push_back(SKeyMap(SDLK_KP_9, KEY_NUMPAD9));
        KeyMap.push_back(SKeyMap(SDLK_KP_A, KEY_KEY_A));
        KeyMap.push_back(SKeyMap(SDLK_KP_B, KEY_KEY_B));
        KeyMap.push_back(SKeyMap(SDLK_KP_BACKSPACE, KEY_BACK));
        KeyMap.push_back(SKeyMap(SDLK_KP_C, KEY_KEY_C));
        KeyMap.push_back(SKeyMap(SDLK_KP_CLEAR, KEY_CLEAR));
        KeyMap.push_back(SKeyMap(SDLK_KP_D, KEY_KEY_D));
        KeyMap.push_back(SKeyMap(SDLK_KP_DECIMAL, KEY_DECIMAL));
        KeyMap.push_back(SKeyMap(SDLK_KP_DIVIDE, KEY_DIVIDE));
        KeyMap.push_back(SKeyMap(SDLK_KP_E, KEY_KEY_E));
        KeyMap.push_back(SKeyMap(SDLK_KP_ENTER, KEY_RETURN));
        KeyMap.push_back(SKeyMap(SDLK_KP_F, KEY_KEY_F));
        KeyMap.push_back(SKeyMap(SDLK_KP_MEMADD, KEY_PLUS));
        KeyMap.push_back(SKeyMap(SDLK_KP_MEMCLEAR, KEY_CLEAR));
        KeyMap.push_back(SKeyMap(SDLK_KP_MEMDIVIDE, KEY_DIVIDE));
        KeyMap.push_back(SKeyMap(SDLK_KP_MEMMULTIPLY, KEY_MULTIPLY));
        KeyMap.push_back(SKeyMap(SDLK_KP_MEMSUBTRACT, KEY_MINUS));
        KeyMap.push_back(SKeyMap(SDLK_KP_MEMDIVIDE, KEY_DIVIDE));
        KeyMap.push_back(SKeyMap(SDLK_KP_MINUS, KEY_SUBTRACT));
        KeyMap.push_back(SKeyMap(SDLK_KP_MULTIPLY, KEY_MULTIPLY));
        KeyMap.push_back(SKeyMap(SDLK_KP_PLUS, KEY_ADD));
        KeyMap.push_back(SKeyMap(SDLK_KP_SPACE, KEY_SPACE));
        KeyMap.push_back(SKeyMap(SDLK_KP_TAB, KEY_TAB));
        KeyMap.push_back(SKeyMap(SDLK_l, KEY_KEY_L));
        KeyMap.push_back(SKeyMap(SDLK_LALT,  KEY_LMENU));
        KeyMap.push_back(SKeyMap(SDLK_LCTRL,  KEY_LCONTROL));
        KeyMap.push_back(SKeyMap(SDLK_LEFT, KEY_LEFT));
        KeyMap.push_back(SKeyMap(SDLK_LGUI, KEY_LWIN));
        KeyMap.push_back(SKeyMap(SDLK_LSHIFT, KEY_LSHIFT));
        KeyMap.push_back(SKeyMap(SDLK_m, KEY_KEY_M));
        KeyMap.push_back(SKeyMap(SDLK_MENU,  KEY_RMENU));
        KeyMap.push_back(SKeyMap(SDLK_MINUS,  KEY_MINUS));
        KeyMap.push_back(SKeyMap(SDLK_n, KEY_KEY_N));
        KeyMap.push_back(SKeyMap(SDLK_NUMLOCKCLEAR, KEY_NUMLOCK));
        KeyMap.push_back(SKeyMap(SDLK_o, KEY_KEY_O));
        KeyMap.push_back(SKeyMap(SDLK_p, KEY_KEY_P));
        KeyMap.push_back(SKeyMap(SDLK_PAGEDOWN, KEY_NEXT));
        KeyMap.push_back(SKeyMap(SDLK_PAGEUP, KEY_PRIOR));
        KeyMap.push_back(SKeyMap(SDLK_PAUSE, KEY_PAUSE));
        KeyMap.push_back(SKeyMap(SDLK_PERIOD, KEY_PERIOD));
        KeyMap.push_back(SKeyMap(SDLK_PLUS,   KEY_PLUS));
        KeyMap.push_back(SKeyMap(SDLK_PRINTSCREEN, KEY_SNAPSHOT));
        KeyMap.push_back(SKeyMap(SDLK_q, KEY_KEY_Q));
        KeyMap.push_back(SKeyMap(SDLK_r, KEY_KEY_R));
        KeyMap.push_back(SKeyMap(SDLK_RALT,  KEY_RMENU));
        KeyMap.push_back(SKeyMap(SDLK_RCTRL,  KEY_RCONTROL));
        KeyMap.push_back(SKeyMap(SDLK_RETURN, KEY_RETURN));
        KeyMap.push_back(SKeyMap(SDLK_RGUI, KEY_RWIN));
        KeyMap.push_back(SKeyMap(SDLK_RIGHT, KEY_RIGHT));
        KeyMap.push_back(SKeyMap(SDLK_RSHIFT, KEY_RSHIFT));
        KeyMap.push_back(SKeyMap(SDLK_s, KEY_KEY_S));
        KeyMap.push_back(SKeyMap(SDLK_SCROLLLOCK, KEY_SCROLL));
    	KeyMap.push_back(SKeyMap(SDLK_SEPARATOR, KEY_SEPARATOR));
    	KeyMap.push_back(SKeyMap(SDLK_SLASH, KEY_DIVIDE));
    	KeyMap.push_back(SKeyMap(SDLK_SLEEP, KEY_SLEEP));
        KeyMap.push_back(SKeyMap(SDLK_SPACE, KEY_SPACE));
        KeyMap.push_back(SKeyMap(SDLK_t, KEY_KEY_T));
        KeyMap.push_back(SKeyMap(SDLK_TAB, KEY_TAB));
        KeyMap.push_back(SKeyMap(SDLK_u, KEY_KEY_U));
        KeyMap.push_back(SKeyMap(SDLK_UP, KEY_UP));
        KeyMap.push_back(SKeyMap(SDLK_v, KEY_KEY_V));
        KeyMap.push_back(SKeyMap(SDLK_w, KEY_KEY_W));
        KeyMap.push_back(SKeyMap(SDLK_x, KEY_KEY_X));
        KeyMap.push_back(SKeyMap(SDLK_y, KEY_KEY_Y));
        KeyMap.push_back(SKeyMap(SDLK_z, KEY_KEY_Z));

        KeyMap.sort();
    }
} // end namespace irr

#endif // _IRR_COMPILE_WITH_SDL_DEVICE_
