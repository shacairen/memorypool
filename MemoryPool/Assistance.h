#pragma once
#include <iostream>
#include <vector>
#include <unordered_map>
#include <cstring>
#include <algorithm>
#include <thread>
#include <mutex>

#include <time.h>
#include <assert.h>
#include <errno.h>

#include<Windows.h>

using std::cout;
using std::endl;

static const size_t MAX_BYTES = 256 * 1024; //设定通过内存储申请的最大内存大小，超过这个大小的就不需要走内存池了，直接向堆申请。
static const size_t FreeListsN = 208;//threadcache层有208个桶，桶中每个freelist的内存块设定见文档。
static const size_t MAX_PAGES = 129;//PageCache层页数的下标数量从1~128，其中0下标处不存放页数0
static const size_t PAGE_SHIFT = 13;//一页为2^13，所以页和地址的转换为右移13位。

typedef size_t PAGE_ID;

inline static void* SystemAlloc(size_t page)
{
	//第一个参数指定要分配的内存块的起始地址，如果设置为NULL，则系统将选择一个合适的地址
	//VirtualAlloc的第二个参数是字节数, 因此要把页数转化为字节数
	void* ptr = VirtualAlloc(NULL, (page << PAGE_SHIFT), MEM_COMMIT | MEM_RESERVE,	PAGE_READWRITE);//使用系统调用接口直接向系统申请空间。
	if (ptr == nullptr)//申请失败返回的是nullptr
		throw std::bad_alloc();
	return ptr;
}

inline static void SystemFree(void* ptr, size_t objectsize)
{
	VirtualFree(ptr, 0, MEM_RELEASE);//使用系统调用接口释放空间。因为第三个参数为MEM_RELEASE，所以第二个参数字节数需要设为0.
}

static inline void*& Next(void* obj)//该函数的目的是在内存块不需要额外封装一个next指针的情况下让链表进行有序连接。
{
	return *(void**)obj;//obj首先转化成void**类型指针，指针可以保证为4字节，然后再解引用，此时就代表了访问void*类型的指针的前四位。
						//我们通过空闲块的前四位来存储该空闲块的next节点地址。
}

class FreeList {//threadcache层使用的内存块管理类。
	//头插法，插入到_head之前
public:
	void Push(void *ptr) {

		assert(ptr);//判断是否为空
		Next(ptr) = _head;
		_head = ptr;//头插法
		CurSize++;

	}

	void PushRange(void *start, void *end, size_t len) {//直接将一条链插进去。

		Next(end) = _head;//end连接_head。
		_head = start;//start变成新的_head。
		CurSize += len;

	}
	//头删法，将头节点删除，然后_head向后指。
	void* Pop() {

		assert(_head);
		void *ptr = _head;
		CurSize--;
		_head = Next(ptr);
		return ptr;

	}
	/**/
	void PopAll(void *&start,void *&end) {//清除整条链表中的所有空闲块，把该链的_head指向nullptr，这些内存块存在start节点为首的链上。
										  //在centralcache层将空闲块变成span存储。
		size_t n = CurSize;
		start = _head;
		end = _head;
		while (--n) {
			end = Next(end);//寻找链尾
		}
		Next(end) = nullptr;
		_head = nullptr;//freelist链断开，设_head为nullptr;
		CurSize = 0;//设链长为0

	}

	bool IsEmpty() {

		return _head == nullptr;//或者CurSize==0

	}

	size_t GetSize() {

		return CurSize;
	}

	size_t& GetMaxSize() {

		return MaxSize;
	}

private:
	void *_head = nullptr;
	size_t MaxSize = 1;
	size_t CurSize = 0;
};

class SizeClass {//本类中的所有函数都是为空间管理服务的，所以所有函数均为static。
public:
	
	static inline size_t _RoundUp(size_t bytes, size_t alignNum)	//本函数是为了计算要申请的空间需要向上取整的数值，例如129-144。
	{
		//整除就直接返回，不能整除就向上取整。
		if (bytes % alignNum != 0)
		{
			return (bytes + alignNum) / alignNum * alignNum;
		}
		else
		{
			return bytes;
		}

	}

	static inline size_t RoundUp(size_t bytes)//根据请求空间的大小范围来判断使用多少字节进行向上对齐。
	{
		if (bytes <= 128)
		{
			return _RoundUp(bytes, 8);//0~128区间内按8字节对齐。
		}
		else if (bytes <= 1024)
		{
			return _RoundUp(bytes, 16);//128~1024区间按16字节对齐
		}
		else if (bytes <= 8 * 1024)
		{
			return _RoundUp(bytes, 128);//1024~8192按128字节对齐
		}
		else if (bytes <= 64 * 1024)
		{
			return _RoundUp(bytes, 1024);
		}
		else if (bytes <= 256 * 1024)
		{
			return _RoundUp(bytes, 8 * 1024);
		}
		else// > 256KB
		{
			//256KB就是32页，>256KB就直接按照"页单位"进行向上对齐
			return _RoundUp(bytes, 1 << PAGE_SHIFT);//1^13 = 8192
		}
	}

	//bytes是字节数，align_shift是该字节数所遵守的对齐数(以位运算中需要左移的位数表示)
	static inline size_t _Index(size_t bytes, size_t align_shift)
	{
		if (bytes % (1 << align_shift) == 0)
		{
			return bytes / (1 << align_shift) - 1;//如果能整除，结果-1即是链表下标。
		}
		else
		{
			return bytes / (1 << align_shift);//不能整除就不需要-1，因为一定不会超过下一个位置的字节数。
		}

	}

	//计算应该放在哪个桶，也就是下标。
	static inline size_t Index(size_t bytes)
	{
		assert(bytes <= MAX_BYTES);

		static int group_freelist[4] = { 16, 72, 128, 184 };	//因为计算的结果是扣除了之前的字节数的，所以计算出的下标需要加上这里数组中的值。
		if (bytes <= 128)
			return _Index(bytes, 3);	//3是2^3, 这里传的是 使用位运算要达到对齐数需要左移的位数
		else if (bytes <= 1024)
			return _Index(bytes - 128, 4) + group_freelist[0];//这里减的是前128字节，因为前128字节还是按8字节进行对齐的，129~1024按16字节对齐。
		else if (bytes <= 8 * 1024)
			return _Index(bytes - 1024, 7) + group_freelist[1];//下面的都是跟上面一个理由 
		else if (bytes <= 64 * 1024)
			return _Index(bytes - 8 * 1024, 10) + group_freelist[2];
		else if (bytes <= 256 * 1024)
			return _Index(bytes - 64 * 1024, 13) + group_freelist[3];
		else
			assert(false);
		return -1;
	}

	//根据链表的下标来计算该链管理多大的内存块。
	static inline size_t Bytes(size_t index)
	{
		static size_t group[4] = { 16, 56, 56, 56 };//每个字节的对齐数有多少个桶。8字节16个桶，其他字节数都是56个桶。
		static size_t total_group[4] = { 16, 72, 128, 184 };

		if (index < total_group[0])
			return (index + 1) * 8;
		else if (index < total_group[1])
			return group[0] * 8 + (index + 1 - group[0]) * 16;
		else if (index < total_group[2])
			return group[0] * 8 + group[1] * 16 + (index + 1 - total_group[1]) * 128;
		else if (index < total_group[3])
			return group[0] * 8 + group[1] * 16 + group[2] * 128 + (index + 1 - total_group[2]) * 1024;
		else
			return group[0] * 8 + group[1] * 16 + group[2] * 128 + group[3] * 1024 + (index + 1 - total_group[3]) * 8 * 1024;
	}

	// 一次ThreadCache应该向CentralCache申请的对象的个数(根据对象的大小计算)
	static size_t SizeToMaxBatchNum(size_t size)//size是内存块大小。
	{
		assert(size > 0);
		// [2, 512],一次批量移动多少个对象的(慢启动)上限值
		// 申请内存块大小越小，申请的数量就越多。
		// 申请内存块大小越大，申请的数量就越少。
		int num = MAX_BYTES / size;//通过内存池管理的最大内存块大小计算一次性申请的数量。
		if (num < 2)//至少申请2个
			num = 2;
		if (num > 512)//最多申请512个。
			num = 512;
		return num;
	}

	//PageCache在向堆申请新的Span时，需要给定Span的页数(有了页数才能向SystemAlloc函数传参)
	//SizeToPage函数是根据申请的小块内存的字节数来计算 申请几页的Span比较合适 
	//函数功能概括:根据对象的大小计算应该要的Span是几页的
	static size_t SizeToPage(size_t size)
	{
		size_t batchNum = SizeToMaxBatchNum(size);//计算申请多少块
		size_t npage = batchNum * size;//计算总共需要的内存大小。
		npage >>= PAGE_SHIFT;//转换成页数

		if (npage == 0)//最少申请1页
			npage = 1;
		return npage;
	}

};

struct Span	//与freelist管理内存块对应的，Span是管理连续页的结构，Span构成spanlist中的节点。
{
	PAGE_ID _pageID = 0;//大块内存起始页的页号(将一个进程的地址空间以页为单位划分,假设一页是8K=2^13)
	size_t _n = 0;		//页的数量
	size_t _useCount = 0;//将切好的小块内存分给threadcache，_useCount记录分出去了多少个小块内存

	bool _isUsed = false;

	size_t _ObjectSize = 0;	//存储当前的Span所进行服务的对象的大小，简单来讲就是对应了多大的内存块，最终切成的内存块的大小。

	Span* _next = nullptr;
	Span* _prev = nullptr;

	void* _freelist = nullptr;//大块内存切成小块并连接起来，这样当threadcache要的时候直接给它小块的，回收的时候也方便管理
							  //这里的是头节点，不是FreeList类型的链表，但是实际上两者有相似之处，这里只不过不存在类结构，直接使用void*管理内存块。
};

//带头节点的双向循环链表，用于存储管理Span。
class SpanList {
public:

	SpanList() {
		_head = new Span;
		_head->_next = _head;
		_head->_prev = _head;
	}

	void PushFront(Span *_span) {//头插法，由于是双向循环链表，所以要修改插入节点的前置和后置节点。
		
		assert(_head->_next&&_span);
		Span *p = _head->_next;
		_head->_next = _span;
		_span->_prev = _head;
		_span->_next = p;
		p->_prev = _span;

	}

	Span* PopFront() {
		
		assert(_head);
		Span *span = _head->_next;
		Erase(span);
		return span;
	}

	void Erase(Span *p) {
		
		assert(p);
		assert(p != _head);
		Span *prev = p->_prev;
		Span *next = p->_next;

		prev->_next = p->_next;
		next->_prev = p->_prev;

	}

	Span* Begin() {//返回头节点之后的第一个节点。
		return _head->_next;
	}

	Span* End() {
		return _head;//因为是双向循环链表，所以_head->_prev就是最后一个节点，所以End返回_head。
	}

	bool IsEmpty() {
		return _head->_next == _head;
	}

	void Lock() { _mtx.lock(); }//上锁
	void UnLock() { _mtx.unlock(); }//解锁

private:
	Span *_head = nullptr;
	std::mutex _mtx;
};