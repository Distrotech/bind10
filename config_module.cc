
#include "config_module.hh"
#include <string>

namespace ISC { namespace Config {

    ConfigModule::~ConfigModule()
    {
        delete config;
    }

    void ConfigModule::init()
    {
        // connect to the message daemon
        
        // receive our configuration
        get_config(name);

        // subscribe to the channel for configuration changes
    }

    Config* ConfigModule::get_config()
    {
        std::string identifier = "/module[@name=" + name + "]";
        return get_config(identifier);
    }

    Config* ConfigModule::get_config(std::string& identifier)
    {
        // request this specific configuration from
        // the configuration manager
        std::cout << "get " << identifier << std::endl;
        return NULL;
    }
}}
