pkglibexecdir = $(libexecdir)/@PACKAGE@

pkglibexec_SCRIPTS = b10-xfrout

b10_xfroutdir = $(DESTDIR)$(pkgdatadir)
b10_xfrout_DATA = xfrout.spec

CLEANFILES=	xfrout.py

# TODO: does this need $$(DESTDIR) also?
# this is done here since configure.ac AC_OUTPUT doesn't expand exec_prefix
b10-xfrout: xfrout.py
	$(SED) "s|@@PYTHONPATH@@|@pyexecdir@|" xfrout.py >$@
	chmod a+x $@
