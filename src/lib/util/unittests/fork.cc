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

#include "fork.h"

#include <util/io/fd.h>

#ifndef _WIN32

/* Unix version */

#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <signal.h>
#include <string.h>
#include <cerrno>
#include <stdlib.h>
#include <stdio.h>

using namespace isc::util::io;

namespace {

// Just a NOP function to ignore a signal but let it interrupt function.
void no_handler(int) { }

};

namespace isc {
namespace util {
namespace unittests {

bool
process_ok(pid_t process) {
    // Create a timeout
    struct sigaction ignored, original;
    memset(&ignored, 0, sizeof ignored);
    ignored.sa_handler = no_handler;
    if (sigaction(SIGALRM, &ignored, &original)) {
        return false;
    }
    // It is long, but if everything is OK, it'll not happen
    alarm(10);
    int status;
    int result(waitpid(process, &status, 0) == -1);
    // Cancel the alarm and return the original handler
    alarm(0);
    if (sigaction(SIGALRM, &original, NULL)) {
        return false;
    }
    // Check what we found out
    if (result) {
        if (errno == EINTR)
            kill(process, SIGTERM);
        return false;
    }
    return WIFEXITED(status) && WEXITSTATUS(status) == 0;
}

/*
 * This creates a pipe, forks and feeds the pipe with given data.
 * Used to provide the input in non-blocking/asynchronous way.
 */
pid_t
provide_input(int *read_pipe, const void *input, const size_t length)
{
    int pipes[2];
    if (pipe(pipes)) {
        return -1;
    }
    *read_pipe = pipes[0];
    pid_t pid(fork());
    if (pid) { // We are in the parent
        return pid;
    } else { // This is in the child, just puth the data there
        close(pipes[0]);
        if (!write_data(pipes[1], input, length)) {
            exit(1);
        } else {
            close(pipes[1]);
            exit(0);
        }
    }
}

/*
 * This creates a pipe, forks and reads the pipe and compares it
 * with given data. Used to check output of run in asynchronous way.
 */
pid_t
check_output(int *write_pipe, const void *output, const size_t length)
{
    int pipes[2];
    if (pipe(pipes)) {
        return -1;
    }
    *write_pipe = pipes[1];
    pid_t pid(fork());
    if (pid) { // We are in parent
        close(pipes[0]);
        return pid;
    } else {
        close(pipes[1]);
        // We don't return the memory, but we're in tests and end this process
        // right away.
        unsigned char *buffer = new unsigned char[length + 1];
        // Try to read one byte more to see if the output ends here
        size_t got_length(read_data(pipes[0], buffer, length + 1));
        bool ok(true);
        if (got_length != length) {
            fprintf(stderr, "Different length (expected %u, got %u)\n",
                static_cast<unsigned>(length),
                static_cast<unsigned>(got_length));
            ok = false;
        }
        if(!ok || memcmp(buffer, output, length)) {
            const unsigned char *output_c(static_cast<const unsigned char *>(
                output));
            // If they differ, print what we have
            for(size_t i(0); i != got_length; ++ i) {
                fprintf(stderr, "%02hhx", buffer[i]);
            }
            fprintf(stderr, "\n");
            for(size_t i(0); i != length; ++ i) {
                fprintf(stderr, "%02hhx", output_c[i]);
            }
            fprintf(stderr, "\n");
            exit(1);
        } else {
            exit(0);
        }
    }
}

}
}
}

#else

/* WIN32 version (using thread: no fork()!) */

#include <io.h>
#include <fcntl.h>
#include <signal.h>
#include <string.h>
#include <cerrno>
#include <stdlib.h>
#include <stdio.h>

using namespace isc::util::io;

namespace isc {
namespace util {
namespace unittests {

bool
process_ok(pid_t process) {
    HANDLE handle;

    handle = OpenThread(PROCESS_ALL_ACCESS, FALSE, process);
    if (handle == NULL)
        return false;
    // It is long, but if everything is OK, it'll not happen
    DWORD result(WaitForSingleObject(handle, 10000));
    if (result != 0) {
        if (result == WAIT_FAILED)
            TerminateThread(handle, SIGTERM);
        CloseHandle(handle);
        return false;
    }
    // Check what we found out
    DWORD status;
    result = GetExitCodeThread(handle, &status);
    CloseHandle(handle);
    return (result != 0) && (status == 0);
}

struct pichildargs {
    HANDLE pipe;
    const void *input;
    const size_t length;
    pichildargs(HANDLE pipe, const void *input, const size_t length) :
    pipe(pipe), input(input), length(length)
    {}
private:
    pichildargs& operator=(pichildargs const&);
};

DWORD WINAPI PIChildProc(void *childparam) {
    pichildargs *args;

    args = static_cast<pichildargs *>(childparam);
    if (!write_data(_open_osfhandle((intptr_t)args->pipe, 0),
                    args->input, args->length)) {
        delete args;
        return 1;
    } else {
        CloseHandle(args->pipe);
        delete args;
        return 0;
    }
}

/*
 * This creates a pipe, forks and feeds the pipe with given data.
 * Used to provide the input in non-blocking/asynchronous way.
 */
pid_t
provide_input(int *read_pipe, const void *input, const size_t length)
{
    HANDLE hReadPipe, hWritePipe;
    if (CreatePipe(&hReadPipe, &hWritePipe, NULL, 0) == 0) {
        return -1;
    }
    *read_pipe = _open_osfhandle((intptr_t)hReadPipe, _O_RDONLY);

    pichildargs *args = new pichildargs(hWritePipe, input, length);
    DWORD pid;
    if (CreateThread(NULL, 0, PIChildProc, args, 0, &pid) == NULL) {
        CloseHandle(hReadPipe);
        CloseHandle(hWritePipe);
        return -1;
    }
    return (pid_t) pid;
}

struct cochildargs {
    HANDLE pipe;
    const void *output;
    const size_t length;
    cochildargs(HANDLE pipe, const void *output, const size_t length) :
    pipe(pipe), output(output), length(length)
    {}
private:
    cochildargs& operator=(cochildargs const&);
};

DWORD WINAPI COChildProc(void *childparam) {
    cochildargs *args;

    args = static_cast<cochildargs *>(childparam);
    // We don't return the memory, but we're in tests and end this process
    // right away.
    unsigned char *buffer = new unsigned char[args->length + 1];
    // Try to read one byte more to see if the output ends here
    size_t got_length(read_data(_open_osfhandle((intptr_t)args->pipe,
                                                _O_RDONLY),
                                buffer, args->length + 1));
    bool ok(true);
    if (got_length != args->length) {
        fprintf(stderr, "Different length (expected %u, got %u)\n",
                static_cast<unsigned>(args->length),
                static_cast<unsigned>(got_length));
        ok = false;
    }
    if(!ok || memcmp(buffer, args->output, args->length)) {
        const unsigned char *output_c(static_cast<const unsigned char *>(
                args->output));
        // If they differ, print what we have
        for(size_t i(0); i != got_length; ++ i) {
            fprintf(stderr, "%02hhx", buffer[i]);
        }
        fprintf(stderr, "\n");
        for(size_t i(0); i != args->length; ++ i) {
            fprintf(stderr, "%02hhx", output_c[i]);
        }
        fprintf(stderr, "\n");
        delete args;
        return 1;
    } else
        delete args;
        return 0;
}

/*
 * This creates a pipe, forks and reads the pipe and compares it
 * with given data. Used to check output of run in asynchronous way.
 */
pid_t
check_output(int *write_pipe, const void *output, const size_t length)
{
    HANDLE hReadPipe, hWritePipe;
    if (CreatePipe(&hReadPipe, &hWritePipe, NULL, 0) == 0) {
        return -1;
    }
    *write_pipe = _open_osfhandle((intptr_t)hWritePipe, 0);

    cochildargs *args = new cochildargs(hReadPipe, output, length);
    DWORD pid;
    if (CreateThread(NULL, 0, COChildProc, args, 0, &pid) == NULL) {
        CloseHandle(hReadPipe);
        CloseHandle(hWritePipe);
        return -1;
    }
    return (pid_t) pid;
}

}
}
}

#endif
