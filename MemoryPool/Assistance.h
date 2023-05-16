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

static const size_t MAX_BYTES = 256 * 1024; //�趨ͨ���ڴ洢���������ڴ��С�����������С�ľͲ���Ҫ���ڴ���ˣ�ֱ��������롣
static const size_t FreeListsN = 208;//threadcache����208��Ͱ��Ͱ��ÿ��freelist���ڴ���趨���ĵ���
static const size_t MAX_PAGES = 129;//PageCache��ҳ�����±�������1~128������0�±괦�����ҳ��0
static const size_t PAGE_SHIFT = 13;//һҳΪ2^13������ҳ�͵�ַ��ת��Ϊ����13λ��

typedef size_t PAGE_ID;

inline static void* SystemAlloc(size_t page)
{
	//��һ������ָ��Ҫ������ڴ�����ʼ��ַ���������ΪNULL����ϵͳ��ѡ��һ�����ʵĵ�ַ
	//VirtualAlloc�ĵڶ����������ֽ���, ���Ҫ��ҳ��ת��Ϊ�ֽ���
	void* ptr = VirtualAlloc(NULL, (page << PAGE_SHIFT), MEM_COMMIT | MEM_RESERVE,	PAGE_READWRITE);//ʹ��ϵͳ���ýӿ�ֱ����ϵͳ����ռ䡣
	if (ptr == nullptr)//����ʧ�ܷ��ص���nullptr
		throw std::bad_alloc();
	return ptr;
}

inline static void SystemFree(void* ptr, size_t objectsize)
{
	VirtualFree(ptr, 0, MEM_RELEASE);//ʹ��ϵͳ���ýӿ��ͷſռ䡣��Ϊ����������ΪMEM_RELEASE�����Եڶ��������ֽ�����Ҫ��Ϊ0.
}

static inline void*& Next(void* obj)//�ú�����Ŀ�������ڴ�鲻��Ҫ�����װһ��nextָ������������������������ӡ�
{
	return *(void**)obj;//obj����ת����void**����ָ�룬ָ����Ա�֤Ϊ4�ֽڣ�Ȼ���ٽ����ã���ʱ�ʹ����˷���void*���͵�ָ���ǰ��λ��
						//����ͨ�����п��ǰ��λ���洢�ÿ��п��next�ڵ��ַ��
}

class FreeList {//threadcache��ʹ�õ��ڴ������ࡣ
	//ͷ�巨�����뵽_head֮ǰ
public:
	void Push(void *ptr) {

		assert(ptr);//�ж��Ƿ�Ϊ��
		Next(ptr) = _head;
		_head = ptr;//ͷ�巨
		CurSize++;

	}

	void PushRange(void *start, void *end, size_t len) {//ֱ�ӽ�һ�������ȥ��

		Next(end) = _head;//end����_head��
		_head = start;//start����µ�_head��
		CurSize += len;

	}
	//ͷɾ������ͷ�ڵ�ɾ����Ȼ��_head���ָ��
	void* Pop() {

		assert(_head);
		void *ptr = _head;
		CurSize--;
		_head = Next(ptr);
		return ptr;

	}
	/**/
	void PopAll(void *&start,void *&end) {//������������е����п��п飬�Ѹ�����_headָ��nullptr����Щ�ڴ�����start�ڵ�Ϊ�׵����ϡ�
										  //��centralcache�㽫���п���span�洢��
		size_t n = CurSize;
		start = _head;
		end = _head;
		while (--n) {
			end = Next(end);//Ѱ����β
		}
		Next(end) = nullptr;
		_head = nullptr;//freelist���Ͽ�����_headΪnullptr;
		CurSize = 0;//������Ϊ0

	}

	bool IsEmpty() {

		return _head == nullptr;//����CurSize==0

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

class SizeClass {//�����е����к�������Ϊ�ռ�������ģ��������к�����Ϊstatic��
public:
	
	static inline size_t _RoundUp(size_t bytes, size_t alignNum)	//��������Ϊ�˼���Ҫ����Ŀռ���Ҫ����ȡ������ֵ������129-144��
	{
		//������ֱ�ӷ��أ���������������ȡ����
		if (bytes % alignNum != 0)
		{
			return (bytes + alignNum) / alignNum * alignNum;
		}
		else
		{
			return bytes;
		}

	}

	static inline size_t RoundUp(size_t bytes)//��������ռ�Ĵ�С��Χ���ж�ʹ�ö����ֽڽ������϶��롣
	{
		if (bytes <= 128)
		{
			return _RoundUp(bytes, 8);//0~128�����ڰ�8�ֽڶ��롣
		}
		else if (bytes <= 1024)
		{
			return _RoundUp(bytes, 16);//128~1024���䰴16�ֽڶ���
		}
		else if (bytes <= 8 * 1024)
		{
			return _RoundUp(bytes, 128);//1024~8192��128�ֽڶ���
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
			//256KB����32ҳ��>256KB��ֱ�Ӱ���"ҳ��λ"�������϶���
			return _RoundUp(bytes, 1 << PAGE_SHIFT);//1^13 = 8192
		}
	}

	//bytes���ֽ�����align_shift�Ǹ��ֽ��������صĶ�����(��λ��������Ҫ���Ƶ�λ����ʾ)
	static inline size_t _Index(size_t bytes, size_t align_shift)
	{
		if (bytes % (1 << align_shift) == 0)
		{
			return bytes / (1 << align_shift) - 1;//��������������-1���������±ꡣ
		}
		else
		{
			return bytes / (1 << align_shift);//���������Ͳ���Ҫ-1����Ϊһ�����ᳬ����һ��λ�õ��ֽ�����
		}

	}

	//����Ӧ�÷����ĸ�Ͱ��Ҳ�����±ꡣ
	static inline size_t Index(size_t bytes)
	{
		assert(bytes <= MAX_BYTES);

		static int group_freelist[4] = { 16, 72, 128, 184 };	//��Ϊ����Ľ���ǿ۳���֮ǰ���ֽ����ģ����Լ�������±���Ҫ�������������е�ֵ��
		if (bytes <= 128)
			return _Index(bytes, 3);	//3��2^3, ���ﴫ���� ʹ��λ����Ҫ�ﵽ��������Ҫ���Ƶ�λ��
		else if (bytes <= 1024)
			return _Index(bytes - 128, 4) + group_freelist[0];//���������ǰ128�ֽڣ���Ϊǰ128�ֽڻ��ǰ�8�ֽڽ��ж���ģ�129~1024��16�ֽڶ��롣
		else if (bytes <= 8 * 1024)
			return _Index(bytes - 1024, 7) + group_freelist[1];//����Ķ��Ǹ�����һ������ 
		else if (bytes <= 64 * 1024)
			return _Index(bytes - 8 * 1024, 10) + group_freelist[2];
		else if (bytes <= 256 * 1024)
			return _Index(bytes - 64 * 1024, 13) + group_freelist[3];
		else
			assert(false);
		return -1;
	}

	//����������±������������������ڴ�顣
	static inline size_t Bytes(size_t index)
	{
		static size_t group[4] = { 16, 56, 56, 56 };//ÿ���ֽڵĶ������ж��ٸ�Ͱ��8�ֽ�16��Ͱ�������ֽ�������56��Ͱ��
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

	// һ��ThreadCacheӦ����CentralCache����Ķ���ĸ���(���ݶ���Ĵ�С����)
	static size_t SizeToMaxBatchNum(size_t size)//size���ڴ���С��
	{
		assert(size > 0);
		// [2, 512],һ�������ƶ����ٸ������(������)����ֵ
		// �����ڴ���СԽС�������������Խ�ࡣ
		// �����ڴ���СԽ�������������Խ�١�
		int num = MAX_BYTES / size;//ͨ���ڴ�ع��������ڴ���С����һ���������������
		if (num < 2)//��������2��
			num = 2;
		if (num > 512)//�������512����
			num = 512;
		return num;
	}

	//PageCache����������µ�Spanʱ����Ҫ����Span��ҳ��(����ҳ��������SystemAlloc��������)
	//SizeToPage�����Ǹ��������С���ڴ���ֽ��������� ���뼸ҳ��Span�ȽϺ��� 
	//�������ܸ���:���ݶ���Ĵ�С����Ӧ��Ҫ��Span�Ǽ�ҳ��
	static size_t SizeToPage(size_t size)
	{
		size_t batchNum = SizeToMaxBatchNum(size);//����������ٿ�
		size_t npage = batchNum * size;//�����ܹ���Ҫ���ڴ��С��
		npage >>= PAGE_SHIFT;//ת����ҳ��

		if (npage == 0)//��������1ҳ
			npage = 1;
		return npage;
	}

};

struct Span	//��freelist�����ڴ���Ӧ�ģ�Span�ǹ�������ҳ�Ľṹ��Span����spanlist�еĽڵ㡣
{
	PAGE_ID _pageID = 0;//����ڴ���ʼҳ��ҳ��(��һ�����̵ĵ�ַ�ռ���ҳΪ��λ����,����һҳ��8K=2^13)
	size_t _n = 0;		//ҳ������
	size_t _useCount = 0;//���кõ�С���ڴ�ָ�threadcache��_useCount��¼�ֳ�ȥ�˶��ٸ�С���ڴ�

	bool _isUsed = false;

	size_t _ObjectSize = 0;	//�洢��ǰ��Span�����з���Ķ���Ĵ�С�����������Ƕ�Ӧ�˶����ڴ�飬�����гɵ��ڴ��Ĵ�С��

	Span* _next = nullptr;
	Span* _prev = nullptr;

	void* _freelist = nullptr;//����ڴ��г�С�鲢����������������threadcacheҪ��ʱ��ֱ�Ӹ���С��ģ����յ�ʱ��Ҳ�������
							  //�������ͷ�ڵ㣬����FreeList���͵���������ʵ��������������֮��������ֻ������������ṹ��ֱ��ʹ��void*�����ڴ�顣
};

//��ͷ�ڵ��˫��ѭ���������ڴ洢����Span��
class SpanList {
public:

	SpanList() {
		_head = new Span;
		_head->_next = _head;
		_head->_prev = _head;
	}

	void PushFront(Span *_span) {//ͷ�巨��������˫��ѭ����������Ҫ�޸Ĳ���ڵ��ǰ�úͺ��ýڵ㡣
		
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

	Span* Begin() {//����ͷ�ڵ�֮��ĵ�һ���ڵ㡣
		return _head->_next;
	}

	Span* End() {
		return _head;//��Ϊ��˫��ѭ����������_head->_prev�������һ���ڵ㣬����End����_head��
	}

	bool IsEmpty() {
		return _head->_next == _head;
	}

	void Lock() { _mtx.lock(); }//����
	void UnLock() { _mtx.unlock(); }//����

private:
	Span *_head = nullptr;
	std::mutex _mtx;
};