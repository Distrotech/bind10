/*
 * Copyright (C) 2010  Internet Systems Consortium, Inc. ("ISC")
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND ISC DISCLAIMS ALL WARRANTIES WITH
 * REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY
 * AND FITNESS.  IN NO EVENT SHALL ISC BE LIABLE FOR ANY SPECIAL, DIRECT,
 * INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM
 * LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE
 * OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 */

/* $Id$ */

#ifndef __RBT_DATASRC_H
#define __RBT_DATASRC_H 1

#include <string>

#include <dns/rrset.h>

namespace isc {
namespace dns {
class AbstractRRset;
class Name;
class RRType;
}
}

struct RbtNodeImpl;
struct RbtDBImpl;
class RbtRRset;

enum RbtDataSrcResult {RbtDataSrcSuccess, RbtDataSrcExist, RbtDataSrcNotFound,
                       RbtDataSrcPartialMatch};

class RbtNode {
public:
    RbtNode() : base_(NULL), impl_(NULL) {}
    void set(RbtNodeImpl* const impl, const void* const base) {
        base_ = base;
        impl_ = impl;
    }
    std::string toText();
    RbtDataSrcResult findRRset(const isc::dns::RRType& rrtype,
                               RbtRRset& rrset) const;
private:
    const void* base_;
    RbtNodeImpl* impl_;
};

class RbtDataSrc {
public:
    enum OpenMode {LOAD, SERVE};         // open mode with a DB file

    RbtDataSrc(const isc::dns::Name& origin);
    RbtDataSrc(const isc::dns::Name& origin, const char& file, OpenMode mode);
    ~RbtDataSrc();
    RbtDataSrcResult addNode(const isc::dns::Name& name, RbtNode* node);
    void addRRset(const isc::dns::AbstractRRset& rrset); // an ad hoc interface
    RbtDataSrcResult findNode(const isc::dns::Name& name, RbtNode* node) const;
    RbtDataSrcResult getApexNode(RbtNode* node) const;
    std::string toText() const;
    std::string nodeToText(const RbtNode& node) const;
    size_t getAllocatedMemorySize() const;
private:
    RbtNode createNode();
    RbtDBImpl* impl_;
};

struct RbtRRsetImpl;
struct RbtRdataHandle;

class RbtRRset : public isc::dns::AbstractRRset {
public:
    RbtRRset();
    virtual ~RbtRRset();
    virtual unsigned int getRdataCount() const;
    virtual const isc::dns::Name& getName() const;
    virtual const isc::dns::RRClass& getClass() const;
    virtual const isc::dns::RRType& getType() const;
    virtual const isc::dns::RRTTL& getTTL() const;
    virtual void setName(const isc::dns::Name& name);
    virtual void setTTL(const isc::dns::RRTTL& ttl);
    virtual std::string toText() const;
    virtual unsigned int toWire(isc::dns::MessageRenderer& renderer) const;
    virtual unsigned int toWire(isc::dns::OutputBuffer& buffer) const;
    virtual void addRdata(isc::dns::rdata::ConstRdataPtr rdata);
    virtual void addRdata(const isc::dns::rdata::Rdata& rdata);
    virtual isc::dns::RdataIteratorPtr getRdataIterator() const;
    RbtDataSrcResult getFirstRdata(RbtRdataHandle& rdata) const;
    void clear();

    RbtRRsetImpl* impl_; // intentionally public, but hide details using pimpl
};

struct RbtRdataFieldHandle;
struct RbtRdata;

class RbtRdataHandle {
public:
    void set(const RbtRdata* rdata, const void* const base) {
        base_ = base;
        rdata_ = rdata;
    }
    RbtDataSrcResult moveToNext();
    RbtDataSrcResult getFirstField(RbtRdataFieldHandle& field);
private:
    const void* base_;
    const RbtRdata* rdata_;
};

struct RdataField;
class RbtRdataFieldHandle {
public:
    void set(const RdataField* field, const void* const base) {
        base_ = base;
        field_ = field;
    }
    void convertToRbtNode(RbtNode* node);
private:
    const void* base_;
    const RdataField* field_;
};

struct CompressOffset {
    //static const size_t MAXNODES = 9182;
    static const size_t MAXNODES = 4096;
    uint16_t offsets[MAXNODES];
};

#endif  // __RBT_DATASRC_H

// Local Variables:
// mode: c++
// End:
