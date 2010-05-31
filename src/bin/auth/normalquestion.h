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

#ifndef __NORMALQUESTION_H
#define __NORMALQUESTION_H 1

#include <dns/question.h>

namespace isc {
namespace dns {
class InputBuffer;
class OutputBuffer;
class MessageRenderer;
class NormalQuestionImpl;

class NormalQuestion : public AbstractQuestion {
public:
    NormalQuestion();
    ~NormalQuestion();
private:
    // make this class non copyable
    NormalQuestion(const NormalQuestion& source);
    NormalQuestion& operator=(const NormalQuestion& source);
public:
    virtual const Name& getName() const;
    virtual const RRType& getType() const;
    virtual const RRClass& getClass() const;
    virtual std::string toText() const;
    virtual unsigned int toWire(MessageRenderer& renderer) const;
    virtual unsigned int toWire(OutputBuffer& buffer) const;
    void setLabelSequenceToQName(LabelSequence& sequence) const;
    void fromWire(InputBuffer& buffer);
    void clear();
private:
    NormalQuestionImpl* impl_;
};
}
}

#endif // __NORMALQUESTION_H

// Local Variables:
// mode: c++
// End:
