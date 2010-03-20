#include <cstdlib>
#include <cstring>
#include <iostream>
#include <boost/asio.hpp>
#include "fd_share.h"
#include "xfrout_client.h"

using boost::asio::local::stream_protocol;

namespace isc {
namespace xfr {

void
XfroutClient::connect()
{
    socket_.connect(stream_protocol::endpoint(file_path_));
}

void
XfroutClient::disconnect()
{
    socket_.close();
}

int 
XfroutClient::sendXfroutRequestInfo(int tcp_sock, uint8_t *msg_data, uint16_t  msg_len)
{
    send_fd(socket_.native(), tcp_sock); //TODO, need send
    send(socket_.native(), (uint8_t *)&msg_len, 2, 0);
    send(socket_.native(), msg_data, msg_len, 0);
    close(tcp_sock);
    return 0;
}

} // End for xfr
} // End for isc

