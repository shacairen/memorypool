#pragma once
#include"Assistance.h"

template <class T>
class ObjectPool
{
public:
	T* New()
	{
		T* obj;
		if (_freeList)	//freeList������п��пռ�ʱ�����ȴ�freeList����ȡ
		{
			obj = (T*)_freeList;
			_freeList = *(void**)_freeList;	//��freeList��ת����һ������
		}
		else
		{
			if (_remainBytes < sizeof(T))//���ʣ�µĿռ䲻��һ������ġ�
			{
				_remainBytes = 128 * 1024;	//���_remainBytes<sizeof(T)���Ҳ�Ϊ0ʱ���ӵ��ⲿ�ֿռ䣬��������
				_memory = (char*)SystemAlloc(_remainBytes >> PAGE_SHIFT);
				if (_memory == nullptr)//����ʧ��
				{
					throw std::bad_alloc();
				}
			}

			obj = (T*)_memory;
			//�������Ĵ�СС��ָ��Ĵ�С����ô����ҲҪÿ�θ�����������ָ��Ĵ�С����ΪfreeList�����ܴ����һ��ָ��Ĵ�С
			//��Ϊ�ռ�����Ҫ����freeList����freeList�������void*�����Ա��������һ��ָ��Ĵ�С��
			size_t objSize = sizeof(T) > sizeof(void*) ? sizeof(T) : sizeof(void*);
			_memory += objSize;
			_remainBytes -= objSize;
		}

		//ʹ�ö�λnew����obj����Ĺ��캯��
		new(obj)T;
		return obj;
	}

	void Delete(T* ptr)
	{
		ptr->~T();	//�ֶ�����T�������������
		//ͷ�巨�������õĿռ�嵽freeList����
		*(void**)ptr = _freeList;	//��32λ�£�*(void**)�ܿ���4���ֽڣ���64λ�£�*(void**)�ܿ���8���ֽ�
		_freeList = ptr;
	}

private:
	char* _memory = nullptr;	//ָ���ڴ���ָ��
	size_t _remainBytes = 0;	//�ڴ���ʣ��ռ��ֽ���
	void* _freeList = nullptr;	//������ڴ�������ͷų����Ŀռ����������
};

