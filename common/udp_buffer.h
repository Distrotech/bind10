// Copyright (C) 2010  Internet Systems Consortium, Inc. ("ISC")
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

// $Id$

#ifndef __DATA_PACKET_H
#define __DATA_PACKET_H

#include <stdint.h>
#include <arpa/inet.h>
#include <boost/asio.hpp>
#include <boost/shared_array.hpp>

#include <algorithm>

#include "defaults.h"
#include "exception.h"


/// \brief Udp Buffer
///
/// Encapsulates the data sent between the various processes in the receptionist
/// tests.
///
/// The packet layout is fixed and comprises
///
/// - (size bytes - 14) (where size is the requested size): the data area
/// - 4 bytes: CRC of the data area (in network byte order)
/// - 4 bytes: Packet number (in network byte order)
/// - 4 bytes: IPV4 address (in network byte order)
/// - 2 bytes: IPV4 port (in network byte order)
///
/// The interpretation of the data - which fields are relevant etc. - is up
/// to the program.
///
/// When constructed, the size of the packet that may be specified is no
/// more than UDP_BUFFER_SIZE - 14;

class UdpBuffer {
public:

    /// \brief Creates the packet
    ///
    /// All packets are created with a capacity equal to UDP_BUFFER_SIZE.
    /// The amount of data can be set via setSize() or implicity by init().
    UdpBuffer() : size_(0), capacity_(UDP_BUFFER_SIZE)
    {
        if (capacity_ > UDP_BUFFER_SIZE) {
            throw PacketTooLarge();
        }

        data_ = boost::shared_array<uint8_t>(new uint8_t[UDP_BUFFER_SIZE]);
    }

    /// The default copy and assignment constructors are OK here although
    /// "A = B" does mean that A and B will point to the same data.  To do
    /// a deep copy, use clone.
    UdpBuffer clone() const;

    /// \brief Initialize Packet
    ///
    /// Initializes the data part of the packet to a sequence of increasing
    /// integers.
    void init(size_t size) {

        // Silently ensure that at least one byte of data will be sent,
        // but that the buffer can hold the data and overhead.
        size_ = std::min(std::max(size, static_cast<size_t>(15)),
            UDP_BUFFER_SIZE);
        for (int i = 0; i < dataSize(); ++i) {
            data_.get()[i] = (i % 256);
        }
    }

    /// \return Pointer to data area.  This is only valid for as long as the
    /// UdpBuffer object is in existence.
    uint8_t* data() {
        return data_.get();
    }

    /// \return Pointer to data area.  This is only valid for as long as the
    /// UdpBuffer object is in existence.
    const uint8_t* data() const {
        return data_.get();
    }

    /// \return Size of the packet
    size_t size() const {
        return (size_);
    }

    /// \return Size of the data in the packet
    size_t dataSize() const {
        return (size() - 14);
    }

    /// \return Capacity of the packet
    size_t capacity() const {
        return (capacity_);
    }

    /// \brief Clear the buffer
    ///
    /// Zeroes the contents of the buffer.
    void zero() {
        memset(data_.get(), 0, capacity());
    }

    /// \brief Sets Size
    ///
    /// If the buffer is used to receive data, the amount of data received is
    /// set here.  As the data is assumed to follow the layout here, the
    /// size() and dataSize() methods will subsequently return correct values.
    /// \param szie Amount of data received.
    void setSize(size_t size) {
        size_ = size;
    }

    // The following methods get and set data in the packet.  No constants
    // are defined for offsets (though they should be), but they are:
    //
    // 0                 Data area of the packet
    // size_ - 14        CRC
    // size_ - 10        Packet number
    // size_ -  6        IPV4 address
    // size_ -  2        Port

    /// \brief Set CRC
    ///
    /// \param crc CRC to set.  Supplied in host byte order this is stored in
    /// network byte order.
    void setCrc(uint32_t crc) {
        setData(size() - 14, htonl(crc));
    }

    /// \return CRC of the data packet in host byte order.
    uint32_t getCrc() const {
        uint32_t crc;
        getData(size() - 14, crc);
        return ntohl(crc);
    }

    /// \brief Set Packet Number
    ///
    /// \param number Number to set the packet number to.  This is provided
    ///  in host byte order but stored in network byte order.
    void setPacketNumber(uint32_t number) {
        setData(size() - 10, htonl(number));
    }

    /// \return Packet number in host byte order
    uint32_t getPacketNumber() const {
        uint32_t    number;
        getData(size() - 10, number);
        return ntohl(number);
    }

    /// \brief Set IPV4 Address
    ///
    /// \param address IPV4 address in host byte order (stored in network byte
    /// order).
    void setAddress(uint32_t address) {
        setData(size() - 6, htonl(address));
    }

    /// \return IPV4 address in host byte order
    uint32_t getAddress() const {
        uint32_t    address;
        getData(size() - 6, address);
        return ntohl(address);
    }

    /// \brief Set IPV4 Port
    ///
    /// \param port Port number in host byte order (stored in network byte
    /// order).
    void setPort(uint16_t port) {
        setData(size() - 2, htons(port));
    }

    /// \return Port number in host byte order
    uint16_t getPort() const {
        uint16_t    port;
        getData(size() - 2, port);
        return ntohs(port);
    }

    // Endpoint Access

    /// \brief Set Endpoint Information
    ///
    /// Given a UDP endpoint, add the information to the data packet.
    /// \param ep Endpoint holding requeted information
    void setAddressInfo(const boost::asio::ip::udp::endpoint& ep);

    /// \return Endpoint of Data Packet
    boost::asio::ip::udp::endpoint getAddressInfo() const;


    // Comparison functions

    /// \return If the data in two packets is the same
    bool compareData(const UdpBuffer& other) const {
        return (
            (size() == other.size()) &&
                (memcmp(static_cast<const void*>(data_.get()),
                    static_cast<const void*>(other.data_.get()),
                    dataSize()) == 0)
        );
    }

    /// \return If the CRCs are the same
    bool compareCrc(const UdpBuffer& other) {
        return (getCrc() == other.getCrc());
    }
                


private:

    /// \brief Get data from packet as a fundamental data type
    ///
    /// \param T template data type required
    /// \param offset Offset into the data packet for the data 
    /// \param result Data requested.  Supplied as a parameter for template
    /// signature reasons.
    template <typename T> void getData(size_t offset, T& result) const {
        memmove(static_cast<void*>(&result),
            static_cast<const void*>(&(data_.get()[offset])), sizeof(T));
        return;
    }

    /// \brief Set data in packet from fundamental data type
    ///
    /// \param T template data type required
    /// \param offset Offset into the data packet for the data 
    /// \param value Data to be inserted into the packet
    template <typename T> void setData(size_t offset, const T& value) {
        memmove(static_cast<void*>(&(data_.get()[offset])),
            static_cast<const void*>(&value), sizeof(T));
        return;
    }

private:

    // Although vector contains a size() method, to prevent reallocation
    // of memory the size of the vector is initialized on construction and
    // never changed; we keep tabs on how much is used.  Therefore:
    //
    // data_.size() is always equal to the amount requested plus 6.
    // size_ is the amount of data in the packet, and is equal to data_.size()
    // if the address and port information has been added to the packet, and
    // data_.size() - 6 if not.
    //

    boost::asio::ip::udp::endpoint  endpoint_;  //< Socket end point
    size_t                          size_;      //< Amount of data in buffer
    size_t                          capacity_;  //< Maximum size of buffer
    boost::shared_array<uint8_t>    data_;      //< Data in the buffer

};

#endif // __DATA_PACKET_H
