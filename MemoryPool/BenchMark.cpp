#include"Allocate.h"
#include<atomic>
#include<fstream>

//ntimes代表次数，nworks代表线程数，rounds代表轮数
void BenchmarkMalloc(size_t ntimes, size_t nworks, size_t rounds)
{
	std::vector<std::thread> vthread(nworks);
	std::atomic<unsigned int> malloc_costtime;
	malloc_costtime = 0;
	std::atomic<unsigned int> free_costtime;
	free_costtime = 0;

	for (size_t k = 0; k < nworks; ++k)
	{
		vthread[k] = std::thread([&]() {
			std::vector<void*> v;
			v.reserve(ntimes);

			for (size_t j = 0; j < rounds; ++j)
			{
				size_t begin1 = clock();
				for (size_t i = 0; i < ntimes; i++)
				{
					v.push_back(malloc((16 + i) % 8192 + 1));
				}
				size_t end1 = clock();

				size_t begin2 = clock();
				for (size_t i = 0; i < ntimes; i++)
				{
					free(v[i]);
				}
				size_t end2 = clock();
				v.clear();

				malloc_costtime += (end1 - begin1);
				free_costtime += (end2 - begin2);
			}
		});
	}

	for (auto& t : vthread)
	{
		t.join();
	}

	std::ofstream file2("output2.csv", std::ios::app);
	file2 << nworks << "," << rounds * ntimes << "," << malloc_costtime << "," << free_costtime << endl;

	//cout << nworks << "个线程并发执行" << rounds << "轮次" << ",每轮次malloc" << ntimes << "次: 花费：" << malloc_costtime << "ms" << endl;

	//cout << nworks << "个线程并发执行" << rounds << "轮次" << ",每轮次free" << ntimes << "次: 花费：" << free_costtime << "ms" << endl;

	//cout << nworks << "个线程并发执行malloc&free " << nworks * rounds*ntimes << "次, 总计花费:：" << malloc_costtime + free_costtime << "ms" << endl;
	file2.close();
}

void BenchmarkMyMalloc(size_t ntimes, size_t nworks, size_t rounds)
{
	std::vector<std::thread> vthread(nworks);
	std::atomic<unsigned long> malloc_costtime;
	malloc_costtime = 0;
	std::atomic<unsigned long> free_costtime;
	free_costtime = 0;

	for (size_t k = 0; k < nworks; ++k)
	{
		vthread[k] = std::thread([&]() {
			std::vector<void*> v;
			v.reserve(ntimes);

			for (size_t j = 0; j < rounds; ++j)
			{
				size_t begin1 = clock();
				for (size_t i = 0; i < ntimes; i++)
				{
					v.push_back(ConcurrentAllocate((16 + i) % 8192 + 1));//byte字节 
					//cout << v[i] << endl;
				}
				size_t end1 = clock();

				size_t begin2 = clock();
				for (size_t i = 0; i < ntimes; i++)
				{
					ConcurrentFree(v[i]);
				}
				size_t end2 = clock();
				v.clear();

				malloc_costtime += (end1 - begin1);
				free_costtime += (end2 - begin2);
			}

			//线程结束之前, Delete并调用ThreadCache的析构函数，释放ThreadCache中的小块对象返回给CentralCache
			if (pTLSThreadCache)
				tcPool.Delete(pTLSThreadCache);
		});
	}

	for (auto& t : vthread)
	{
		t.join();
	}
	std::ofstream file1("output1.csv", std::ios::app);
	file1 << nworks << "," << rounds * ntimes << "," << malloc_costtime <<","<<free_costtime<< endl;
	//cout << nworks << "个线程并发执行" << rounds << "轮次" << ",每轮次concurrent alloc" << ntimes << "次: 花费：" << malloc_costtime << "ms" << endl;

	//cout << nworks << "个线程并发执行" << rounds << "轮次" << ",每轮次concurrent dealloc" << ntimes << "次: 花费：" << free_costtime << "ms" << endl;

	//cout << nworks << "个线程并发执行concurrent alloc&dealloc " << nworks * rounds*ntimes << "次, 总计花费:：" << malloc_costtime + free_costtime << "ms" << endl;

	file1.close();
}


/*int main()
{
	size_t n = 1000;
	cout << "========================================================" << endl;
	//BenchmarkMyMalloc(n, 4, 100);//n代表申请次数，4代表线程数，100代表轮次
	BenchmarkMyMalloc(100, 1, 500);
	cout << endl << endl;

	BenchmarkMalloc(100, 1, 500);
	cout << "==========================================================" << endl;

	
	for (int j = 0; j < 10; j++) {
		for (int i = 0; i < 100; i++) {
			BenchmarkMyMalloc(100, j+1, 200);
			//cout << endl << endl;

			BenchmarkMalloc(100, j+1, 200);
		}
	}

	//std::thread t1(Routine1);
	//std::thread t2(Routine2);

	//t1.join();
	//t2.join();

	return 0;
}
*/