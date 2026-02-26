#ifndef _PW_PATHNAMES_H_
#define _PW_PATHNAMES_H_

#ifndef _PATH_ROOT
#define _PATH_ROOT "/"
#endif
#ifndef _PATH_BIN
#define _PATH_BIN "/bin"
#endif
/* Torrekie: On Darwin this really should be "/System/Library/User Template" */
#ifndef _PATH_SKEL
#define _PATH_SKEL "/usr/share/skel"
#endif

#ifdef __APPLE__
#ifndef _PATH_HOME
#define _PATH_HOME "/var"
#endif
#ifndef _PATH_NONEXIST
#define _PATH_NONEXIST "/var/empty"
#endif
#ifndef _PATH_ATRM
#define _PATH_ATRM "/usr/bin/atrm"
#endif
#ifndef _PATH_BMAKE
#define _PATH_BMAKE "/usr/bin/bmake"
#endif
#ifndef _PATH_AT
#define _PATH_AT "/usr/lib/cron"
#endif
#else
#ifndef _PATH_HOME
#define _PATH_HOME "/home"
#endif
#ifndef _PATH_NONEXIST
#define _PATH_NONEXIST "/nonexist"
#endif
#ifndef _PATH_ATRM
#define _PATH_ATRM "/usr/sbin/atrm"
#endif
#ifndef _PATH_BMAKE
#define _PATH_BMAKE "/usr/bin/make"
#endif
#ifndef _PATH_AT
#define _PATH_AT "/var/cron"
#endif
#endif

#endif /* _PW_PATHNAMES_H_ */
