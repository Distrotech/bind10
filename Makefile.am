ACLOCAL_AMFLAGS = -I m4macros -I examples/m4 ${ACLOCAL_FLAGS}
# ^^^^^^^^ This has to be the first line and cannot come later in this
# Makefile.am due to some bork in some versions of autotools.

SUBDIRS = compatcheck doc . src tests m4macros
USE_LCOV=@USE_LCOV@
LCOV=@LCOV@
GENHTML=@GENHTML@
DISTCHECK_GTEST_CONFIGURE_FLAG=@DISTCHECK_GTEST_CONFIGURE_FLAG@

DISTCLEANFILES = config.report

# When running distcheck target, do not install the configurations
DISTCHECK_CONFIGURE_FLAGS = --disable-install-configurations

# Use same --with-gtest flag if set
DISTCHECK_CONFIGURE_FLAGS += $(DISTCHECK_GTEST_CONFIGURE_FLAG)

dist_doc_DATA = AUTHORS COPYING ChangeLog README

.PHONY: check-valgrind check-valgrind-suppress

install-exec-hook:
	-@echo -e "\033[1;33m" # switch to yellow color text
	@echo "NOTE: BIND 10 does not automatically start DNS services when it is run"
	@echo "      in its default configuration. Please see the Guide for information"
	@echo "      on how to configure these services to be started automatically."
	-@echo -e "\033[m" # switch back to normal

check-valgrind:
if HAVE_VALGRIND
	@VALGRIND_COMMAND="$(VALGRIND) -q --gen-suppressions=all --track-origins=yes --num-callers=48 --leak-check=full --fullpath-after=" \
	make -C $(abs_top_builddir) check
else
	@echo "*** Valgrind is required for check-valgrind ***"; exit 1;
endif

check-valgrind-suppress:
if HAVE_VALGRIND
	@VALGRIND_COMMAND="$(VALGRIND) -q --gen-suppressions=all --track-origins=yes --error-exitcode=1 --suppressions=$(abs_top_srcdir)/src/valgrind-suppressions --suppressions=$(abs_top_srcdir)/src/valgrind-suppressions.revisit --num-callers=48 --leak-check=full --fullpath-after=" \
	make -C $(abs_top_builddir) check
else
	@echo "*** Valgrind is required for check-valgrind-suppress ***"; exit 1;
endif

clean-cpp-coverage:
	@if [ $(USE_LCOV) = yes ] ; then \
		$(LCOV) --directory . --zerocounters; \
		rm -rf $(abs_top_srcdir)/coverage-cpp-html/; \
	else \
		echo "C++ code coverage not enabled at configuration time." ; \
		echo "Use: ./configure --with-lcov" ; \
	fi

clean-python-coverage:
	@if [ $(USE_PYCOVERAGE) = yes ] ; then \
		rm -f $(abs_top_srcdir)/.coverage ; \
		rm -rf $(abs_top_srcdir)/py-coverage-html ; \
	else \
		echo "Python code coverage not enabled at configuration time." ; \
		echo "Use: ./configure --with-pycoverage" ; \
	fi

perform-coverage: check

report-cpp-coverage:
	@if [ $(USE_LCOV) = yes ] ; then \
		$(LCOV) --capture --directory . --output-file all.info ; \
		$(LCOV) --remove all.info \
			c++/4.4\*/\* \
			c++/4.4\*/backward/\* \
			c++/4.4\*/bits/\* \
			c++/4.4\*/ext/\* \
			c++/4.4\*/\*-\*/bits/\* \
			boost/\* \
			botan/\* \
			asio/\* \
			ext/coroutine/\* \
			gtest/\* \
			log4cplus/\* \
			usr/include/\* \
			tests/\* \
			unittests/\* \
			\*_unittests.cc \
			\*_unittest.cc \
			\*_unittests.h \
			--output report.info ; \
		$(GENHTML) --legend -o $(abs_top_builddir)/coverage-cpp-html report.info ; \
		echo "Generated C++ Code Coverage report in HTML at $(abs_top_builddir)/coverage-cpp-html" ; \
	else \
		echo "C++ code coverage not enabled at configuration time." ; \
		echo "Use: ./configure --with-lcov" ; \
	fi

report-python-coverage:
	@if [ $(USE_PYCOVERAGE) = yes ] ; then \
		$(PYCOVERAGE) html -d $(abs_top_builddir)/coverage-python-html --omit=src/bin/bind10/tests/,src/bin/bindctl/tests/,src/bin/cfgmgr/tests/,src/bin/cmdctl/tests/,src/bin/loadzone/tests/,src/bin/msgq/tests/,src/bin/stats/tests/,src/bin/tests/,src/bin/xfrin/tests/,src/bin/xfrout/tests/,src/bin/zonemgr/tests/,src/lib/dns/python/tests/,src/lib/dns/tests/,src/lib/python/isc/cc/tests/,src/lib/python/isc/config/tests/,src/lib/python/isc/datasrc/tests/,src/lib/python/isc/log/tests/,src/lib/python/isc/net/tests/,src/lib/python/isc/notify/tests/,src/lib/python/isc/util/tests/ ; \
		echo "Generated Python Code Coverage report in HTML at $(abs_top_builddir)/coverage-python-html" ; \
	else \
		echo "Python code coverage not enabled at configuration time." ; \
		echo "Use: ./configure --with-pycoverage" ; \
	fi

# for python and c++ test coverage
coverage: clean-coverage perform-coverage report-coverage

clean-coverage: clean-cpp-coverage clean-python-coverage

report-coverage: report-cpp-coverage report-python-coverage

# for static C++ check using cppcheck (when available)
cppcheck:
	cppcheck -I./src/lib -I./src/bin --enable=all --suppressions \
		src/cppcheck-suppress.lst --inline-suppr \
		--quiet --error-exitcode=1 \
		--template '{file}:{line}: check_fail: {message} ({severity},{id})' \
		src

### include tool to generate documentation from log message specifications
### in the distributed tarball:
EXTRA_DIST = tools/system_messages.py

#### include external sources in the distributed tarball:
EXTRA_DIST += ext/coroutine/LICENSE_1_0.txt
EXTRA_DIST += ext/coroutine/coroutine.h

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = dns++.pc

CLEANFILES = $(abs_top_builddir)/logger_lockfile

# config.h may be included by headers supplied for building user-written
# hooks libraries, so we need to include it in the distribution.
pkginclude_HEADERS = config.h

if HAVE_GTEST_SOURCE
noinst_LIBRARIES = libgtest.a
libgtest_a_CXXFLAGS = $(GTEST_INCLUDES) $(AM_CXXFLAGS)
nodist_libgtest_a_SOURCES = $(GTEST_SOURCE)/src/gtest-all.cc
endif
