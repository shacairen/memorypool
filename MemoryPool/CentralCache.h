#pragma once
#include"Assistance.h"

//设定为单例模式，防止多次创建。
class CentralCache
{
public:
	static CentralCache* GetInstance()
	{
		return &_sInst;
	}

	//获取一个非空的Span
	Span* GetOneSpan(SpanList& list, size_t size);

	//从CentralCache获取一定数量的对象给ThreadCache
	size_t GiveObjToThread(void*& start, void*& end, size_t batchNum, size_t size);

	//将从start起始的freelist中的所有小块内存都归还给spanlists[index]中的各自的span
	void ObjBackToSpan(void* start, size_t size);

private:
	CentralCache() {}
	CentralCache(const CentralCache&) = delete;
	CentralCache& operator=(const CentralCache&) = delete;

	static CentralCache _sInst;	
private:
	SpanList _spanlists[FreeListsN];	//哈希桶结构
};
