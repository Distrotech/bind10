
#ifndef _CONFIG_MANAGER_HH
#define _CONFIG_MANAGER_HH 1

#include <string>
#include "config_obj.hh"

namespace ISC { namespace Config {

    /* exception class */
    class ConfigManagerError : public std::exception {
    public:
        ConfigManagerError(std::string m="exception!") : msg(m) {}
        ~ConfigManagerError() throw() {}
        const char* what() const throw() { return msg.c_str(); }

    private:
        std::string msg;
    };

    class ConfigManager {
        Config *config;

    public:
        ConfigManager();
        ~ConfigManager();

        /* Load a specific configuration from the given file */
        void loadConfiguration(std::string file);
        /* Save the configuration to the given file */
        void saveConfiguration(std::string file);

        /* Initialize configuration manager, set up environment etc. */
        void initConfigManager();

        /* Run the configuration manager */
        void run();
    };

}}

#endif
