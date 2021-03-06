# We need CMake 2.8.11 at least, because we use CMake features
# "Target Usage Requirements" and "Generator Toolset selection"
CMAKE_MINIMUM_REQUIRED( VERSION 2.8.11 )

project("AviSynth+")

include_directories(../xy-VSFilter/src/filters/baseClasses)

# Avoid uselessly linking to unused libraries
set(CMAKE_STANDARD_LIBRARIES "" CACHE STRING "" FORCE)
set(CMAKE_C_STANDARD_LIBRARIES "" CACHE STRING "" FORCE)
set(CMAKE_CXX_STANDARD_LIBRARIES "" CACHE STRING "" FORCE)

option(ENABLE_PLUGINS "Build set of default external plugins" ON)

if(CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_CONFIGURATION_TYPES Debug Release RelWithDebInfo)
  set(CMAKE_CONFIGURATION_TYPES "${CMAKE_CONFIGURATION_TYPES}" CACHE STRING "Reset the configurations to what we need" FORCE)
endif()

IF( MSVC_IDE )  # Check for Visual Studio

  IF( MSVC_VERSION VERSION_LESS 1700 )  # Check for v11 (VS 2012)
    MESSAGE(FATAL_ERROR "Visual C++ 2012 or newer required.")
  ENDIF()

  file(MAKE_DIRECTORY "${CMAKE_BINARY_DIR}/Output/plugins")
  file(MAKE_DIRECTORY "${CMAKE_BINARY_DIR}/Output/system")

  IF(CMAKE_GENERATOR_TOOLSET STREQUAL "v141_clang_c2")
     #1900 is reported
    message("v141_clang_c2 toolset was specified via -T. Reported MSVC_VERSION is: ${MSVC_VERSION}")
    set(CLANG_IN_VS "1")
  ENDIF()
  
  # We want our project to also run on Windows XP
  IF( MSVC_VERSION VERSION_LESS 1900 )  # Check for v11 (VS 2012)
    set(CMAKE_GENERATOR_TOOLSET "v120_xp" CACHE STRING "The compiler toolset to use for Visual Studio." FORCE)
  ELSEIF(MSVC_VERSION VERSION_LESS 1910 )
    IF(NOT CLANG_IN_VS STREQUAL "1")
      set(CMAKE_GENERATOR_TOOLSET "v140_xp" CACHE STRING "The compiler toolset to use for Visual Studio." FORCE) # VS2015
      # https://connect.microsoft.com/VisualStudio/feedback/details/1789709/visual-c-2015-runtime-broken-on-windows-server-2003-c-11-magic-statics
      add_definitions("/Zc:threadSafeInit-")
  ENDIF()
  ELSE()
    IF(NOT CLANG_IN_VS STREQUAL "1")
      set(CMAKE_GENERATOR_TOOLSET "v141_xp" CACHE STRING "The compiler toolset to use for Visual Studio." FORCE) # VS2017
      # https://connect.microsoft.com/VisualStudio/feedback/details/1789709/visual-c-2015-runtime-broken-on-windows-server-2003-c-11-magic-statics
      add_definitions("/Zc:threadSafeInit-")
    ENDIF()
  ENDIF()

  IF(CLANG_IN_VS STREQUAL "1")
      set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fexceptions")
      set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fexceptions")
      set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wno-inconsistent-missing-override")
      set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-inconsistent-missing-override")
  ELSE()
	  # Enable C++ with SEH exceptions
	  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} /EHa")
	  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /EHa")
	  # or add_compile_options( /EHa ) for CMake>=3?
  ENDIF()
  # Prevent VC++ from complaining about not using MS-specific functions
  add_definitions("/D _CRT_SECURE_NO_WARNINGS /D _SECURE_SCL=0")
  
  # Enable CRT heap debugging - only effective in debug builds
  add_definitions("/D _CRTDBG_MAP_ALLOC")
  
  add_definitions("/D __SSE2__")

  if(CMAKE_SIZEOF_VOID_P EQUAL 4)
    # VC++ enables the SSE2 instruction set by default even on 32-bits. Step back a bit.
    #add_definitions("/arch:SSE")
    add_definitions("/arch:SSE2") # Better use this one, it's 2017 now, and helps optimizing hbd stuff still in C
  endif() 

  # Set additional optimization flags
  set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} /Oy /Ot /GS- /Oi /MT")
  set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} /Oy /Ot /GS- /Oi /MT")
ELSE()

  SET( CMAKE_CXX_FLAGS  "${CMAKE_CXX_FLAGS} -std=gnu++14" )
  SET( CMAKE_CXX_FLAGS  "${CMAKE_CXX_FLAGS} -march=native" )

ENDIF()

add_subdirectory("avs_core")
if(ENABLE_PLUGINS)
  add_subdirectory("plugins")
endif()

# uninstall target
configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake_uninstall.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/cmake_uninstall.cmake"
    IMMEDIATE @ONLY)

add_custom_target(uninstall
    COMMAND ${CMAKE_COMMAND} -P ${CMAKE_CURRENT_BINARY_DIR}/cmake_uninstall.cmake)