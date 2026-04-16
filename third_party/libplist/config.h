/* Minimal autotools-style config.h for vendored libplist on Android. */
#ifndef LIBPLIST_FIREPLAY_CONFIG_H
#define LIBPLIST_FIREPLAY_CONFIG_H

#define PACKAGE "libplist"
#define VERSION "2.6.0"
#define PACKAGE_VERSION "2.6.0"

#define HAVE_INTTYPES_H 1
#define HAVE_STDINT_H 1
#define HAVE_STDLIB_H 1
#define HAVE_STRING_H 1
#define HAVE_STRINGS_H 1
#define HAVE_SYS_TYPES_H 1
#define HAVE_SYS_STAT_H 1
#define HAVE_UNISTD_H 1
#define HAVE_TIME_H 1
#define HAVE_DLFCN_H 1
#define HAVE_MEMORY_H 1

/* Modern Android (API >= 21) ships gmtime_r/asprintf/strndup/etc. */
#define HAVE_GMTIME_R 1
#define HAVE_LOCALTIME_R 1
#define HAVE_ASPRINTF 1
#define HAVE_STRNDUP 1
#define HAVE_VASPRINTF 1

#define LT_OBJDIR ".libs/"
#define STDC_HEADERS 1

#endif
