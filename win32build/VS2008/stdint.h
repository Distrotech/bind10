/* C99 needed include... */

typedef char int8_t;
typedef unsigned char uint8_t;
typedef short int16_t;
typedef unsigned short uint16_t;
typedef int int32_t;
typedef unsigned int uint32_t;
typedef long long int64_t;
typedef unsigned long long uint64_t;

typedef short int_least16_t;

#ifdef _WIN64
typedef unsigned long long uintptr_t;
#else
typedef unsigned int uintptr_t;
#endif

#define UINT32_MAX 0xffffffff
