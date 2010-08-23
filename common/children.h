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

#ifndef __CHILDREN_H
#define __CHILDREN_H

#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>

#include <string>
#include <vector>

/// \brief Child handling
///
/// All functions required to handle the spawning of child processes.

class Children {
public:

    /// \brief Spawn child processes
    ///
    /// Invoked if the --worker option is given on the command line, this
    /// invokes the worker process with the command:
    ///
    /// [image] --burst 1 --queue <n> --memsize <value> --debug <value>
    ///
    /// ... where <n> is different for each process and <value> is the debug
    /// value specified for the main program.
    ///
    /// The PIDs are placed in the external "children" vector.  A termination
    /// handler will kill them.
    ///
    /// \param image Name of the program to run
    /// \param queue Number of queues.  This number of children are started,
    /// with queue numbers in the range 1..<queue>
    /// \param memsize Memory size
    /// \param debug Debug flags
    static void spawnChildren(const std::string& image, uint32_t queue,
        uint32_t memsize, uint32_t debug);

private:
    /// \brief Exit Handler
    ///
    /// Sends a SIGTERM signal to the children.
    static void exitHandler();

    static std::vector<pid_t> children;    // Child processes
};



#endif // __CHILDREN_H
