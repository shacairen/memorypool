#include"CentralCache.h"
#include"PageCache.h"

//静态成员变量在类外进行初始化。
CentralCache CentralCache::_sInst;

//获取一个非空的Span，这个Span至少有1个freelist的空闲块。
Span* CentralCache::GetOneSpan(SpanList& slist, size_t size) 
{
	Span* it = slist.Begin();
	while (it != slist.End())
	{
		if (it->_freelist != nullptr)
		{
			return it;	//找到了非空的span，直接返回。另一种解法就是我们找到足够空闲块的Span再返回，但是实际上这样也会造成一些问题。
		}
		it = it->_next;
	}

	slist.UnLock();
	//没找到非空的span, 向PageCache索要.这里涉及到了CentralCache向PageCache索要Span。
	PageCache::GetInstance()->PageLock();	//由于我们用到了PageCache，PageCache是要全程加锁的。
	Span* newSpan = PageCache::GetInstance()->NewSpan(SizeClass::SizeToPage(size));	
	PageCache::GetInstance()->PageUnLock();//使用后要记得解锁。

	newSpan->_ObjectSize = size;

	//将newSpan切分成小块内存, 并将这些小块内存放到newSpan的成员_freelist下，使用Next(obj)函数进行连接，这个函数实际上就起到了一个next指针的作用。
	//先计算出大块内存的起始地址(页号*每页的大小)和终止地址(页数*每页的大小+起始地址)
	char* start = (char*)(newSpan->_pageID << PAGE_SHIFT);
	char* end = start + (newSpan->_n << PAGE_SHIFT);

	//本次切分操作实际上就是要把每个空闲块之间使用Next函数进行连接，首先我们让freelist指向start，作为头指针。
	//然后我们每次让start后移size个字节，也就是指向了下一个空闲块的第一个字节。直到start等于end，那么这条链也就完成了。
	newSpan->_freelist = (void*)start;
	void* ptr = start;
	start += size;
	Next(ptr) = start;

	int j = 1;
	//开始切分
	while (start < end)
	{
		++j;
		Next(ptr) = start;
		start += size;//相当于每个size大小start都往后移了一个空闲块。
		if (start >= end) {
			Next(ptr) = nullptr;
			break;
		}
		ptr = Next(ptr);
	}

	Next(ptr) = nullptr;

	slist.Lock();//这里一定要记得加锁，因为使用了SpanList，而解锁的操作在出口之后。
	slist.PushFront(newSpan);	//将新的newSpan插入到slist中
	return newSpan;

}

//这个函数是ThreadCache层向CentralCache层申请一定数量的空闲块给ThreadCache层。
//参数中的start跟end设置为引用，因为我们在返回后要直接使用start跟end来管理返回的空闲块链表。
//参数中的batchNum是期望分配的空闲块，但是不一定会真的分配这么多，size是空闲块大小，根据size找到SpanList。
//函数的返回值是实际上我们给ThreadCache层的空闲块数量，这个数量需要返回是因为当前Span不一定有足够的空闲块，但是至少能保证有一块。
size_t CentralCache::GiveObjToThread(void*& start, void*& end, size_t batchNum, size_t size)
{
	size_t index = SizeClass::Index(size);	//确定哪个桶
	_spanlists[index].Lock();	//SpanList中每条链在使用的时候都要加锁，因为是公共资源，防止多线程问题。

	Span* span = GetOneSpan(_spanlists[index], size);//获取一个Span。
	assert(span && span->_freelist);

	start = span->_freelist;//指向头
	end = start;//指向最终的尾
	size_t actualNum = 1;//因为GetOneSpan中我们选择的Span的freelist不为空，所以预设为1。
	while (--batchNum && Next(end) != nullptr)	//两种情况，要么就是空闲块数量够分配过去，要么就是空闲块不够但是我们也进行分配。
	{//因为是--batchNum，所以end向后移动了batchNum-1次。
		++actualNum;
		end = Next(end);
	}
	span->_freelist = Next(end);//Span的freelist的头节点设置为end的下一个节点。
	Next(end) = nullptr;
	
	span->_useCount += actualNum;

	_spanlists[index].UnLock();
	return actualNum;

}

//将以start为头节点的freelist中的所有空闲块都归还给相应的Spanlist。
void CentralCache::ObjBackToSpan(void* start, size_t size)
{
	assert(start);
	size_t index = SizeClass::Index(size);
	_spanlists[index].Lock();

	while (start) {

		void* NextPos = Next(start);
		PAGE_ID id = ((PAGE_ID)start >> PAGE_SHIFT);//在某一页当中的所有地址除以页的大小，它们得到的结果都是当前页的页号
		//PageCache::GetInstance()->PageLock();
		Span* ret = PageCache::GetInstance()->PAGEIDtoSpan(id);//映射关系在PageCache将Span分给CentralCache时进行存储，通过页号定位Span。
		//PageCache::GetInstance()->PageUnLock();
		if (ret != nullptr)
		{
			Next(start) = ret->_freelist;//_freelist是头节点
			ret->_freelist = start;//头插法。
		}
		else
		{
			assert(false);//如果为nullptr就要终止。
		}

		--ret->_useCount;//要记得把Span分出去的块数减1.

		//这里使用的策略是，如果一个Span被使用的空闲块数为0，那么我们就把这个Span归还给PageCache层，如果需要的话重新进行申请。
		if (ret->_useCount == 0)
		{
			_spanlists[index].Erase(ret);//脱链操作。
			ret->_freelist = nullptr;
			ret->_prev = nullptr;
			ret->_next = nullptr;

			_spanlists[index].UnLock();
			PageCache::GetInstance()->PageLock();//进入PageCache层加锁。
			PageCache::GetInstance()->BackToPageCache(ret);
			PageCache::GetInstance()->PageUnLock();
			_spanlists[index].Lock();
		}
		start = NextPos;
	}
	_spanlists[index].UnLock();
}