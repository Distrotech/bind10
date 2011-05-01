/* config.h for WIN32 */

/* Define to 1 if sockaddr has a sa_len member, and corresponding sin_len and
   sun_len */
/* #undef HAVE_SA_LEN */

/* Need boost sunstudio workaround */
/* #undef NEED_SUNPRO_WORKAROUND */

/* Name of package */
#define PACKAGE "bind10-devel"

/* Define to the address where bug reports for this package should be sent. */
#define PACKAGE_BUGREPORT "bind10-dev@isc.org"

/* Define to the full name of this package. */
#define PACKAGE_NAME "bind10-devel"

/* Define to the full name and version of this package. */
#define PACKAGE_STRING "bind10-devel 20110322"

/* Define to the one symbol short name of this package. */
#define PACKAGE_TARNAME "bind10-devel"

/* Define to the home page for this package. */
#define PACKAGE_URL ""

/* Define to the version of this package. */
#define PACKAGE_VERSION "20110322"

/* Use boost threads */
/* #undef USE_BOOST_THREADS */

/* Version number of package */
#define VERSION "20110322"

/* Additional things */

/* At least Vista */

#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0600
#endif

/* WIN32 specials */

#define strncasecmp _strnicmp
#define unlink _unlink
#define getpid _getpid
#define random rand
#define srandom srand
typedef unsigned int uid_t;

/* Prevent inclusion of winsock.h in windows.h */
#ifndef _WINSOCKAPI_
#define _WINSOCKAPI_
#define _RESTORE_WSA_
#endif

/*
 * Make the number of available sockets large
 * The number of sockets needed can get large and memory's cheap
 * This must be defined before winsock2.h gets included as the
 * macro is used there.
 */

#define FD_SETSIZE 16384
#include <windows.h>

/* Restore _WINSOCKAPI_ */
#ifdef _RESTORE_WSA_
#undef _WINSOCKAPI_
#endif
