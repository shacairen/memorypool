#pragma once
#include"Assistance.h"

template <class T>
class ObjectPool
{
public:
	T* New()
	{
		T* obj;
		if (_freeList)	//freeList里面存有空闲空间时，优先从freeList里面取
		{
			obj = (T*)_freeList;
			_freeList = *(void**)_freeList;	//将freeList跳转到下一个对象处
		}
		else
		{
			if (_remainBytes < sizeof(T))//如果剩下的空间不足一次申请的。
			{
				_remainBytes = 128 * 1024;	//如果_remainBytes<sizeof(T)并且不为0时，扔掉这部分空间，重新申请
				_memory = (char*)SystemAlloc(_remainBytes >> PAGE_SHIFT);
				if (_memory == nullptr)//申请失败
				{
					throw std::bad_alloc();
				}
			}

			obj = (T*)_memory;
			//如果对象的大小小于指针的大小，那么我们也要每次给对象分配最低指针的大小。因为freeList必须能存得下一个指针的大小
			//因为空间分配后还要还给freeList，而freeList管理的是void*，所以必须给至少一个指针的大小。
			size_t objSize = sizeof(T) > sizeof(void*) ? sizeof(T) : sizeof(void*);
			_memory += objSize;
			_remainBytes -= objSize;
		}

		//使用定位new调用obj对象的构造函数
		new(obj)T;
		return obj;
	}

	void Delete(T* ptr)
	{
		ptr->~T();	//手动调用T对象的析构函数
		//头插法，将不用的空间插到freeList当中
		*(void**)ptr = _freeList;	//在32位下，*(void**)能看到4个字节；在64位下，*(void**)能看到8个字节
		_freeList = ptr;
	}

private:
	char* _memory = nullptr;	//指向内存块的指针
	size_t _remainBytes = 0;	//内存块的剩余空间字节数
	void* _freeList = nullptr;	//管理从内存块中所释放出来的空间的自由链表
};

