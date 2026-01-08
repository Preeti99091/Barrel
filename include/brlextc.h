#ifndef BRL_EXTC
#define BRL_EXTC

#if defined(_WIN32)
    #if defined(__TINYC__)
        #define __declspec(x) __attribute__((x))
    #endif
    #if defined(BUILD_LIBTYPE_SHARED)
        #define BRLAPI __declspec(dllexport)
    #elif defined(USE_LIBTYPE_SHARED)
        #define BRLAPI __declspec(dllimport)
    #endif
#else
    #if defined(BUILD_LIBTYPE_SHARED)
        #define BRLAPI __attribute__((visibility("default")))
    #endif
#endif

#ifndef BRLAPI
    #define BRLAPI
#endif

#ifdef __cplusplus
    #define BRL_EXTERN_C_START extern "C" {
#else
    #define BRL_EXTERN_C_START
#endif

#ifdef __cplusplus
    #define BRL_EXTERN_C_END }
#else
    #define BRL_EXTERN_C_END
#endif

#endif // BRL_EXTC
