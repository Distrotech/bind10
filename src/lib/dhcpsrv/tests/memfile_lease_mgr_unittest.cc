// Copyright (C) 2012-2014 Internet Systems Consortium, Inc. ("ISC")
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

#include <config.h>

#include <asiolink/io_address.h>
#include <dhcp/duid.h>
#include <dhcpsrv/cfgmgr.h>
#include <dhcpsrv/lease_mgr.h>
#include <dhcpsrv/lease_mgr_factory.h>
#include <dhcpsrv/memfile_lease_mgr.h>
#include <dhcpsrv/tests/lease_file_io.h>
#include <dhcpsrv/tests/test_utils.h>
#include <dhcpsrv/tests/generic_lease_mgr_unittest.h>
#include <gtest/gtest.h>

#include <iostream>
#include <sstream>

using namespace std;
using namespace isc;
using namespace isc::asiolink;
using namespace isc::dhcp;
using namespace isc::dhcp::test;

namespace {

// empty class for now, but may be extended once Addr6 becomes bigger
class MemfileLeaseMgrTest : public GenericLeaseMgrTest {
public:

    /// @brief memfile lease mgr test constructor
    ///
    /// Creates memfile and stores it in lmptr_ pointer
    MemfileLeaseMgrTest() :
        io4_(getLeaseFilePath("leasefile4_0.csv")),
        io6_(getLeaseFilePath("leasefile6_0.csv")) {

        // Make sure there are no dangling files after previous tests.
        io4_.removeFile();
        io6_.removeFile();
    }

    /// @brief Reopens the connection to the backend.
    ///
    /// This function is called by unit tests to force reconnection of the
    /// backend to check that the leases are stored and can be retrieved
    /// from the storage.
    ///
    /// @param u Universe (V4 or V6)
    virtual void reopen(Universe u) {
        LeaseMgrFactory::destroy();
        startBackend(u);
    }

    /// @brief destructor
    ///
    /// destroys lease manager backend.
    virtual ~MemfileLeaseMgrTest() {
        LeaseMgrFactory::destroy();
    }

    /// @brief Return path to the lease file used by unit tests.
    ///
    /// @param filename Name of the lease file appended to the path to the
    /// directory where test data is held.
    ///
    /// @return Full path to the lease file.
    static std::string getLeaseFilePath(const std::string& filename) {
        std::ostringstream s;
        s << TEST_DATA_BUILDDIR << "/" << filename;
        return (s.str());
    }

    /// @brief Returns the configuration string for the backend.
    ///
    /// This string configures the @c LeaseMgrFactory to create the memfile
    /// backend and use leasefile4_0.csv and leasefile6_0.csv files as
    /// storage for leases.
    ///
    /// @param no_universe Indicates whether universe parameter should be
    /// included (false), or not (true).
    ///
    /// @return Configuration string for @c LeaseMgrFactory.
    static std::string getConfigString(Universe u) {
        std::ostringstream s;
        s << "type=memfile " << (u == V4 ? "universe=4 " : "universe=6 ")
          << "name="
          << getLeaseFilePath(u == V4 ? "leasefile4_0.csv" : "leasefile6_0.csv");
        return (s.str());
    }

    /// @brief Creates instance of the backend.
    ///
    /// @param u Universe (v4 or V6).
    void startBackend(Universe u) {
        try {
            LeaseMgrFactory::create(getConfigString(u));
        } catch (...) {
            std::cerr << "*** ERROR: unable to create instance of the Memfile\n"
                " lease database backend.\n";
            throw;
        }
        lmptr_ = &(LeaseMgrFactory::instance());
    }

    /// @brief Object providing access to v4 lease IO.
    LeaseFileIO io4_;

    /// @brief Object providing access to v6 lease IO.
    LeaseFileIO io6_;

};

// This test checks if the LeaseMgr can be instantiated and that it
// parses parameters string properly.
TEST_F(MemfileLeaseMgrTest, constructor) {
    LeaseMgr::ParameterMap pmap;
    pmap["universe"] = "4";
    pmap["persist"] = "false";
    boost::scoped_ptr<Memfile_LeaseMgr> lease_mgr;

    EXPECT_NO_THROW(lease_mgr.reset(new Memfile_LeaseMgr(pmap)));

    pmap["persist"] = "true";
    pmap["name"] = getLeaseFilePath("leasefile4_1.csv");
    EXPECT_NO_THROW(lease_mgr.reset(new Memfile_LeaseMgr(pmap)));

    // Expecting that persist parameter is yes or no. Everything other than
    // that is wrong.
    pmap["persist"] = "bogus";
    pmap["name"] = getLeaseFilePath("leasefile4_1.csv");
    EXPECT_THROW(lease_mgr.reset(new Memfile_LeaseMgr(pmap)), isc::BadValue);
}

// Checks if the getType() and getName() methods both return "memfile".
TEST_F(MemfileLeaseMgrTest, getTypeAndName) {
    startBackend(V4);
    EXPECT_EQ(std::string("memfile"), lmptr_->getType());
    EXPECT_EQ(std::string("memory"),  lmptr_->getName());
}

// Checks if the path to the lease files is initialized correctly.
TEST_F(MemfileLeaseMgrTest, getLeaseFilePath) {
    // Initialize IO objects, so as the test csv files get removed after the
    // test (when destructors are called).
    LeaseFileIO io4(getLeaseFilePath("leasefile4_1.csv"));
    LeaseFileIO io6(getLeaseFilePath("leasefile6_1.csv"));

    LeaseMgr::ParameterMap pmap;
    pmap["universe"] = "4";
    pmap["name"] = getLeaseFilePath("leasefile4_1.csv");
    boost::scoped_ptr<Memfile_LeaseMgr> lease_mgr(new Memfile_LeaseMgr(pmap));

    EXPECT_EQ(pmap["name"],
              lease_mgr->getLeaseFilePath(Memfile_LeaseMgr::V4));

    pmap["persist"] = "false";
    lease_mgr.reset(new Memfile_LeaseMgr(pmap));
    EXPECT_TRUE(lease_mgr->getLeaseFilePath(Memfile_LeaseMgr::V4).empty());
    EXPECT_TRUE(lease_mgr->getLeaseFilePath(Memfile_LeaseMgr::V6).empty());
}

// Check if the persitLeases correctly checks that leases should not be written
// to disk when disabled through configuration.
TEST_F(MemfileLeaseMgrTest, persistLeases) {
    // Initialize IO objects, so as the test csv files get removed after the
    // test (when destructors are called).
    LeaseFileIO io4(getLeaseFilePath("leasefile4_1.csv"));
    LeaseFileIO io6(getLeaseFilePath("leasefile6_1.csv"));

    LeaseMgr::ParameterMap pmap;
    pmap["universe"] = "4";
    // Specify the names of the lease files. Leases will be written.
    pmap["name"] = getLeaseFilePath("leasefile4_1.csv");
    boost::scoped_ptr<Memfile_LeaseMgr> lease_mgr(new Memfile_LeaseMgr(pmap));

    lease_mgr.reset(new Memfile_LeaseMgr(pmap));
    EXPECT_TRUE(lease_mgr->persistLeases(Memfile_LeaseMgr::V4));
    EXPECT_FALSE(lease_mgr->persistLeases(Memfile_LeaseMgr::V6));

    pmap["universe"] = "6";
    pmap["name"] = getLeaseFilePath("leasefile6_1.csv");
    lease_mgr.reset(new Memfile_LeaseMgr(pmap));
    EXPECT_FALSE(lease_mgr->persistLeases(Memfile_LeaseMgr::V4));
    EXPECT_TRUE(lease_mgr->persistLeases(Memfile_LeaseMgr::V6));

    // This should disable writes of leases to disk.
    pmap["persist"] = "false";
    lease_mgr.reset(new Memfile_LeaseMgr(pmap));
    EXPECT_FALSE(lease_mgr->persistLeases(Memfile_LeaseMgr::V4));
    EXPECT_FALSE(lease_mgr->persistLeases(Memfile_LeaseMgr::V6));
}


// Checks that adding/getting/deleting a Lease6 object works.
TEST_F(MemfileLeaseMgrTest, addGetDelete6) {
    startBackend(V6);
    testAddGetDelete6(true); // true - check T1,T2 values
    // memfile is able to preserve those values, but some other
    // backends can't do that.
}

/// @brief Basic Lease4 Checks
///
/// Checks that the addLease, getLease4 (by address) and deleteLease (with an
/// IPv4 address) works.
TEST_F(MemfileLeaseMgrTest, basicLease4) {
    startBackend(V4);
    testBasicLease4();
}

/// @todo Write more memfile tests

// Simple test about lease4 retrieval through client id method
TEST_F(MemfileLeaseMgrTest, getLease4ClientId) {
    startBackend(V4);
    testGetLease4ClientId();
}

// Checks that lease4 retrieval client id is null is working
TEST_F(MemfileLeaseMgrTest, getLease4NullClientId) {
    startBackend(V4);
    testGetLease4NullClientId();
}

// Checks lease4 retrieval through HWAddr
TEST_F(MemfileLeaseMgrTest, getLease4HWAddr1) {
    startBackend(V4);
    testGetLease4HWAddr1();
}

/// @brief Check GetLease4 methods - access by Hardware Address
///
/// Adds leases to the database and checks that they can be accessed via
/// a combination of DUID and IAID.
TEST_F(MemfileLeaseMgrTest, getLease4HWAddr2) {
    startBackend(V4);
    testGetLease4HWAddr2();
}

// Checks lease4 retrieval with clientId, HWAddr and subnet_id
TEST_F(MemfileLeaseMgrTest, getLease4ClientIdHWAddrSubnetId) {
    startBackend(V4);
    testGetLease4ClientIdHWAddrSubnetId();
}

/// @brief Basic Lease4 Checks
///
/// Checks that the addLease, getLease4(by address), getLease4(hwaddr,subnet_id),
/// updateLease4() and deleteLease (IPv4 address) can handle NULL client-id.
/// (client-id is optional and may not be present)
TEST_F(MemfileLeaseMgrTest, lease4NullClientId) {
    startBackend(V4);
    testLease4NullClientId();
}

/// @brief Check GetLease4 methods - access by Hardware Address & Subnet ID
///
/// Adds leases to the database and checks that they can be accessed via
/// a combination of hardware address and subnet ID
TEST_F(MemfileLeaseMgrTest, DISABLED_getLease4HwaddrSubnetId) {

    /// @todo: fails on memfile. It's probably a memfile bug.
    startBackend(V4);
    testGetLease4HWAddrSubnetId();
}

/// @brief Check GetLease4 methods - access by Client ID
///
/// Adds leases to the database and checks that they can be accessed via
/// the Client ID.
TEST_F(MemfileLeaseMgrTest, getLease4ClientId2) {
    startBackend(V4);
    testGetLease4ClientId2();
}

// @brief Get Lease4 by client ID
//
// Check that the system can cope with a client ID of any size.
TEST_F(MemfileLeaseMgrTest, getLease4ClientIdSize) {
    startBackend(V4);
    testGetLease4ClientIdSize();
}

/// @brief Check GetLease4 methods - access by Client ID & Subnet ID
///
/// Adds leases to the database and checks that they can be accessed via
/// a combination of client and subnet IDs.
TEST_F(MemfileLeaseMgrTest, getLease4ClientIdSubnetId) {
    startBackend(V4);
    testGetLease4ClientIdSubnetId();
}

/// @brief Basic Lease6 Checks
///
/// Checks that the addLease, getLease6 (by address) and deleteLease (with an
/// IPv6 address) works.
TEST_F(MemfileLeaseMgrTest, basicLease6) {
    startBackend(V6);
    testBasicLease6();
}


/// @brief Check GetLease6 methods - access by DUID/IAID
///
/// Adds leases to the database and checks that they can be accessed via
/// a combination of DUID and IAID.
/// @todo: test disabled, because Memfile_LeaseMgr::getLeases6(Lease::Type,
/// const DUID& duid, uint32_t iaid) const is not implemented yet.
TEST_F(MemfileLeaseMgrTest, DISABLED_getLeases6DuidIaid) {
    startBackend(V6);
    testGetLeases6DuidIaid();
}

// Check that the system can cope with a DUID of allowed size.

/// @todo: test disabled, because Memfile_LeaseMgr::getLeases6(Lease::Type,
/// const DUID& duid, uint32_t iaid) const is not implemented yet.
TEST_F(MemfileLeaseMgrTest, DISABLED_getLeases6DuidSize) {
    startBackend(V6);
    testGetLeases6DuidSize();
}

/// @brief Check that getLease6 methods discriminate by lease type.
///
/// Adds six leases, two per lease type all with the same duid and iad but
/// with alternating subnet_ids.
/// It then verifies that all of getLeases6() method variants correctly
/// discriminate between the leases based on lease type alone.
/// @todo: Disabled, because type parameter in Memfile_LeaseMgr::getLease6
/// (Lease::Type, const isc::asiolink::IOAddress& addr) const is not used.
TEST_F(MemfileLeaseMgrTest, DISABLED_lease6LeaseTypeCheck) {
    startBackend(V6);
    testLease6LeaseTypeCheck();
}

/// @brief Check GetLease6 methods - access by DUID/IAID/SubnetID
///
/// Adds leases to the database and checks that they can be accessed via
/// a combination of DIUID and IAID.
TEST_F(MemfileLeaseMgrTest, getLease6DuidIaidSubnetId) {
    startBackend(V6);
    testGetLease6DuidIaidSubnetId();
}

/// Checks that getLease6(type, duid, iaid, subnet-id) works with different
/// DUID sizes
TEST_F(MemfileLeaseMgrTest, getLease6DuidIaidSubnetIdSize) {
    startBackend(V6);
    testGetLease6DuidIaidSubnetIdSize();
}

/// @brief Lease4 update tests
///
/// Checks that we are able to update a lease in the database.
/// @todo: Disabled, because memfile does not throw when lease is updated.
/// We should reconsider if lease{4,6} structures should have a limit
/// implemented in them.
TEST_F(MemfileLeaseMgrTest, DISABLED_updateLease4) {
    startBackend(V4);
    testUpdateLease4();
}

/// @brief Lease6 update tests
///
/// Checks that we are able to update a lease in the database.
/// @todo: Disabled, because memfile does not throw when lease is updated.
/// We should reconsider if lease{4,6} structures should have a limit
/// implemented in them.
TEST_F(MemfileLeaseMgrTest, DISABLED_updateLease6) {
    startBackend(V6);
    testUpdateLease6();
}

/// @brief DHCPv4 Lease recreation tests
///
/// Checks that the lease can be created, deleted and recreated with
/// different parameters. It also checks that the re-created lease is
/// correctly stored in the lease database.
TEST_F(MemfileLeaseMgrTest, testRecreateLease4) {
    startBackend(V4);
    testRecreateLease4();
}

/// @brief DHCPv6 Lease recreation tests
///
/// Checks that the lease can be created, deleted and recreated with
/// different parameters. It also checks that the re-created lease is
/// correctly stored in the lease database.
TEST_F(MemfileLeaseMgrTest, testRecreateLease6) {
    startBackend(V6);
    testRecreateLease6();
}

// The following tests are not applicable for memfile. When adding
// new tests to the list here, make sure to provide brief explanation
// why they are not applicable:
//
// testGetLease4HWAddrSubnetIdSize() - memfile just keeps Lease structure
//     and does not do any checks of HWAddr content

}; // end of anonymous namespace
