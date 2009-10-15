
//
// This is an example module, in which the ConfigModule is used
//

#include "config_module.hh"
#include "example_module.hh"

#include <iostream>

ISC::Config::ConfigModule::check_result
ExampleModuleConfig::check_config(ISC::Config::Config *config)
{
    return ISC::Config::ConfigModule::ok;
}

int
main(int argc, char **argv)
{
    ExampleModuleConfig module_config;
    std::cout << "Hello, moduleworld!" << std::endl;
    std::cout << module_config.get_name() << std::endl;

    // run around, do your things
    //while (1) {
        sleep(10);
    //}
}
