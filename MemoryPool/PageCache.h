#pragma once
#include"Assistance.h"
#include"ObjectPool.h"
#include<unordered_map>
#include"PageMap.h"

//设置为单例模式，防止多次创建。
class PageCache
{
public:
	static PageCache* GetInstance()
	{
		return &_sInst;
	}

	//向堆申请一个"页数"大小为"NPAGES - 1"的新的Span
	Span* NewSpan(size_t page);

	//将PAGE_ID映射到一个Span*上, 这样可以通过页号直接找到对应的Span*的位置
	Span* PAGEIDtoSpan(PAGE_ID id);

	//将useCount减为0的Span返回给PageCache，以用来合并成更大的Span
	void BackToPageCache(Span* span);

	void PageLock() { _pageMutex.lock(); }
	void PageUnLock() { _pageMutex.unlock(); }
private:
	PageCache() {}
	PageCache(const PageCache&) = delete;
	PageCache& operator=(const PageCache&) = delete;

	static PageCache _sInst;
	std::recursive_mutex _pageMutex;	//一把大锁，一旦访问PageCache就要加锁

private:
	SpanList _spanlists[MAX_PAGES];	//以页数为映射的规则(直接定址法)
	TCMalloc_PageMap1<BITSNUM> _IDtoSpan;
	//std::unordered_map<PAGE_ID,Span*> _idSpanMap;//使用哈希表存储页号和span的对应关系。
	ObjectPool<Span> _spanPool;
};
