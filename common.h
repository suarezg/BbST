#ifndef COMMON_H
#define COMMON_H

#include <limits.h>
#include <vector>
#include "bits/stdc++.h"

#define RANDOM_DATA
//#define SDSL_REC_NEW

#define MAX_T_VALUE INT32_MAX
#define MAX_T_ARRAYSIZE UINT32_MAX

#define VALUE_BYTES 4
#define VALUE_AND_LOCATION_BYTES 8
#define LOCATION_OFFSET_IN_BYTES VALUE_BYTES

typedef int32_t t_value;

typedef unsigned int t_array_size;
typedef long long int t_array_size_2x;

using namespace std;

#endif /* COMMON_H */

