PW_SRCS=	pw.c pw_conf.c pw_user.c pw_group.c pw_log.c pw_nis.c pw_vpw.c \
		grupd.c pwupd.c psdate.c bitmap.c cpdir.c rm_r.c strtounum.c \
		pw_utils.c strtonum.c

LIBUTIL_PART=	flopen.c gr_util.c pw_scan.c pw_util.c

MAN+=		pw.conf.5 pw.8
