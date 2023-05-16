#pragma once
#include "ThreadCache.h"
#include "PageCache.h"
#include "ObjectPool.h"
#include "PageMap.h"

static ObjectPool<ThreadCache> tcPool;

static void* ConcurrentAllocate(size_t size) 
{
	//大于内存池管理的大小后，那么就不交给内存池管理，这里复用NewSpan函数的第一种情况进行分配。
	if (size > MAX_BYTES)
	{
		size_t alignSize = SizeClass::RoundUp(size);
		PageCache::GetInstance()->PageLock();
		Span* span = PageCache::GetInstance()->NewSpan(alignSize >> PAGE_SHIFT);
		PageCache::GetInstance()->PageUnLock();
		void* ptr = (void*)(span->_pageID << PAGE_SHIFT);//span当中的_pageID和_n是管理开辟出来的空间的字段
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

	size_t size = span->_ObjectSize;	//通过Span当中的ObjectSize获取对象大小

	//cout << size << endl;

	if (size > MAX_BYTES)//(大于最大字节数的情况放到ReleaseSpanToPageCache里面处理)
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