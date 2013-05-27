// Copyright (C) 2013  Internet Systems Consortium, Inc. ("ISC")
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

#include <util/hooks/callout_handle.h>
#include <util/hooks/library_handle.h>
#include <util/hooks/server_hooks.h>

#include <gtest/gtest.h>

using namespace isc::util;
using namespace std;

// Dummy class for testing
namespace isc {
namespace util {
class HookManager {};
}
}

namespace {

class LibraryHandleTest : public ::testing::Test {
public:
    /// @brief Constructor
    ///
    /// Sets up an appropriate number of server hooks to pass to the
    /// constructed callout handle objects.
    LibraryHandleTest()
        : hooks_(new ServerHooks()), manager_(new HookManager()) {
        hooks_->registerHook("alpha");
        hooks_->registerHook("beta");
        hooks_->registerHook("gamma");

        // Also initialize the callout variables.
        one_count = 0;
        two_count = 0;
        callout_value = 0;
    }

    /// Obtain constructed server hooks
    boost::shared_ptr<ServerHooks>& getServerHooks() {
        return (hooks_);
    }

    // Obtain constructed hook manager
    boost::shared_ptr<HookManager>& getHookManager() {
        return (manager_);
    }

    /// Variables for callouts test. These are public and static to allow non-
    /// member functions to access them, but declared as class variables to
    /// allow initialization every time the test starts.
    static int one_count;
    static int two_count;
    static int callout_value;

private:
    boost::shared_ptr<ServerHooks> hooks_;
    boost::shared_ptr<HookManager> manager_;
};

// Definition of the static variables.
int LibraryHandleTest::one_count = 0;
int LibraryHandleTest::two_count = 0;
int LibraryHandleTest::callout_value = 0;

// *** Context Tests ***
//
// The first set of tests check that the LibraryHandle can store and retrieve
// context.

// Test that we can store multiple values of the same type and that they
// are distinct.

TEST_F(LibraryHandleTest, ContextDistinctSimpleType) {
    LibraryHandle handle(getServerHooks(), 1);

    // Store and retrieve an int (random value).
    int a = 42;
    handle.setContext("integer1", a);
    EXPECT_EQ(42, a);

    int b = 0;
    handle.getContext("integer1", b);
    EXPECT_EQ(42, b);

    // Add another integer (another random value).
    int c = 142;
    handle.setContext("integer2", c);
    EXPECT_EQ(142, c);

    int d = -1;
    handle.getContext("integer2", d);
    EXPECT_EQ(142, d);

    // Add a short (random value).
    short e = 81; 
    handle.setContext("short", e);
    EXPECT_EQ(81, e);

    short f = -1;
    handle.getContext("short", f);
    EXPECT_EQ(81, f);
}

// Test that trying to get something with an incorrect name throws an
// exception.

TEST_F(LibraryHandleTest, ContextUnknownName) {
    LibraryHandle handle(getServerHooks(), 1);

    // Set an integer
    int a = 42;
    handle.setContext("integer1", a);
    EXPECT_EQ(42, a);

    // Check we can retrieve it
    int b = 0;
    handle.getContext("integer1", b);
    EXPECT_EQ(42, b);

    // Check that getting an unknown name throws an exception.
    int c = -1;
    EXPECT_THROW(handle.getContext("unknown", c), NoSuchContext);
}

// Test that trying to get something with an incorrect type throws an exception.

TEST_F(LibraryHandleTest, ContextIncorrectType) {
    LibraryHandle handle(getServerHooks(), 1);

    // Set an integer
    int a = 42;
    handle.setContext("integer1", a);
    EXPECT_EQ(42, a);

    // Check we can retrieve it
    long b = 0;
    EXPECT_THROW(handle.getContext("integer1", b), boost::bad_any_cast);
}

// Now try with some very complex types.  The types cannot be defined within
// the function and they should contain a copy constructor.  For this reason,
// a simple "struct" is used.

struct Alpha {
    int a;
    int b;
    Alpha(int first = 0, int second = 0) : a(first), b(second) {}
};

struct Beta {
    int c;
    int d;
    Beta(int first = 0, int second = 0) : c(first), d(second) {}
};

TEST_F(LibraryHandleTest, ComplexTypes) {
    LibraryHandle handle(getServerHooks(), 1);

    // Declare two variables of different (complex) types. (Note as to the
    // variable names: aleph and beth are the first two letters of the Hebrew
    // alphabet.)
    Alpha aleph(1, 2);
    EXPECT_EQ(1, aleph.a);
    EXPECT_EQ(2, aleph.b);
    handle.setContext("aleph", aleph);

    Beta beth(11, 22);
    EXPECT_EQ(11, beth.c);
    EXPECT_EQ(22, beth.d);
    handle.setContext("beth", beth);

    // Ensure we can extract the data correctly
    Alpha aleph2;
    EXPECT_EQ(0, aleph2.a);
    EXPECT_EQ(0, aleph2.b);
    handle.getContext("aleph", aleph2);
    EXPECT_EQ(1, aleph2.a);
    EXPECT_EQ(2, aleph2.b);

    Beta beth2;
    EXPECT_EQ(0, beth2.c);
    EXPECT_EQ(0, beth2.d);
    handle.getContext("beth", beth2);
    EXPECT_EQ(11, beth2.c);
    EXPECT_EQ(22, beth2.d);

    // Ensure that complex types also thrown an exception if we attempt to
    // get a context element of the wrong type.
    EXPECT_THROW(handle.getContext("aleph", beth), boost::bad_any_cast);
}

// Check that the context can store pointers. And also check that it respects
// that a "pointer to X" is not the same as a "pointer to const X".

TEST_F(LibraryHandleTest, PointerTypes) {
    LibraryHandle handle(getServerHooks(), 1);

    // Declare a couple of variables, const and non-const.
    Alpha aleph(5, 10);
    const Beta beth(15, 20);

    Alpha* pa = &aleph;
    const Beta* pcb = &beth;

    // Check pointers can be set and retrieved OK
    handle.setContext("non_const_pointer", pa);
    handle.setContext("const_pointer", pcb);

    Alpha* pa2 = 0;
    handle.getContext("non_const_pointer", pa2);
    EXPECT_TRUE(pa == pa2);

    const Beta* pcb2 = 0;
    handle.getContext("const_pointer", pcb2);
    EXPECT_TRUE(pcb == pcb2);

    // Check that the "const" is protected in the context.
    const Alpha* pca3;
    EXPECT_THROW(handle.getContext("non_const_pointer", pca3),
                 boost::bad_any_cast);

    Beta* pb3;
    EXPECT_THROW(handle.getContext("const_pointer", pb3),
                 boost::bad_any_cast);
}

// *** Callout Tests ***
//
// The next set of tests check that callouts can be registered.

// Supply callouts structured in such a way that we can determine the order
// that they are called and whether they are called at all. In particular
// if the callout order is:
//
// * one followed by two, the resulting value is 20
// * two followed by one, the resuling value is -10
// * one and two is not called, the resulting value is 10
// * two and one is not called, the resulting value is -20
// * neither called, the resulting value is 0
//
// The variable xxx_count is the number of times the function has been called
// in the current test.

extern "C" {
int one(CalloutHandle&) {

    ++LibraryHandleTest::one_count;
    if (LibraryHandleTest::callout_value == 0) {
        LibraryHandleTest::callout_value = 10;
    } else {
        LibraryHandleTest::callout_value = -10;
    }

    return (0);
}

int two(CalloutHandle&) {

    ++LibraryHandleTest::two_count;
    if (LibraryHandleTest::callout_value == 10) {
        LibraryHandleTest::callout_value = 20;
    } else {
        LibraryHandleTest::callout_value = -20;
    }

    return (0);
}

// The next function is a duplicate of "one", but returns an error status.

int one_error(CalloutHandle& handle) {
    (void) one(handle);
    return (1);
}

// The next function is a duplicate of "one", but sets the skip flag.

int one_skip(CalloutHandle& handle) {
    (void) one(handle);
    handle.setSkip(true);
    return (0);
}

};  // extern "C"

// Check that we can register callouts on a particular hook.

TEST_F(LibraryHandleTest, RegisterSingleCallout) {
    LibraryHandle handle(getServerHooks(), 1);

    // Register callouts for hooks alpha and see that it is registered.
    EXPECT_FALSE(handle.calloutsPresent(getServerHooks()->getIndex("alpha")));
    handle.registerCallout("alpha", one);
    EXPECT_TRUE(handle.calloutsPresent(getServerHooks()->getIndex("alpha")));

    // Do the same for beta (which checks that the hooks are independent).
    EXPECT_FALSE(handle.calloutsPresent(getServerHooks()->getIndex("beta")));
    handle.registerCallout("beta", one);
    EXPECT_TRUE(handle.calloutsPresent(getServerHooks()->getIndex("beta")));
}

// Check that we can call a single callout on a particular hook.  Refer
// to the above definition of the callouts "one" and "two" to understand
// the expected return values.

TEST_F(LibraryHandleTest, CallSingleCallout) {
    LibraryHandle handle(getServerHooks(), 1);

    // Register callout for hook alpha...
    EXPECT_FALSE(handle.calloutsPresent(getServerHooks()->getIndex("alpha")));
    handle.registerCallout("alpha", one);
    EXPECT_TRUE(handle.calloutsPresent(getServerHooks()->getIndex("alpha")));

    // Call it.

    EXPECT_EQ(0, LibraryHandleTest::one_count);
    EXPECT_EQ(0, LibraryHandleTest::two_count);
    EXPECT_EQ(0, LibraryHandleTest::callout_value);

    int index = getServerHooks()->getIndex("alpha");
    CalloutHandle callout_handle(getHookManager());
    int status = handle.callCallouts(index, callout_handle);

    EXPECT_EQ(0, status);
    EXPECT_EQ(1, LibraryHandleTest::one_count);
    EXPECT_EQ(0, LibraryHandleTest::two_count);
    EXPECT_EQ(10, LibraryHandleTest::callout_value);

}

// Check that we can register two callouts for a hook and that they are called
// in order.

TEST_F(LibraryHandleTest, TwoCallouts) {
    LibraryHandle handle(getServerHooks(), 1);

    // Register two callouts for hook alpha...
    handle.registerCallout("alpha", one);
    handle.registerCallout("alpha", two);

    // ... and call them.
    EXPECT_EQ(0, LibraryHandleTest::one_count);
    EXPECT_EQ(0, LibraryHandleTest::two_count);
    EXPECT_EQ(0, LibraryHandleTest::callout_value);

    int index = getServerHooks()->getIndex("alpha");
    CalloutHandle callout_handle(getHookManager());
    int status = handle.callCallouts(index, callout_handle);

    EXPECT_EQ(0, status);
    EXPECT_EQ(1, LibraryHandleTest::one_count);
    EXPECT_EQ(1, LibraryHandleTest::two_count);
    EXPECT_EQ(20, LibraryHandleTest::callout_value);
}

// Check that we can register two callouts for a hook and that the second is not
// called if the first returns a non-zero status.

TEST_F(LibraryHandleTest, TwoCalloutsWithError) {
    LibraryHandle handle(getServerHooks(), 1);

    // Register callout for hook alpha...
    handle.registerCallout("alpha", one_error);
    handle.registerCallout("alpha", two);

    // Call them.
    EXPECT_EQ(0, LibraryHandleTest::one_count);
    EXPECT_EQ(0, LibraryHandleTest::two_count);
    EXPECT_EQ(0, LibraryHandleTest::callout_value);

    int index = getServerHooks()->getIndex("alpha");
    CalloutHandle callout_handle(getHookManager());
    int status = handle.callCallouts(index, callout_handle);

    EXPECT_EQ(1, status);
    EXPECT_EQ(1, LibraryHandleTest::one_count);
    EXPECT_EQ(0, LibraryHandleTest::two_count);
    EXPECT_EQ(10, LibraryHandleTest::callout_value);
}

// Check that we can register two callouts for a hook and that the second is not
// called if the first returns a non-zero status.

TEST_F(LibraryHandleTest, TwoCalloutsWithSkip) {
    LibraryHandle handle(getServerHooks(), 1);

    // Register callout for hook alpha...
    handle.registerCallout("alpha", one_skip);
    handle.registerCallout("alpha", two);

    // Call them.
    EXPECT_EQ(0, LibraryHandleTest::one_count);
    EXPECT_EQ(0, LibraryHandleTest::two_count);
    EXPECT_EQ(0, LibraryHandleTest::callout_value);

    int index = getServerHooks()->getIndex("alpha");
    CalloutHandle callout_handle(getHookManager());
    int status = handle.callCallouts(index, callout_handle);

    EXPECT_EQ(0, status);
    EXPECT_EQ(1, LibraryHandleTest::one_count);
    EXPECT_EQ(0, LibraryHandleTest::two_count);
    EXPECT_EQ(10, LibraryHandleTest::callout_value);
}

// Check that a callout can be registered more than once.

TEST_F(LibraryHandleTest, MultipleRegistration) {
    LibraryHandle handle(getServerHooks(), 1);

    // Register callouts for hook alpha...
    handle.registerCallout("alpha", one);
    handle.registerCallout("alpha", two);
    handle.registerCallout("alpha", one);

    // Call them.
    EXPECT_EQ(0, LibraryHandleTest::one_count);
    EXPECT_EQ(0, LibraryHandleTest::two_count);
    EXPECT_EQ(0, LibraryHandleTest::callout_value);

    int index = getServerHooks()->getIndex("alpha");
    CalloutHandle callout_handle(getHookManager());
    int status = handle.callCallouts(index, callout_handle);

    EXPECT_EQ(0, status);
    EXPECT_EQ(2, LibraryHandleTest::one_count);
    EXPECT_EQ(1, LibraryHandleTest::two_count);
    EXPECT_EQ(-10, LibraryHandleTest::callout_value);
}

// Check that a callout can be deregistered.

TEST_F(LibraryHandleTest, Deregister) {
    LibraryHandle handle(getServerHooks(), 1);

    // Register callouts for hook alpha...
    handle.registerCallout("alpha", one);
    handle.registerCallout("alpha", two);
    handle.registerCallout("alpha", one);

    // Get rid of all the "one" callbacks.
    handle.deregisterCallout("alpha", one);

    // Call it.
    EXPECT_EQ(0, LibraryHandleTest::one_count);
    EXPECT_EQ(0, LibraryHandleTest::two_count);
    EXPECT_EQ(0, LibraryHandleTest::callout_value);

    int index = getServerHooks()->getIndex("alpha");
    CalloutHandle callout_handle(getHookManager());
    int status = handle.callCallouts(index, callout_handle);

    EXPECT_EQ(0, status);
    EXPECT_EQ(0, LibraryHandleTest::one_count);
    EXPECT_EQ(1, LibraryHandleTest::two_count);
    EXPECT_EQ(-20, LibraryHandleTest::callout_value);
}

// Check that all callouts can be deregistered.

TEST_F(LibraryHandleTest, DeregisterAll) {
    LibraryHandle handle(getServerHooks(), 1);

    // Register callouts for hook alpha...
    EXPECT_FALSE(handle.calloutsPresent(getServerHooks()->getIndex("alpha")));
    handle.registerCallout("alpha", one);
    handle.registerCallout("alpha", two);
    EXPECT_TRUE(handle.calloutsPresent(getServerHooks()->getIndex("alpha")));

    // ... and remove them.
    handle.deregisterAll("alpha");
    EXPECT_FALSE(handle.calloutsPresent(getServerHooks()->getIndex("alpha")));
}

// Add checks that invalid names etc. all throw.  With the base hooks added
// by the constructor, there are five valid hooks, with valid indexes 0 to 4.

TEST_F(LibraryHandleTest, InvalidNameAndIndex) {
    LibraryHandle handle(getServerHooks(), 1);

    EXPECT_THROW(handle.registerCallout("omega", one), NoSuchHook);
    EXPECT_THROW(handle.deregisterCallout("omega", one), NoSuchHook);
    EXPECT_THROW(handle.deregisterAll("omega"), NoSuchHook);

    EXPECT_THROW(static_cast<void>(handle.calloutsPresent(-1)), NoSuchHook);
    EXPECT_THROW(static_cast<void>(handle.calloutsPresent(5)), NoSuchHook);

    CalloutHandle callout_handle(getHookManager());
    EXPECT_THROW(static_cast<void>(handle.callCallouts(-1, callout_handle)),
                 NoSuchHook);
    EXPECT_THROW(static_cast<void>(handle.callCallouts(10, callout_handle)),
                 NoSuchHook);
}


} // Anonymous namespace