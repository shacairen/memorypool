#pragma once
#include"Assistance.h"
#include"ObjectPool.h"
#include<unordered_map>
#include"PageMap.h"

//����Ϊ����ģʽ����ֹ��δ�����
class PageCache
{
public:
	static PageCache* GetInstance()
	{
		return &_sInst;
	}

	//�������һ��"ҳ��"��СΪ"NPAGES - 1"���µ�Span
	Span* NewSpan(size_t page);

	//��PAGE_IDӳ�䵽һ��Span*��, ��������ͨ��ҳ��ֱ���ҵ���Ӧ��Span*��λ��
	Span* PAGEIDtoSpan(PAGE_ID id);

	//��useCount��Ϊ0��Span���ظ�PageCache���������ϲ��ɸ����Span
	void BackToPageCache(Span* span);

	void PageLock() { _pageMutex.lock(); }
	void PageUnLock() { _pageMutex.unlock(); }
private:
	PageCache() {}
	PageCache(const PageCache&) = delete;
	PageCache& operator=(const PageCache&) = delete;

	static PageCache _sInst;
	std::recursive_mutex _pageMutex;	//һ�Ѵ�����һ������PageCache��Ҫ����

private:
	SpanList _spanlists[MAX_PAGES];	//��ҳ��Ϊӳ��Ĺ���(ֱ�Ӷ�ַ��)
	TCMalloc_PageMap1<BITSNUM> _IDtoSpan;
	//std::unordered_map<PAGE_ID,Span*> _idSpanMap;//ʹ�ù�ϣ��洢ҳ�ź�span�Ķ�Ӧ��ϵ��
	ObjectPool<Span> _spanPool;
};
