#include"ThreadCache.h"
#include"CentralCache.h"

//从CentralCache中获取内存块。
void* ThreadCache::FetchFromCentralCache(size_t index, size_t size)
{
	size_t batchNum = min(_freelists[index].GetMaxSize(), SizeClass::SizeToMaxBatchNum(size));//限制大块空间和小块空间的数量
	if (_freelists[index].GetMaxSize() == batchNum)//如果已经达到了当前一次申请的最大数量，那么我们增大这个值，类似于慢开始算法。
	{
		_freelists[index].GetMaxSize() += 1;
	}

	void* start = nullptr;
	void* end = nullptr;
	//这里为什么要返回实际申请到的空间数量，因为我们可能申请的空间块数当前Span中不够，那么我们只能返回实际申请了多少，而就算申请的不足也没事。
	//因为我们只要申请到了一块，那么就可以进行当前Allocate的分配，然后下次再进行分配的时候再申请即可。
	size_t actualNum = CentralCache::GetInstance()->GiveObjToThread(start, end, batchNum, size);//返回实际申请到的空间数量。

	if (actualNum == 1)//如果只申请到了一块内存块。
	{
		assert(start == end);//检验一下是否start==end，因为只有一块。 
	}
	else
	{
		//如果内存块大于1块，那么我们实际上也只需要返回start所指向的内存块即可，因为一次使用的只能是一块内存块。
		//所以我们把start的下一个节点到end节点的这条链直接放到freelist对应的链上，
		_freelists[index].PushRange(Next(start), end, actualNum - 1);
	}
	return start;//实际上我们只需要使用到这个start指向的内存块，其他内存块是为了慢启动算法提高效率。
}

//向ThreadCache申请空间
void* ThreadCache::Allocate(size_t size)
{
	assert(size <= MAX_BYTES);
	size_t alignNum = SizeClass::RoundUp(size);	//对齐后的字节数(对齐到每块对象的大小为alignNum的自由链表上)，要向上取整。
	size_t index = SizeClass::Index(size);		//具体是第几号自由链表(数组下标)

	if (!_freelists[index].IsEmpty())//如果该桶中有空闲块
	{
		return _freelists[index].Pop();	//从freelist上对应的桶中分配一块空间
	}
	else//若没有空闲块，则要去CentralCache层申请。
	{
		return FetchFromCentralCache(index, alignNum);	//从CentralCache上申请一块空间并返回
	}
}

void ThreadCache::Deallocate(void* ptr, size_t size)
{
	assert(ptr);
	assert(size <= MAX_BYTES);

	size_t index = SizeClass::Index(size);	//计算出应该插到哪个自由链表上
	_freelists[index].Push(ptr);	//将ptr这块空间头插到对应的自由链表上

	//当自由链表下面挂着的小块内存的数量 >= 一次批量申请的小块内存的数量时,
	//我们将Size()大小的小块内存全部返回给CentralCache的Span上
	//这里的原因在ListTooLong函数中进行注释
	if (_freelists[index].GetSize() >= _freelists[index].GetMaxSize())//MaxSize是该链的空闲块最大数量，是申请的标准。
	{
		BackToCentral(_freelists[index], size);
	}
}

void ThreadCache::BackToCentral(FreeList& freelist, size_t size)
{
	void* start = nullptr;//拆下来的一长条链表的起始节点。
	void* end = nullptr;//拆下来的一长条链表的结尾节点。

	freelist.PopAll(start, end);//把从start节点到end节点的链全部拆除下来，实际上与函数名相同，我们就是把该freelist链全部清除。
								//我们使用这个函数实际上有很多种情况，并不是链表过长的时候才使用，也有可能是ThreadCache要进行销毁时使用。
	CentralCache::GetInstance()->ObjBackToSpan(start, size);
}

//析构函数，在ThreadCache使用后要销毁时我们要对ThreadCache进行多种操作。
//我们要将整个ThreadCache中所有的桶的内存全部释放掉，这里的释放是将这些内存块还给CentralCache，而不是直接delete给操作系统。
//还要注意的一点是，我们每个线程都申请了一个static变量，也就是线程私有内存，这个内存要进行手动释放，不然会出现内存泄露。
ThreadCache::~ThreadCache()
{
	for (int i = 0; i < FreeListsN; ++i)
	{
		if (!_freelists[i].IsEmpty())
		{
			BackToCentral(_freelists[i], SizeClass::Bytes(i));
		}
	}
}
