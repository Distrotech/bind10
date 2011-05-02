// Copyright (C) 2009  Internet Systems Consortium, Inc. ("ISC")
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

#include <stdint.h>

#include <algorithm>
#include <string>
#include <sstream>
#include <vector>

#include <boost/foreach.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/shared_ptr.hpp>

#include <exceptions/exceptions.h>

#include <util/buffer.h>

#include <dns/edns.h>
#include <dns/exceptions.h>
#include <dns/message.h>
#include <dns/messagerenderer.h>
#include <dns/name.h>
#include <dns/opcode.h>
#include <dns/rcode.h>
#include <dns/question.h>
#include <dns/rdataclass.h>
#include <dns/rrclass.h>
#include <dns/rrtype.h>
#include <dns/rrttl.h>
#include <dns/rrset.h>

using namespace std;
using namespace boost;
using namespace isc::dns::rdata;
using namespace isc::util;

namespace isc {
namespace dns {

const uint16_t Message::DEFAULT_MAX_UDPSIZE = 512;
const uint16_t Message::DEFAULT_MAX_EDNS0_UDPSIZE = 4096;

namespace {
// protocol constants
const size_t HEADERLEN = 12;

const unsigned int OPCODE_MASK = 0x7800;
const unsigned int OPCODE_SHIFT = 11;
const unsigned int RCODE_MASK = 0x000f;

// This diagram shows the wire-format representation of the 2nd 16 bits of
// the DNS header section, which contain all defined flag bits.
//
//    +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
//    |QR|   Opcode  |AA|TC|RD|RA|  |AD|CD|   RCODE   |
//    +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
//      1  0  0  0| 0  1  1  1| 1  0  1  1| 0  0  0  0|
//         0x8         0x7         0xb         0x0
//
// This mask covers all the flag bits, and those bits only.
// Note: we reject a "flag" the is not covered by this mask in some of the
// public methods.  This means our current definition is not fully extendable;
// applications cannot introduce a new flag bit temporarily without modifying
// the source code.
const unsigned int HEADERFLAG_MASK = 0x87b0;

// This is a set of flag bits that should be preserved when building a reply
// from a request.
// Note: we assume the specific definition of HEADERFLAG_xx.  We may change
// the definition in future, in which case we need to adjust this definition,
// too (see also the description about the Message::HeaderFlag type).
const uint16_t MESSAGE_REPLYPRESERVE = (Message::HEADERFLAG_RD |
                                        Message::HEADERFLAG_CD);

const char *sectiontext[] = {
    "QUESTION",
    "ANSWER",
    "AUTHORITY",
    "ADDITIONAL"
};
}

class MessageImpl {
public:
    MessageImpl(Message::Mode mode);
    // Open issues: should we rather have a header in wire-format
    // for efficiency?
    Message::Mode mode_;
    qid_t qid_;

    // We want to use NULL for [op,r]code_ to mean the code being not
    // correctly parsed or set.  We store the real code object in
    // xxcode_placeholder_ and have xxcode_ refer to it when the object
    // is valid.
    const Rcode* rcode_;
    Rcode rcode_placeholder_;
    const Opcode* opcode_;
    Opcode opcode_placeholder_;

    uint16_t flags_;            // wire-format representation of header flags.

    bool header_parsed_;
    static const unsigned int NUM_SECTIONS = 4; // TODO: revisit this design
    int counts_[NUM_SECTIONS];   // TODO: revisit this definition
    vector<QuestionPtr> questions_;
    vector<RRsetPtr> rrsets_[NUM_SECTIONS];
    ConstEDNSPtr edns_;

    // tsig/sig0: TODO
    // RRsetsSorter* sorter_; : TODO

    void init();
    void setOpcode(const Opcode& opcode);
    void setRcode(const Rcode& rcode);
    int parseQuestion(InputBuffer& buffer);
    int parseSection(const Message::Section section, InputBuffer& buffer);
};

MessageImpl::MessageImpl(Message::Mode mode) :
    mode_(mode),
    rcode_placeholder_(Rcode(0)), // as a placeholder the value doesn't matter
    opcode_placeholder_(Opcode(0)) // ditto
{
    init();
}

void
MessageImpl::init() {
    flags_ = 0;
    qid_ = 0;
    rcode_ = NULL;
    opcode_ = NULL;
    edns_ = EDNSPtr();

    for (int i = 0; i < NUM_SECTIONS; ++i) {
        counts_[i] = 0;
    }

    header_parsed_ = false;
    questions_.clear();
    rrsets_[Message::SECTION_ANSWER].clear();
    rrsets_[Message::SECTION_AUTHORITY].clear();
    rrsets_[Message::SECTION_ADDITIONAL].clear();
}

void
MessageImpl::setOpcode(const Opcode& opcode) {
    opcode_placeholder_ = opcode;
    opcode_ = &opcode_placeholder_;
}

void
MessageImpl::setRcode(const Rcode& rcode) {
    rcode_placeholder_ = rcode;
    rcode_ = &rcode_placeholder_;
}

Message::Message(Mode mode) :
    impl_(new MessageImpl(mode))
{}

Message::~Message() {
    delete impl_;
}

bool
Message::getHeaderFlag(const HeaderFlag flag) const {
    if (flag == 0 || (flag & ~HEADERFLAG_MASK) != 0) {
        isc_throw(InvalidParameter,
                  "Message::getHeaderFlag:: Invalid flag is specified: " <<
                  flag);
    }
    return ((impl_->flags_ & flag) != 0);
}

void
Message::setHeaderFlag(const HeaderFlag flag, const bool on) {
    if (impl_->mode_ != Message::RENDER) {
        isc_throw(InvalidMessageOperation,
                  "setHeaderFlag performed in non-render mode");
    }
    if (flag == 0 || (flag & ~HEADERFLAG_MASK) != 0) {
        isc_throw(InvalidParameter,
                  "Message::getHeaderFlag:: Invalid flag is specified: " <<
                  flag);
    }
    if (on) {
        impl_->flags_ |= flag;
    } else {
        impl_->flags_ &= ~flag;
    }
}

qid_t
Message::getQid() const {
    return (impl_->qid_);
}

void
Message::setQid(qid_t qid) {
    if (impl_->mode_ != Message::RENDER) {
        isc_throw(InvalidMessageOperation,
                  "setQid performed in non-render mode");
    }
    impl_->qid_ = qid;
}

const Rcode&
Message::getRcode() const {
    if (impl_->rcode_ == NULL) {
        isc_throw(InvalidMessageOperation, "getRcode attempted before set");
    }
    return (*impl_->rcode_);
}

void
Message::setRcode(const Rcode& rcode) {
    if (impl_->mode_ != Message::RENDER) {
        isc_throw(InvalidMessageOperation,
                  "setRcode performed in non-render mode");
    }
    impl_->setRcode(rcode);
}

const Opcode&
Message::getOpcode() const {
    if (impl_->opcode_ == NULL) {
        isc_throw(InvalidMessageOperation, "getOpcode attempted before set");
    }
    return (*impl_->opcode_);
}

void
Message::setOpcode(const Opcode& opcode) {
    if (impl_->mode_ != Message::RENDER) {
        isc_throw(InvalidMessageOperation,
                  "setOpcode performed in non-render mode");
    }
    impl_->setOpcode(opcode);
}

ConstEDNSPtr
Message::getEDNS() const {
    return (impl_->edns_);
}

void
Message::setEDNS(ConstEDNSPtr edns) {
    if (impl_->mode_ != Message::RENDER) {
        isc_throw(InvalidMessageOperation,
                  "setEDNS performed in non-render mode");
    }
    impl_->edns_ = edns;
}

unsigned int
Message::getRRCount(const Section section) const {
    if (section >= MessageImpl::NUM_SECTIONS) {
        isc_throw(OutOfRange, "Invalid message section: " << section);
    }
    return (impl_->counts_[section]);
}

void
Message::addRRset(const Section section, RRsetPtr rrset, const bool sign) {
    if (impl_->mode_ != Message::RENDER) {
        isc_throw(InvalidMessageOperation,
                  "addRRset performed in non-render mode");
    }
    if (section >= MessageImpl::NUM_SECTIONS) {
        isc_throw(OutOfRange, "Invalid message section: " << section);
    }

    impl_->rrsets_[section].push_back(rrset);
    impl_->counts_[section] += rrset->getRdataCount();

    RRsetPtr sp = rrset->getRRsig();
    if (sign && sp != NULL) {
        impl_->rrsets_[section].push_back(sp);
        impl_->counts_[section] += sp->getRdataCount();
    }
}

bool
Message::hasRRset(const Section section, const Name& name,
                  const RRClass& rrclass, const RRType& rrtype)
{
    if (section >= MessageImpl::NUM_SECTIONS) {
        isc_throw(OutOfRange, "Invalid message section: " << section);
    }

    BOOST_FOREACH(ConstRRsetPtr r, impl_->rrsets_[section]) {
        if (r->getClass() == rrclass &&
            r->getType() == rrtype &&
            r->getName() == name) {
            return (true);
        }
    }

    return (false);
}

bool
Message::hasRRset(const Section section, const RRsetPtr& rrset) {
    return (hasRRset(section, rrset->getName(), rrset->getClass(), rrset->getType()));
}

bool
Message::removeRRset(const Section section, RRsetIterator& iterator) {
    if (section >= MessageImpl::NUM_SECTIONS) {
        isc_throw(OutOfRange, "Invalid message section: " << section);
    }

    bool removed = false;
    for (vector<RRsetPtr>::iterator i = impl_->rrsets_[section].begin();
            i != impl_->rrsets_[section].end(); ++i) {
        if (((*i)->getName() == (*iterator)->getName()) &&
            ((*i)->getClass() == (*iterator)->getClass()) &&
            ((*i)->getType() == (*iterator)->getType())) {

            // Found the matching RRset so remove it & ignore rest
            impl_->counts_[section] -= (*iterator)->getRdataCount();
            impl_->rrsets_[section].erase(i);
            removed = true;
            break;
        }
    }

    return (removed);
}

void
Message::clearSection(const Section section) {
    if (section >= MessageImpl::NUM_SECTIONS) {
        isc_throw(OutOfRange, "Invalid message section: " << section);
    }
    impl_->rrsets_[section].clear();
    impl_->counts_[section] = 0;
}

void
Message::addQuestion(const QuestionPtr question) {
    if (impl_->mode_ != Message::RENDER) {
        isc_throw(InvalidMessageOperation,
                  "addQuestion performed in non-render mode");
    }

    impl_->questions_.push_back(question);
    ++impl_->counts_[SECTION_QUESTION];
}

void
Message::addQuestion(const Question& question) {
    addQuestion(QuestionPtr(new Question(question)));
}

namespace {
template <typename T>
struct RenderSection {
    RenderSection(MessageRenderer& renderer, const bool partial_ok) :
        counter_(0), renderer_(renderer), partial_ok_(partial_ok),
        truncated_(false)
    {}
    void operator()(const T& entry) {
        // If it's already truncated, ignore the rest of the section.
        if (truncated_) {
            return;
        }
        const size_t pos0 = renderer_.getLength();
        counter_ += entry->toWire(renderer_);
        if (renderer_.isTruncated()) {
            truncated_ = true;
            if (!partial_ok_) {
                // roll back to the end of the previous RRset.
                renderer_.trim(renderer_.getLength() - pos0);
            }
        }
    }
    unsigned int getTotalCount() { return (counter_); }
    unsigned int counter_;
    MessageRenderer& renderer_;
    const bool partial_ok_;
    bool truncated_;
};
}

void
Message::toWire(MessageRenderer& renderer) {
    if (impl_->mode_ != Message::RENDER) {
        isc_throw(InvalidMessageOperation,
                  "Message rendering attempted in non render mode");
    }
    if (impl_->rcode_ == NULL) {
        isc_throw(InvalidMessageOperation,
                  "Message rendering attempted without Rcode set");
    }
    if (impl_->opcode_ == NULL) {
        isc_throw(InvalidMessageOperation,
                  "Message rendering attempted without Opcode set");
    }

    // reserve room for the header
    renderer.skip(HEADERLEN);

    uint16_t qdcount =
        for_each(impl_->questions_.begin(), impl_->questions_.end(),
                 RenderSection<QuestionPtr>(renderer, false)).getTotalCount();

    // TBD: sort RRsets in each section based on configuration policy.
    uint16_t ancount = 0;
    if (!renderer.isTruncated()) {
        ancount =
            for_each(impl_->rrsets_[SECTION_ANSWER].begin(),
                     impl_->rrsets_[SECTION_ANSWER].end(),
                     RenderSection<RRsetPtr>(renderer, true)).getTotalCount();
    }
    uint16_t nscount = 0;
    if (!renderer.isTruncated()) {
        nscount =
            for_each(impl_->rrsets_[SECTION_AUTHORITY].begin(),
                     impl_->rrsets_[SECTION_AUTHORITY].end(),
                     RenderSection<RRsetPtr>(renderer, true)).getTotalCount();
    }
    uint16_t arcount = 0;
    if (renderer.isTruncated()) {
        setHeaderFlag(HEADERFLAG_TC, true);
    } else {
        arcount =
            for_each(impl_->rrsets_[SECTION_ADDITIONAL].begin(),
                     impl_->rrsets_[SECTION_ADDITIONAL].end(),
                     RenderSection<RRsetPtr>(renderer, false)).getTotalCount();
    }

    // Add EDNS OPT RR if necessary.  Basically, we add it only when EDNS
    // has been explicitly set.  However, if the RCODE would require it and
    // no EDNS has been set we generate a temporary local EDNS and use it.
    if (!renderer.isTruncated()) {
        ConstEDNSPtr local_edns = impl_->edns_;
        if (!local_edns && impl_->rcode_->getExtendedCode() != 0) {
            local_edns = ConstEDNSPtr(new EDNS());
        }
        if (local_edns) {
            arcount += local_edns->toWire(renderer,
                                          impl_->rcode_->getExtendedCode());
        }
    }
 
    // Adjust the counter buffer.
    // XXX: these may not be equal to the number of corresponding entries
    // in rrsets_[] or questions_ if truncation occurred or an EDNS OPT RR
    // was inserted.  This is not good, and we should revisit the entire
    // design.
    impl_->counts_[SECTION_QUESTION] = qdcount;
    impl_->counts_[SECTION_ANSWER] = ancount;
    impl_->counts_[SECTION_AUTHORITY] = nscount;
    impl_->counts_[SECTION_ADDITIONAL] = arcount;

    // TBD: TSIG, SIG(0) etc.

    // fill in the header
    size_t header_pos = 0;
    renderer.writeUint16At(impl_->qid_, header_pos);
    header_pos += sizeof(uint16_t);

    uint16_t codes_and_flags =
        (impl_->opcode_->getCode() << OPCODE_SHIFT) & OPCODE_MASK;
    codes_and_flags |= (impl_->rcode_->getCode() & RCODE_MASK);
    codes_and_flags |= (impl_->flags_ & HEADERFLAG_MASK);
    renderer.writeUint16At(codes_and_flags, header_pos);
    header_pos += sizeof(uint16_t);
    // XXX: should avoid repeated pattern (TODO)
    renderer.writeUint16At(qdcount, header_pos);
    header_pos += sizeof(uint16_t);
    renderer.writeUint16At(ancount, header_pos);
    header_pos += sizeof(uint16_t);
    renderer.writeUint16At(nscount, header_pos);
    header_pos += sizeof(uint16_t);
    renderer.writeUint16At(arcount, header_pos);
    header_pos += sizeof(uint16_t);
}

void
Message::parseHeader(InputBuffer& buffer) {
    if (impl_->mode_ != Message::PARSE) {
        isc_throw(InvalidMessageOperation,
                  "Message parse attempted in non parse mode");
    }

    if ((buffer.getLength() - buffer.getPosition()) < HEADERLEN) {
        isc_throw(MessageTooShort, "Malformed DNS message (short length): "
                  << buffer.getLength() - buffer.getPosition());
    }

    impl_->qid_ = buffer.readUint16();
    const uint16_t codes_and_flags = buffer.readUint16();
    impl_->setOpcode(Opcode((codes_and_flags & OPCODE_MASK) >> OPCODE_SHIFT));
    impl_->setRcode(Rcode(codes_and_flags & RCODE_MASK));
    impl_->flags_ = (codes_and_flags & HEADERFLAG_MASK);
    impl_->counts_[SECTION_QUESTION] = buffer.readUint16();
    impl_->counts_[SECTION_ANSWER] = buffer.readUint16();
    impl_->counts_[SECTION_AUTHORITY] = buffer.readUint16();
    impl_->counts_[SECTION_ADDITIONAL] = buffer.readUint16();

    impl_->header_parsed_ = true;
}

void
Message::fromWire(InputBuffer& buffer) {
    if (impl_->mode_ != Message::PARSE) {
        isc_throw(InvalidMessageOperation,
                  "Message parse attempted in non parse mode");
    }

    if (!impl_->header_parsed_) {
        parseHeader(buffer);
    }

    impl_->counts_[SECTION_QUESTION] = impl_->parseQuestion(buffer);
    impl_->counts_[SECTION_ANSWER] =
        impl_->parseSection(SECTION_ANSWER, buffer);
    impl_->counts_[SECTION_AUTHORITY] =
        impl_->parseSection(SECTION_AUTHORITY, buffer);
    impl_->counts_[SECTION_ADDITIONAL] =
        impl_->parseSection(SECTION_ADDITIONAL, buffer);
}

int
MessageImpl::parseQuestion(InputBuffer& buffer) {
    unsigned int added = 0;

    for (unsigned int count = 0;
         count < counts_[Message::SECTION_QUESTION];
         ++count) {
        const Name name(buffer);

        if ((buffer.getLength() - buffer.getPosition()) <
            2 * sizeof(uint16_t)) {
            isc_throw(DNSMessageFORMERR, "Question section too short: " <<
                      (buffer.getLength() - buffer.getPosition()) << " bytes");
        }
        const RRType rrtype(buffer.readUint16());
        const RRClass rrclass(buffer.readUint16());

        // XXX: need a duplicate check.  We might also want to have an
        // optimized algorithm that requires the question section contain
        // exactly one RR.

        questions_.push_back(QuestionPtr(new Question(name, rrclass, rrtype)));
        ++added;
    }

    return (added);
}

namespace {
struct MatchRR : public unary_function<RRsetPtr, bool> {
    MatchRR(const Name& name, const RRType& rrtype, const RRClass& rrclass) :
        name_(name), rrtype_(rrtype), rrclass_(rrclass) {}
    bool operator()(const RRsetPtr& rrset) const {
        return (rrset->getType() == rrtype_ &&
                rrset->getClass() == rrclass_ &&
                rrset->getName() == name_);
    }
    const Name& name_;
    const RRType& rrtype_;
    const RRClass& rrclass_;
};
}

// Note about design decision:
// we need some type specific processing here, including EDNS and TSIG.
// how much we should generalize/hardcode the special logic is subject
// to discussion.  In terms of modularity it would be ideal to introduce
// an abstract class (say "MessageAttribute") and let other such
// concrete notions as EDNS or TSIG inherit from it.  Then we would
// just do:
// message->addAttribute(rrtype, rrclass, buffer);
// to create and attach type-specific concrete object to the message.
//
// A major downside of this approach is, as usual, complexity due to
// indirection and performance penalty.  Also, it may not be so easy
// to separate the processing logic because in many cases we'll need
// parse context for which the message class is responsible (e.g.
// to check the EDNS OPT RR only appears in the additional section,
// and appears only once).
//
// Another point to consider is that we may not need so many special
// types other than EDNS and TSIG (and when and if we implement it,
// SIG(0)); newer optional attributes of the message would more likely
// be standardized as new flags or options of EDNS.  If that's the case,
// introducing an abstract class with all the overhead and complexity
// may not make much sense.
//
// Conclusion: don't over-generalize type-specific logic for now.
// introduce separate concrete classes, and move context-independent
// logic to that class; processing logic dependent on parse context
// is hardcoded here.
int
MessageImpl::parseSection(const Message::Section section,
                          InputBuffer& buffer)
{
    assert(section < MessageImpl::NUM_SECTIONS);

    unsigned int added = 0;

    for (unsigned int count = 0; count < counts_[section]; ++count) {
        const Name name(buffer);

        // buffer must store at least RR TYPE, RR CLASS, TTL, and RDLEN.
        if ((buffer.getLength() - buffer.getPosition()) <
            3 * sizeof(uint16_t) + sizeof(uint32_t)) {
            isc_throw(DNSMessageFORMERR, sectiontext[section] <<
                      " section too short: " <<
                      (buffer.getLength() - buffer.getPosition()) << " bytes");
        }

        const RRType rrtype(buffer.readUint16());
        const RRClass rrclass(buffer.readUint16());
        const RRTTL ttl(buffer.readUint32());
        const size_t rdlen = buffer.readUint16();
        ConstRdataPtr rdata = createRdata(rrtype, rrclass, buffer, rdlen);

        if (rrtype == RRType::OPT()) {
            if (section != Message::SECTION_ADDITIONAL) {
                isc_throw(DNSMessageFORMERR,
                          "EDNS OPT RR found in an invalid section");
            }
            if (edns_) {
                isc_throw(DNSMessageFORMERR, "multiple EDNS OPT RR found");
            }

            uint8_t extended_rcode;
            edns_ = ConstEDNSPtr(createEDNSFromRR(name, rrclass, rrtype, ttl,
                                                  *rdata, extended_rcode));
            setRcode(Rcode(rcode_->getCode(), extended_rcode));
            continue;
        } else {
            vector<RRsetPtr>::iterator it =
                find_if(rrsets_[section].begin(), rrsets_[section].end(),
                        MatchRR(name, rrtype, rrclass));
            if (it != rrsets_[section].end()) {
                (*it)->setTTL(min((*it)->getTTL(), ttl));
                (*it)->addRdata(rdata);
            } else {
                RRsetPtr rrset =
                    RRsetPtr(new RRset(name, rrclass, rrtype, ttl)); 
                rrset->addRdata(rdata);
                rrsets_[section].push_back(rrset);
            }
            ++added;
        }
    }

    return (added);
}

namespace {
template <typename T>
struct SectionFormatter {
    SectionFormatter(const Message::Section section, string& output) :
        section_(section), output_(output) {}
    void operator()(const T& entry) {
        if (section_ == Message::SECTION_QUESTION) {
            output_ += ";";
        }
        output_ += entry->toText();
    }
    const Message::Section section_;
    string& output_;
};
}

string
Message::toText() const {
    if (impl_->rcode_ == NULL) {
        isc_throw(InvalidMessageOperation,
                  "Message::toText() attempted without Rcode set");
    }
    if (impl_->opcode_ == NULL) {
        isc_throw(InvalidMessageOperation,
                  "Message::toText() attempted without Opcode set");
    }

    string s;

    s += ";; ->>HEADER<<- opcode: " + impl_->opcode_->toText();
    // for simplicity we don't consider extended rcode (unlike BIND9)
    s += ", status: " + impl_->rcode_->toText();
    s += ", id: " + boost::lexical_cast<string>(impl_->qid_);
    s += "\n;; flags: ";
    if (getHeaderFlag(HEADERFLAG_QR)) {
        s += "qr ";
    }
    if (getHeaderFlag(HEADERFLAG_AA)) {
        s += "aa ";
    }
    if (getHeaderFlag(HEADERFLAG_TC)) {
        s += "tc ";
    }
    if (getHeaderFlag(HEADERFLAG_RD)) {
        s += "rd ";
    }
    if (getHeaderFlag(HEADERFLAG_RA)) {
        s += "ra ";
    }
    if (getHeaderFlag(HEADERFLAG_AD)) {
        s += "ad ";
    }
    if (getHeaderFlag(HEADERFLAG_CD)) {
        s += "cd ";
    }

    // for simplicity, don't consider the update case for now
    s += "; QUESTION: " +
        lexical_cast<string>(impl_->counts_[SECTION_QUESTION]);
    s += ", ANSWER: " +
        lexical_cast<string>(impl_->counts_[SECTION_ANSWER]);
    s += ", AUTHORITY: " +
        lexical_cast<string>(impl_->counts_[SECTION_AUTHORITY]);

    unsigned int arcount = impl_->counts_[SECTION_ADDITIONAL];
    if (impl_->edns_ != NULL) {
        ++arcount;
    }
    s += ", ADDITIONAL: " + lexical_cast<string>(arcount) + "\n";

    if (impl_->edns_ != NULL) {
        s += "\n;; OPT PSEUDOSECTION:\n";
        s += impl_->edns_->toText();
    }

    if (!impl_->questions_.empty()) {
        s += "\n;; " +
            string(sectiontext[SECTION_QUESTION]) + " SECTION:\n";
        for_each(impl_->questions_.begin(), impl_->questions_.end(),
                 SectionFormatter<QuestionPtr>(SECTION_QUESTION, s));
    }
    if (!impl_->rrsets_[SECTION_ANSWER].empty()) {
        s += "\n;; " +
            string(sectiontext[SECTION_ANSWER]) + " SECTION:\n";
        for_each(impl_->rrsets_[SECTION_ANSWER].begin(),
                 impl_->rrsets_[SECTION_ANSWER].end(),
                 SectionFormatter<RRsetPtr>(SECTION_ANSWER, s));
    }
    if (!impl_->rrsets_[SECTION_AUTHORITY].empty()) {
        s += "\n;; " +
            string(sectiontext[SECTION_AUTHORITY]) + " SECTION:\n";
        for_each(impl_->rrsets_[SECTION_AUTHORITY].begin(),
                 impl_->rrsets_[SECTION_AUTHORITY].end(),
                 SectionFormatter<RRsetPtr>(SECTION_AUTHORITY, s));
    }
    if (!impl_->rrsets_[SECTION_ADDITIONAL].empty()) {
        s += "\n;; " +
            string(sectiontext[SECTION_ADDITIONAL]) +
            " SECTION:\n";
        for_each(impl_->rrsets_[SECTION_ADDITIONAL].begin(),
                 impl_->rrsets_[SECTION_ADDITIONAL].end(),
                 SectionFormatter<RRsetPtr>(SECTION_ADDITIONAL, s));
    }

    return (s);
}

void
Message::clear(Mode mode) {
    impl_->init();
    impl_->mode_ = mode;
}

void
Message::appendSection(const Section section, const Message& source) {
    if (section >= MessageImpl::NUM_SECTIONS) {
        isc_throw(OutOfRange, "Invalid message section: " << section);
    }

    if (section == SECTION_QUESTION) {
        for (QuestionIterator qi = source.beginQuestion();
             qi != source.endQuestion();
             ++qi) {
            addQuestion(*qi);
        }
    } else {
        for (RRsetIterator rrsi = source.beginSection(section);
             rrsi != source.endSection(section);
             ++rrsi) {
            addRRset(section, *rrsi);
        }
    }
}

void
Message::makeResponse() {
    if (impl_->mode_ != Message::PARSE) {
        isc_throw(InvalidMessageOperation,
                  "makeResponse() is performed in non-parse mode");
    }

    impl_->mode_ = Message::RENDER;

    impl_->edns_ = EDNSPtr();
    impl_->flags_ &= MESSAGE_REPLYPRESERVE;
    setHeaderFlag(HEADERFLAG_QR, true);

    impl_->rrsets_[SECTION_ANSWER].clear();
    impl_->counts_[SECTION_ANSWER] = 0;
    impl_->rrsets_[SECTION_AUTHORITY].clear();
    impl_->counts_[SECTION_AUTHORITY] = 0;
    impl_->rrsets_[SECTION_ADDITIONAL].clear();
    impl_->counts_[SECTION_ADDITIONAL] = 0;
}

///
/// Template version of Section Iterator
///
template <typename T>
struct SectionIteratorImpl {
    SectionIteratorImpl(const typename vector<T>::const_iterator& it) :
        it_(it) {}
    typename vector<T>::const_iterator it_;
};

template <typename T>
SectionIterator<T>::SectionIterator(const SectionIteratorImpl<T>& impl) {
    impl_ = new SectionIteratorImpl<T>(impl.it_);
}

template <typename T>
SectionIterator<T>::~SectionIterator() {
    delete impl_;
}

template <typename T>
SectionIterator<T>::SectionIterator(const SectionIterator<T>& source) :
    impl_(new SectionIteratorImpl<T>(source.impl_->it_))
{}

template <typename T>
void
SectionIterator<T>::operator=(const SectionIterator<T>& source) {
    if (impl_ == source.impl_) {
        return;
    }
    SectionIteratorImpl<T>* newimpl =
        new SectionIteratorImpl<T>(source.impl_->it_);
    delete impl_;
    impl_ = newimpl;
}

template <typename T>
SectionIterator<T>&
SectionIterator<T>::operator++() {
    ++(impl_->it_);
    return (*this);
}

template <typename T>
SectionIterator<T>
SectionIterator<T>::operator++(int) {
    SectionIterator<T> tmp(*this);
    ++(*this);
    return (tmp);
}

template <typename T>
const T&
SectionIterator<T>::operator*() const {
    return (*(impl_->it_));
}

template <typename T>
const T*
SectionIterator<T>::operator->() const {
    return (&(operator*()));
}

template <typename T>
bool
SectionIterator<T>::operator==(const SectionIterator<T>& other) const {
    return (impl_->it_ == other.impl_->it_);
}

template <typename T>
bool
SectionIterator<T>::operator!=(const SectionIterator<T>& other) const {
    return (impl_->it_ != other.impl_->it_);
}

///
/// We need to explicitly instantiate these template classes because these
/// are public classes but defined in this implementation file.
///
template class SectionIterator<QuestionPtr>;
template class SectionIterator<RRsetPtr>;

namespace {
typedef SectionIteratorImpl<QuestionPtr> QuestionIteratorImpl;
typedef SectionIteratorImpl<RRsetPtr> RRsetIteratorImpl;
}

///
/// Question iterator
///
const QuestionIterator
Message::beginQuestion() const {
    return (QuestionIterator(QuestionIteratorImpl(impl_->questions_.begin())));
}

const QuestionIterator
Message::endQuestion() const {
    return (QuestionIterator(QuestionIteratorImpl(impl_->questions_.end())));
}

///
/// RRsets iterators
///
const SectionIterator<RRsetPtr>
Message::beginSection(const Section section) const {
    if (section >= MessageImpl::NUM_SECTIONS) {
        isc_throw(OutOfRange, "Invalid message section: " << section);
    }
    if (section == SECTION_QUESTION) {
        isc_throw(InvalidMessageSection,
                  "RRset iterator is requested for question");
    }

    return (RRsetIterator(RRsetIteratorImpl(impl_->rrsets_[section].begin())));
}

const SectionIterator<RRsetPtr>
Message::endSection(const Section section) const {
    if (section >= MessageImpl::NUM_SECTIONS) {
        isc_throw(OutOfRange, "Invalid message section: " << section);
    }
    if (section == SECTION_QUESTION) {
        isc_throw(InvalidMessageSection,
                  "RRset iterator is requested for question");
    }

    return (RRsetIterator(RRsetIteratorImpl(impl_->rrsets_[section].end())));
}

ostream&
operator<<(ostream& os, const Message& message) {
    return (os << message.toText());
}
} // end of namespace dns
} // end of namespace isc
