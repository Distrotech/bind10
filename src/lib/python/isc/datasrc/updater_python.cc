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

// Enable this if you use s# variants with PyArg_ParseTuple(), see
// http://docs.python.org/py3k/c-api/arg.html#strings-and-buffers
//#define PY_SSIZE_T_CLEAN

// Python.h needs to be placed at the head of the program file, see:
// http://docs.python.org/py3k/extending/extending.html#a-simple-example
#include <Python.h>

#include <util/python/pycppwrapper_util.h>

#include <datasrc/client.h>
#include <datasrc/database.h>
#include <datasrc/exceptions.h>
#include <datasrc/sqlite3_accessor.h>
#include <datasrc/zone.h>

#include <dns/python/name_python.h>
#include <dns/python/rrset_python.h>
#include <dns/python/rrclass_python.h>
#include <dns/python/rrtype_python.h>
#include <dns/python/rrset_collection_python.h>

#include "datasrc.h"
#include "updater_python.h"

#include "updater_inc.cc"
#include "finder_inc.cc"

using namespace std;
using namespace isc::util::python;
using namespace isc::dns::python;
using namespace isc::datasrc;
using namespace isc::datasrc::python;

namespace isc_datasrc_internal {
// See finder_python.cc
PyObject* ZoneFinder_helper(ZoneFinder* finder, PyObject* args);
PyObject* ZoneFinder_helper_all(ZoneFinder* finder, PyObject* args);
}

namespace {
// The s_* Class simply covers one instantiation of the object
class s_ZoneUpdater : public PyObject {
public:
    s_ZoneUpdater() : cppobj(ZoneUpdaterPtr()), base_obj(NULL) {};
    ZoneUpdaterPtr cppobj;
    // This is a reference to a base object; if the object of this class
    // depends on another object to be in scope during its lifetime,
    // we use INCREF the base object upon creation, and DECREF it at
    // the end of the destructor
    // This is an optional argument to createXXX(). If NULL, it is ignored.
    PyObject* base_obj;
};

// Shortcut type which would be convenient for adding class variables safely.
typedef CPPPyObjectContainer<s_ZoneUpdater, ZoneUpdater> ZoneUpdaterContainer;

//
// We declare the functions here, the definitions are below
// the type definition of the object, since both can use the other
//

// General creation and destruction
int
ZoneUpdater_init(PyObject*, PyObject*, PyObject*) {
    // can't be called directly
    PyErr_SetString(PyExc_TypeError,
                    "ZoneUpdater cannot be constructed directly");

    return (-1);
}

void
ZoneUpdater_destroy(PyObject* po_self) {
    s_ZoneUpdater* const self = static_cast<s_ZoneUpdater*>(po_self);

    // cppobj is a shared ptr, but to make sure things are not destroyed in
    // the wrong order, we reset it here.
    self->cppobj.reset();
    if (self->base_obj != NULL) {
        Py_DECREF(self->base_obj);
    }
    Py_TYPE(self)->tp_free(self);
}

PyObject*
ZoneUpdater_addRRset(PyObject* po_self, PyObject* args) {
    s_ZoneUpdater* const self = static_cast<s_ZoneUpdater*>(po_self);
    PyObject* rrset_obj;
    if (PyArg_ParseTuple(args, "O!", &rrset_type, &rrset_obj)) {
        try {
            self->cppobj->addRRset(PyRRset_ToRRset(rrset_obj));
            Py_RETURN_NONE;
        } catch (const DataSourceError& dse) {
            PyErr_SetString(getDataSourceException("Error"), dse.what());
            return (NULL);
        } catch (const std::exception& exc) {
            PyErr_SetString(getDataSourceException("Error"), exc.what());
            return (NULL);
        }
    } else {
        return (NULL);
    }
}

PyObject*
ZoneUpdater_deleteRRset(PyObject* po_self, PyObject* args) {
    s_ZoneUpdater* const self = static_cast<s_ZoneUpdater*>(po_self);
    PyObject* rrset_obj;
    if (PyArg_ParseTuple(args, "O!", &rrset_type, &rrset_obj)) {
        try {
            self->cppobj->deleteRRset(PyRRset_ToRRset(rrset_obj));
            Py_RETURN_NONE;
        } catch (const DataSourceError& dse) {
            PyErr_SetString(getDataSourceException("Error"), dse.what());
            return (NULL);
        } catch (const std::exception& exc) {
            PyErr_SetString(getDataSourceException("Error"), exc.what());
            return (NULL);
        }
    } else {
        return (NULL);
    }
}

PyObject*
ZoneUpdater_commit(PyObject* po_self, PyObject*) {
    s_ZoneUpdater* const self = static_cast<s_ZoneUpdater*>(po_self);
    try {
        self->cppobj->commit();
        Py_RETURN_NONE;
    } catch (const DataSourceError& dse) {
        PyErr_SetString(getDataSourceException("Error"), dse.what());
        return (NULL);
    } catch (const std::exception& exc) {
        PyErr_SetString(getDataSourceException("Error"), exc.what());
        return (NULL);
    }
}

PyObject*
ZoneUpdater_getClass(PyObject* po_self, PyObject*) {
    s_ZoneUpdater* self = static_cast<s_ZoneUpdater*>(po_self);
    try {
        return (createRRClassObject(self->cppobj->getFinder().getClass()));
    } catch (const std::exception& exc) {
        PyErr_SetString(getDataSourceException("Error"), exc.what());
        return (NULL);
    } catch (...) {
        PyErr_SetString(getDataSourceException("Error"),
                        "Unexpected exception");
        return (NULL);
    }
}

PyObject*
ZoneUpdater_getOrigin(PyObject* po_self, PyObject*) {
    s_ZoneUpdater* self = static_cast<s_ZoneUpdater*>(po_self);
    try {
        return (createNameObject(self->cppobj->getFinder().getOrigin()));
    } catch (const std::exception& exc) {
        PyErr_SetString(getDataSourceException("Error"), exc.what());
        return (NULL);
    } catch (...) {
        PyErr_SetString(getDataSourceException("Error"),
                        "Unexpected exception");
        return (NULL);
    }
}

PyObject*
ZoneUpdater_find(PyObject* po_self, PyObject* args) {
    s_ZoneUpdater* const self = static_cast<s_ZoneUpdater*>(po_self);
    return (isc_datasrc_internal::ZoneFinder_helper(&self->cppobj->getFinder(),
                                                    args));
}

PyObject*
ZoneUpdater_find_all(PyObject* po_self, PyObject* args) {
    s_ZoneUpdater* const self = static_cast<s_ZoneUpdater*>(po_self);
    return (isc_datasrc_internal::ZoneFinder_helper_all(
        &self->cppobj->getFinder(), args));
}

namespace {
// Below we define Python RRsetCollection class generated by the updater.
// It's never expected to be instantiated directly from Python code, so
// everything is hidden here, and tp_init always fails.

class s_UpdaterRRsetCollection : public s_RRsetCollection {
public:
    s_UpdaterRRsetCollection() : s_RRsetCollection(), base_obj_(NULL) {}
    PyObject* base_obj_;
};

int
RRsetCollection_init(PyObject*, PyObject*, PyObject*) {
    // can't be called directly; actually, the constructor shouldn't even be
    // called, but we catch the case just in case.
    PyErr_SetString(PyExc_TypeError,
                    "datasrc.RRsetCollection cannot be constructed directly");

    return (-1);
}

void
RRsetCollection_destroy(PyObject* po_self) {
    s_UpdaterRRsetCollection* const self =
        static_cast<s_UpdaterRRsetCollection*>(po_self);

    // We don't own the C++ collection object; we shouldn't delete it here.

    // Note: we need to check if this is NULL; it remains NULL in case of
    // direct instantiation (which fails).
    if (self->base_obj_ != NULL) {
        Py_DECREF(self->base_obj_);
    }
    Py_TYPE(self)->tp_free(self);
}

PyTypeObject updater_rrset_collection_type = {
    PyVarObject_HEAD_INIT(NULL, 0)
    "datasrc.UpdaterRRsetCollection",
    sizeof(s_UpdaterRRsetCollection),   // tp_basicsize
    0,                                  // tp_itemsize
    RRsetCollection_destroy,            // tp_dealloc
    NULL,                               // tp_print
    NULL,                               // tp_getattr
    NULL,                               // tp_setattr
    NULL,                               // tp_reserved
    NULL,                               // tp_repr
    NULL,                               // tp_as_number
    NULL,                               // tp_as_sequence
    NULL,                               // tp_as_mapping
    NULL,                               // tp_hash
    NULL,                               // tp_call
    NULL,                               // tp_str
    NULL,                               // tp_getattro
    NULL,                               // tp_setattro
    NULL,                               // tp_as_buffer
    Py_TPFLAGS_DEFAULT,                 // tp_flags
    NULL,
    NULL,                               // tp_traverse
    NULL,                               // tp_clear
    NULL,                               // tp_richcompare
    0,                                  // tp_weaklistoffset
    NULL,                               // tp_iter
    NULL,                               // tp_iternext
    NULL,                               // tp_methods
    NULL,                               // tp_members
    NULL,                               // tp_getset
    NULL,   // tp_base (rrset_collection_base_type, set at run time)
    NULL,                               // tp_dict
    NULL,                               // tp_descr_get
    NULL,                               // tp_descr_set
    0,                                  // tp_dictoffset
    RRsetCollection_init,               // tp_init
    NULL,                               // tp_alloc
    PyType_GenericNew,                  // tp_new
    NULL,                               // tp_free
    NULL,                               // tp_is_gc
    NULL,                               // tp_bases
    NULL,                               // tp_mro
    NULL,                               // tp_cache
    NULL,                               // tp_subclasses
    NULL,                               // tp_weaklist
    NULL,                               // tp_del
    0                                   // tp_version_tag
};
} // unnamed namespace

PyObject*
ZoneUpdater_getRRsetCollection(PyObject* po_self, PyObject*) {
    s_ZoneUpdater* const self = static_cast<s_ZoneUpdater*>(po_self);

    s_UpdaterRRsetCollection* collection =
        static_cast<s_UpdaterRRsetCollection*>(
            PyObject_New(s_RRsetCollection, &updater_rrset_collection_type));
    collection->cppobj = &self->cppobj->getRRsetCollection();
    collection->base_obj_ = po_self;;
    Py_INCREF(collection->base_obj_);

    return (collection);
}

// This list contains the actual set of functions we have in
// python. Each entry has
// 1. Python method name
// 2. Our static function here
// 3. Argument type
// 4. Documentation
PyMethodDef ZoneUpdater_methods[] = {
    { "add_rrset", ZoneUpdater_addRRset,
      METH_VARARGS, ZoneUpdater_addRRset_doc },
    { "delete_rrset", ZoneUpdater_deleteRRset,
      METH_VARARGS, ZoneUpdater_deleteRRset_doc },
    { "commit", ZoneUpdater_commit, METH_NOARGS, ZoneUpdater_commit_doc },
    { "get_rrset_collection", ZoneUpdater_getRRsetCollection,
      METH_NOARGS, ZoneUpdater_getRRsetCollection_doc },
    // Instead of a getFinder, we implement the finder functionality directly
    // This is because ZoneFinder is non-copyable, and we should not create
    // a ZoneFinder object from a reference only (which is what is returned
    // by getFinder(). Apart from that
    { "get_origin", ZoneUpdater_getOrigin,
      METH_NOARGS, ZoneFinder_getOrigin_doc },
    { "get_class", ZoneUpdater_getClass,
      METH_NOARGS, ZoneFinder_getClass_doc },
    { "find", ZoneUpdater_find, METH_VARARGS, ZoneFinder_find_doc },
    { "find_all", ZoneUpdater_find_all, METH_VARARGS,
      ZoneFinder_findAll_doc },
    { NULL, NULL, 0, NULL }
};

} // end of unnamed namespace

namespace isc {
namespace datasrc {
namespace python {
PyTypeObject zoneupdater_type = {
    PyVarObject_HEAD_INIT(NULL, 0)
    "datasrc.ZoneUpdater",
    sizeof(s_ZoneUpdater),              // tp_basicsize
    0,                                  // tp_itemsize
    ZoneUpdater_destroy,                // tp_dealloc
    NULL,                               // tp_print
    NULL,                               // tp_getattr
    NULL,                               // tp_setattr
    NULL,                               // tp_reserved
    NULL,                               // tp_repr
    NULL,                               // tp_as_number
    NULL,                               // tp_as_sequence
    NULL,                               // tp_as_mapping
    NULL,                               // tp_hash
    NULL,                               // tp_call
    NULL,                               // tp_str
    NULL,                               // tp_getattro
    NULL,                               // tp_setattro
    NULL,                               // tp_as_buffer
    Py_TPFLAGS_DEFAULT,                 // tp_flags
    ZoneUpdater_doc,
    NULL,                               // tp_traverse
    NULL,                               // tp_clear
    NULL,                               // tp_richcompare
    0,                                  // tp_weaklistoffset
    NULL,                               // tp_iter
    NULL,                               // tp_iternext
    ZoneUpdater_methods,                // tp_methods
    NULL,                               // tp_members
    NULL,                               // tp_getset
    NULL,                               // tp_base
    NULL,                               // tp_dict
    NULL,                               // tp_descr_get
    NULL,                               // tp_descr_set
    0,                                  // tp_dictoffset
    ZoneUpdater_init,                   // tp_init
    NULL,                               // tp_alloc
    PyType_GenericNew,                  // tp_new
    NULL,                               // tp_free
    NULL,                               // tp_is_gc
    NULL,                               // tp_bases
    NULL,                               // tp_mro
    NULL,                               // tp_cache
    NULL,                               // tp_subclasses
    NULL,                               // tp_weaklist
    NULL,                               // tp_del
    0                                   // tp_version_tag
};

PyObject*
createZoneUpdaterObject(isc::datasrc::ZoneUpdaterPtr source,
                        PyObject* base_obj)
{
    s_ZoneUpdater* py_zu = static_cast<s_ZoneUpdater*>(
        zoneupdater_type.tp_alloc(&zoneupdater_type, 0));
    if (py_zu != NULL) {
        py_zu->cppobj = source;
        py_zu->base_obj = base_obj;
        if (base_obj != NULL) {
            Py_INCREF(base_obj);
        }
    }
    return (py_zu);
}

bool
initModulePart_ZoneUpdater(PyObject* mod) {
    // We initialize the static description object with PyType_Ready(),
    // then add it to the module. This is not just a check! (leaving
    // this out results in segmentation faults)
    if (PyType_Ready(&zoneupdater_type) < 0) {
        return (false);
    }
    void* zip = &zoneupdater_type;
    if (PyModule_AddObject(mod, "ZoneUpdater",
                           static_cast<PyObject*>(zip)) < 0)
    {
        return (false);
    }
    Py_INCREF(&zoneupdater_type);

    // get_rrset_collection() needs the base class type information.  Since
    // it's defined in a separate loadable module, we retrieve its C object
    // via the Python interpreter.  Directly referring to
    // isc::dns::python::rrset_collection_base_type might work depending on
    // the runtime environment (and in fact it does for some), but that would
    // be less portable.
    try {
        if (updater_rrset_collection_type.tp_base == NULL) {
            PyObjectContainer dns_module(PyImport_ImportModule("isc.dns"));
            PyObjectContainer dns_dict(PyModule_GetDict(dns_module.get()));
            // GetDict doesn't acquire a reference, so we need to get it to
            // meet the container's requirement.
            Py_INCREF(dns_dict.get());
            PyObjectContainer base(
                PyDict_GetItemString(dns_dict.get(), "RRsetCollectionBase"));
            PyTypeObject* pt_rrset_collection_base =
                static_cast<PyTypeObject*>(static_cast<void*>(base.get()));
            updater_rrset_collection_type.tp_base = pt_rrset_collection_base;
            if (PyType_Ready(&updater_rrset_collection_type) < 0) {
                isc_throw(Unexpected, "failed to import isc.dns module");
            }

            // Make sure the base type won't suddenly disappear.  Note that we
            // are effectively leaking it; it's intentional.
            Py_INCREF(base.get());
        }
    } catch (...) {
        PyErr_SetString(PyExc_SystemError,
                        "Unexpected failure in ZoneUpdater initialization");
        return (false);
    }

    return (true);
}
} // namespace python
} // namespace datasrc
} // namespace isc

