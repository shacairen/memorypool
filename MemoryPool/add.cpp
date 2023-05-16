#include"Allocate.h"
#include"Assistance.h"
#include<atomic>

void BenchmarkMyMalloc()
{
	std::thread t1=std::thread([&]() {
			int sum = 0;//总共申请的空间
			int waste = 0;
			std::vector<void *> vec;
			while (1) {
				cout << "请输入要申请的空间大小" << endl;
				int sz = 0;
				std::cin >> sz;
				vec.push_back(ConcurrentAllocate(sz));
				size_t cur= SizeClass::RoundUp(sz);
				sum += cur;
				waste += cur - sz;
				cout << "目前申请的空闲块数为：" << vec.size() << endl;
				cout << "目前空间利用率为：" << sum - waste << "/" << sum << endl;
				cout << "目前内存碎片约为：" << (double)waste/(double)sum << endl;
			}
			//线程结束之前, Delete并调用ThreadCache的析构函数，释放ThreadCache中的小块对象返回给CentralCache
			if (pTLSThreadCache)
				tcPool.Delete(pTLSThreadCache);
		});

	t1.join();
}


int main() {
	BenchmarkMyMalloc();
}