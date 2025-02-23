#include <iostream>
#include <thread>
#include <vector>

#include "../include/memory.h"

using namespace mP;

// ��������
class P1
{
	int id_;
};

class P2
{
	int id_[5];
};

class P3
{
	int id_[10];
};

class P4
{
	int id_[20];
};

// ���ִ������ͷŴ��� �߳��� �ִ�
void BenchmarkMemoryPool(size_t ntimes, size_t nworks, size_t rounds)
{
	std::vector<std::thread> vthread(nworks); // �̳߳�
	size_t total_costtime = 0;
	for (size_t k = 0; k < nworks; ++k) // ���� nworks ���߳�
	{
		vthread[k] = std::thread([&]() {
			for (size_t j = 0; j < rounds; ++j)
			{
				size_t begin1 = clock();
				for (size_t i = 0; i < ntimes; i++)
				{
					P1* p1 = newElement<P1>(); // �ڴ�ض���ӿ�
					deleteElement<P1>(p1);
					P2* p2 = newElement<P2>();
					deleteElement<P2>(p2);
					P3* p3 = newElement<P3>();
					deleteElement<P3>(p3);
					P4* p4 = newElement<P4>();
					deleteElement<P4>(p4);
				}
				size_t end1 = clock();

				total_costtime += end1 - begin1;
			}
			});
	}
	for (auto& t : vthread)
	{
		t.join();
	}
	printf("%zu���̲߳���ִ��%zu�ִΣ�ÿ�ִ�newElement&deleteElement %zu�Σ��ܼƻ��ѣ�%zu ms\n", nworks, rounds, ntimes, total_costtime);
}

void BenchmarkNew(size_t ntimes, size_t nworks, size_t rounds)
{
	std::vector<std::thread> vthread(nworks);
	size_t total_costtime = 0;
	for (size_t k = 0; k < nworks; ++k)
	{
		vthread[k] = std::thread([&]() {
			for (size_t j = 0; j < rounds; ++j)
			{
				size_t begin1 = clock();
				for (size_t i = 0; i < ntimes; i++)
				{
					P1* p1 = new P1;
					delete p1;
					P2* p2 = new P2;
					delete p2;
					P3* p3 = new P3;
					delete p3;
					P4* p4 = new P4;
					delete p4;
				}
				size_t end1 = clock();

				total_costtime += end1 - begin1;
			}
			});
	}
	for (auto& t : vthread)
	{
		t.join();
	}
	printf("%zu���̲߳���ִ��%zu�ִΣ�ÿ�ִ�malloc&free %zu�Σ��ܼƻ��ѣ�%zu ms\n", nworks, rounds, ntimes, total_costtime);
}

int main()
{
	HashBucket::initMemoryPool(); // ʹ���ڴ�ؽӿ�ǰһ��Ҫ�ȵ��øú���
	BenchmarkMemoryPool(10000, 5, 10); // �����ڴ��
	std::cout << "===========================================================================" << std::endl;
	std::cout << "===========================================================================" << std::endl;
	BenchmarkNew(10000, 5, 10); // ���� new delete

	return 0;
}