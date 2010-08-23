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

#include <sys/types.h>
#include <signal.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <iostream>
#include <iomanip>
#include <vector>

#include <boost/lexical_cast.hpp>


#include "children.h"

std::vector<pid_t> Children::children;    // Child processes

/// \brief Exit Handler

void Children::exitHandler() {
    std::vector<pid_t>::iterator i;
    for (i = children.begin(); i != children.end(); ++i) {
        (void) kill(*i, SIGTERM);
    }
}

/// \brief Spawn child processes

void Children::spawnChildren(const std::string& image, uint32_t queue,
    uint32_t memsize, uint32_t debug)
{
    // Declare the exit handler.
    atexit(Children::exitHandler);

    // Construct the exec argument list
    char* argv[10];

    argv[0] = strdup(image.c_str());

    argv[1] = strdup("--burst");
    argv[2] = strdup("1");

    argv[3] = strdup("--memsize");
    std::string smemsize = boost::lexical_cast<std::string>(memsize);
    argv[4] = strdup(smemsize.c_str());

    argv[5] = strdup("--debug");
    std::string sdebug = boost::lexical_cast<std::string>(debug);
    argv[6] = strdup(sdebug.c_str());

    argv[7] = strdup("--queue");
    argv[8] = NULL;

    argv[9] = NULL;

    // Kick off the subprocesses
    for (int i = 1; i <= queue; ++i) {
        std::string squeue = boost::lexical_cast<std::string>(i);
        argv[8] = strdup(squeue.c_str());

        pid_t child = vfork();
        if (child == 0) {
            int status = execv(argv[0], argv);
            std::cout << "Error - failed to start " << argv[0] << std::endl;
            exit(1);
        }

        // Save the PID of the child
        children.push_back(child);
        
        // Tidy up and start next process
        free(argv[8]);
        argv[8] = NULL;
    }

    // Free up resources
    for (int i = 0; i < 8; ++i) {
        free(argv[i]);
    }
}
