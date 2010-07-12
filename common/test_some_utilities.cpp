#include <iostream>
#include <boost/shared_ptr.hpp>
#include <boost/asio.hpp>

#include "udp_buffer.h"
#include "utilities.h"

namespace ip = boost::asio::ip;

int main(int argc, char** argv) {

    // Create a UDP buffer endpoint.

    ip::address addr = ip::address::from_string("8.4.2.1");
    unsigned short port = 42;
    ip::udp::endpoint ep(addr, port);

    boost::shared_array<uint8_t> array =
        boost::shared_array<uint8_t>(new uint8_t[65536]);

    UdpBuffer buffer(ep, 20, 65536, array);

    // Transform the buffer
    Utilities::AppendEndpoint(buffer);
    std::cout << "New allocated size = " << buffer.size << "\n";

    // Clear the endpoint
    buffer.endpoint = ip::udp::endpoint();

    // Transform the data back
    Utilities::ExtractEndpoint(buffer);
    std::cout << "New allocated size = " << buffer.size << "\n";
    std::cout << "IP address " <<
        buffer.endpoint.address().to_string() << "\n";
    std::cout << "Port " << buffer.endpoint.port() << "\n";

    return 0;


}
