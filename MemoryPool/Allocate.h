#pragma once
#include "ThreadCache.h"
#include "PageCache.h"
#include "ObjectPool.h"
#include "PageMap.h"

static ObjectPool<ThreadCache> tcPool;

static void* ConcurrentAllocate(size_t size) 
{
	//�����ڴ�ع���Ĵ�С����ô�Ͳ������ڴ�ع������︴��NewSpan�����ĵ�һ��������з��䡣
	if (size > MAX_BYTES)
	{
		size_t alignSize = SizeClass::RoundUp(size);
		PageCache::GetInstance()->PageLock();
		Span* span = PageCache::GetInstance()->NewSpan(alignSize >> PAGE_SHIFT);
		PageCache::GetInstance()->PageUnLock();
		void* ptr = (void*)(span->_pageID << PAGE_SHIFT);//span���е�_pageID��_n�ǹ����ٳ����Ŀռ���ֶ�
		return ptr;
	}
	else
	{
		if (pTLSThreadCache == nullptr)
		{
			pTLSThreadCache = tcPool.New();
		}
		return pTLSThreadCache->Allocate(size);
	}
}

static void ConcurrentFree(void* ptr)
{
	PAGE_ID pageID = ((PAGE_ID)ptr >> PAGE_SHIFT);
	//PageCache::GetInstance()->PageLock();
	//Span* span = PageCache::GetInstance()->MapPAGEIDToSpan(pageID);
	//PageCache::GetInstance()->PageUnLock();

	Span* span = PageCache::GetInstance()->PAGEIDtoSpan(pageID);

	size_t size = span->_ObjectSize;	//ͨ��Span���е�ObjectSize��ȡ�����С

	//cout << size << endl;

	if (size > MAX_BYTES)//(��������ֽ���������ŵ�ReleaseSpanToPageCache���洦��)
	{
		PageCache::GetInstance()->PageLock();
		PageCache::GetInstance()->BackToPageCache(span);
		PageCache::GetInstance()->PageUnLock();
	}
	else
	{
		assert(pTLSThreadCache);
		pTLSThreadCache->Deallocate(ptr, size);
	}
}

template<class T>
T * New() {
	void *ptr = ConcurrentAllocate(sizeof(T));
	T *m = new(ptr)T;
	return m;
}

template<class T>
void Delete(T *m) {
	m->~T();
	void *ptr = (void *)m;
	//cout << sizeof(*ptr) << endl;
	ConcurrentFree(ptr);
}