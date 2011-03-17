// Copyright (C) 2011  Internet Systems Consortium, Inc. ("ISC")
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice appear in all copies.
//
// THE SOFTWARE IS PROVIDED "AS IS" AND ISC DISCLAIMS ALL WARRANTIES WITH
// REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY
// AND FITNESS.  IN NO EVENT SHALL ISC BE LIABLE FOR ANY SPECIAL, DIRECT,
// INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM
// LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE
// OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
// PERFORMANCE OF THIS SOFTWARE.

#ifndef __HEADER_FLAG_SCAN_H
#define __HEADER_FLAG_SCAN_H

#include <algorithm>
#include <string>

#include <config.h>

#include <asiolink/io_address.h>
#include <asiolink/io_fetch.h>
#include <asiolink/io_service.h>
#include <dns/buffer.h>
#include <dns/message.h>

namespace isc {
namespace test {

/// \brief Header Flags Scan
///
/// This class implements a header flag scan.  It treats the DNS packet header
/// flags as a 16-bit value then cycles through values sending and receiving
/// packets.

class HeaderFlagsScan : public asiolink::IOFetch::Callback {
public:

    /// \brief Constructor
    ///
    /// \param address Address of target nameserver
    /// \param port Port to use
    /// \param timeout Query timeout (in ms)
    /// \param start Start value for the scan
    /// \param end End value for the scan
    HeaderFlagsScan(std::string address = "127.0.0.1", uint16_t port = 53,
                    int timeout = 500, uint16_t start = 0,
                    uint16_t end = 65535) :
        service_(), result_(), address_(address), port_(port),
        timeout_(timeout), start_(start), end_(end)
    {}

    /// \brief Run Scan
    ///
    /// \param qname Name in the query being mangled.  The query will be for
    ///        IN/A.
    void scan(const std::string& qname);

    /// \brief Perform I/O
    ///
    /// Performs a single query to the nameserver and reads the response.  It
    /// outputs a summary of the result.
    ///
    /// \param sendbuf Buffer sent to the nameserver
    /// \param recvbuf Buffer to hold reply from the nameserver
    void performIO(isc::dns::OutputBufferPtr& sendbuf, isc::dns::OutputBufferPtr& recvbuf);
    
    /// \brief Callback
    ///
    /// When an I/O to the nameserver has completed, this method will be called
    virtual void operator()(asiolink::IOFetch::Result result);

private:
    /// \brief Scan One Value
    ///
    /// \param msgbuf Message that will be sent to the remote nameserver.  The
    ///        flags field (and QID) will be modified in the packet sent.
    /// \param value Single value used in this scan.
    void scanOne(isc::dns::OutputBufferPtr& msgbuf, uint16_t value);

    /// \brief Get Fields
    ///
    /// \param msg Message for which the header is value
    ///
    /// \return A string that holds a textual interpretation of all the fields
    ///         in the header.
    std::string getFields(isc::dns::OutputBufferPtr& msgbuf);

    // Member variables

    boost::scoped_ptr<asiolink::IOService> service_;///< Service to run the scan
    asiolink::IOFetch::Result   result_;            ///< Result of the I/O
    asiolink::IOAddress         address_;           ///< Address of nameserver
    uint16_t                    port_;              ///< Port on nameserver to use
    int                         timeout_;           ///< Query timeout (in ms)
    uint16_t                    start_;             ///< Start of scan value
    uint16_t                    end_;               ///< End of scan value
};

} // namespace test
} // namespace isc

#endif // __HEADER_FLAG_SCAN_H
