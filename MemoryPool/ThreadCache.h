#pragma once
#include"Assistance.h"


class ThreadCache
{
public:
	//������ͷ��ڴ����
	void* Allocate(size_t size);
	void Deallocate(void* ptr, size_t size);

	void BackToCentral(FreeList& freelist, size_t size);

	//��CentralCache��ȡalignNum�����С��С���ڴ�
	void* FetchFromCentralCache(size_t Index, size_t alignNum);
	~ThreadCache();
private:
	FreeList _freelists[FreeListsN];
};

// TLS thread local storage
static thread_local ThreadCache* pTLSThreadCache = nullptr;//�������static����Ȼ���ڶ��.cpp�����ض��� //�ı�thread_local
