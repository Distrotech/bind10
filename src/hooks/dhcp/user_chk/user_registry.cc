// Copyright (C) 2013 Internet Systems Consortium, Inc. ("ISC")
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

#include <user_registry.h>
#include <user.h>

namespace user_chk {

UserRegistry::UserRegistry() {
}

UserRegistry::~UserRegistry(){
}

void
UserRegistry::addUser(UserPtr& user) {
    if (!user) {
        isc_throw (UserRegistryError, "UserRegistry cannot add blank user");
    }

    UserPtr found_user;
    if ((found_user = findUser(user->getUserId()))) {
        isc_throw (UserRegistryError, "UserRegistry duplicate user: "
                   << user->getUserId());
    }

    users_[user->getUserId()] = user;
}

const UserPtr&
UserRegistry::findUser(const UserId& id) const {
    static UserPtr empty;
    UserMap::const_iterator it = users_.find(id);
    if (it != users_.end()) {
        const UserPtr tmp = (*it).second;
        return ((*it).second);
    }

    return empty;
}

void
UserRegistry::removeUser(const UserId& id) {
    static UserPtr empty;
    UserMap::iterator it = users_.find(id);
    if (it != users_.end()) {
        users_.erase(it);
    }
}

const UserPtr&
UserRegistry::findUser(const isc::dhcp::HWAddr& hwaddr) const {
    UserId id(UserId::HW_ADDRESS, hwaddr.hwaddr_);
    return (findUser(id));
}

const UserPtr&
UserRegistry::findUser(const isc::dhcp::DUID& duid) const {
    UserId id(UserId::DUID, duid.getDuid());
    return (findUser(id));
}

void UserRegistry::refresh() {
    if (!source_) {
        isc_throw(UserRegistryError,
                  "UserRegistry: cannot refresh, no data source");
    }

    // If the source isn't open, open it.
    if (!source_->isOpen()) {
        source_->open();
    }

    // Make a copy in case something goes wrong midstream.
    UserMap backup(users_);

    // Empty the registry then read users from source until source is empty.
    clearall();
    try {
        UserPtr user;
        while ((user = source_->readNextUser())) {
            addUser(user);
        }
    } catch (const std::exception& ex) {
        // Source was compromsised so restore registry from backup.
        users_ = backup;
        // Close the source.
        source_->close();
        isc_throw (UserRegistryError, "UserRegistry: refresh failed during read"
                   << ex.what());
    }

    // Close the source.
    source_->close();
}

void UserRegistry::clearall() {
    users_.clear();
}

void UserRegistry::setSource(UserDataSourcePtr& source) {
    if (!source) {
        isc_throw (UserRegistryError,
                   "UserRegistry: data source cannot be set to null");
    }

    source_ = source;
}

const UserDataSourcePtr& UserRegistry::getSource() {
    return (source_);
}

} // namespace user_chk
