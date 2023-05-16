#include"PageCache.h"

PageCache PageCache::_sInst;	//静态成员类外定义

/*
我们首先要明确向PageCache申请Span的时候我们遇到的情况有三种。
第一种:我们申请的Span，在PageCache的对应的桶中，正好有挂载的Span块，那么我们可以直接使用这个块进行分配。
第二种:我们申请的Span，在PageCache的对应的桶中没有Span块，那么此时我们需要做的就是向上寻找。我们要找到当前桶之后的桶有没有可以使用的Span。
找到之后我们要做的就是将Span块切分成两个Span，一个是我们申请的大小，另一个是剩余的大小，然后一个用于分配，另一个放在对应桶中。
第三种情况:我们在整个PageCache的所有桶中，都找不到合适的Span块，那么我们此时就需要向操作系统申请一个最大的Span，然后将最大的Span分成两块。
一块用于分配，另一块放在对应的桶中。
*/
Span* PageCache::NewSpan(size_t page)
{
	//疑问:大于这个值为什么哈希表里还要进行存储。
	//超过MAX_PAGES-1就直接向堆申请即可，不由内存池进行管理。
	if (page > MAX_PAGES - 1)
	{
		void* ptr = SystemAlloc(page);
		Span* newSpan = _spanPool.New();
		newSpan->_pageID = ((PAGE_ID)ptr >> PAGE_SHIFT);
		newSpan->_n = page;
		newSpan->_ObjectSize = (page << PAGE_SHIFT);	//告知Span它分配对象的大小(这里给的是对齐数)
		//_idSpanMap[newSpan->_pageID]= newSpan;//将对应的页号存储在哈希表中。
		_IDtoSpan.set(newSpan->_pageID, newSpan);
		return newSpan;
	}

	//查看当前位置有没有合适的Span，如果有则直接返回。对应第一种情况。
	if (!_spanlists[page].IsEmpty())
	{
		Span* span = _spanlists[page].PopFront();

		//将每一页都要存入到哈希表中，为了之后根据页号查找对应的Span。
		for (PAGE_ID i = span->_pageID; i < span->_pageID + span->_n; ++i)
		{
			//_idSpanMap[i] = span;
			_IDtoSpan.set(i, span);
		}
		//
		span->_isUsed = true;

		return span;
	}

	//遍历这之后的每一个SpanList，寻找一个更大的Span，也就是对应第二种情况。
	for (int i = page + 1; i < MAX_PAGES; ++i)
	{
		//找到了Span，这里的Span是更大的Span，所以我们要将大的切成两个小的。
		if (!_spanlists[i].IsEmpty())
		{
			Span* BigSpan = _spanlists[i].PopFront();
			Span* SmallSpan = _spanPool.New();//用于分配出去的Span。

			SmallSpan->_n = page;//设定页数
			SmallSpan->_pageID = BigSpan->_pageID;//设置起始页号
			BigSpan->_n -= page;//分割之后剩下的部分的页数
			
			BigSpan->_pageID += page;//分割之后剩下的部分的起始页号要从原来的起始页号+分割下去的页数。

			SmallSpan->_ObjectSize = (page << PAGE_SHIFT);	//告知Span它分配对象的大小(这里给的是对齐数)


			//也要把BigSpan的首页和尾页存到映射表当中，这样在空闲的Span进行合并查找时能够通过映射表找到BigSpan随后进行合并
			//这里解释一下为什么不需要存储每一页，因为我们在分配出去的时候才需要知道每一页对Span的映射，而在PageCache层不会涉及到每一页。
			//_idSpanMap[BigSpan->_pageID] = BigSpan;
			//_idSpanMap[BigSpan->_pageID + BigSpan->_n - 1] = BigSpan;	
			_IDtoSpan.set(BigSpan->_pageID, BigSpan);
			_IDtoSpan.set(BigSpan->_pageID + BigSpan->_n - 1, BigSpan);

			_spanlists[BigSpan->_n].PushFront(BigSpan);

			//保存返回给CentralCache时的SmallSpan的每一页与SmallSpan的映射关系, 这样方便SmallSpan切分成小块内存后，然后小块内存通过映射关系找回SmallSpan
			for (PAGE_ID i = SmallSpan->_pageID; i < SmallSpan->_pageID + SmallSpan->_n; ++i)
			{
				//_idSpanMap[i] = SmallSpan;
				_IDtoSpan.set(i, SmallSpan);
			}

			SmallSpan->_isUsed = true;
			return SmallSpan;
		}
	}

	//此处为第三种情况，第三种情况就是我们整个SpanList都没有可以使用的Span，那么我们就要向系统申请一个页数为MAX_PAGES-1的Span空闲块。
	//这个空闲块申请之后把它放到最后一个桶中，然后我们return NewSpan(page)，也就是递归调用一次NewSpan，然后在下一次调用中就可以使用第二种情况。
	Span* newSpan = _spanPool.New();

	void* ptr = SystemAlloc(MAX_PAGES - 1);	//向系统申请的空间是交给newSpan中的_pageID和_n进行管理的
	newSpan->_n = MAX_PAGES - 1;
	newSpan->_pageID = ((PAGE_ID)ptr >> PAGE_SHIFT);
	_spanlists[MAX_PAGES - 1].PushFront(newSpan);
	return NewSpan(page);

}

//将PAGE_ID映射到一个Span*上, 这样可以通过页号直接找到对应的Span*的位置
//简单来说就是参数是页号，返回值是Span。
Span* PageCache::PAGEIDtoSpan(PAGE_ID id)
{
	//auto it = _idSpanMap.find(id);
	//if (it != _idSpanMap.end())
	//{
	//	return it->second;
	//}
	
	//return nullptr;

	Span* ret = (Span*)_IDtoSpan.get(id);
	return ret;
	
}

//将没有被使用的Span在PageCache层合成更大的Span。
void PageCache::BackToPageCache(Span* span)
{
	//大于这个值说明这块内存是直接向堆申请的，内存池不做管理，直接用堆释放。
	if (span->_n > MAX_PAGES - 1)
	{
		void* ptr = (void*)(span->_pageID << PAGE_SHIFT);
		SystemFree(ptr, span->_ObjectSize);	//VirtualFree
		return;
	}

	//向前查找，是否有可以进行合并的Span。
	//为什么要用while(1)，因为可能连续很多页属于不同的Span，要把他们都连在一起。
	while (1)
	{
		PAGE_ID prevID = span->_pageID - 1;	//当前Span的起始页号-1就是之前的页。
		Span* prevSpan = PAGEIDtoSpan(prevID);//通过映射关系，找到前面一个页所在的Span当中
		if (prevSpan == nullptr)	//判断前面的Span是否被申请过(可能还在系统中)[如果在映射表当中，就证明被申请过]
			break;
		if (prevSpan->_isUsed == true)	//前面的Span正在被CentralCache使用(如果正在被使用，那么就不可以进行合并)
			break;
		if (prevSpan->_n + span->_n > MAX_PAGES - 1)//合并后的大小>128时, 不能合并,因为内存池不会管理超过这个大小的内存块。
			break;
		//开始合并
		span->_pageID = prevSpan->_pageID;//合并后的起始页号就是prevSpan的起始页号了。
		span->_n += prevSpan->_n;//页数相加。

		_spanlists[prevSpan->_n].Erase(prevSpan);//合并所以要删除。
		_spanPool.Delete(prevSpan);
		prevSpan = nullptr;
	}

	//向后查找，和向前查找一样。
	while (1)
	{
		PAGE_ID nextID = span->_pageID + span->_n;
		Span* nextSpan = PAGEIDtoSpan(nextID);
		if (nextSpan == nullptr)
			break;
		if (nextSpan->_isUsed == true)
			break;
		if (nextSpan->_n + span->_n > MAX_PAGES - 1)
			break;
		//开始合并
		span->_n += nextSpan->_n;

		_spanlists[nextSpan->_n].Erase(nextSpan);
		//delete nextSpan;
		_spanPool.Delete(nextSpan);
		nextSpan = nullptr;
	}

	span->_isUsed = false;
	_spanlists[span->_n].PushFront(span);	//将合并后的Span插入到spanlist当中
	//_idSpanMap[span->_pageID] = span;//哈希表存储第一页和最后一页。
	//_idSpanMap[span->_pageID + span->_n - 1] = span;
	_IDtoSpan.set(span->_pageID, span);
	_IDtoSpan.set(span->_pageID + span->_n - 1, span);
	
}

