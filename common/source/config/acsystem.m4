AC_DEFUN(CHECK_HEADERS, [

dnl
dnl System header files
dnl
AC_CHECK_HEADERS(libc.h)
AC_CHECK_HEADERS(malloc.h)
AC_CHECK_HEADERS(unistd.h)
AC_CHECK_HEADERS(stdarg.h)
AC_CHECK_HEADERS(netdb.h)
AC_CHECK_HEADERS(values.h)
AC_CHECK_HEADERS(getopt.h)
AC_HEADER_STDC
AC_HEADER_TIME
AC_CHECK_HEADERS(sys/types.h)
AC_CHECK_HEADERS(proj.h)
AC_CHECK_HEADERS(sys/param.h)
AC_CHECK_HEADERS(sys/access.h)
AC_CHECK_HEADERS(sys/errno.h)
AC_CHECK_HEADERS(sys/sysmp.h)
AC_CHECK_HEADERS(sys/lwp.h)
AC_CHECK_HEADERS(sys/stat.h)
AC_CHECK_HEADERS(sys/file.h)
AC_CHECK_HEADERS(sys/uio.h)
AC_CHECK_HEADERS(sys/time.h)
AC_CHECK_HEADERS(sys/signal.h)
AC_CHECK_HEADERS(sys/select.h)
AC_CHECK_HEADERS(sys/ioctl.h)
AC_CHECK_HEADERS(sys/cnx_pattr.h)
AC_CHECK_HEADERS(dce/cma.h)
AC_CHECK_HEADERS(dce/cma_ux.h)
AC_CHECK_HEADERS(sys/param.h)
AC_CHECK_HEADERS(limits.h)
AC_CHECK_HEADERS(sys/limits.h)
AC_CHECK_HEADERS(string.h)
AC_CHECK_HEADERS(ctype.h)
AC_CHECK_HEADERS(fcntl.h)
AC_CHECK_HEADERS(utime.h)
AC_CHECK_HEADERS(arpa/inet.h)
AC_CHECK_HEADERS(signal.h)
AC_CHECK_HEADERS(inttypes.h)

dnl Broken IRIX 6.5.3 headers
case $target in
    *irix*6.*)
        AC_DEFINE(HAVE_NETINET_TCP_H)
        ac_cv_header_netinet_tcp_h=1
	;;
    *)
       AC_CHECK_HEADERS(netinet/tcp.h)
       ;;
esac

AC_HEADER_SYS_WAIT
dnl
dnl System types
dnl
AC_CHECK_TYPE(ssize_t, int)
AC_CHECK_TYPE(size_t, unsigned int)
AC_TYPE_SIGNAL
AC_HEADER_DIRENT

])

AC_DEFUN(CHECK_FUNCS, [
dnl
dnl System function
dnl
AC_CHECK_FUNCS(waitpid)
AC_CHECK_FUNCS(strtoul)
AC_CHECK_FUNCS(wait3)
dnl AC_FUNC_WAIT3
AC_CHECK_FUNCS(sighold)
AC_CHECK_FUNCS(sigblock)
AC_CHECK_FUNCS(sigset)
AC_CHECK_FUNCS(getwd)
AC_CHECK_FUNCS(getcwd)
AC_CHECK_FUNCS(memmove)
AC_CHECK_FUNCS(usleep)
AC_CHECK_FUNCS(strptime)
AC_CHECK_FUNCS(gethostbyaddr)
AC_CHECK_FUNCS(telldir)
AC_CHECK_FUNCS(seekdir)
AC_CHECK_FUNCS(nrand48)
AC_CHECK_FUNCS(mktime)
AC_CHECK_FUNCS(writev)
AC_CHECK_FUNCS(strerror)
AC_CHECK_FUNCS(snprintf)
AC_CHECK_FUNCS(vsnprintf)
dnl used in RSL
AC_FUNC_ALLOCA

AC_CHECK_FUNCS(poll)

])
