bin_PROGRAMS = \
	gbindctl

AM_CPPFLAGS = \
	$(GTK_CFLAGS) \
	-DG_LOG_DOMAIN=\"gbindctl\" \
	-DB10_LOCALE_DIR=\""$(b10localedir)"\"

gbindctl_SOURCES = \
	gbc-main-window.c \
	gbc-main-window.h \
	main.c

gbindctl_LDADD = \
	$(GTK_LIBS)
