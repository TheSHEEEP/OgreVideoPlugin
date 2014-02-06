#include "FFmpegVideoPlugin.h"
#include "FFmpegPluginPrerequisites.h"

#ifndef OGRE_STATIC_LIB

FFmpegVideoPlugin* ffmpegPlugin;

extern "C" void _FFmpegPluginExport dllStartPlugin( void )
{
    // Create the plugin
    ffmpegPlugin = OGRE_NEW FFmpegVideoPlugin();

    // Register
    Ogre::Root::getSingleton().installPlugin(ffmpegPlugin);
}

extern "C" void _FFmpegPluginExport dllStopPlugin( void )
{
    Ogre::Root::getSingleton().uninstallPlugin(ffmpegPlugin);
    OGRE_DELETE ffmpegPlugin;
}

#endif
