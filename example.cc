
#include <iostream>
#include <fstream>
#include <boost/algorithm/string.hpp>
#include <boost/algorithm/string/trim.hpp>

#include "nsconfig.hh"
#include "util.hh"

using namespace std;

bool
no_change(const string &value)
{
    return false;
}

void
alert_change(const ISC::Config::ConfigEntry *entry) {
    cout << "Entry " << entry->getName();
    cout << " changed to " << entry->getStringValue();
    cout << endl;
}

ISC::Config::ConfigTree *
readConfig(std::string filename)
{
    ifstream file;
    ISC::Config::ConfigEntry *entry;
    ISC::Config::ConfigTree *config;
    string line;
    
    file.open(filename.c_str());
    if (!file) {
        return NULL;
    }
    config = new ISC::Config::ConfigTree();
    while (getline(file, line)) {
        /* ignore empty lines and comments */
        boost::trim(line);
        if (line.length() != 0 && line[0] != '\n' && line[0] != '#') {
            entry = new ISC::Config::ConfigEntry(line);
            config->setEntry(entry->getName(), entry);
        }
    }
    file.close();
    return config;
}

void
writeConfig(ISC::Config::ConfigTree *config, std::string filename)
{
    ofstream file;
    file.open(filename.c_str(), fstream::out);
    if (!file) {
        cout << "Could not open " << filename << " for writing" << endl;
    }
    config->print(file);
    file.close();
}

int
main(int argc, char **argv)
{
    ISC::Config::ConfigTree *config;
    ISC::Config::ConfigEntry *entry;
    
    config = readConfig("example.conf");
    if (!config) {
        config = readConfig("example.defaults");
    }
    if (!config) {
        config = new ISC::Config::ConfigTree();
    }
    
    string cmd;
    vector<string> cmdp;
    cout << "> ";
    while (getline(cin, cmd) && !boost::iequals(cmd, "quit")) {
        cmdp.clear();
        Tokenize(cmd, cmdp, " ");
        if (boost::iequals(cmdp[0], "show")) {
            if (cmdp.size() > 1) {
                try {
                    config->getBranch(cmdp[1])->print(cout);
                } catch (ISC::Config::ConfigNoSuchNameError nsne) {
                    try {
                        cout << config->getEntry(cmdp[1])->to_string();
                        cout << endl;
                    } catch (ISC::Config::ConfigNoSuchNameError nsne) {
                        cout << "No such branch or entry: ";
                        cout << cmdp[1] << endl;
                    }
                }
            } else {
                config->print(cout);
            }
        } else if (boost::iequals(cmdp[0], "set")) {
            if (cmdp.size() < 3) {
                cout << "usage: set <name> <new value>" << endl;
            } else {
                try {
                    entry = config->getEntry(cmdp[1]);
                    if (!entry->setStringValue(cmdp[2])) {
                        cout << "Could not change value to ";
                        cout << cmdp[2];
                        cout << " (locked?)";
                        cout << endl;
                    }
                } catch (ISC::Config::ConfigNoSuchNameError nsne) {
                    cout << "No such entry: " << cmdp[1] << endl;
                } catch (ISC::Config::ConfigTypeError te) {
                    cout << "Bad value for entry" << endl;
                }
            }
        } else if (boost::iequals(cmdp[0], "lock")) {
            if (cmdp.size() < 2) {
                cout << "usage: lock <name>";
            } else {
                try {
                    entry = config->getEntry(cmdp[1]);
                    entry->addCanChange(&no_change);
                } catch (ISC::Config::ConfigNoSuchNameError nsne) {
                    cout << "No such entry: " << cmdp[1] << endl;
                } catch (ISC::Config::ConfigTypeError te) {
                    cout << "Bad value for entry" << endl;
                }
            }
        } else if (boost::iequals(cmdp[0], "alert")) {
            if (cmdp.size() < 2) {
                cout << "usage: alert <name>";
            } else {
                try {
                    entry = config->getEntry(cmdp[1]);
                    entry->addOnChange(&alert_change);
                } catch (ISC::Config::ConfigNoSuchNameError nsne) {
                    cout << "No such entry: " << cmdp[1] << endl;
                } catch (ISC::Config::ConfigTypeError te) {
                    cout << "Bad value for entry" << endl;
                }
            }
        } else if (boost::iequals(cmdp[0], "help")) {
            cout << "Commands" << endl;
            cout << "show [branch|entry]" << endl;
            cout << "\tprints the entry or branch" << endl;
            cout << "set <entry> <value>" << endl;
            cout << "\tchanges the value of entry to value" << endl;
            cout << "alert <entry>" << endl;
            cout << "\tprints a message when the entry is changed" << endl;
            cout << "lock <entry>" << endl;
            cout << "\tlocks the entry for this session" << endl;
        } else {
            cout << "Unknown command, try 'help'" << endl;
        }
        cout << "> ";
    }

    writeConfig(config, "example.conf");
    return 0;
}
