dnl This is based on X.org's util/macros/xorgversion.m4

# This defines the variable GENERATE_CHANGELOG_CMD as the
# command to generate the ChangeLog from the git commit messages
# history.
#
AC_DEFUN([ISC_GIT_CHANGELOG], [
GENERATE_CHANGELOG_CMD="(GIT_DIR=\$(top_srcdir)/.git git log > \$(top_builddir)/.changelog.tmp && \
mv \$(top_builddir)/.changelog.tmp \$(top_builddir)/ChangeLog) \
|| (rm -f \$(top_builddir)/.changelog.tmp; touch \$(top_builddir)/ChangeLog; \
echo 'Warning: git repo not found: installing possibly empty ChangeLog file.' >&2)"
AC_SUBST([GENERATE_CHANGELOG_CMD])
]) # ISC_GIT_CHANGELOG
