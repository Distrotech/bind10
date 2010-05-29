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
#include <config.h>             // for UNUSED_PARAM

#include <sys/mman.h>
#include <sys/errno.h>
#include <fcntl.h>

#include <cassert>

#include <string>
#include <sstream>

#include <exceptions/exceptions.h>

#include <dns/buffer.h>
#include <dns/messagerenderer.h>
#include <dns/name.h>
#include <dns/rdataclass.h>
#include <dns/rrclass.h>
#include <dns/rrset.h>
#include <dns/rrttl.h>
#include <dns/rrtype.h>

#include "rbt_datasrc.h"
#include "offsetptr.h"

using namespace std;
using namespace isc;
using namespace isc::dns;

struct RbtRRset;
class RbtNodeChain;
struct RbtRdataSet;
struct RbtRdata;
struct RdataField;

typedef OffsetPtr<RbtNodeImpl> RbtNodeImplPtr; 
typedef OffsetPtr<unsigned char> DataPtr;
typedef OffsetPtr<RbtRRset> RbtRRsetPtr;
typedef OffsetPtr<RbtRdataSet> RbtRdataSetPtr;
typedef OffsetPtr<RbtRdata> RbtRdataPtr;
typedef OffsetPtr<RdataField> RdataFieldPtr;

namespace {
// inline bool isRootNode(const RbtNodeImplPtr& node) {
//     return (node.getPtr()->is_root_);
// }

enum { RED, BLACK };
}

struct RbtNodeImpl {
    RbtNodeImpl() : parent_(RbtNodeImplPtr()), left_(RbtNodeImplPtr()),
                    right_(RbtNodeImplPtr()), down_(RbtNodeImplPtr()),
                    namelen_(0), namedata_(DataPtr()), color_(BLACK),
                    data_(RbtRdataSetPtr()), references_(0), is_root_(false),
                    is_delegating_(false), absolute_(false), index_(0) {}
    string nodeNameToText(const void* const base) const {
        LabelSequence sequence;
        sequence.set(namedata_.getConstPtr(base));
        return (sequence.toText());
    }
    Name getFullNodeName(const void* const base) const;
    const RbtNodeImpl* findUp(const void* const base) const {
        const RbtNodeImpl* root;
        for (root = this;
             !root->is_root_;
             root = root->parent_.getConstPtr(base)) {
            ;
        }
        return (root->parent_.getConstPtr(base));         
    }
    bool isAbsolute() const { return (absolute_); }
    const unsigned char* getNameData(const void* base) const {
        return (namedata_.getConstPtr(base));
    }
    unsigned int getNameLen() const { return (namelen_); }
    unsigned long getIndex() const { return (index_); }
    RbtNodeImplPtr parent_;
    RbtNodeImplPtr left_;
    RbtNodeImplPtr right_;
    RbtNodeImplPtr down_;

    unsigned int namelen_;
    DataPtr namedata_;
    unsigned int color_;
    RbtRdataSetPtr data_;
    unsigned int references_;
    bool is_root_;
    bool is_delegating_;
    bool absolute_;
    unsigned long index_;
};

struct RbtDBImpl {
    static const size_t ALIGNMENT_SIZE = sizeof(void*);
    enum DBType { MEMORY, FILE_LOAD, FILE_SERVE };

    RbtDataSrcResult addNode(const Name& name, RbtNodeImpl** nodep);
    RbtNodeImplPtr createNode(const LabelSequence& sequence);
    RbtDataSrcResult findNode(const Name& name, RbtNodeImpl** nodep) const;
    void addOnLevel(RbtNodeImplPtr& node, RbtNodeImpl* current, int order,
                    RbtNodeImplPtr* root);
    void rotateLeft(RbtNodeImpl* node, RbtNodeImplPtr* rootp);
    void rotateRight(RbtNodeImpl* node, RbtNodeImplPtr* rootp);
    // This should eventually be an allocator from the mmaped area.
    void* allocateRegion(size_t size);
    void moveChainToLast(RbtNodeChain* chain, RbtNodeImpl* node) const;
    RbtDataSrcResult nodeChainPrev(RbtNodeChain* chain) const;
    string printTree(RbtNodeImpl* root, RbtNodeImpl* parent, int depth) const;
    string printNodeName(RbtNodeImpl* root) const;
    // sum of allocated memory fragments (used for estimation):
    DBType dbtype_;
    size_t allocated_;
    void* base_;
    void* current_;             // effective for the FILE_LOAD mode only
    size_t dbsize_;             // effective for the FILE_xxx mode only
    RbtNodeImplPtr root_;
    unsigned int nodecount_;
    RbtNodeImpl* apexnode_;
};

inline size_t
alignUp(size_t size) {
    return ((size + RbtDBImpl::ALIGNMENT_SIZE - 1) &
            (~(RbtDBImpl::ALIGNMENT_SIZE - 1)));
}

class RbtNodeChain {
public:
    RbtNodeChain() : end_(NULL), level_count_(0), level_matches_(0) {
        memset(levels_, 0, sizeof(levels_));
    }
    void reset() {
        end_ = NULL;
        level_count_ = 0;
        level_matches_ = 0;
    }
    void addLevel(RbtNodeImpl* node) {
        levels_[level_count_++] = node;
    }
    void trimLevel() {
        assert(level_count_ > 0);
        end_ = levels_[--level_count_];
    }
    void setEnd(RbtNodeImpl* node) { end_ = node; }
    void setMatchLevel(const unsigned int level) {
        assert(level <= level_count_);
        level_matches_ = level;
    }
    unsigned int getCurrentLevel() { return (level_count_); }
private:
    static const size_t RBT_LEVELBLOCK = 254;
    RbtNodeImpl* end_;
    RbtNodeImpl* levels_[RBT_LEVELBLOCK];
    unsigned int level_count_;
    unsigned int level_matches_;
};

struct RbtRdataSet {
    RbtRdataSet(const RRTTL& ttl, const RRType& rrtype,
                const RRClass& rrclass) :
        next_(RbtRdataSetPtr()), rrttl_(ttl.getValue()),
        rrtype_(rrtype.getCode()), rrclass_(rrclass.getCode()),
        rdata_(RbtRdataPtr()) {}
    RbtRdataSetPtr next_;
    uint32_t rrttl_;
    uint16_t rrtype_;
    uint16_t rrclass_;
    unsigned int nrdata_;
    RbtRdataPtr rdata_;
};

struct RdataField {
    RdataField() : next_(RdataFieldPtr()), data_(DataPtr()) {}
    RdataFieldPtr next_;
    DataPtr data_;        // opaque data or pointer to RbtNode (dname)
};

struct RbtRdata {
    RbtRdata() : next_(RbtRdataPtr()), fields_(RdataFieldPtr()), nfields_(0) {}
    RbtRdataPtr next_;
    RdataFieldPtr fields_;
    uint16_t nfields_;
};

struct RbtRRsetImpl {
    RbtRRsetImpl() : rbtnode_(NULL), rdataset_(NULL), base_(NULL),
                     owner_(NULL), rrclass_(0), rrtype_(0), rrttl_(0) {}
    ~RbtRRsetImpl() { delete owner_; }
    const RbtNodeImpl* rbtnode_;
    const RbtRdataSet* rdataset_;
    const void* base_;

    // used for abstract class requirement
    Name* owner_;
    RRClass rrclass_;
    RRType rrtype_;
    RRTTL rrttl_;
};

RbtRRset::RbtRRset() : impl_(new RbtRRsetImpl) {}

RbtRRset::~RbtRRset() {
    delete impl_;
}

namespace {
inline bool
isRed(const RbtNodeImplPtr& node, const void* base) {
    return (!node.isNull() && node.getConstPtr(base)->color_ == RED);
}

inline bool
isBlack(const RbtNodeImplPtr& node, void* base) {
    return (node.isNull() || node.getConstPtr(base)->color_ == BLACK);
}
}

RbtDataSrc::RbtDataSrc(const Name& origin) {
    impl_ = new(RbtDBImpl);
    impl_->base_ = NULL;
    impl_->root_ = RbtNodeImplPtr();
    impl_->nodecount_ = 0;
    impl_->allocated_ = 0;
    impl_->dbtype_ = RbtDBImpl::MEMORY;

    impl_->apexnode_ = NULL;
    RbtDataSrcResult result = impl_->addNode(origin, &impl_->apexnode_);
    assert(result == RbtDataSrcSuccess); // XXX assert is a bad choice here.
}

RbtDataSrc::RbtDataSrc(const Name& origin, const char& dbfile,
                       OpenMode mode) : impl_(NULL)
{
    impl_ = new(RbtDBImpl);

    impl_->nodecount_ = 0;      // meaningless for the SERVE mode
    impl_->allocated_ = 0;      // ditto
    impl_->dbtype_ = mode == LOAD ? RbtDBImpl::FILE_LOAD : RbtDBImpl::FILE_SERVE;

    int fd = open(&dbfile, O_RDWR);
    if (fd < 0) {
        isc_throw(Exception, "failed to open DB file(" << string(&dbfile)
                  << "): " << string(strerror(errno)));
    }
    uint64_t dbsize;
    read(fd, &dbsize, sizeof(dbsize));
    impl_->dbsize_ = (size_t)dbsize;
    if (mode == LOAD) {
        
        impl_->base_ = mmap(NULL, impl_->dbsize_, PROT_READ | PROT_WRITE,
                            MAP_FILE | MAP_SHARED, fd, 0);
        if (impl_->base_ == MAP_FAILED) {
            close(dbfile);
            isc_throw(Exception, "mmap for write failed: " <<
                      string(strerror(errno)));
        }
        close(dbfile);

        // In this very simple implementation, "header" is the 64-bit DB size
        // field followed by an offset pointer to the root node.
        size_t header_size = sizeof(dbsize) + alignUp(sizeof(impl_->root_));
        
        uintptr_t headptr = reinterpret_cast<uintptr_t>(impl_->base_);
        impl_->current_ = reinterpret_cast<void*>(headptr + header_size);
        impl_->root_ = RbtNodeImplPtr();
        impl_->apexnode_ = NULL;
        RbtDataSrcResult result = impl_->addNode(origin, &impl_->apexnode_);
        if (result != RbtDataSrcSuccess) {
            munmap(impl_->base_, impl_->dbsize_);
            isc_throw(Exception, "Failed to add the apexnode: " << result);
        }
    } else {
        impl_->base_ = mmap(NULL, impl_->dbsize_,
                            PROT_READ, MAP_FILE | MAP_PRIVATE, fd, 0);
        if (impl_->base_ == MAP_FAILED) {
            close(dbfile);
            isc_throw(Exception, "mmap for load failed: " <<
                      string(strerror(errno)));
        }
        close(dbfile);
        impl_->current_ = impl_->base_; // unused
        uintptr_t headptr = reinterpret_cast<uintptr_t>(impl_->base_);
        memcpy(&impl_->root_,
               reinterpret_cast<const void*>(headptr + sizeof(dbsize)),
               sizeof(impl_->root_));
        impl_->apexnode_ = NULL;
        RbtDataSrcResult result = impl_->findNode(origin, &impl_->apexnode_);
        if (result != RbtDataSrcSuccess) {
            munmap(impl_->base_, impl_->dbsize_);
            isc_throw(Exception, "Unexpected result for apexnode: " << result);
        }
    }
}

RbtDataSrc::~RbtDataSrc() {
    if (impl_->dbtype_ == RbtDBImpl::FILE_LOAD) {
        // copy the offset pointer to the root node into the "header".
        memcpy(reinterpret_cast<char*>(impl_->base_) + sizeof(uint64_t),
               &impl_->root_, sizeof(impl_->root_));

        // XXX: should check the return values of the following
        msync(impl_->base_, impl_->dbsize_, MS_SYNC);
    }
    if (impl_->dbtype_ == RbtDBImpl::FILE_LOAD ||
        impl_->dbtype_ == RbtDBImpl::FILE_SERVE) {
        munmap(impl_->base_, impl_->dbsize_);
    }
    delete impl_;
}

size_t
RbtDataSrc::getAllocatedMemorySize() const {
    return (impl_->allocated_);
}

string
RbtDataSrc::toText() const {
    return (impl_->printTree(impl_->root_.getPtr(impl_->base_), NULL, 0));
}


RbtDataSrcResult
RbtDataSrc::addNode(const Name& name, RbtNode* retnode) {
    RbtNodeImpl* node = NULL;
    RbtDataSrcResult result = impl_->addNode(name, &node);
    if (result == RbtDataSrcSuccess || result == RbtDataSrcExist) {
        retnode->set(node, impl_->base_);
    }
    return (result);
}

void *
RbtDBImpl::allocateRegion(const size_t size) {
    void* ret;
    const size_t alloc_size = alignUp(size);
    if (dbtype_ == FILE_LOAD) {
        uintptr_t curptr = reinterpret_cast<uintptr_t>(current_);
        uintptr_t baseptr = reinterpret_cast<uintptr_t>(base_);
        assert(curptr - baseptr + alloc_size <= dbsize_); // XXX
        ret = current_;
        current_ = reinterpret_cast<void*>(
            reinterpret_cast<uintptr_t>(current_) + alloc_size);
    } else if (dbtype_ == MEMORY) {
        ret = new char[size];
    } else {
        isc_throw(Exception, "memory allocation required for read-only DB");
    }

    allocated_ += alloc_size;
    return (ret);
}

RbtDataSrcResult
RbtDBImpl::addNode(const Name& name, RbtNodeImpl** nodep) {
    void* baseptr = base_;

    if (root_.isNull()) {
        LabelSequence sequence;
        name.setLabelSequence(sequence);
        RbtNodeImplPtr new_current = createNode(sequence);
        root_ = new_current;
        new_current.getPtr(baseptr)->is_root_ = true;
        new_current.getPtr(baseptr)->absolute_ = true;
        *nodep = new_current.getPtr(baseptr);
        return (RbtDataSrcSuccess);
    }

    RbtNodeImplPtr* root = &root_;
    assert(root->getPtr(baseptr)->is_root_);

    RbtNodeImplPtr parent;
    RbtNodeImplPtr child = *root;
    RbtNodeImpl* current;

    int order;
    RbtDataSrcResult result = RbtDataSrcSuccess;
    LabelSequence new_sequence;
    name.setLabelSequence(new_sequence);
    LabelSequence current_sequence, prefix, suffix;
    do {
        current = child.getPtr(baseptr);

        current_sequence.set(current->namedata_.getConstPtr(baseptr));
        NameComparisonResult cmpresult = new_sequence.compare(current_sequence);
        order = cmpresult.getOrder();
        if (cmpresult.getRelation() == NameComparisonResult::EQUAL) {
            *nodep = current;
            result = RbtDataSrcExist;
            break;
        }

        if (cmpresult.getRelation() == NameComparisonResult::NONE) {
            parent = child;
            if (order < 0) {
                child = current->left_;
            } else {
                assert(order > 0);
                child = current->right_;
            }
        } else {
            const unsigned int common_labels = cmpresult.getCommonLabels();
            if (cmpresult.getRelation() == NameComparisonResult::SUBDOMAIN) {
                // All of the existing labels are in common,
                // so the new name is in a subtree.
                //newname = newname.split(0, newname.getLabelCount() -
                //common_labels);
                new_sequence.split(-common_labels);
                root = &current->down_;
                assert((*root).isNull() ||
                       ((*root).getPtr(baseptr)->is_root_ &&
                        (*root).getPtr(baseptr)->parent_.getPtr(baseptr) ==
                        current));
                parent = RbtNodeImplPtr();
                child = current->down_;
            } else {
                // The number of labels in common is fewer
                // than the number of labels at the current
                // node, so the current node must be adjusted
                // to have just the common suffix, and a down
                // pointer made to a new tree.
                assert(cmpresult.getRelation() ==
                       NameComparisonResult::COMMONANCESTOR ||
                       cmpresult.getRelation() ==
                       NameComparisonResult::SUPERDOMAIN);

                // Ensure the number of levels in the tree
                // does not exceed the number of logical
                // levels allowed by DNSSEC.
                // (Is not necessary?  Let's skip it for now)

                current_sequence.split(prefix, suffix, -common_labels);
                RbtNodeImplPtr new_current = createNode(suffix);
                new_current.getPtr(baseptr)->absolute_ = current->absolute_;
                new_current.getPtr(baseptr)->is_root_ = current->is_root_;
                // Skip for now
                //new_current->nsec = (current->nsec == DNS_RBT_NSEC_HAS_NSEC) ?
                //DNS_RBT_NSEC_NORMAL : current->nsec;
                new_current.getPtr(baseptr)->parent_ = current->parent_;
                new_current.getPtr(baseptr)->left_ = current->left_;
                new_current.getPtr(baseptr)->right_ = current->right_;
                new_current.getPtr(baseptr)->color_ = current->color_;

                if (!parent.isNull()) {
                    if (parent.getPtr(baseptr)->left_.getPtr(baseptr) ==
                        current) {
                        parent.getPtr(baseptr)->left_ = new_current;
                    } else {
                        parent.getPtr(baseptr)->right_ = new_current;
                    }
                }
                if (!new_current.getPtr(baseptr)->left_.isNull()) {
                    new_current.getPtr(baseptr)->left_.getPtr(baseptr)->parent_
                        = new_current;
                }
                if (!new_current.getPtr(baseptr)->right_.isNull()) {
                    new_current.getPtr(baseptr)->right_.getPtr(baseptr)->parent_
                        = new_current;
                }
                if ((*root).getPtr(baseptr) == current) {
                    *root = new_current;
                }

                OutputBuffer b(0);
                prefix.toWire(b);
                memcpy(current->namedata_.getPtr(baseptr), b.getData(),
                       b.getLength());
                current->namelen_ = prefix.getDataLength();

                // Set up the new root of the next level.
                current->is_root_ = true;
                current->parent_ = new_current;
                new_current.getPtr(baseptr)->down_ = RbtNodeImplPtr(current,
                                                                    baseptr);
                root = &new_current.getPtr(baseptr)->down_;

                current->left_ = RbtNodeImplPtr();
                current->right_ = RbtNodeImplPtr();
                current->color_ = BLACK;
                current->absolute_ = false;

                if (common_labels == new_sequence.getOffsetLength()) {
                    *nodep = new_current.getPtr(baseptr);
                    return (RbtDataSrcSuccess);
                } else {
                    new_sequence.split(-common_labels);
                }
            }
        }
    } while (child.getPtr(baseptr) != NULL);

    if (result == RbtDataSrcSuccess) {
        RbtNodeImplPtr new_current = createNode(new_sequence);
        *nodep = new_current.getPtr(baseptr);
        if (new_sequence.getDataLength() == name.getLength()) {
            (*nodep)->absolute_ = true;
        }
        addOnLevel(new_current, current, order, root);
    }

    return (result);
}

Name
RbtNodeImpl::getFullNodeName(const void* const base) const {
    const RbtNodeImpl* node = this;
    OutputBuffer ob(0);
    LabelSequence sequence;

    sequence.set(namedata_.getConstPtr(base));
    ob.writeData(sequence.getData(), sequence.getDataLength());

    for (node = this->findUp(base); node != NULL; node = node->findUp(base)) {
        sequence.set(node->namedata_.getConstPtr(base));
        ob.writeData(sequence.getData(), sequence.getDataLength());
        if (node->absolute_) {
            break;
        }
    }
    assert(node != NULL || this->absolute_);

    InputBuffer ib(ob.getData(), ob.getLength());
    return (Name(ib));
}

RbtDataSrcResult
RbtDataSrc::getApexNode(RbtNode* node) const {
    if (impl_->apexnode_ != NULL) {
        node->set(impl_->apexnode_, impl_->base_);
        return (RbtDataSrcSuccess);
    } else {
        return (RbtDataSrcNotFound);
    }
}

RbtDataSrcResult
RbtDataSrc::findNode(const Name& name, RbtNode* node) const {
    RbtNodeImpl* implnode = NULL;
    RbtDataSrcResult result = impl_->findNode(name, &implnode);
    if (result == RbtDataSrcSuccess || result == RbtDataSrcPartialMatch) {
        node->set(implnode, impl_->base_);
    }
    return (result);
}

void
RbtDataSrc::addRRset(const isc::dns::AbstractRRset& rrset) {
    RbtNodeImpl* node = NULL;
    RbtDataSrcResult result = impl_->addNode(rrset.getName(), &node);
    assert((result == RbtDataSrcSuccess || result == RbtDataSrcExist) &&
           node != NULL); 

    // XXX: omit duplicate check in the prototype

    RbtRdataSet* rdataset =
        new(impl_->allocateRegion(sizeof(RbtRdataSet)))
        RbtRdataSet(rrset.getTTL(), rrset.getType(), rrset.getClass());

    RdataIteratorPtr it = rrset.getRdataIterator();
    OutputBuffer ob(0);
    for (it->first(); !it->isLast(); it->next()) {
        if (rrset.getType() == RRType::NS()) {
            if (node != impl_->apexnode_) {
                node->is_delegating_ = true;
            }

            const Name& nsname =
                dynamic_cast<const rdata::generic::NS&>(
                    it->getCurrent()).getNSName();
            RbtNodeImpl* rbtnode = NULL;
            RbtDataSrcResult result = impl_->addNode(nsname, &rbtnode);
            assert(result == RbtDataSrcSuccess || result == RbtDataSrcExist);

            RdataField* field =
                new(impl_->allocateRegion(sizeof(RdataField))) RdataField;
            // XXX: evil cast
            field->data_ = DataPtr(reinterpret_cast<unsigned char*>(rbtnode),
                                   impl_->base_);

            RbtRdata* rdata =
                new(impl_->allocateRegion(sizeof(RbtRdata))) RbtRdata;
            rdata->nfields_ = 1;
            rdata->fields_ = RdataFieldPtr(field, impl_->base_);
            rdata->next_ = rdataset->rdata_;
            rdataset->rdata_ = RbtRdataPtr(rdata, impl_->base_);
        } else if (rrset.getType() == RRType::SOA()) {
            const rdata::generic::SOA& soa =
                dynamic_cast<const rdata::generic::SOA&>(
                    it->getCurrent());
            RbtNodeImpl* rbtnode = NULL;
            RbtDataSrcResult result = impl_->addNode(soa.getMName(),
                                                     &rbtnode);
            assert(result == RbtDataSrcSuccess || result == RbtDataSrcExist);

            RdataField* field1 =
                new(impl_->allocateRegion(sizeof(RdataField))) RdataField;
            field1->data_ = DataPtr(reinterpret_cast<unsigned char*>(rbtnode),
                                   impl_->base_);

            rbtnode = NULL;
            result = impl_->addNode(soa.getRName(), &rbtnode);
            assert(result == RbtDataSrcSuccess || result == RbtDataSrcExist);
            RdataField* field2 =
                new(impl_->allocateRegion(sizeof(RdataField))) RdataField;
            field2->data_ = DataPtr(reinterpret_cast<unsigned char*>(rbtnode),
                                   impl_->base_);
            field1->next_ = RdataFieldPtr(field2, impl_->base_);

            size_t names_len = soa.getMName().getLength() +
                soa.getRName().getLength();
            ob.clear();
            ob.skip(2);
            soa.toWire(ob);
            ob.writeUint16At(ob.getLength() - names_len - 2, 0);
            unsigned char* data = static_cast<unsigned char*>(
                impl_->allocateRegion(ob.getLength() - names_len));
            memcpy(data, ob.getData(), 2);
            memcpy(data + 2, reinterpret_cast<const char*>(ob.getData()) +
                   2 + names_len, ob.getLength() - names_len - 2);
            RdataField* field3 =
                new(impl_->allocateRegion(sizeof(RdataField))) RdataField;
            field3->data_ = DataPtr(data, impl_->base_);
            field2->next_ = RdataFieldPtr(field3, impl_->base_);

            RbtRdata* rdata =
                new(impl_->allocateRegion(sizeof(RbtRdata))) RbtRdata;
            rdata->nfields_ = 3;
            rdata->fields_ = RdataFieldPtr(field1, impl_->base_);
            rdata->next_ = rdataset->rdata_;
            rdataset->rdata_ = RbtRdataPtr(rdata, impl_->base_);
        } else {
            ob.clear();
            ob.skip(2);
            it->getCurrent().toWire(ob);
            ob.writeUint16At(ob.getLength() - 2, 0);
            unsigned char* data = static_cast<unsigned char*>(
                impl_->allocateRegion(ob.getLength()));
            memcpy(data, ob.getData(), ob.getLength());

            RdataField* field =
                new(impl_->allocateRegion(sizeof(RdataField))) RdataField;
            field->data_ = DataPtr(data, impl_->base_);

            RbtRdata* rdata =
                new(impl_->allocateRegion(sizeof(RbtRdata))) RbtRdata;
            rdata->nfields_ = 1;
            rdata->fields_ = RdataFieldPtr(field, impl_->base_);
            rdata->next_ = rdataset->rdata_;
            rdataset->rdata_ = RbtRdataPtr(rdata, impl_->base_);
        }
    }

    rdataset->next_ = node->data_;
    node->data_ = RbtRdataSetPtr(rdataset, impl_->base_);
}

RbtNodeImplPtr
RbtDBImpl::createNode(const LabelSequence& sequence) {
    RbtNodeImpl* node = new(allocateRegion(sizeof(RbtNodeImpl))) RbtNodeImpl;

    OutputBuffer b(0);
    sequence.toWire(b);
    unsigned char* data_region =
        new(allocateRegion(b.getLength())) unsigned char[b.getLength()];
    memcpy(data_region, b.getData(), b.getLength());

    node->namelen_ = sequence.getDataLength();
    node->namedata_ = DataPtr(data_region, base_);

    ++nodecount_;
    node->index_ = nodecount_;

    return (RbtNodeImplPtr(node, base_));
}

void
RbtDBImpl::addOnLevel(RbtNodeImplPtr& node, RbtNodeImpl* current, int order,
                      RbtNodeImplPtr* rootp)
{
    RbtNodeImplPtr root = *rootp;
    if (root.isNull()) {
        node.getPtr(base_)->color_ = BLACK;
        node.getPtr(base_)->is_root_ = true;
        node.getPtr(base_)->parent_ = RbtNodeImplPtr(current, base_);
        *rootp = node;
        return;
    }

    if (order < 0) {
        assert(current->left_.isNull());
        current->left_ = node;
    } else {
        assert(current->right_.isNull());
        current->right_ = node;
    }

    assert(node.getPtr(base_)->parent_.isNull());
    node.getPtr(base_)->parent_ = RbtNodeImplPtr(current, base_);

    node.getPtr(base_)->color_ = RED;

    while (node != root && isRed(node.getPtr(base_)->parent_, base_)) {
        RbtNodeImpl* parent = node.getPtr(base_)->parent_.getPtr(base_);
        RbtNodeImpl* grandparent = parent->parent_.getPtr(base_);
        const bool from_left = (parent == grandparent->left_.getPtr(base_));

        RbtNodeImpl* child = (from_left ?
                              grandparent->right_.getPtr(base_) :
                              grandparent->left_.getPtr(base_));
        if (child != NULL && child->color_ == RED) {
            parent->color_ = BLACK;
            child->color_ = BLACK;
            grandparent->color_ = RED;
            node = RbtNodeImplPtr(grandparent, base_);
        } else {
            if (node == (from_left ? parent->right_ : parent->left_)) {
                from_left ? rotateLeft(parent, &root) :
                    rotateRight(parent, &root);
                node = RbtNodeImplPtr(parent, base_);
                parent = parent->parent_.getPtr(base_);
                grandparent = parent->parent_.getPtr(base_);
            }
            parent->color_ = BLACK;
            grandparent->color_ = RED;
            from_left ? rotateRight(grandparent, &root) :
                rotateLeft(grandparent, &root);
        }
    }

    root.getPtr(base_)->color_ = BLACK;
    assert(root.getPtr(base_)->is_root_);
    *rootp = root;

    return;
}

inline void
RbtDBImpl::rotateLeft(RbtNodeImpl* node, RbtNodeImplPtr* rootp) {
    RbtNodeImpl* child = node->right_.getPtr(base_);
    assert(child != NULL);

    node->right_ = child->left_;
    if (!child->left_.isNull()) {
        child->left_.getPtr(base_)->parent_ = RbtNodeImplPtr(node, base_);
    }
    child->left_ = RbtNodeImplPtr(node, base_);
    child->parent_ = node->parent_;
    if (node->is_root_) {
        *rootp = RbtNodeImplPtr(child, base_);
        child->is_root_ = true;
        node->is_root_ = false;
    } else {
        if (node->parent_.getPtr(base_)->left_.getPtr(base_) == node) {
            node->parent_.getPtr(base_)->left_ = RbtNodeImplPtr(child, base_);
        } else {
            node->parent_.getPtr(base_)->right_ = RbtNodeImplPtr(child, base_);
        }
    }

    node->parent_ = RbtNodeImplPtr(child, base_);
}

inline void
RbtDBImpl::rotateRight(RbtNodeImpl* node, RbtNodeImplPtr* rootp) {
    RbtNodeImpl* child = node->left_.getPtr(base_);
    assert(child != NULL);

    node->left_ = child->right_;
    if (!child->right_.isNull()) {
        child->right_.getPtr(base_)->parent_ = RbtNodeImplPtr(node, base_);
    }
    child->right_ = RbtNodeImplPtr(node, base_);
    child->parent_ = node->parent_;
    if (node->is_root_) {
        *rootp = RbtNodeImplPtr(child, base_);
        child->is_root_ = true;
        node->is_root_ = false;
    } else {
        if (node->parent_.getPtr(base_)->left_.getPtr(base_) == node) {
            node->parent_.getPtr(base_)->left_ = RbtNodeImplPtr(child, base_);
        } else {
            node->parent_.getPtr(base_)->right_ = RbtNodeImplPtr(child, base_);
        }
    }

    node->parent_ = RbtNodeImplPtr(child, base_);
}

inline void
RbtDBImpl::moveChainToLast(RbtNodeChain* chain, RbtNodeImpl* node) const {
    do {
        while (!node->right_.isNull()) {
            node = node->right_.getPtr(base_);
        }
        if (node->down_.isNull()) {
            break;
        }
        chain->addLevel(node);
        node = node->down_.getPtr(base_);
    } while (true);

    chain->setEnd(node);
}

inline RbtDataSrcResult
RbtDBImpl::nodeChainPrev(RbtNodeChain* chain UNUSED_PARAM) const {
    // TBD
    return (RbtDataSrcSuccess);
}

RbtDataSrcResult
RbtDBImpl::findNode(const Name& name, RbtNodeImpl** nodep) const {
    const bool empty_ok = true; // should eventually be configurable.
    const bool noexact = false; // ditto.
    const bool no_predecessor = false; // ditto

    if (root_.isNull()) {
        return (RbtDataSrcNotFound);
    }

    // chain: not sure if we already need this, but add it anyway.
    RbtNodeChain local_chain;
    RbtNodeChain* chain = &local_chain;

    // callback name

    // search_sequence is a dname label sequence being sought in each tree
    // level.
    LabelSequence search_sequence;
    name.setLabelSequence(search_sequence);

    RbtNodeImpl* current = root_.getPtr(base_);
    RbtNodeImpl* last_compared = NULL;

    RbtDataSrcResult result = RbtDataSrcSuccess;
    bool found_partial = false;
    bool was_subdomain = false;
    bool was_norelation = false;
    int order = 0;

    LabelSequence current_sequence;
    while (current != NULL) {
        current_sequence.set(current->namedata_.getPtr(base_));
        NameComparisonResult cmpresult =
            search_sequence.compare(current_sequence);
        last_compared = current;
        was_norelation = false;
        was_subdomain = false;
        order = cmpresult.getOrder();
        if (cmpresult.getRelation() == NameComparisonResult::EQUAL) {
            break;
        }

        if (cmpresult.getRelation() == NameComparisonResult::NONE) {
            was_norelation = true;

            if (order < 0) {
                current = current->left_.getPtr(base_);
            } else {
                current = current->right_.getPtr(base_);
            }
        } else {
            const unsigned int common_labels = cmpresult.getCommonLabels();
            if (cmpresult.getRelation() == NameComparisonResult::SUBDOMAIN) {
                was_subdomain = true;
                search_sequence.split(-common_labels);
                if (current->is_delegating_ &&
                    (!current->data_.isNull() || empty_ok)) {
                    *nodep = current;
                    found_partial = true;
                    current = NULL;
                    break;
                }

                // Point the chain to the next level.
                chain->addLevel(current);

                // callback for NS: omitting for now

                // Finally, head to the next tree level.
                current = current->down_.getPtr(base_);
            } else {
                assert(cmpresult.getRelation() ==
                       NameComparisonResult::COMMONANCESTOR ||
                       cmpresult.getRelation() ==
                       NameComparisonResult::SUPERDOMAIN);
                current = NULL;
            }
        }
    }

    if (current != NULL && !noexact &&
        (!current->data_.isNull() || empty_ok)) {
        // Found an exact match.
        chain->setEnd(current);
        chain->setMatchLevel(chain->getCurrentLevel());
        *nodep = current;
        result = RbtDataSrcSuccess;
    } else {
        // Did not find an exact match (or did not want one).
        if (found_partial) {
            // Unwind the chain to the partial match node: not sure if necessary
            result = RbtDataSrcPartialMatch;
        } else {
            result = RbtDataSrcNotFound;
        }

        if (current != NULL) {
            // There was an exact match but either noexact was set, or
            // empty data is okay and the node had no data.

            // chain->end = current;
        } else if (no_predecessor) {
            chain->setEnd(NULL);
        } else {
            // no exact match.  chain needs to be pointed at the DNSSEC
            // predecessor of the search name.
            if (was_subdomain) {
                chain->trimLevel();
            } else {
                // Point current to the node that stopped the search.
                // We don't use hashing within a tree unlike BIND 9, so
                // this should be able to be simplified.
                assert(last_compared != NULL);
                current = last_compared;
                if (order > 0) {
                    if (!current->down_.isNull()) {
                        chain->addLevel(current);
                        moveChainToLast(chain, current->down_.getPtr(base_));
                    } else {
                        chain->setEnd(current);
                    }
                } else {
                    assert(order < 0);
                    chain->setEnd(current);
                    if (nodeChainPrev(chain) == RbtDataSrcNotFound) {
                        chain->reset();
                    }
                }
            }
        }
    }

    return (result);
}

string
RbtDBImpl::printNodeName(RbtNodeImpl*node) const {
    return (node->nodeNameToText(base_));
}

namespace {
inline void
addIndent(stringstream& ss, int depth) {
    for (int i = 0; i < depth; ++i) {
        ss << "\t";
    }
}
}

string
RbtDBImpl::printTree(RbtNodeImpl* root, RbtNodeImpl* parent, int depth) const {
    stringstream ss;
    addIndent(ss, depth);

    if (root != NULL) {
        ss << printNodeName(root);
        ss << " (" << root->index_ << ": " <<
            ((root->color_ == RED) ? "RED" : "black");
        if (root->absolute_) {
            ss << " absolute";
        }
        if (parent != NULL) {
            ss << " from ";
            ss << printNodeName(parent);
        }

        if ((!root->is_root_ && root->parent_.getPtr(base_) != parent) ||
            (root->is_root_ && depth > 0 &&
             root->parent_.getPtr(base_)->down_.getPtr(base_) != root)) {
            ss << " (BAD parent pointer! -> ";
            if (root->parent_.getPtr(base_) != NULL) {
                ss << printNodeName(root->parent_.getPtr(base_));
            } else {
                ss << "NULL";
            }
            ss << ")";
        }
        ss << ")" << endl;
        ++depth;

        if (root->down_.getPtr(base_) != NULL) {
            addIndent(ss, depth);
            ss << "++ BEG down from " << printNodeName(root) << endl;
            ss << printTree(root->down_.getPtr(base_), NULL, depth);
            addIndent(ss, depth);
            ss << "++ END down from " << printNodeName(root) << endl;
        }

        if (root->color_ == RED && isRed(root->left_, base_)) {
            ss << "** Red/Red color violation on left" << endl;
        }
        ss << printTree(root->left_.getPtr(base_), root, depth);

        if (root->color_ == RED && isRed(root->right_, base_)) {
            ss << "** Red/Red color violation on right" << endl;
        }
        ss << printTree(root->right_.getPtr(base_), root, depth);
    } else {
        ss << "NULL" << endl;
    }

    return (ss.str());
}

string
RbtNode::toText() {
    return (impl_->nodeNameToText(base_));
}

RbtDataSrcResult
RbtNode::findRRset(const RRType& rrtype, RbtRRset& rrset) const {
    const RbtRdataSet* rdataset;
    for (rdataset = impl_->data_.getConstPtr(base_);
         rdataset != NULL;
         rdataset = rdataset->next_.getConstPtr(base_)) {
        if (RRType(rdataset->rrtype_) == rrtype) {
            break;
        }
    }

    if (rdataset != NULL) {
        rrset.impl_->rbtnode_ = impl_;
        rrset.impl_->rdataset_ = rdataset;
        rrset.impl_->base_ = base_;
        return (RbtDataSrcSuccess);
    }

    return (RbtDataSrcNotFound);
}

unsigned int
RbtRRset::getRdataCount() const {
    return (1);                 // XXX: dummy
    //return impl_->rdataset_->...
}

const Name&
RbtRRset::getName() const {
    // slow, but okay.  we don't use this in a performance critical path.
    if (impl_->owner_ == NULL) {
        impl_->owner_ =
            new Name(impl_->rbtnode_->getFullNodeName(impl_->base_));
    }
    return (*impl_->owner_);
}

const RRClass&
RbtRRset::getClass() const {
    impl_->rrclass_ = RRClass(impl_->rdataset_->rrclass_);
    return (impl_->rrclass_);
}

const RRType&
RbtRRset::getType() const {
    impl_->rrtype_ = RRType(impl_->rdataset_->rrtype_);
    return (impl_->rrtype_);
}

const RRTTL&
RbtRRset::getTTL() const {
    impl_->rrttl_ = RRTTL(impl_->rdataset_->rrttl_);
    return (impl_->rrttl_);
}

void
RbtRRset::setName(const Name& name UNUSED_PARAM) {
    isc_throw(isc::Unexpected, "RbtRRset::setName is prohibited");
}

void
RbtRRset::setTTL(const RRTTL& ttl UNUSED_PARAM) {
    isc_throw(isc::Unexpected, "RbtRRset::setTTL is prohibited");
}

string
RbtRRset::toText() const {
    return (AbstractRRset::toText());
}

void
RbtRRset::addRdata(rdata::ConstRdataPtr rdata UNUSED_PARAM) {
    isc_throw(isc::Unexpected, "RbtRRset::addRdata is prohibited");
}

void
RbtRRset::addRdata(const rdata::Rdata& rdata UNUSED_PARAM) {
    isc_throw(isc::Unexpected, "RbtRRset::addRdata is prohibited");
}

void
renderName(MessageRenderer& renderer, const RbtNodeImpl* node,
           const void* base)
{
    CompressOffsetTable* offset_table =
        reinterpret_cast<CompressOffsetTable*>(renderer.getArg());
    if (offset_table == NULL) {
        // this is slow.
        renderer.writeName(node->getFullNodeName(base));
        return;
    }

    uint16_t offset = CompressOffsetTable::OFFSET_NOTFOUND;
    while (!node->isAbsolute() &&
           (offset = offset_table->find(node->getIndex())) ==
            CompressOffsetTable::OFFSET_NOTFOUND) {
        offset_table->insert(node->getIndex(), renderer.getLength());
        renderer.writeData(node->getNameData(base) + 1, node->getNameLen());
        node = node->findUp(base);
    }
    if (node->isAbsolute()) {
        offset_table->insert(node->getIndex(), renderer.getLength());
        renderer.writeData(node->getNameData(base) + 1, node->getNameLen());
    } else {
        if (offset >= Name::COMPRESS_POINTER_MARK16) {
            isc_throw(Exception, "invalid offset found in renderName: "
                      << offset << " node index: " << node->getIndex()
                      << " name: " << node->nodeNameToText(base));
        }
        renderer.writeUint16(Name::COMPRESS_POINTER_MARK16 | offset);
    }
}

unsigned int
RbtRRset::toWire(MessageRenderer& renderer) const {
    const RbtNodeImpl* rbtnode = impl_->rbtnode_;
    const RbtRdataSet* rdataset = impl_->rdataset_;
    const void* base = impl_->base_;
    const size_t limit = renderer.getLengthLimit();
    const RbtRdata* rdata;
    unsigned int rendered = 0;

    for (rdata = rdataset->rdata_.getConstPtr(base);
         rdata != NULL;
         ++rendered, rdata = rdata->next_.getConstPtr(base)) {
        const size_t pos0 = renderer.getLength();
        assert(pos0 < 65536);

        renderName(renderer, rbtnode, base);
        renderer.writeUint16(rdataset->rrtype_);
        renderer.writeUint16(rdataset->rrclass_);
        renderer.writeUint32(rdataset->rrttl_);

        const size_t pos = renderer.getLength();
        renderer.skip(sizeof(uint16_t)); // leave the space for RDLENGTH
        // do render data
        const RdataField* field = rdata->fields_.getConstPtr(base);
        if (RRType(rdataset->rrtype_) == RRType::NS()) {
            const RbtNodeImpl* nsnode = reinterpret_cast<const RbtNodeImpl*>(
                field->data_.getConstPtr(base));
            renderName(renderer, nsnode, base);
        } else {
            if (RRType(rdataset->rrtype_) == RRType::SOA()) {
                const RbtNodeImpl* mnode =
                    reinterpret_cast<const RbtNodeImpl*>(
                        field->data_.getConstPtr(base));
                renderName(renderer, mnode, base);

                field = field->next_.getConstPtr(base);
                const RbtNodeImpl* rnode =
                    reinterpret_cast<const RbtNodeImpl*>(
                        field->data_.getConstPtr(base));
                renderName(renderer, rnode, base);

                field = field->next_.getConstPtr(base);
            }
            const unsigned char* data = field->data_.getConstPtr(base);
            size_t datalen = data[0] * 256 + data[1];
            renderer.writeData(&data[2], datalen);
        }
        renderer.writeUint16At(renderer.getLength() - pos - sizeof(uint16_t),
                               pos);

        if (limit > 0 && renderer.getLength() > limit) {
            // truncation is needed
            renderer.trim(renderer.getLength() - pos0);
            break;
        }
    }

    if (rdata != NULL) {
        renderer.setTruncated();
    }

    return (rendered);    
}

unsigned int
RbtRRset::toWire(OutputBuffer& buffer) const {
    getName().toWire(buffer);
    return (1);
}

namespace {
class RbtRdataIterator : public RdataIterator {
public:
    RbtRdataIterator() {}
    ~RbtRdataIterator() {}
    virtual void first() {}
    virtual void next() {}
    virtual const rdata::Rdata& getCurrent() const {
        // XXX: dummy code
        return (*rdata::createRdata(RRType::A(), RRClass::IN(),
                                    "192.0.2.1"));
    }                               
    virtual bool isLast() const { return (true); }
};
}

RdataIteratorPtr
RbtRRset::getRdataIterator() const {
    return (RdataIteratorPtr(new RbtRdataIterator()));
}

void
RbtRRset::clear() {
    impl_->rbtnode_ = NULL;
    impl_->rdataset_ = NULL;
    impl_->base_ = NULL;
    delete impl_->owner_;
    impl_->owner_ = NULL;
    impl_->rrclass_ = RRClass(0);
    impl_->rrtype_ = RRType(0);
    impl_->rrttl_ = RRTTL(0);
}

RbtDataSrcResult
RbtRRset::getFirstRdata(RbtRdataHandle& rdata) const {
    if (!impl_->rdataset_->rdata_.isNull()) {
        rdata.set(impl_->rdataset_->rdata_.getConstPtr(impl_->base_),
                  impl_->base_);
        return (RbtDataSrcSuccess);
    }
    return (RbtDataSrcNotFound);
}

RbtDataSrcResult
RbtRdataHandle::moveToNext() {
    const RbtRdata* next = rdata_->next_.getConstPtr(base_);
    if (next != NULL) {
        rdata_ = next;
        return (RbtDataSrcSuccess);
    }
    return (RbtDataSrcNotFound);
}

RbtDataSrcResult
RbtRdataHandle::getFirstField(RbtRdataFieldHandle& field) {
    const RdataField* f = rdata_->fields_.getConstPtr(base_);
    if (f != NULL) {
        field.set(f, base_);
        return (RbtDataSrcSuccess);
    }
    return (RbtDataSrcNotFound);
}

void
RbtRdataFieldHandle::convertToRbtNode(RbtNode* node) {
    const RbtNodeImpl* nodeimpl = reinterpret_cast<const RbtNodeImpl*>(
        field_->data_.getConstPtr(base_));
    node->set(const_cast<RbtNodeImpl*>(nodeimpl), // XXX
              base_);
}
