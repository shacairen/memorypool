#include"CentralCache.h"
#include"PageCache.h"

//��̬��Ա������������г�ʼ����
CentralCache CentralCache::_sInst;

//��ȡһ���ǿյ�Span�����Span������1��freelist�Ŀ��п顣
Span* CentralCache::GetOneSpan(SpanList& slist, size_t size) 
{
	Span* it = slist.Begin();
	while (it != slist.End())
	{
		if (it->_freelist != nullptr)
		{
			return it;	//�ҵ��˷ǿյ�span��ֱ�ӷ��ء���һ�ֽⷨ���������ҵ��㹻���п��Span�ٷ��أ�����ʵ��������Ҳ�����һЩ���⡣
		}
		it = it->_next;
	}

	slist.UnLock();
	//û�ҵ��ǿյ�span, ��PageCache��Ҫ.�����漰����CentralCache��PageCache��ҪSpan��
	PageCache::GetInstance()->PageLock();	//���������õ���PageCache��PageCache��Ҫȫ�̼����ġ�
	Span* newSpan = PageCache::GetInstance()->NewSpan(SizeClass::SizeToPage(size));	
	PageCache::GetInstance()->PageUnLock();//ʹ�ú�Ҫ�ǵý�����

	newSpan->_ObjectSize = size;

	//��newSpan�зֳ�С���ڴ�, ������ЩС���ڴ�ŵ�newSpan�ĳ�Ա_freelist�£�ʹ��Next(obj)�����������ӣ��������ʵ���Ͼ�����һ��nextָ������á�
	//�ȼ��������ڴ����ʼ��ַ(ҳ��*ÿҳ�Ĵ�С)����ֹ��ַ(ҳ��*ÿҳ�Ĵ�С+��ʼ��ַ)
	char* start = (char*)(newSpan->_pageID << PAGE_SHIFT);
	char* end = start + (newSpan->_n << PAGE_SHIFT);

	//�����зֲ���ʵ���Ͼ���Ҫ��ÿ�����п�֮��ʹ��Next�����������ӣ�����������freelistָ��start����Ϊͷָ�롣
	//Ȼ������ÿ����start����size���ֽڣ�Ҳ����ָ������һ�����п�ĵ�һ���ֽڡ�ֱ��start����end����ô������Ҳ������ˡ�
	newSpan->_freelist = (void*)start;
	void* ptr = start;
	start += size;
	Next(ptr) = start;

	int j = 1;
	//��ʼ�з�
	while (start < end)
	{
		++j;
		Next(ptr) = start;
		start += size;//�൱��ÿ��size��Сstart����������һ�����п顣
		if (start >= end) {
			Next(ptr) = nullptr;
			break;
		}
		ptr = Next(ptr);
	}

	Next(ptr) = nullptr;

	slist.Lock();//����һ��Ҫ�ǵü�������Ϊʹ����SpanList���������Ĳ����ڳ���֮��
	slist.PushFront(newSpan);	//���µ�newSpan���뵽slist��
	return newSpan;

}

//���������ThreadCache����CentralCache������һ�������Ŀ��п��ThreadCache�㡣
//�����е�start��end����Ϊ���ã���Ϊ�����ڷ��غ�Ҫֱ��ʹ��start��end�������صĿ��п�����
//�����е�batchNum����������Ŀ��п飬���ǲ�һ������ķ�����ô�࣬size�ǿ��п��С������size�ҵ�SpanList��
//�����ķ���ֵ��ʵ�������Ǹ�ThreadCache��Ŀ��п����������������Ҫ��������Ϊ��ǰSpan��һ�����㹻�Ŀ��п飬���������ܱ�֤��һ�顣
size_t CentralCache::GiveObjToThread(void*& start, void*& end, size_t batchNum, size_t size)
{
	size_t index = SizeClass::Index(size);	//ȷ���ĸ�Ͱ
	_spanlists[index].Lock();	//SpanList��ÿ������ʹ�õ�ʱ��Ҫ��������Ϊ�ǹ�����Դ����ֹ���߳����⡣

	Span* span = GetOneSpan(_spanlists[index], size);//��ȡһ��Span��
	assert(span && span->_freelist);

	start = span->_freelist;//ָ��ͷ
	end = start;//ָ�����յ�β
	size_t actualNum = 1;//��ΪGetOneSpan������ѡ���Span��freelist��Ϊ�գ�����Ԥ��Ϊ1��
	while (--batchNum && Next(end) != nullptr)	//���������Ҫô���ǿ��п������������ȥ��Ҫô���ǿ��п鲻����������Ҳ���з��䡣
	{//��Ϊ��--batchNum������end����ƶ���batchNum-1�Ρ�
		++actualNum;
		end = Next(end);
	}
	span->_freelist = Next(end);//Span��freelist��ͷ�ڵ�����Ϊend����һ���ڵ㡣
	Next(end) = nullptr;
	
	span->_useCount += actualNum;

	_spanlists[index].UnLock();
	return actualNum;

}

//����startΪͷ�ڵ��freelist�е����п��п鶼�黹����Ӧ��Spanlist��
void CentralCache::ObjBackToSpan(void* start, size_t size)
{
	assert(start);
	size_t index = SizeClass::Index(size);
	_spanlists[index].Lock();

	while (start) {

		void* NextPos = Next(start);
		PAGE_ID id = ((PAGE_ID)start >> PAGE_SHIFT);//��ĳһҳ���е����е�ַ����ҳ�Ĵ�С�����ǵõ��Ľ�����ǵ�ǰҳ��ҳ��
		//PageCache::GetInstance()->PageLock();
		Span* ret = PageCache::GetInstance()->PAGEIDtoSpan(id);//ӳ���ϵ��PageCache��Span�ָ�CentralCacheʱ���д洢��ͨ��ҳ�Ŷ�λSpan��
		//PageCache::GetInstance()->PageUnLock();
		if (ret != nullptr)
		{
			Next(start) = ret->_freelist;//_freelist��ͷ�ڵ�
			ret->_freelist = start;//ͷ�巨��
		}
		else
		{
			assert(false);//���Ϊnullptr��Ҫ��ֹ��
		}

		--ret->_useCount;//Ҫ�ǵð�Span�ֳ�ȥ�Ŀ�����1.

		//����ʹ�õĲ����ǣ����һ��Span��ʹ�õĿ��п���Ϊ0����ô���ǾͰ����Span�黹��PageCache�㣬�����Ҫ�Ļ����½������롣
		if (ret->_useCount == 0)
		{
			_spanlists[index].Erase(ret);//����������
			ret->_freelist = nullptr;
			ret->_prev = nullptr;
			ret->_next = nullptr;

			_spanlists[index].UnLock();
			PageCache::GetInstance()->PageLock();//����PageCache�������
			PageCache::GetInstance()->BackToPageCache(ret);
			PageCache::GetInstance()->PageUnLock();
			_spanlists[index].Lock();
		}
		start = NextPos;
	}
	_spanlists[index].UnLock();
}