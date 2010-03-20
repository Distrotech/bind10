#include <stdlib.h>

namespace isc {
namespace xfr {

#define FD_BUFFER_CREATE(n) \
    struct { \
        struct cmsghdr h; \
        int fd[n]; \
    }

int
recv_fd(int sock);

int
send_fd(int sock, int fd);

} // End for namespace xfr
} // End for namespace isc
