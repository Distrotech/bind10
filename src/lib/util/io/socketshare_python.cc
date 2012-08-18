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

#define PY_SSIZE_T_CLEAN
#include <Python.h>
#include <structmember.h>

#include <config.h>

#include "socket_share.h"


static PyObject*
socketshare_recv_socket(PyObject*, PyObject* args) {
    int result;
    socket_type sock, sd;
    if (!PyArg_ParseTuple(args, "i", &sock)) {
        return (NULL);
    }
    result = isc::util::io::recv_socket(sock, &sd);
    if (result != 0) {
        return (Py_BuildValue("i", result));
    } else {
        return (Py_BuildValue("i", sd));
    }
}

static PyObject*
socketshare_send_socket(PyObject*, PyObject* args) {
    int result;
    socket_type sock, sd;
    if (!PyArg_ParseTuple(args, "ii", &sock, &sd)) {
        return (NULL);
    }
    result = isc::util::io::send_socket(sock, sd);
    return (Py_BuildValue("i", result));
}

static PyMethodDef socketshare_Methods[] = {
    {"send_socket",  socketshare_send_socket, METH_VARARGS, ""},
    {"recv_socket",  socketshare_recv_socket, METH_VARARGS, ""},
    {NULL, NULL, 0, NULL}        /* Sentinel */
};


static PyModuleDef bind10_socketshare_python = {
    { PyObject_HEAD_INIT(NULL) NULL, 0, NULL},
    "bind10_socketshare",
    "Python bindings for socketshare",
    -1,
    socketshare_Methods,
    NULL,
    NULL,
    NULL,
    NULL
};

PyMODINIT_FUNC
PyInit_libutil_io_python(void) {
    PyObject *mod = PyModule_Create(&bind10_socketshare_python);
    if (mod == NULL) {
        return (NULL);
    }

    PyObject* SOCKET_SYSTEM_ERROR =
        Py_BuildValue("i", isc::util::io::SOCKET_SYSTEM_ERROR);
    if (SOCKET_SYSTEM_ERROR == NULL) {
        Py_XDECREF(mod);
        return (NULL);
    }
    int ret;
    ret = PyModule_AddObject(mod, "SOCKET_SYSTEM_ERROR", SOCKET_SYSTEM_ERROR);
    if (ret == -1) {
        Py_XDECREF(SOCKET_SYSTEM_ERROR);
        Py_XDECREF(mod);
        return (NULL);
    }

    PyObject* SOCKET_OTHER_ERROR =
        Py_BuildValue("i", isc::util::io::SOCKET_OTHER_ERROR);
    if (SOCKET_OTHER_ERROR == NULL) {
        Py_XDECREF(mod);
        return (NULL);
    }
    ret = PyModule_AddObject(mod, "SOCKET_OTHER_ERROR", SOCKET_OTHER_ERROR);
    if (ret == -1) {
        Py_XDECREF(SOCKET_OTHER_ERROR);
        Py_XDECREF(mod);
        return (NULL);
    }

    return (mod);
}

