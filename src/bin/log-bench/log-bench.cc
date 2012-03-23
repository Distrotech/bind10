#include "log.h"
#include <log/logger_support.h>
#include <bench/benchmark.h>
#include <iostream>

using namespace isc::bench;
using namespace std;

const size_t iterations(1000000);

class NoPrint {
public:
    size_t run() const {
        LOG_DEBUG(logger, 50, TEST_MESSAGE);
        return (1);
    }
};

class Print {
public:
    size_t run() const {
        LOG_ERROR(logger, TEST_MESSAGE);
        return (1);
    }
};

int main(int, const char*[]) {
    isc::log::initLogger("test", isc::log::INFO, 0, NULL);
    BenchMark<NoPrint>(iterations * 100, NoPrint());
    BenchMark<Print>(iterations, Print());
    return 0;
}
