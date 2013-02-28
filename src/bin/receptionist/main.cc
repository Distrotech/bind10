#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/epoll.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>

// This is experimental code. There's only minimal error handling,
// hardcoded values, no tests, etc. In short, it doesn't work :-P
// We also avoid use of asio, etc. It doesn't allow us to use
// low-level fancy functions like sendmmsg. Also, writing the code
// fast is easier with the low-level (though unsafe, less portable
// and harder to maintain) interface.
//
// Ah, also, many calls here are modern-linux specific.

using namespace std;

namespace {

// Configuration constants
const uint16_t listen_port = 5310;
const uint16_t up_port = 5311;
const uint16_t answer_port = 5312;
const size_t conn_count = 1;
const size_t event_cnt = 10;
const size_t buffer_size = 6553600;
const size_t msg_size = 65536;
const size_t maxmsg_count = 50;

struct epoll_handler {
    void (*handler)(size_t param);
    size_t param;
};

void udp_ready(size_t);

int udp_socket;
struct epoll_handler udp_handler = { udp_ready, 0 };
uint8_t udp_buffer[buffer_size * conn_count];
size_t udp_buff_pos = 0;
struct mmsghdr out_headers[maxmsg_count * conn_count];
struct iovec out_vectors[maxmsg_count * conn_count];
size_t out_hdrpos = 0;

struct upconn {
    int sock;
    struct epoll_handler handler;
    uint8_t buff[buffer_size];
    size_t buff_size;
};
struct upconn conns[conn_count];

int epoll_socket;

void
check(int ecode, const char *what, size_t line) {
    if (ecode == -1) {
        fprintf(stderr, "%s on line %zu failed: %m\n", what, line);
        abort();
    }
}

#define CHECK(WHAT) check((WHAT), #WHAT, __LINE__)

uint8_t udp_buffers[maxmsg_count][msg_size];
struct mmsghdr udp_headers[maxmsg_count];
struct iovec udp_vectors[maxmsg_count];
struct sockaddr_in6 remote_addr;

void udp_ready(size_t) {
    int result;
    CHECK(result = recvmmsg(udp_socket, udp_headers, maxmsg_count, MSG_DONTWAIT, NULL));
    for (size_t i = 0; i < result; i ++) {
        size_t upstream = random() % conn_count;
        uint16_t len = htons(udp_headers[i].msg_len);
        memcpy(conns[upstream].buff + conns[upstream].buff_size, &len, 2);
        conns[upstream].buff_size += 2;
        memcpy(conns[upstream].buff + conns[upstream].buff_size, udp_buffers[i], udp_headers[i].msg_len);
        conns[upstream].buff_size += udp_headers[i].msg_len;
    }
}

void upstream_ready(size_t index) {
    uint32_t length;
    CHECK(recv(conns[index].sock, &length, sizeof length, MSG_WAITALL));
    length = ntohl(length);
    CHECK(recv(conns[index].sock, udp_buffer + udp_buff_pos, length, MSG_WAITALL));
    size_t pos = 0;
    while (pos < length) {
        uint16_t msg_len;
        memcpy(&msg_len, udp_buffer + udp_buff_pos + pos, 2);
        msg_len = ntohs(msg_len);
        pos += 2;
        out_headers[out_hdrpos].msg_hdr.msg_iov[0].iov_base = udp_buffer + udp_buff_pos + pos;
        out_headers[out_hdrpos].msg_hdr.msg_iov[0].iov_len = msg_len;
        pos += msg_len;
        out_hdrpos ++;
    }
}

}


int main() {
    // Initialize buffers
    for (size_t i = 0; i < maxmsg_count; i ++) {
        udp_headers[i].msg_hdr.msg_iovlen = 1;
        udp_headers[i].msg_hdr.msg_iov = &udp_vectors[i];
        udp_vectors[i].iov_base = udp_buffers[i];
        udp_vectors[i].iov_len = msg_size;
    }
    // We expect all the queries to come from the same source. This is a trick
    // to pass the answer to the reply.
    udp_headers[0].msg_hdr.msg_name = &remote_addr;
    udp_headers[0].msg_hdr.msg_namelen = sizeof remote_addr;
    for (size_t i = 0; i < maxmsg_count * conn_count; i ++) {
        out_headers[i].msg_hdr.msg_iovlen = 1;
        out_headers[i].msg_hdr.msg_iov = &out_vectors[i];
        out_headers[i].msg_hdr.msg_namelen = sizeof remote_addr;
        out_headers[i].msg_hdr.msg_name = &remote_addr;
    }
    // Initialize epoll
    CHECK(epoll_socket = epoll_create(10));
    // Initialize the listening UDP socket
    CHECK(udp_socket = socket(AF_INET6, SOCK_DGRAM, 0));
    struct sockaddr_in6 addr;
    memset(&addr, 0, sizeof addr);
    addr.sin6_family = AF_INET6;
    addr.sin6_port = htons(listen_port);
    addr.sin6_addr = in6addr_any;
    CHECK(bind(udp_socket, (const struct sockaddr *) &addr, sizeof addr));
    addr.sin6_port = htons(answer_port);
    struct epoll_event event;
    event.events = EPOLLIN;
    event.data.ptr = &udp_handler;
    CHECK(epoll_ctl(epoll_socket, EPOLL_CTL_ADD, udp_socket, &event));
    // Initialize the upstream connections to the auth servers.
    addr.sin6_port = htons(up_port);
    addr.sin6_addr = in6addr_loopback;
    for (size_t i = 0; i < conn_count; i ++) {
        CHECK(conns[i].sock = socket(AF_INET6, SOCK_STREAM, 0));
        CHECK(connect(conns[i].sock, (const struct sockaddr *) &addr, sizeof addr));
        conns[i].handler.handler = upstream_ready;
        conns[i].handler.param = i;
        event.data.ptr = &conns[i].handler;
        CHECK(epoll_ctl(epoll_socket, EPOLL_CTL_ADD, conns[i].sock, &event));
        conns[i].buff_size = 4; // After the length header // After the length header
    }
    // Initialization complete. Handle events forever.
    for (;;) {
        struct epoll_event events[event_cnt];
        int epoll_result;
        CHECK(epoll_result = epoll_wait(epoll_socket, events, event_cnt, -1));
        // Handle the events we got now, reading everything (up to some maximal size)
        for (size_t i = 0; i < epoll_result; i ++) {
            struct epoll_handler *handler = (struct epoll_handler *) events[i].data.ptr;
            handler->handler(handler->param);
        }
        // Send all read data
        for (size_t i = 0; i < conn_count; i ++) {
            if (conns[i].buff_size == 4)
                continue;
            uint32_t size = htonl(conns[i].buff_size - 4);
            memcpy(conns[i].buff, &size, 4);
            size_t pos = 0;
            ssize_t result;
            while (pos < conns[i].buff_size) {
                CHECK(result = send(conns[i].sock, conns[i].buff + pos, conns[i].buff_size - pos, 0));
                pos += result;
            }
            conns[i].buff_size = 4;
        }
        size_t pos = 0;
        ssize_t result;
        while (pos < out_hdrpos) {
            CHECK(result = sendmmsg(udp_socket, out_headers + pos, out_hdrpos - pos, 0));
            pos += result;
        }
        out_hdrpos = 0;
        udp_buff_pos = 0;
    }
}
