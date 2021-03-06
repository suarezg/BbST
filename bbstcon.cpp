#include <algorithm>
#include <iostream>
#include <numeric>
#include "bbstcon.h"

#include <omp.h>
#include <parallel/algorithm>
#include "utils/kxsort.h"
#include "utils/parallel_stable_sort.h"

BbSTcon::BbSTcon(sortingAlg_enum sortingAlg, int kExp) {
    this->sortingAlg = sortingAlg;
    this->kExp = kExp;
    this->k = 1 << kExp;
}

void BbSTcon::rmqBatch(const t_value* valuesArray, const t_array_size n, const vector<t_array_size> &queries, t_array_size *resultLoc) {
    this->valuesArray = valuesArray;
    this->n = n;
    this->q = queries.size();
    getUniqueBoundsSorted(queries);
    getContractedMins();
    getBlocksMins();
    #pragma omp parallel for
    for(int i = 0; i < queries.size(); i = i + 2) {
        if (queries[i] == queries[i+1]) {
            resultLoc[i/2] = queries[i];
            continue;
        }
        resultLoc[i/2] = getContractedRMQ(queries2ContractedIdx[i], queries2ContractedIdx[i + 1]);
    }
    cleanup();
/**/
}

struct RadixTraitsBounds {
    static const int nBytes = sizeof(t_array_size);
    int kth_byte(const t_array_size_2x &x, int k) {
        return *((t_array_size*) &x) >> (k * 8) & 0xFF;
    }
    bool compare(const t_array_size_2x &a, const t_array_size_2x &b) {
        return *((t_array_size*) &a) < *((t_array_size*) &b);
    }
};

void BbSTcon::getUniqueBoundsSorted(const vector<t_array_size> &queries) {
    bounds = new t_array_size_2x[queries.size()];
    for (t_array_size i = 0; i < queries.size(); i++) {
        *((t_array_size *) (&bounds[i])) = queries[i];
        *(((t_array_size *) (&bounds[i])) + 1) = i;
    }
    switch (sortingAlg) {
        case stdsort : std::sort(bounds, bounds + queries.size(), [](const t_array_size_2x& a, const t_array_size_2x& b) -> bool { return *((t_array_size*) &a) < *((t_array_size*) &b); });
            break;
        case csort : qsort((void*) &bounds[0], queries.size(), sizeof(unsigned long long int), [](const void* a, const void* b) -> int { return *((int32_t*) a) - *((t_array_size*) b); });
            break;
        case kxradixsort : kx::radix_sort(bounds, bounds + queries.size(), RadixTraitsBounds());
            break;
        case pssparallelsort : pss::parallel_stable_sort(bounds, bounds + queries.size(), [](const t_array_size_2x& a, const t_array_size_2x& b) -> bool { return *((t_array_size*) &a) < *((t_array_size*) &b); });
            break;
        case ompparallelsort  : __gnu_parallel::sort(bounds, bounds + queries.size(), [](const t_array_size_2x& a, const t_array_size_2x& b) -> bool { return *((t_array_size*) &a) < *((t_array_size*) &b); }, __gnu_parallel::multiway_mergesort_tag());
            break;
    }
    queries2ContractedIdx = new t_array_size[queries.size()];
    for(t_array_size i = 0; i < queries.size(); i++) {
        queries2ContractedIdx[*(((t_array_size*) &bounds[i]) + 1)] = i;
    }
}

void BbSTcon::getContractedMins() {
    contractedVal = new t_value[q - 1];
    contractedLoc = new t_array_size[q - 1];

    #pragma omp parallel for
    for(int i = 1;  i < q; i++) {
        t_array_size minIdx = *((t_array_size*) (bounds + i - 1));
        const t_array_size endIdx = *((t_array_size*) (bounds + i));

        const int *const minPtr = std::min_element(&valuesArray[minIdx], &valuesArray[endIdx + 1]);
        contractedVal[i-1] = *minPtr;
        contractedLoc[i-1] = minPtr - &valuesArray[0];
    }
}

void BbSTcon::getBlocksMins() {
    this->blocksCount = (q - 1 + k - 1) >> kExp;
    D = 32 - __builtin_clz(blocksCount);
    const t_array_size blocksSize = blocksCount * D;
    blocksVal2D = new t_value[blocksSize];
    blocksLoc2D = new t_array_size[blocksSize];
    #pragma omp parallel for
    for (t_array_size i = 0; i < blocksCount - 1; i++) {
        auto minPtr = std::min_element(&contractedVal[i << kExp], &contractedVal[(i + 1) << kExp]);
        blocksVal2D[i] = *minPtr;
        blocksLoc2D[i] = contractedLoc[minPtr - contractedVal];
    }
    auto minPtr = std::min_element(&contractedVal[(blocksCount - 1) << kExp], &contractedVal[q - 1]);
    blocksVal2D[blocksCount - 1] = *minPtr;
    blocksLoc2D[blocksCount - 1] = contractedLoc[minPtr - contractedVal];
    for(t_array_size e = 1, step = 1; e < D; ++e, step <<= 1) {
        for (t_array_size i = 0; i < blocksCount; i++) {
            t_array_size minIdx = i;
            const t_array_size e0offset = (e - 1) * blocksCount;
            if (i + step < blocksCount && blocksVal2D[i + step + e0offset] < blocksVal2D[i + e0offset]) {
                minIdx = i + step;
            }
            const t_array_size e1offset = e * blocksCount;
            blocksVal2D[i + e1offset] = blocksVal2D[minIdx + e0offset];
            blocksLoc2D[i + e1offset] = blocksLoc2D[minIdx + e0offset];
        }
    }
}

t_array_size BbSTcon::getContractedRMQ(const t_array_size &begContIdx, const t_array_size &endContIdx) {
    t_array_size result = -1;
    const t_array_size begCompIdx = begContIdx >> kExp;
    const t_array_size endCompIdx = endContIdx >> kExp;
    const t_array_size firstBlockMinLoc = blocksLoc2D[begCompIdx];
    if (endCompIdx == begCompIdx) {
        if (contractedLoc[begContIdx] <= firstBlockMinLoc && firstBlockMinLoc < contractedLoc[endContIdx])
            return firstBlockMinLoc;
        return contractedLoc[scanContractedMinIdx(begContIdx, endContIdx)];
    }
    t_value minVal = MAX_T_VALUE;
    if (endCompIdx - begCompIdx > 1) {
        t_array_size kBlockCount = endCompIdx - begCompIdx - 1;
        t_array_size e = 31 - __builtin_clz(kBlockCount);
        t_array_size step = 1 << e;
        minVal = blocksVal2D[(begCompIdx + 1) + e * blocksCount];
        result = blocksLoc2D[(begCompIdx + 1) + e * blocksCount];
        t_array_size endShiftCompIdx = endCompIdx - step;
        if (endShiftCompIdx != begCompIdx + 1) {
            t_value temp = blocksVal2D[(endShiftCompIdx) + e * blocksCount];
            if (temp < minVal) {
                minVal = temp;
                result = blocksLoc2D[(endShiftCompIdx) + e * blocksCount];
            }
        }
    }
    if (blocksVal2D[begCompIdx] <= minVal && begContIdx != (begCompIdx + 1) << kExp) {
        if (firstBlockMinLoc >= contractedLoc[begContIdx]) {
            minVal = blocksVal2D[begCompIdx];
            result = firstBlockMinLoc;
        } else {
            t_array_size contMinIdx = scanContractedMinIdx(begContIdx, (begCompIdx + 1) << kExp);
            if (contractedVal[contMinIdx] <= minVal) {
                minVal = contractedVal[contMinIdx];
                result = contractedLoc[contMinIdx];
            }
        }
    }
    if (blocksVal2D[endCompIdx] < minVal && endCompIdx << kExp != endContIdx) {
        t_array_size lastBlockMinLoc = blocksLoc2D[endCompIdx];
        if (lastBlockMinLoc < contractedLoc[endContIdx]) {
            result = lastBlockMinLoc;
        } else {
            t_array_size contMinIdx = scanContractedMinIdx(endCompIdx << kExp, endContIdx);
            if (contractedVal[contMinIdx] < minVal) {
                result = contractedLoc[contMinIdx];
            }
        }
    }
    return result;
}

t_array_size BbSTcon::scanContractedMinIdx(const t_array_size &begContIdx, const t_array_size &endContIdx) {
    t_array_size minValIdx = begContIdx;
    for(t_array_size i = begContIdx + 1; i < endContIdx; i++) {
        if (contractedVal[i] < contractedVal[minValIdx]) {
            minValIdx = i;
        }
    }
    return minValIdx;
}

void BbSTcon::cleanup() {
    delete[] this->bounds;
    delete[] this->blocksLoc2D;
    delete[] this->blocksVal2D;
    delete[] this->contractedLoc;
    delete[] this->contractedVal;
    delete[] this->queries2ContractedIdx;
}

size_t BbSTcon::memUsageInBytes() {
    const size_t boundsBytes = q * sizeof(t_array_size_2x);
    const size_t queries2ContractedIdxBytes = q * sizeof(t_array_size);
    const size_t contractedBytes = (q - 1) * (sizeof(t_value) + sizeof(t_array_size));
    const t_array_size blocksCount = ((q - 1 + k - 1)/ k);
    D = 32 - __builtin_clz(blocksCount);
    const t_array_size blocksSize = blocksCount * D;
    const size_t blocksBytes = blocksSize * (sizeof(t_value) + sizeof(t_array_size));
    const size_t bytes = boundsBytes + queries2ContractedIdxBytes + contractedBytes + blocksBytes;
    return bytes;
}
