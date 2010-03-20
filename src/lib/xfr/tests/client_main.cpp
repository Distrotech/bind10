
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <boost/asio.hpp>
#include "fd_share.h"
#include "xfrout_client.h"
using namespace isc::xfr;

void
create_tcp_socket(int *sockp)
{
    int fd = 0;
    struct sockaddr_in addr;
    fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd  < 0)
        printf("create tcp socket error");

    bzero(&addr, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(8080);
    inet_pton(AF_INET, "localhost", &addr.sin_addr);
    int ret = connect(fd, (struct sockaddr *)&addr, sizeof(addr));
    if (ret < 0)
        printf("connect server error");

    char *str = "client one send: data";
    write(fd, str, strlen(str));
    *sockp = fd;
}


int main(int argc, char* argv[])
{
  try
  {
    int tcp_sock = 0;
    create_tcp_socket(&tcp_sock);

    boost::asio::io_service io_serv;
    printf("%s\n", argv[1]);
    XfroutClient client(io_serv, argv[1]);
    client.connect();

    char *msgp = "xfr request from client";
    client.sendXfroutRequestInfo(tcp_sock, (uint8_t *)msgp, (uint16_t)strlen(msgp));
    printf("sock:%d: msglen:%d\n", tcp_sock, strlen(msgp));
    printf("mesasge:%s\n", msgp);
  }
  catch (std::exception& e)
  {
    std::cerr << "Exception: " << e.what() << "\n";
  }

  return 0;
}

