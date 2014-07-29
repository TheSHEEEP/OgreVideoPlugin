# Change this to your project's name
set(PROJECT_NAME "OgreVideoPlugin")

# Set install prefix to something not silly
# MUST be done before the project() call - silly cmake
get_filename_component(PREFIXPATH "${CMAKE_CURRENT_LIST_DIR}/../OgreVideoPlugin_built" REALPATH)
set(CMAKE_INSTALL_PREFIX "${PREFIXPATH}" CACHE PATH "Prefix prepended to install directories")

project(${PROJECT_NAME} CXX C)
cmake_minimum_required(VERSION 2.8)

# Avoid source tree pollution
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_BINARY_DIR)
message(FATAL_ERROR "In-source builds are not permitted. Make a separate folder for building:\nmkdir build; cd build; cmake ..\nBefore that, remove the files already created:\nrm -rf CMakeCache.txt CMakeFiles")
endif(CMAKE_SOURCE_DIR STREQUAL CMAKE_BINARY_DIR)

# Add a sensible build type default and warning because empty means no optimization and no debug info.
if(NOT CMAKE_BUILD_TYPE)
message("WARNING: CMAKE_BUILD_TYPE is not defined!\n Defaulting to CMAKE_BUILD_TYPE=Release. Use ccmake to set a proper value.")
set(CMAKE_BUILD_TYPE Release
CACHE STRING "Type of build, options are: None Debug Release RelWithDebInfo MinSizeRel." FORCE)
endif(NOT CMAKE_BUILD_TYPE)


# We need the path to the FFmpeg libraries
set(FFMPEG_PATH "/SET_PATH_TO_ffmpeg_HERE" CACHE PATH "The path where FFmpeg is installed. Needs to have /lib and /include directory.")

# Additional options for ogg, vorbis and theora
set(OGG1_USE_OGG_THEORA_VORBIS OFF CACHE BOOL "If you want to use ogg container format with theora video and vorbis audio, you must set this and the paths.")
set(OGG2_VORBIS_PATH "/SET_PATH_TO_vorbis_HERE" CACHE PATH "The path where vorbis is installed. Needs to have /lib and /include directory.")
set(OGG3_THEORA_PATH "/SET_PATH_TO_theora_HERE" CACHE PATH "The path where theora is installed. Needs to have /lib and /include directory.")

# Include path for additional CMake library finding scripts
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/cmake/")

# We want the binaries to be easily accessible
set(EXECUTABLE_OUTPUT_PATH ${CMAKE_BINARY_DIR})

# Check path existence
if(NOT EXISTS "${FFMPEG_PATH}/lib" OR NOT EXISTS "${FFMPEG_PATH}/include")
	message(FATAL_ERROR "The FFmpeg path is not correct! ${FFMPEG_PATH}/lib or /include doesn't exist. Remember, no trailing /" )
endif(NOT EXISTS "${FFMPEG_PATH}/lib" OR NOT EXISTS "${FFMPEG_PATH}/include")
if(OGG1_USE_OGG_THEORA_VORBIS)
	if(NOT EXISTS "${OGG2_VORBIS_PATH}/lib" OR NOT EXISTS "${OGG2_VORBIS_PATH}/include")
		message(FATAL_ERROR "The vorbis path is not correct! ${OGG2_VORBIS_PATH}/lib or /include doesn't exist. Remember, no trailing /" )
	endif(NOT EXISTS "${OGG2_VORBIS_PATH}/lib" OR NOT EXISTS "${OGG2_VORBIS_PATH}/include")
	if(NOT EXISTS "${OGG3_THEORA_PATH}/lib" OR NOT EXISTS "${OGG3_THEORA_PATH}/include")
		message(FATAL_ERROR "The theora path is not correct! ${OGG3_THEORA_PATH}/lib or /include doesn't exist. Remember, no trailing /" )
	endif(NOT EXISTS "${OGG3_THEORA_PATH}/lib" OR NOT EXISTS "${OGG3_THEORA_PATH}/include")
endif(OGG1_USE_OGG_THEORA_VORBIS)

# Add the libraries
# Boost
set(Boost_USE_STATIC_LIBS ON)
set(Boost_USE_MULTITHREADED ON)
set(Boost_USE_STATIC_RUNTIME ON)
find_package(Boost COMPONENTS date_time thread system chrono REQUIRED) # Specify the required components
include_directories(${Boost_INCLUDE_DIRS})
link_directories(${Boost_LIBRARY_DIRS})
list(APPEND LIBS ${Boost_LIBRARIES})

# OGRE
find_package(OGRE REQUIRED)
link_directories(${OGRE_LIBRARY_DIRS})
include_directories(${OGRE_INCLUDE_DIRS})
list(APPEND LIBS ${OGRE_LIBRARIES})

# FFmpeg
link_directories("${FFMPEG_PATH}/lib")
include_directories("${FFMPEG_PATH}/include")
list(APPEND LIBS "avformat" "avcodec" "swscale" "swresample" "avutil")

# if we make use of ogg, vorbis and theora, add those
if(OGG1_USE_OGG_THEORA_VORBIS)
    link_directories("${OGG2_VORBIS_PATH}/lib")
    link_directories("${OGG3_THEORA_PATH}/lib")
    include_directories("${OGG2_VORBIS_PATH}/include")
    include_directories("${OGG3_THEORA_PATH}/include")
    list(APPEND LIBS "vorbis" "vorbisenc" "theora")
endif(OGG1_USE_OGG_THEORA_VORBIS)

# Include directory
include_directories("./include")

# The project's sources
list(APPEND PROJECT_SOURCES
    src/FFmpegVideoDecodingThread.cpp
    src/FFmpegVideoPlayer.cpp
    src/FFmpegVideoPlugin.cpp
    src/FFmpegVideoPluginDLL.cpp
    include/FFmpegPluginPrerequisites.h
    include/FFmpegVideoDecodingThread.h
    include/FFmpegVideoPlayer.h
    include/FFmpegVideoPlugin.h
)

# Set required flags
set(CMAKE_CXX_FLAGS " -D__STDC_CONSTANT_MACROS -DBOOST_THREAD_USE_LIB -fPIC")

# Add library
add_library(${PROJECT_NAME} STATIC ${PROJECT_SOURCES})

# Add libraries to link against
target_link_libraries(${PROJECT_NAME} ${LIBS})
if(MINGW)
    target_link_libraries(${PROJECT_NAME} "pthread" "iconv")
endif(MINGW)
if(WIN32)
    target_link_libraries(${PROJECT_NAME} "ws2_32" "wsock32")
endif(WIN32)

# Install paths
INSTALL(FILES 
    include/FFmpegPluginPrerequisites.h
    include/FFmpegVideoDecodingThread.h
    include/FFmpegVideoPlayer.h
    include/FFmpegVideoPlugin.h
	DESTINATION include)
INSTALL(TARGETS ${PROJECT_NAME} 
  RUNTIME DESTINATION bin
  LIBRARY DESTINATION lib
  ARCHIVE DESTINATION lib
)
