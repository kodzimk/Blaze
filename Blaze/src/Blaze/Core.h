#pragma once

#ifdef BZ_PLATFORM_WINDOWS
   #ifdef BZ_BUILD_DLL
      #define BLAZE_API _declspec(dllexport) 
    #else 
       #define BLAZE_API _declspec(dllimport)
    #endif
#else 
 #error Blaze only support Windows!
#endif