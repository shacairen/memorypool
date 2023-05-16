#include"ThreadCache.h"
#include"CentralCache.h"

//��CentralCache�л�ȡ�ڴ�顣
void* ThreadCache::FetchFromCentralCache(size_t index, size_t size)
{
	size_t batchNum = min(_freelists[index].GetMaxSize(), SizeClass::SizeToMaxBatchNum(size));//���ƴ��ռ��С��ռ������
	if (_freelists[index].GetMaxSize() == batchNum)//����Ѿ��ﵽ�˵�ǰһ������������������ô�����������ֵ������������ʼ�㷨��
	{
		_freelists[index].GetMaxSize() += 1;
	}

	void* start = nullptr;
	void* end = nullptr;
	//����ΪʲôҪ����ʵ�����뵽�Ŀռ���������Ϊ���ǿ�������Ŀռ������ǰSpan�в�������ô����ֻ�ܷ���ʵ�������˶��٣�����������Ĳ���Ҳû�¡�
	//��Ϊ����ֻҪ���뵽��һ�飬��ô�Ϳ��Խ��е�ǰAllocate�ķ��䣬Ȼ���´��ٽ��з����ʱ�������뼴�ɡ�
	size_t actualNum = CentralCache::GetInstance()->GiveObjToThread(start, end, batchNum, size);//����ʵ�����뵽�Ŀռ�������

	if (actualNum == 1)//���ֻ���뵽��һ���ڴ�顣
	{
		assert(start == end);//����һ���Ƿ�start==end����Ϊֻ��һ�顣 
	}
	else
	{
		//����ڴ�����1�飬��ô����ʵ����Ҳֻ��Ҫ����start��ָ����ڴ�鼴�ɣ���Ϊһ��ʹ�õ�ֻ����һ���ڴ�顣
		//�������ǰ�start����һ���ڵ㵽end�ڵ��������ֱ�ӷŵ�freelist��Ӧ�����ϣ�
		_freelists[index].PushRange(Next(start), end, actualNum - 1);
	}
	return start;//ʵ��������ֻ��Ҫʹ�õ����startָ����ڴ�飬�����ڴ����Ϊ���������㷨���Ч�ʡ�
}

//��ThreadCache����ռ�
void* ThreadCache::Allocate(size_t size)
{
	assert(size <= MAX_BYTES);
	size_t alignNum = SizeClass::RoundUp(size);	//�������ֽ���(���뵽ÿ�����Ĵ�СΪalignNum������������)��Ҫ����ȡ����
	size_t index = SizeClass::Index(size);		//�����ǵڼ�����������(�����±�)

	if (!_freelists[index].IsEmpty())//�����Ͱ���п��п�
	{
		return _freelists[index].Pop();	//��freelist�϶�Ӧ��Ͱ�з���һ��ռ�
	}
	else//��û�п��п飬��ҪȥCentralCache�����롣
	{
		return FetchFromCentralCache(index, alignNum);	//��CentralCache������һ��ռ䲢����
	}
}

void ThreadCache::Deallocate(void* ptr, size_t size)
{
	assert(ptr);
	assert(size <= MAX_BYTES);

	size_t index = SizeClass::Index(size);	//�����Ӧ�ò嵽�ĸ�����������
	_freelists[index].Push(ptr);	//��ptr���ռ�ͷ�嵽��Ӧ������������

	//����������������ŵ�С���ڴ������ >= һ�����������С���ڴ������ʱ,
	//���ǽ�Size()��С��С���ڴ�ȫ�����ظ�CentralCache��Span��
	//�����ԭ����ListTooLong�����н���ע��
	if (_freelists[index].GetSize() >= _freelists[index].GetMaxSize())//MaxSize�Ǹ����Ŀ��п����������������ı�׼��
	{
		BackToCentral(_freelists[index], size);
	}
}

void ThreadCache::BackToCentral(FreeList& freelist, size_t size)
{
	void* start = nullptr;//��������һ�����������ʼ�ڵ㡣
	void* end = nullptr;//��������һ��������Ľ�β�ڵ㡣

	freelist.PopAll(start, end);//�Ѵ�start�ڵ㵽end�ڵ����ȫ�����������ʵ�����뺯������ͬ�����Ǿ��ǰѸ�freelist��ȫ�������
								//����ʹ���������ʵ�����кܶ�����������������������ʱ���ʹ�ã�Ҳ�п�����ThreadCacheҪ��������ʱʹ�á�
	CentralCache::GetInstance()->ObjBackToSpan(start, size);
}

//������������ThreadCacheʹ�ú�Ҫ����ʱ����Ҫ��ThreadCache���ж��ֲ�����
//����Ҫ������ThreadCache�����е�Ͱ���ڴ�ȫ���ͷŵ���������ͷ��ǽ���Щ�ڴ�黹��CentralCache��������ֱ��delete������ϵͳ��
//��Ҫע���һ���ǣ�����ÿ���̶߳�������һ��static������Ҳ�����߳�˽���ڴ棬����ڴ�Ҫ�����ֶ��ͷţ���Ȼ������ڴ�й¶��
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
