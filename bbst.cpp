#include <algorithm>
#include <iostream>
#include <numeric>
#include "bbst.h"

BbST::BbST(vector<t_value> valuesArray, vector<t_array_size> queries, t_array_size *resultLoc, int k) {
    this->valuesArray = valuesArray;
    this->queries = queries;
    this->resultLoc = resultLoc;
    this->k = k;
}

void BbST::solve() {
    getBlocksMins();
    for(int i = 0; i < queries.size(); i = i + 2) {
        if (queries[i] == queries[i+1]) {
            this->resultLoc[i/2] = queries[i];
            continue;
        }
        this->resultLoc[i/2] = getRangeMinLoc(queries[i], queries[i+1]);
    }
    cleanup();
/**/
}

void BbST::verify() {
    cout << "Solution verification..." << std::endl;
    unsigned int i;
    const int q = queries.size() / 2;
    this->verifyLoc.resize(q);
    this->verifyVal.resize(q);
    t_value *const vaPtr = &this->valuesArray[0];
    t_array_size *const qPtr = &this->queries[0];
    for(i = 0; i < q; i++) {
        t_value *const begPtr = vaPtr + qPtr[2*i];
        t_value *const endPtr = vaPtr + qPtr[2*i + 1] + 1;
        t_value *const minValPtr = std::min_element(begPtr, endPtr);
        verifyVal[i] = *minValPtr;
        verifyLoc[i] = minValPtr - vaPtr;
        if (verifyLoc[i] != resultLoc[i]) {
            cout << "Error: " << i << " query (" << queries[i * 2] << ", " << queries[i * 2 + 1] << ") - expected "
                 << verifyLoc[i] << " is " << resultLoc[i] << std::endl;
        }
    }
}

void BbST::getBlocksMins() {
    const t_array_size blocksCount = ((valuesArray.size() - 1 + k - 1)/ k);
    D = 32 - __builtin_clz(blocksCount);
    const t_array_size blocksSize = blocksCount * D;
    blocksVal2D = new t_value[blocksSize];
    blocksLoc2D = new t_array_size[blocksSize];
    t_array_size i = 0;
    t_value* ptr;
    t_value* endPtr = &valuesArray[0] + (valuesArray.size() - 1);
    for(ptr = &valuesArray[0]; ptr < endPtr - k; ptr = ptr + k) {
        auto minPtr = std::min_element(ptr, ptr + k);
        blocksVal2D[i * D] = *minPtr;
        blocksLoc2D[i++ * D] = minPtr - &valuesArray[0];
    }
    auto minPtr = std::min_element(ptr, endPtr);
    blocksVal2D[i * D] = *minPtr;
    blocksLoc2D[i * D] = minPtr - &valuesArray[0];
    for(t_array_size e = 1, step = 1; e < D; ++e, step <<= 1) {
        for (i = 0; i < blocksCount; i++) {
            t_array_size minIdx = i;
            if (i + step < blocksCount && blocksVal2D[(i + step) * D + e - 1] < blocksVal2D[i * D + e - 1]) {
                minIdx = i + step;
            }
            blocksVal2D[i * D + e] = blocksVal2D[minIdx * D + e - 1];
            blocksLoc2D[i * D + e] = blocksLoc2D[minIdx * D + e - 1];
        }
    }
}

t_array_size BbST::getRangeMinLoc(const t_array_size &begIdx, const t_array_size &endIdx) {
    t_array_size result = -1;
    const t_array_size begCompIdx = begIdx / k;
    const t_array_size endCompIdx = endIdx / k;
    if (endCompIdx - begCompIdx <= 1) {
        return scanMinIdx(begIdx, endIdx);
    }

    t_array_size kBlockCount = endCompIdx - begCompIdx - 1;
    t_array_size e = 31 - __builtin_clz(kBlockCount);
    t_array_size step = 1 << e;
    t_value minVal = blocksVal2D[(begCompIdx + 1) * D + e];
    result = blocksLoc2D[(begCompIdx + 1) * D + e];
    t_array_size endShiftCompIdx = endCompIdx - step;
    if (endShiftCompIdx != begCompIdx + 1) {
        t_value temp = blocksVal2D[(endShiftCompIdx) * D + e];
        if (temp < minVal) {
            minVal = temp;
            result = blocksLoc2D[(endShiftCompIdx) * D + e];
        }
    }

    if (blocksVal2D[begCompIdx * D + 0] <= minVal && begIdx != (begCompIdx + 1) * k) {
        t_array_size minIdx = scanMinIdx(begIdx, (begCompIdx + 1) * k);
        if (valuesArray[minIdx] <= minVal) {
            minVal = valuesArray[minIdx];
            result = minIdx;
        }
    }
    if (blocksVal2D[endCompIdx * D + 0] < minVal && endCompIdx * k != endIdx) {
       t_array_size minIdx = scanMinIdx(endCompIdx * k, endIdx);
        if (valuesArray[minIdx] < minVal) {
//            minVal = valuesArray[minIdx];
            result = minIdx;
        }
    }
    return result;
}

t_array_size BbST::scanMinIdx(const t_array_size &begIdx, const t_array_size &endIdx) {
    t_array_size minValIdx = begIdx;
    for(t_array_size i = begIdx + 1; i < endIdx; i++) {
        if (valuesArray[i] < valuesArray[minValIdx]) {
            minValIdx = i;
        }
    }
    return minValIdx;
}

void BbST::cleanup() {
    delete[] this->blocksLoc2D;
    delete[] this->blocksVal2D;
}