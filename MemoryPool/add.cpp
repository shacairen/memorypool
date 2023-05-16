#include"Allocate.h"
#include"Assistance.h"
#include<atomic>

void BenchmarkMyMalloc()
{
	std::thread t1=std::thread([&]() {
			int sum = 0;//�ܹ�����Ŀռ�
			int waste = 0;
			std::vector<void *> vec;
			while (1) {
				cout << "������Ҫ����Ŀռ��С" << endl;
				int sz = 0;
				std::cin >> sz;
				vec.push_back(ConcurrentAllocate(sz));
				size_t cur= SizeClass::RoundUp(sz);
				sum += cur;
				waste += cur - sz;
				cout << "Ŀǰ����Ŀ��п���Ϊ��" << vec.size() << endl;
				cout << "Ŀǰ�ռ�������Ϊ��" << sum - waste << "/" << sum << endl;
				cout << "Ŀǰ�ڴ���ƬԼΪ��" << (double)waste/(double)sum << endl;
			}
			//�߳̽���֮ǰ, Delete������ThreadCache�������������ͷ�ThreadCache�е�С����󷵻ظ�CentralCache
			if (pTLSThreadCache)
				tcPool.Delete(pTLSThreadCache);
		});

	t1.join();
}


int main() {
	BenchmarkMyMalloc();
}