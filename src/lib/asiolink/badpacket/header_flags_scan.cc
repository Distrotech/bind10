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

#include <iostream>
#include <iomanip>
#include <sstream>
#include <string>

#include <boost/scoped_ptr.hpp>

#include <asio.hpp>

#include <asiolink/io_address.h>
#include <asiolink/io_fetch.h>
#include <dns/buffer.h>
#include <dns/message.h>
#include <dns/messagerenderer.h>
#include <dns/opcode.h>
#include <dns/question.h>
#include <dns/rcode.h>
#include <dns/rrclass.h>
#include <dns/rrtype.h>
#include <log/strutil.h>

#include "header_flags_scan.h"

using namespace std;
using namespace asiolink;
using namespace isc::dns;
using namespace isc::strutil;

namespace isc {
namespace test {

// Perform the scan from start to end on a single question.
void
HeaderFlagsScan::scan(const std::string& qname) {

    // Create the message buffer to use
    Message message(Message::RENDER);
    message.setOpcode(Opcode::QUERY());
    message.setRcode(Rcode::NOERROR());
    message.addQuestion(Question(Name(qname), RRClass::IN(), RRType::A()));

    OutputBufferPtr msgbuf(new OutputBuffer(512));
    MessageRenderer renderer(*msgbuf);
    message.toWire(renderer);

    // Now loop through the settings sending the data
    if (start_ < end_) {
        for (uint16_t i = start_; i <= end_; ++i) {
            scanOne(msgbuf, i);
        }
    } else {
        for (uint16_t i = start_; i >= end_; --i) {
            scanOne(msgbuf, i);
        }
    }
}

// Perform the scan on a single value
void
HeaderFlagsScan::scanOne(isc::dns::OutputBufferPtr& msgbuf, uint16_t value) {

    // Set the flags field in the buffer and store the interpretation of it
    msgbuf->writeUint16At(value, 2);
    string fields = getFields(msgbuf);

    // Do the I/O, waiting for a reply
    OutputBufferPtr replybuf(new OutputBuffer(512));
    performIO(msgbuf, replybuf);

    string returned;
    switch (result_) {
    case IOFetch::SUCCESS:
        {
            // Parse the reply and get the fields
            returned = getFields(replybuf);

            // Put in a format suitable for returning
            lowercase(returned);
            returned = "SUCCESS: " + returned;
        }
        break;

    case IOFetch::TIME_OUT:
        returned = "TIMEOUT";
        break;

    case IOFetch::STOPPED:
        returned = "STOPPED";
        break;

    default:
        returned = "UNKNOWN - unknown fetch return code";
    }
        

    // ... and output the result
    cout << std::dec << setw(5) << value << " " << fields <<
            "    " << returned << "\n";
}

// Get interpretation of the message fields.
//
// This takes the second and third bytes of the passed buffer and interprets
// the values.  A summary string listing them is returned.
std::string
HeaderFlagsScan::getFields(isc::dns::OutputBufferPtr& msgbuf) {
    struct header {
        unsigned int rc : 4;
        unsigned int cd : 1;
        unsigned int ad : 1;
        unsigned int rs : 1;    // Reserved
        unsigned int ra : 1;
        unsigned int rd : 1;
        unsigned int tc : 1;
        unsigned int aa : 1;
        unsigned int op : 4;
        unsigned int qr : 1;
    };

    union {
        uint16_t        value;
        struct header   fields;
    } flags;

    // Extract the flags field from the buffer
    InputBuffer inbuf(msgbuf->getData(), msgbuf->getLength());
    inbuf.setPosition(2);
    flags.value = inbuf.readUint16();

    std::ostringstream os;
    os << std::hex << std::uppercase <<
        "QR:" << flags.fields.qr << " " <<
        "OP:" << flags.fields.op << " " <<
        "AA:" << flags.fields.aa << " " <<
        "TC:" << flags.fields.tc << " " <<
        "RD:" << flags.fields.rd << " " <<
        "RA:" << flags.fields.ra << " " <<
        "RS:" << flags.fields.rs << " " <<
        "AD:" << flags.fields.ad << " " <<
        "CD:" << flags.fields.cd << " " <<
        "RC:" << flags.fields.rc;
    return (os.str());
}

// Perform the I/O
void
HeaderFlagsScan::performIO(OutputBufferPtr& sendbuf, OutputBufferPtr& recvbuf) {

    // Set up an I/O service for the I/O.  This needs to be initialized before
    // every call as the callback when the I/O completes stops it.
    service_.reset(new IOService());

    // The object that will do the I/O
    IOFetch fetch(IOFetch::UDP, *service_, sendbuf, address_, port_, recvbuf,
                  this, timeout_);

    // Execute the I/O
    (service_->get_io_service()).post(fetch); 
    service_->run();
}

// I/O Callback
void
HeaderFlagsScan::operator()(IOFetch::Result result) {

    // Record the result.  This is accessed when deciding what was returned
    // (if a timeout, nothing was returned).
    result_ = result;

    // Stop the I/O service.  This will cause the call to run() to return.
    service_->stop();
}



} // namespace test
} // namespace isc
