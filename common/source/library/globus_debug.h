
#ifndef GLOBUS_DEBUG_H
#define GLOBUS_DEBUG_H

#include "globus_common_include.h"

EXTERN_C_BEGIN

#ifdef BUILD_DEBUG

void
globus_debug_init(
    const char *                        env_name,
    const char *                        level_names,
    unsigned *                          debug_level,
    FILE **                             out_file,
    globus_bool_t *                     using_file,
    globus_bool_t *                     show_tids);

#ifdef BUILD_LITE
#define GlobusDebugThreadId() getpid()
#else
#define GlobusDebugThreadId() globus_thread_self()
#endif

/* call in same file as module_activate func (before (de)activate funcs) */
#define GlobusDebugDefine(module_name)                                      \
    static FILE * globus_i_##module_name##_debug_file;                      \
    static globus_bool_t globus_i_##module_name##_using_file;               \
    static globus_bool_t globus_i_##module_name##_print_threadids;          \
    void globus_i_##module_name##_debug_printf(const char * fmt, ...)       \
    {                                                                       \
        va_list ap;                                                         \
	                                                                    \
        if(!globus_i_##module_name##_debug_file)                            \
            return;                                                         \
                                                                            \
        va_start(ap, fmt);                                                  \
        if(globus_i_##module_name##_print_threadids)                        \
        {                                                                   \
            char buf[4096]; /* XXX better not use a fmt bigger than this */ \
            sprintf(                                                        \
                buf, "%lu::%s", (unsigned long) GlobusDebugThreadId(), fmt);\
            vfprintf(globus_i_##module_name##_debug_file, buf, ap);         \
        }                                                                   \
        else                                                                \
        {                                                                   \
            vfprintf(globus_i_##module_name##_debug_file, fmt, ap);         \
        }                                                                   \
        va_end(ap);                                                         \
    }                                                                       \
    unsigned globus_i_##module_name##_debug_level

/* call this in a header file (if needed externally) */
#define GlobusDebugDeclare(module_name)                                     \
    extern void globus_i_##module_name##_debug_printf(const char *, ...);   \
    extern unsigned globus_i_##module_name##_debug_level

/* call this in module activate func
 *
 * 'levels' is a space separated list of level names that can be used in env
 *    they will map to a 2^i value (so, list them in same order as value)
 *
 * will look in env for {module_name}_DEBUG whose value is:
 * <levels> [ , [ [ # ] <file name> ] [ , <show tids>] ]
 * where <levels> can be a single numeric or '|' separated level names
 * <file name> is a debug output file... can be empty.  stderr by default
 *    if a '#' precedes the filename, the file will be overwritten on each run
 *    otherwise, the default is to append to the existing (if one exists)
 * <show tids> is 0 or 1 to show thread ids on all messages.  0 by default
 * 
 * Also, user's can use the ALL level in their env setting to turn on 
 * all levels
 */
#define GlobusDebugInit(module_name, levels)                                \
    globus_debug_init(                                                      \
        #module_name "_DEBUG",                                              \
        #levels,                                                            \
        &globus_i_##module_name##_debug_level,                              \
        &globus_i_##module_name##_debug_file,                               \
        &globus_i_##module_name##_using_file,                               \
        &globus_i_##module_name##_print_threadids)

/* call this in module deactivate func */
#define GlobusDebugDestroy(module_name)                                     \
    do                                                                      \
    {                                                                       \
        if(globus_i_##module_name##_using_file)                             \
        {                                                                   \
            fclose(globus_i_##module_name##_debug_file);                    \
        }                                                                   \
        globus_i_##module_name##_debug_file = GLOBUS_NULL;                  \
    } while(0)

/* use this to print a message unconditionally (message must be enclosed in
 * parenthesis and contains a format and possibly var args
 */
#define GlobusDebugMyPrintf(module_name, message)                           \
    globus_i_##module_name##_debug_printf message

/* use this in an if() to debug enable blocks of code 
 * for example
 * 
 * if(GlobusDebugTrue(MY_MODULE, VERIFICATION))
 * {
 *    compute stats
 *    GlobusDebugMyPrintf(MY_MODULE, "Stats = %d\n", stats);
 * }
 */
#define GlobusDebugTrue(module_name, level)                                 \
    (globus_i_##module_name##_debug_level & (level))

/* most likely wrap this with your own macro,
 * so you dont need to pass module_name all the time
 * 'message' needs to be wrapped with parens and contains a format and
 * possibly var args
 */
#define GlobusDebugPrintf(module_name, level, message)                      \
    do                                                                      \
    {                                                                       \
        if(GlobusDebugTrue(module_name, level))                             \
        {                                                                   \
            GlobusDebugMyPrintf(module_name, message);                      \
        }                                                                   \
    } while(0)

#else

#define GlobusDebugThreadId()
#define GlobusDebugDeclare(module_name)
#define GlobusDebugDefine(module_name)
#define GlobusDebugInit(module_name, levels)
#define GlobusDebugDestroy(module_name)
#define GlobusDebugPrintf(module_name, level, message)
#define GlobusDebugMyPrintf(module_name, message)
#define GlobusDebugTrue(module_name, level) 0

#endif

EXTERN_C_END

#endif /* GLOBUS_DEBUG_H */
