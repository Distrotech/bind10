#include "data.h"
#include "session.h"

#include <cstdio>
#include <iostream>
#include <sstream>

#include <boost/foreach.hpp>
#include <boost/algorithm/string.hpp>

using namespace std;
using namespace ISC::Data;

static void
hexdump(std::string s)
{
    const unsigned char *c = (const unsigned char *)s.c_str();
    int len = s.length();

    int count = 0;

    printf("%4d: ", 0);
    while (len) {
        printf("%02x %c ", (*c & 0xff),
               (isprint((*c & 0xff)) ? (*c & 0xff) : '.'));
        count++;
        c++;
        len--;
        if (count % 16 == 0)
            printf("\n%4d: ", count);
        else if (count % 8 == 0)
            printf(" | ");
    }
    printf("\n");
}

int main(int argc, char **argv)
{
    
    ElementPtr ie = Element::create(12);
    cout << "ie value: " << ie->int_value() << endl;
    ElementPtr de = Element::create(12.0);
    cout << "de value: " << de->double_value() << endl;
    ElementPtr se = Element::create(std::string("hello, world").c_str());
    cout << "se type " << se->get_type() << endl;
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
    std::string s = "{ \"test\": [ 47806, True, 42, 12.23, 1, \"1asdf\"], \"foo\": \"bar\", \"aaa\": { \"bbb\": { \"ccc\": 1234, \"ddd\": \"blup\" } } }";
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
    ElementPtr be = Element::create(true);
    cout << "boolelement: " << be << endl;

    std::string s_skan = "{ \"test\": \"testval\", \"xxx\": \"that\", \"int\": 123456, \"list\": [ 1, 2, 3 ], \"map\": { \"one\": \"ONE\" }, \"double\": 5.4, \"boolean\": True }";
    std::stringstream in_ss_skan;
    in_ss_skan.str(s_skan);
    ElementPtr e_skan = Element::create_from_string(in_ss_skan);
    std::stringstream ss_skan;
    ss_skan << e_skan->to_wire();
    hexdump(std::string(ss_skan.str()));

    ElementPtr decoded = Element::from_wire(ss_skan, ss_skan.str().length());
    cout << decoded << endl;


    //
    // Test the session stuff
    //

    ISC::CC::Session session;
    session.establish();

    session.subscribe("test");

    int counter = 0;

    ElementPtr env = Element::create(std::map<std::string, ElementPtr>());
    env->set("counter", Element::create(counter++));

    ElementPtr routing, data;

    while (true) {
        env->set("counter", Element::create(counter++));

        session.group_sendmsg(env, "test", "foo");

        session.group_recvmsg(routing, data, false);
        //cout << "routing: " << routing->get("from")->string_value() << endl;
        //cout << "data: " << data << endl;

        if ((counter & 0xffff) == 0)
            cout << "Send/received " << counter << " messages" << endl;
    }

    return 0;
}
