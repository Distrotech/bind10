# On some systems, it appears the dynamic linker gets
# confused if the order is not right here
# There is probably a solution for this, but for now:
# order is important here!
import isc.cc
import isc.config
# Importing datasrc at an earlier stage is particularly important (for now),
# because it ensures the log messages defined in the underlying C++ library
# will be registered in the global dictionary before the Python application
# intilializes logging.  If the order is reversed we'll see various troubles
# such as ill-formatted log messages.
import isc.datasrc
