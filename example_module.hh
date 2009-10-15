
#ifndef _EXAMPLE_MODULE_HH
#define _EXAMPLE_MODULE_HH

class ExampleModuleConfig : public ISC::Config::ConfigModule {
public:
    ExampleModuleConfig() : ConfigModule("example") {};
    check_result check_config(ISC::Config::Config *config);
};

#endif // _EXAMPLE_MODULE_HH
