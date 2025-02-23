// testCorrectness.cpp
#include <iostream>
#include <vector>
#include <thread>
#include <cassert>
#include "../include/memory.h"

using namespace mP;

void TestBasicAllocation() {
    // 移除 initMemoryPool()
    const int TEST_SIZE = 32;
    void* ptr1 = HashBucket::useMemory(TEST_SIZE);
    assert(ptr1 != nullptr);
    HashBucket::freeMemory(ptr1, TEST_SIZE);

    std::vector<void*> ptrs;
    for (int i = 0; i < 1000; ++i) {
        void* p = HashBucket::useMemory(TEST_SIZE);
        assert(p != nullptr);
        ptrs.push_back(p);
    }
    for (void* p : ptrs) {
        HashBucket::freeMemory(p, TEST_SIZE);
    }
    std::cout << "TestBasicAllocation Passed!" << std::endl;
}

void TestBoundaryConditions() {
    void* hugePtr = HashBucket::useMemory(1024);
    assert(hugePtr != nullptr);
    HashBucket::freeMemory(hugePtr, 1024);

    void* nullPtr = HashBucket::useMemory(0);
    assert(nullPtr == nullptr);
    std::cout << "TestBoundaryConditions Passed!" << std::endl;
}

void ConcurrentTask(int threadId) {
    for (int i = 0; i < 1000; ++i) {
        void* ptr = HashBucket::useMemory(64);
        assert(ptr != nullptr);
        memset(ptr, threadId, 64);
        HashBucket::freeMemory(ptr, 64);
    }
}

void TestConcurrency() {
    const int THREAD_NUM = 4;
    std::vector<std::thread> threads;
    for (int i = 0; i < THREAD_NUM; ++i) {
        threads.emplace_back(ConcurrentTask, i);
    }
    for (auto& t : threads) {
        t.join();
    }
    std::cout << "TestConcurrency Passed!" << std::endl;
}

int main() {
    HashBucket::initMemoryPool(); // 全局初始化一次
    TestBasicAllocation();
    TestBoundaryConditions();
    TestConcurrency();
    std::cout << "All tests passed!" << std::endl;
    return 0;
}