
#include <iostream>

#include "nsconfig.hh"

using namespace std;
using namespace ISC::Config;

bool
cc2(int a)
{
    return false;
}
bool
can_change(const string &new_value)
{
    /* example function, we may change to any string, except 'isc' */
    if (new_value.compare("isc") == 0) {
        return false;
    } else {
        return true;
    }
}

void
on_change_str_entry(const ConfigEntry *entry)
{
    /* example onchange callback for an entry */
    cout << "Entry " << entry->getName();
    cout << " changed to value " << entry->getStringValue();
    cout << endl;
}

int
main()
{
    ConfigTree *config = new ConfigTree();
    ConfigEntry *entry;
    ConfigTree *branch;
    config->setEntry("system/general/test", "asdf");
    config->setEntry("system/general/test2", true);
    config->setEntry("system/general/test3", false);
    config->setEntry("system/general/test4", 1234);
    config->setEntry("system/general/test5", "jajaja");

    config->addBranch("zones/tjeb.nl");
    try {
        branch = config->getBranch("zones/tjeb.nl");
        branch->setEntry("type", "master");
        branch->setEntry("file", "/var/named/zones/tjeb.nl");
        branch->setEntry("masters/master1/address", "192.168.8.8");
        branch->setEntry("masters/master1/key", "mykey");
        branch->setEntry("masters/master1/axfr", true);
        branch->print(cout);
    } catch (ConfigNoSuchNameError nsne) {
        cout << "Error: unknown branch" << endl;
    }
    
    cout << "full config:" << endl;
    config->printXML(cout);
    cout << "end of full config:" << endl;
    cout << "full config:" << endl;
    config->print(cout);
    cout << "end of full config:" << endl;

    try {
        entry = config->getEntry("system/general/test");
        cout << entry->to_string() << endl;

        /* let's add callbacks */
        entry->addCanChange(&can_change);
        entry->addOnChange(&on_change_str_entry);
        
        cout << "Trying to modify value to 'isc'" << endl;
        entry->setStringValue("isc");

        cout << "result: " << entry->to_string() << endl;

        cout << "Trying to modify value to 'bbb'" << endl;
        entry->setStringValue("bbb");

        cout << "result: " << entry->to_string() << endl;

        entry = config->getEntry("system/general/test2");
        cout << "result: " << entry->to_string() << endl;
        if (entry->getBoolValue()) {
            cout << "value is true" << endl;
        } else {
            cout << "value is false" << endl;
        }

        entry->setStringValue("false");
        cout << "result: " << entry->to_string() << endl;
        if (entry->getBoolValue()) {
            cout << "value is true" << endl;
        } else {
            cout << "value is false" << endl;
        }
        
        entry->setStringValue("True");
        cout << "result: " << entry->to_string() << endl;
        if (entry->getBoolValue()) {
            cout << "value is true" << endl;
        } else {
            cout << "value is false" << endl;
        }

        entry->setStringValue("fAlse");
        cout << "result: " << entry->to_string() << endl;
        if (entry->getBoolValue()) {
            cout << "value is true" << endl;
        } else {
            cout << "value is false" << endl;
        }

        entry->setStringValue("trUE");
        cout << "result: " << entry->to_string() << endl;
        if (entry->getBoolValue()) {
            cout << "value is true" << endl;
        } else {
            cout << "value is false" << endl;
        }

    } catch (ConfigNoSuchNameError nsne) {
        cout << "Error: unknown configuration option" << endl;
    }

    

    delete config;
    return 0;
}
