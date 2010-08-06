dnl
dnl LAC_THREADS()
dnl     Adds thread-related options to the configure command-line handling
dnl     Set the appropriate lac_cv_* variables:
dnl             lac_cv_threads_type ("no", "pthreads", 
dnl                                     "solaristhreads")
dnl             lac_cv_threads_defines
dnl             lac_cv_threads_CFLAGS
dnl             lac_cv_threads_CXXFLAGS
dnl             lac_cv_threads_LDFLAGS
dnl             lac_cv_threads_LIBS
dnl     The *FLAGS and *LIBS variables should only be set to flags
dnl     that are independent of the compiler.  Compiler dependent
dnl     flags should be specified in accompiler.m4.
dnl     Also setup lac_threads_* variables that mirror the lac_cv_threads_*
dnl     variables.

dnl LAC_THREADS()
AC_DEFUN([LAC_THREADS],
[
LAC_THREADS_PTHREADS
LAC_THREADS_WINDOWS
])

dnl LAC_THREADS_PTHREADS
AC_DEFUN([LAC_THREADS_PTHREADS],
[
    lib_type=""

    pthread_cflags=""
    pthread_libs=""
    pthread_ldflags=""

    AC_CACHE_CHECK(
        [if compiler recognizes -pthread], 
        [myapp_cv_gcc_pthread], 
        [
        ac_save_CFLAGS=$CFLAGS 
        CFLAGS="$CFLAGS -pthread" 
        AC_TRY_LINK([#include <pthread.h>], 
            [void *p = pthread_create;], 
            [myapp_cv_gcc_pthread=yes], 
            [myapp_cv_gcc_pthread=no] 
        ) 
        pthread_cflags="$CFLAGS"
        CFLAGS="$ac_save_CFLAGS"
        ])

    have_pthreads=no
    have_sched_yield=no
    save_LIBS="$LIBS"
    AC_CHECK_HEADERS([pthread.h],
        AC_SEARCH_LIBS([pthread_create], [pthread], [have_pthreads=yes]))
    AC_CHECK_HEADERS([sched.h],
        AC_SEARCH_LIBS([sched_yield], [pthread posix4], [have_sched_yield=yes]))
    pthread_libs="$LIBS"
    LIBS="$save_LIBS"

    if test "$have_pthreads" = "no"; then
        AC_MSG_NOTICE([pthread package not found])
    else
        case "$host" in
          *-hp-hpux11* )
            pthread_libs="$pthread_libs -lm"
            pthread_cflags="$pthread_cflags -D_REENTRANT"
          ;;
          *solaris2* )
            pthread_cflags="$pthread_cflags -D_REENTRANT"
          ;;
          *86-*-linux* | *darwin* )
            :
          ;;
          * )
            :
          ;;
        esac
        build_pthreads=yes
    fi
    AC_SUBST([PTHREAD_CFLAGS], [$pthread_cflags])
    AC_SUBST([PTHREAD_LDFLAGS], [$pthread_ldflags])
    AC_SUBST([PTHREAD_LIBS], [$pthread_libs])
    AM_CONDITIONAL([BUILD_PTHREADS], [test "$build_pthreads" = "yes"])
])


dnl LAC_THREADS_WINDOWS
AC_DEFUN([LAC_THREADS_WINDOWS],
[
    found_inc="no"
    found_lib="no"
    found_compat_lib="no"

    windowsthreads_cflags=""
    windowsthreads_libs=""
    windowsthreads_ldflags=""

    case "$host" in
        *cygwin* | *mingw* [)]
            build_windows_threads="yes"
            ;;
    esac

    AM_CONDITIONAL([BUILD_WINDOWS_THREADS], [test "$build_windows_threads" = "yes"])
])
