noinst_SCRIPTS = b10-mixer

CLEANFILES = b10-mixer
CLEANFILES += $(PYTHON_LOGMSGPKG_DIR)/work/mixer_messages.py
CLEANFILES += $(PYTHON_LOGMSGPKG_DIR)/work/mixer_messages.pyc

# this is done here since configure.ac AC_OUTPUT doesn't expand exec_prefix
b10-mixer: mixer.py
	$(SED) -e "s|@@PYTHONPATH@@|@pyexecdir@|" mixer.py >$@
	chmod a+x $@

CLEANDIRS = __pycache__

clean-local:
	rm -rf $(CLEANDIRS)
