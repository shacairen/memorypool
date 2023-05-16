#include"PageCache.h"

PageCache PageCache::_sInst;	//��̬��Ա���ⶨ��

/*
��������Ҫ��ȷ��PageCache����Span��ʱ��������������������֡�
��һ��:���������Span����PageCache�Ķ�Ӧ��Ͱ�У������й��ص�Span�飬��ô���ǿ���ֱ��ʹ���������з��䡣
�ڶ���:���������Span����PageCache�Ķ�Ӧ��Ͱ��û��Span�飬��ô��ʱ������Ҫ���ľ�������Ѱ�ҡ�����Ҫ�ҵ���ǰͰ֮���Ͱ��û�п���ʹ�õ�Span��
�ҵ�֮������Ҫ���ľ��ǽ�Span���зֳ�����Span��һ������������Ĵ�С����һ����ʣ��Ĵ�С��Ȼ��һ�����ڷ��䣬��һ�����ڶ�ӦͰ�С�
���������:����������PageCache������Ͱ�У����Ҳ������ʵ�Span�飬��ô���Ǵ�ʱ����Ҫ�����ϵͳ����һ������Span��Ȼ������Span�ֳ����顣
һ�����ڷ��䣬��һ����ڶ�Ӧ��Ͱ�С�
*/
Span* PageCache::NewSpan(size_t page)
{
	//����:�������ֵΪʲô��ϣ���ﻹҪ���д洢��
	//����MAX_PAGES-1��ֱ��������뼴�ɣ������ڴ�ؽ��й���
	if (page > MAX_PAGES - 1)
	{
		void* ptr = SystemAlloc(page);
		Span* newSpan = _spanPool.New();
		newSpan->_pageID = ((PAGE_ID)ptr >> PAGE_SHIFT);
		newSpan->_n = page;
		newSpan->_ObjectSize = (page << PAGE_SHIFT);	//��֪Span���������Ĵ�С(��������Ƕ�����)
		//_idSpanMap[newSpan->_pageID]= newSpan;//����Ӧ��ҳ�Ŵ洢�ڹ�ϣ���С�
		_IDtoSpan.set(newSpan->_pageID, newSpan);
		return newSpan;
	}

	//�鿴��ǰλ����û�к��ʵ�Span���������ֱ�ӷ��ء���Ӧ��һ�������
	if (!_spanlists[page].IsEmpty())
	{
		Span* span = _spanlists[page].PopFront();

		//��ÿһҳ��Ҫ���뵽��ϣ���У�Ϊ��֮�����ҳ�Ų��Ҷ�Ӧ��Span��
		for (PAGE_ID i = span->_pageID; i < span->_pageID + span->_n; ++i)
		{
			//_idSpanMap[i] = span;
			_IDtoSpan.set(i, span);
		}
		//
		span->_isUsed = true;

		return span;
	}

	//������֮���ÿһ��SpanList��Ѱ��һ�������Span��Ҳ���Ƕ�Ӧ�ڶ��������
	for (int i = page + 1; i < MAX_PAGES; ++i)
	{
		//�ҵ���Span�������Span�Ǹ����Span����������Ҫ������г�����С�ġ�
		if (!_spanlists[i].IsEmpty())
		{
			Span* BigSpan = _spanlists[i].PopFront();
			Span* SmallSpan = _spanPool.New();//���ڷ����ȥ��Span��

			SmallSpan->_n = page;//�趨ҳ��
			SmallSpan->_pageID = BigSpan->_pageID;//������ʼҳ��
			BigSpan->_n -= page;//�ָ�֮��ʣ�µĲ��ֵ�ҳ��
			
			BigSpan->_pageID += page;//�ָ�֮��ʣ�µĲ��ֵ���ʼҳ��Ҫ��ԭ������ʼҳ��+�ָ���ȥ��ҳ����

			SmallSpan->_ObjectSize = (page << PAGE_SHIFT);	//��֪Span���������Ĵ�С(��������Ƕ�����)


			//ҲҪ��BigSpan����ҳ��βҳ�浽ӳ����У������ڿ��е�Span���кϲ�����ʱ�ܹ�ͨ��ӳ����ҵ�BigSpan�����кϲ�
			//�������һ��Ϊʲô����Ҫ�洢ÿһҳ����Ϊ�����ڷ����ȥ��ʱ�����Ҫ֪��ÿһҳ��Span��ӳ�䣬����PageCache�㲻���漰��ÿһҳ��
			//_idSpanMap[BigSpan->_pageID] = BigSpan;
			//_idSpanMap[BigSpan->_pageID + BigSpan->_n - 1] = BigSpan;	
			_IDtoSpan.set(BigSpan->_pageID, BigSpan);
			_IDtoSpan.set(BigSpan->_pageID + BigSpan->_n - 1, BigSpan);

			_spanlists[BigSpan->_n].PushFront(BigSpan);

			//���淵�ظ�CentralCacheʱ��SmallSpan��ÿһҳ��SmallSpan��ӳ���ϵ, ��������SmallSpan�зֳ�С���ڴ��Ȼ��С���ڴ�ͨ��ӳ���ϵ�һ�SmallSpan
			for (PAGE_ID i = SmallSpan->_pageID; i < SmallSpan->_pageID + SmallSpan->_n; ++i)
			{
				//_idSpanMap[i] = SmallSpan;
				_IDtoSpan.set(i, SmallSpan);
			}

			SmallSpan->_isUsed = true;
			return SmallSpan;
		}
	}

	//�˴�Ϊ��������������������������������SpanList��û�п���ʹ�õ�Span����ô���Ǿ�Ҫ��ϵͳ����һ��ҳ��ΪMAX_PAGES-1��Span���п顣
	//������п�����֮������ŵ����һ��Ͱ�У�Ȼ������return NewSpan(page)��Ҳ���ǵݹ����һ��NewSpan��Ȼ������һ�ε����оͿ���ʹ�õڶ��������
	Span* newSpan = _spanPool.New();

	void* ptr = SystemAlloc(MAX_PAGES - 1);	//��ϵͳ����Ŀռ��ǽ���newSpan�е�_pageID��_n���й����
	newSpan->_n = MAX_PAGES - 1;
	newSpan->_pageID = ((PAGE_ID)ptr >> PAGE_SHIFT);
	_spanlists[MAX_PAGES - 1].PushFront(newSpan);
	return NewSpan(page);

}

//��PAGE_IDӳ�䵽һ��Span*��, ��������ͨ��ҳ��ֱ���ҵ���Ӧ��Span*��λ��
//����˵���ǲ�����ҳ�ţ�����ֵ��Span��
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

//��û�б�ʹ�õ�Span��PageCache��ϳɸ����Span��
void PageCache::BackToPageCache(Span* span)
{
	//�������ֵ˵������ڴ���ֱ���������ģ��ڴ�ز�������ֱ���ö��ͷš�
	if (span->_n > MAX_PAGES - 1)
	{
		void* ptr = (void*)(span->_pageID << PAGE_SHIFT);
		SystemFree(ptr, span->_ObjectSize);	//VirtualFree
		return;
	}

	//��ǰ���ң��Ƿ��п��Խ��кϲ���Span��
	//ΪʲôҪ��while(1)����Ϊ���������ܶ�ҳ���ڲ�ͬ��Span��Ҫ�����Ƕ�����һ��
	while (1)
	{
		PAGE_ID prevID = span->_pageID - 1;	//��ǰSpan����ʼҳ��-1����֮ǰ��ҳ��
		Span* prevSpan = PAGEIDtoSpan(prevID);//ͨ��ӳ���ϵ���ҵ�ǰ��һ��ҳ���ڵ�Span����
		if (prevSpan == nullptr)	//�ж�ǰ���Span�Ƿ������(���ܻ���ϵͳ��)[�����ӳ����У���֤���������]
			break;
		if (prevSpan->_isUsed == true)	//ǰ���Span���ڱ�CentralCacheʹ��(������ڱ�ʹ�ã���ô�Ͳ����Խ��кϲ�)
			break;
		if (prevSpan->_n + span->_n > MAX_PAGES - 1)//�ϲ���Ĵ�С>128ʱ, ���ܺϲ�,��Ϊ�ڴ�ز�������������С���ڴ�顣
			break;
		//��ʼ�ϲ�
		span->_pageID = prevSpan->_pageID;//�ϲ������ʼҳ�ž���prevSpan����ʼҳ���ˡ�
		span->_n += prevSpan->_n;//ҳ����ӡ�

		_spanlists[prevSpan->_n].Erase(prevSpan);//�ϲ�����Ҫɾ����
		_spanPool.Delete(prevSpan);
		prevSpan = nullptr;
	}

	//�����ң�����ǰ����һ����
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
		//��ʼ�ϲ�
		span->_n += nextSpan->_n;

		_spanlists[nextSpan->_n].Erase(nextSpan);
		//delete nextSpan;
		_spanPool.Delete(nextSpan);
		nextSpan = nullptr;
	}

	span->_isUsed = false;
	_spanlists[span->_n].PushFront(span);	//���ϲ����Span���뵽spanlist����
	//_idSpanMap[span->_pageID] = span;//��ϣ��洢��һҳ�����һҳ��
	//_idSpanMap[span->_pageID + span->_n - 1] = span;
	_IDtoSpan.set(span->_pageID, span);
	_IDtoSpan.set(span->_pageID + span->_n - 1, span);
	
}

