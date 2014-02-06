/* 
 * File:   FFmpegPluginPrerequesites.h
 * Author: TheSHEEEP
 *
 * Created on 10. Januar 2014, 10:29
 */

#ifndef FFMPEGPLUGINPREREQUESITES_H
#define	FFMPEGPLUGINPREREQUESITES_H
 
#include <OgrePrerequisites.h>
 
//-----------------------------------------------------------------------
// Forward declarations
//-----------------------------------------------------------------------

//-----------------------------------------------------------------------
// Windows Settings
//-----------------------------------------------------------------------

#if (OGRE_PLATFORM == OGRE_PLATFORM_WIN32 || OGRE_PLATFORM == OGRE_PLATFORM_WINRT) && !defined(OGRE_STATIC_LIB)
#   ifdef OGRE_FFMPEGPLUGIN_EXPORTS
#       define _FFmpegPluginExport __declspec(dllexport)
#   else
#       if defined( __MINGW32__ )
#           define _FFmpegPluginExport
#       else
#    		define _FFmpegPluginExport __declspec(dllimport)
#       endif
#   endif
#elif defined ( OGRE_GCC_VISIBILITY )
#    define _FFmpegPluginExport  __attribute__ ((visibility("default")))
#else
#   define _FFmpegPluginExport
#endif

#endif	/* FFMPEGPLUGINPREREQUESITES_H */

