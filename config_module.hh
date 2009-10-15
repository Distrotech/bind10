//
// This is the module side of the configuration
// Modules need to inherit from this in their config
// code
//

#ifndef _CONFIG_MODULE_HH
#define _CONFIG_MODULE_HH 1

#include "config_obj.hh"

#include <string>

namespace ISC { namespace Config {

    class ConfigModule {
        std::string name;
        ISC::Config::Config *config;

    public:
        // result values the check_config can return
        enum check_result { ok, error, not_live };
        
        // Create a configmodule with the given name, this name
        // is used by the config_manager to separate module-specific
        // configurations.
        explicit ConfigModule(const std::string& n) : name(n) { init(); };

        // Base class, so make destructor virtual
        virtual ~ConfigModule();

        std::string get_name() { return name; }

        // The check_config method the module needs to define can have
        // the following return values:
        // ok: all settings in the given config part are ok
        // error: there is an error in the given config part (todo: how to
        // feedback what exactly?)
        // not_live: the config values are ok, but cannot be changed while
        //           running
        virtual check_result check_config(ISC::Config::Config *config_part) = 0;

    private:
        void init();
        Config* get_config();
        Config* get_config(std::string& identifier);
    };
}
}

#endif // _MODULE_CONFIG_HH
