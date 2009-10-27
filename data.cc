
#include "data.h"

#include <iostream>
#include <sstream>
#include <boost/foreach.hpp>


using namespace std;
using namespace ISC::Data;

//
// factory functions
//
ElementPtr
Element::create(const int i)
{
    try {
        return ElementPtr(new IntElement(i));
    } catch (std::bad_alloc) {
        return ElementPtr();
    }
}

ElementPtr
Element::create(const double d)
{
    try {
        return ElementPtr(new DoubleElement(d));
    } catch (std::bad_alloc) {
        return ElementPtr();
    }
}

ElementPtr
Element::create(const std::string& s)
{
    try {
        return ElementPtr(new StringElement(s));
    } catch (std::bad_alloc) {
        return ElementPtr();
    }
}

ElementPtr
Element::create(const std::vector<ElementPtr>& v)
{
    try {
        return ElementPtr(new ListElement(v));
    } catch (std::bad_alloc) {
        return ElementPtr();
    }
}

ElementPtr
Element::create(const std::map<std::string, ElementPtr>& m)
{
    try {
        return ElementPtr(new MapElement(m));
    } catch (std::bad_alloc) {
        return ElementPtr();
    }
}


//
// helper functions for create_from_string factory
// these should probably also be moved to member functions
//

static bool
char_in(char c, const char *chars)
{
    for (size_t i = 0; i < strlen(chars); i++) {
        if (chars[i] == c) {
            return true;
        }
    }
    return false;
}

static void
skip_chars(std::stringstream &in, const char *chars)
{
    char c = in.peek();
    while (char_in(c, chars) && c != EOF) {
        in.get();
        c = in.peek();
    }
}

// skip on the input stream to one of the characters in chars
// if another character is found this function returns false
// unles that character is specified in the optional may_skip
//
// the character found is left on the stream
static bool
skip_to(std::stringstream &in, const char *chars, const char *may_skip="")
{
    char c = in.get();
    while (c != EOF) {
        if (char_in(c, may_skip)) {
            c = in.get();
        } else if (char_in(c, chars)) {
            while(char_in(in.peek(), may_skip)) {
                in.get();
            }
            in.putback(c);
            return true;
        } else {
            // TODO: provide feeback mechanism?
            cout << "error, '" << c << "' read; one of \"" << chars << "\" expected" << endl;
            in.putback(c);
            return false;
        }
    }
    // TODO: provide feeback mechanism?
    cout << "error, EOF read; one of \"" << chars << "\" expected" << endl;
            in.putback(c);
    return false;
}

static std::string
str_from_stringstream(std::stringstream &in)
{
    char c = 0;
    std::stringstream ss;
    c = in.get();
    if (c == '"') {
        c = in.get();
    } else {
        return "badstring";
    }
    while (c != EOF && c != '"') {
        ss << c;
        if (c == '\\' && in.peek() == '"') {
            ss << in.get();
        }
        c = in.get();
    }
    return ss.str();
}

static ElementPtr
from_stringstream_int_or_double(std::stringstream &in)
{
    int i;
    in >> i;
    if (in.peek() == '.') {
        double d;
        in >> d;
        d += i;
        return Element::create(d);
    } else {
        return Element::create(i);
    }
}

static ElementPtr
from_stringstream_string(std::stringstream &in)
{
    return Element::create(str_from_stringstream(in));
}

static ElementPtr
from_stringstream_list(std::stringstream &in)
{
    char c = 0;
    std::vector<ElementPtr> v;
    ElementPtr cur_list_element;

    skip_chars(in, " \t\n");
    while (c != EOF && c != ']') {
        cur_list_element = Element::create_from_string(in);
        v.push_back(cur_list_element);
        if (!skip_to(in, ",]", " \t\n")) {
            return ElementPtr();
        }
        c = in.get();
    }
    return Element::create(v);
}

static ElementPtr
from_stringstream_map(std::stringstream &in)
{
    char c = 0;
    std::map<std::string, ElementPtr> m;
    std::pair<std::string, ElementPtr> p;
    std::string cur_map_key;
    ElementPtr cur_map_element;
    skip_chars(in, " \t\n");
    while (c != EOF && c != '}') {
        p.first = str_from_stringstream(in);
        if (!skip_to(in, ":", " \t\n")) {
            return ElementPtr();
        } else {
            // skip the :
            in.get();
        }
        p.second = Element::create_from_string(in);
        if (!p.second) { return ElementPtr(); };
        m.insert(p);
        skip_to(in, ",}", " \t\n");
        c = in.get();
    }
    return Element::create(m);
}

ElementPtr
Element::create_from_string(std::stringstream &in)
{
    char c = 0;
    ElementPtr element;
    bool el_read = false;
    skip_chars(in, " \n\t");
    while (c != EOF && !el_read) {
        c = in.get();
        switch(c) {
            case '1':
            case '2':
            case '3':
            case '4':
            case '5':
            case '6':
            case '7':
            case '8':
            case '9':
            case '0':
                in.putback(c);
                element = from_stringstream_int_or_double(in);
                el_read = true;
                break;
            case '[':
                element = from_stringstream_list(in);
                el_read = true;
                break;
            case '"':
                in.putback('"');
                element = from_stringstream_string(in);
                el_read = true;
                break;
            case '{':
                element = from_stringstream_map(in);
                el_read = true;
                break;
            default:
                // TODO this might not be a fatal error
                // provide feedback mechanism?
                cout << "error: unexpected '" << c << "'" << endl;
                return ElementPtr();
                break;
        }
    }
    if (el_read) {
        return element;
    } else {
        // throw exception?
        return ElementPtr();
    }
}

//
// a general to_str() function
//
std::string
IntElement::str()
{
    std::stringstream ss;
    ss << int_value();
    return ss.str();
}

std::string
DoubleElement::str()
{
    std::stringstream ss;
    ss << double_value();
    return ss.str();
}

std::string
StringElement::str()
{
    std::stringstream ss;
    ss << "\"";
    ss << string_value();
    ss << "\"";
    return ss.str();
}

std::string
ListElement::str()
{
    std::stringstream ss;
    std::vector<ElementPtr> v;
    ss << "[ ";
    v = list_value();
    for (std::vector<ElementPtr>::iterator it = v.begin(); it != v.end(); ++it) {
        if (it != v.begin()) {
            ss << ", ";
        }
        ss << (*it)->str();
    }
    ss << " ]";
    return ss.str();
}

std::string
MapElement::str()
{
    std::stringstream ss;
    std::map<std::string, ElementPtr> m;
    ss << "{";
    m = map_value();
    for (std::map<std::string, ElementPtr>::iterator it = m.begin(); it != m.end(); ++it) {
        if (it != m.begin()) {
            ss << ", ";
        }
        ss << "\"" << (*it).first << "\": ";
        ss << (*it).second->str();
    }
    ss << "}";
    return ss.str();
}

//
// helpers for str_xml() functions
//

// prefix with 'prefix' number of spaces
static void
pre(std::ostream &out, size_t prefix)
{
    for (size_t i = 0; i < prefix; i++) {
        out << " ";
    }
}

std::string
IntElement::str_xml(size_t prefix)
{
    std::stringstream ss;
    pre(ss, prefix);
    ss << str();
    return ss.str();
}

std::string
DoubleElement::str_xml(size_t prefix)
{
    std::stringstream ss;
    pre(ss, prefix);
    ss << str();
    return ss.str();
}

std::string
StringElement::str_xml(size_t prefix)
{
    std::stringstream ss;
    pre(ss, prefix);
    ss << string_value();
    return ss.str();
}

std::string
ListElement::str_xml(size_t prefix)
{
    std::stringstream ss;
    std::vector<ElementPtr> v;
    pre(ss, prefix);
    ss << "<list>" << endl;;
    v = list_value();
    for (std::vector<ElementPtr>::iterator it = v.begin(); it != v.end(); ++it) {
        pre(ss, prefix + 4);
        ss << "<listitem>" << endl;
        ss << (*it)->str_xml(prefix + 8) << endl;
        pre(ss, prefix + 4);
        ss << "</listitem>" << endl;
    }
    pre(ss, prefix);
    ss << "</list>";
    return ss.str();
}

std::string
MapElement::str_xml(size_t prefix)
{
    std::stringstream ss;
    std::map<std::string, ElementPtr> m;
    m = map_value();
    pre(ss, prefix);
    ss << "<map>" << endl;
    for (std::map<std::string, ElementPtr>::iterator it = m.begin(); it != m.end(); ++it) {
        pre(ss, prefix + 4);
        ss << "<mapitem name=\"" << (*it).first << "\">" << endl;
        pre(ss, prefix);
        ss << (*it).second->str_xml(prefix+8) << endl;
        pre(ss, prefix + 4);
        ss << "</mapitem>" << endl;
    }
    pre(ss, prefix);
    ss << "</map>";
    return ss.str();
}

// currently throws when one of the types in the path (except the one
// we're looking for) is not a MapElement
// returns 0 if it could simply not be found
// should that also be an exception?
ElementPtr
MapElement::find(const std::string& id)
{
    if (get_type() != map) {
        throw TypeError();
    }
    size_t sep = id.find('/');
    if (sep == std::string::npos) {
        return get(id);
    } else {
        ElementPtr ce = get(id.substr(0, sep));
        if (ce) {
            return ce->find(id.substr(sep+1));
        } else {
            return ElementPtr();
        }
    }
}

bool
MapElement::find(const std::string& id, ElementPtr& t) {
    ElementPtr p;
    try {
        p = find(id);
        if (p) {
            t = p;
            return true;
        }
    } catch (TypeError e) {
        // ignore
    }
    return false;
}

int main(int argc, char **argv)
{
    
    ElementPtr ie = Element::create(12);
    cout << "ie value: " << ie->int_value() << endl;
    ElementPtr de = Element::create(12.0);
    cout << "de value: " << de->double_value() << endl;
    ElementPtr se = Element::create("hello, world");
    cout << "se value: " << se->string_value() << endl;
    std::vector<ElementPtr> v;
    v.push_back(Element::create(12));
    v.push_back(ie);
    ElementPtr ve = Element::create(v);
    cout << "Vector element:" << endl;
    cout << ve->str() << endl;
    cout << "Vector element2:" << endl;
    BOOST_FOREACH(ElementPtr e, ve->list_value()) {
        cout << "\t" << e->str() << endl;
    }
    //cout << "Vector element direct: " << ve->string_value() << endl;

    //std::string s = "[ 1, 2, 3, 4]";
    std::string s = "{ \"test\": [ 47806, 42, 12.23, 1, \"1asdf\"], \"foo\": \"bar\", \"aaa\": { \"bbb\": { \"ccc\": 1234, \"ddd\": \"blup\" } } }";
    //std::string s = "{ \"test\": 1 }";
    //std::string s = "[ 1, 2 ,3\" ]";
    std::stringstream ss;
    ss.str(s);
    ElementPtr e = Element::create_from_string(ss);
    if (e) {
        cout << "element read: " << e->str() << endl;
        //cout << "xml" << endl << e->str_xml() << endl;
    } else {
        cout << "could not read element" << endl;
        exit(0);
    }
    cout << "find aaa/bbb/ccc: " << e->find("aaa/bbb/ccc") << endl;

    ElementPtr founde;
    if (e->find("aaa/bbb", founde)) {
        cout << "found aaa/bbb: " << founde << endl;
    } else {
        cout << "aaa/bbb not found" << endl;
    }
    if (e->find("aaa/ccc", founde)) {
        cout << "found aaa/ccc: " << founde << endl;
    } else {
        cout << "aaa/ccc not found" << endl;
    }
    //cout << "part: " << e->get("test")->str() << endl;
/*
    int i;
    ie->get_value(i);
    cout << "value of ie: " << i << endl;
    std::vector<ElementPtr> v2;
    ve->set(0, Element::create(123));
    ve->get_value(v2);
    cout << "V2:" << endl;
    BOOST_FOREACH(ElementPtr e, v2) {
        cout << "\t" << e->str() << endl;
    }

    cout << "test: " << e << endl;
*/
    return 0;
}
