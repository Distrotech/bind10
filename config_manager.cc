/**
 * This is the main configuration manager
 *
 * It stores the configuration in <config backend> (xml file? db?)
 *
 * When a module starts, it requests the relevant part(s) for its own
 * configuration from the manager
 *
 * When an admin makes a change, the manager pushes, this change to
 * the client.
 */
#include "config_manager.hh"
#include "config_obj.hh"

#include <iostream>

using namespace std;
using namespace ISC::Config;

ConfigManager::ConfigManager()
{
    config = NULL;
    ISC::Config::config_init();
}

ConfigManager::~ConfigManager()
{
    delete config;
    ISC::Config::config_cleanup();
}

void
ConfigManager::load_configuration(std::string filename)
{
    try {
        config = new Config(filename);
    } catch (ConfigError& e) {
        delete config;
        std::string err;
        err = "Error reading " + filename + ": " + e.what();
        throw ConfigManagerError(err);
    }
}

void
ConfigManager::save_configuration(std::string filename)
{
    try {
        config->write_file(filename);
    } catch (ConfigError& e) {
        std::string err;
        err = "Error writing " + filename + ": " + e.what();
        throw ConfigManagerError(err);
    }
}

void
ConfigManager::init_config_manager()
{
    /* set up communication channel */
    return;
}

void
ConfigManager::run()
{
#if 0
    while (1) {
        /* listen for commands, on read-type command
         * (GET identifier?) do something like
         * Config *subconfig = config->get_config_part(identifier);
         * subconfig->write_stream(outstream);
         * delete subconfig;
         *
         * on set do something like
         * PUT identifier
         * <xmlblob>
         * 
         * Config *subconfig = config->read_stream(intstream);
         * config->check_subconfig(identifier, subconfig);
         * config->set_config_part(identifier, subconfig);
         * delete subconfig;
         * write_ok_to_outstream;
         * notify_registered_clients
         */
    }
#endif
    /* just some random actions the manager can do on the config */
    if (config) {
        std::string a = config->get_value("/module[@name=authoritative]@name");
        cout << "module name: " << a << endl;
        config->set_value("/module@name", "recursive");
        std::string b = config->get_value("/module@name");
        cout << "module name now: " << b << endl;
        cout << endl;

        cout << "Trying to get zone theo.com from the module named authoritative" << endl;
        try {
            Config *config_part;
            config_part = config->get_config_part("/module[@name=authoritative]/zones/zone[@name=theo.com]");
            //config_part->set_value("@name", "asdf.com");
            cout << "Selected zone: " << endl;
            config_part->write_stream(std::cout);
        } catch (ConfigError& ce) {
            cout << "Caught ConfigError: " << ce.what() << endl;
        }

        cout << endl;
        cout << "Oh yes, we have just changed the name of the module to recursive..." << endl;
        cout << endl;
        
        cout << "Trying to get zone theo.com from the module named recursive" << endl;
        try {
            Config *config_part;
            config_part = config->get_config_part("/module[@name=recursive]/zones/zone[@name=theo.com]");
            cout << "Selected zone configuration: " << endl;
            config_part->write_stream(std::cout);

            cout << "Changing file to /tmp/myfile" << endl;
            config_part->set_value("/file", "/tmp/myfile");

            cout << "Add a new element, 'auto-notify'" << endl;
            config_part->add_child("auto-notify");
            cout << "Set value of 'auto-notify' to false" << endl;
            config_part->set_value("/auto-notify", "false");
            
            cout << "Selected zone configuration now: " << endl;
            config_part->write_stream(std::cout);

            cout << endl;

            cout << "Putting the updated configuration part back in the main config." << endl;
            /* make sure this is the exact same as the getconfig part above, no checking is done atm! */
            config->set_config_part("/module[@name=recursive]/zones/zone[@name=theo.com]", config_part);

            delete config_part;
        } catch (ConfigError& ce) {
            cout << "Caught ConfigError: " << ce.what() << endl;
        }

    }
}



int
main(int argc, char **argv)
{
    ConfigManager cm;
    cm.load_configuration("./myconf.conf");
    cm.init_config_manager();
    cm.run();

    cout << "Storing the main config" << endl;
    cm.save_configuration("./myconf_new.conf");
    cout << "New configuration stored in ./myconf_new.conf" << endl;

}
