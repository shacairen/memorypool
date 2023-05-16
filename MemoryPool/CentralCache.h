#pragma once
#include"Assistance.h"

//�趨Ϊ����ģʽ����ֹ��δ�����
class CentralCache
{
public:
	static CentralCache* GetInstance()
	{
		return &_sInst;
	}

	//��ȡһ���ǿյ�Span
	Span* GetOneSpan(SpanList& list, size_t size);

	//��CentralCache��ȡһ�������Ķ����ThreadCache
	size_t GiveObjToThread(void*& start, void*& end, size_t batchNum, size_t size);

	//����start��ʼ��freelist�е�����С���ڴ涼�黹��spanlists[index]�еĸ��Ե�span
	void ObjBackToSpan(void* start, size_t size);

private:
	CentralCache() {}
	CentralCache(const CentralCache&) = delete;
	CentralCache& operator=(const CentralCache&) = delete;

	static CentralCache _sInst;	
private:
	SpanList _spanlists[FreeListsN];	//��ϣͰ�ṹ
};
