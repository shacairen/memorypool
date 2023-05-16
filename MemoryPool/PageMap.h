#pragma once
#include"Assistance.h"
#include "ObjectPool.h"

const static int BITSNUM = 32 - PAGE_SHIFT;

//一层基数树
//程序运行期间不会进行结构上的改变
template <int BITS>
class TCMalloc_PageMap1 {
private:
	static const int LENGTH = 1 << BITS;
	void** array_;

public:
	typedef uintptr_t Number;//可以表达指针的整型

	//explicit TCMalloc_PageMap1(void* (*allocator)(size_t)) {
	explicit TCMalloc_PageMap1() {
		//array_ = reinterpret_cast<void**>((*allocator)(sizeof(void*) << BITS));
		size_t size = sizeof(void*) << BITS;
		size_t alignSize = SizeClass::_RoundUp(size, 1 << PAGE_SHIFT);
		array_ = (void**)SystemAlloc(alignSize >> PAGE_SHIFT);
		memset(array_, 0, sizeof(void*) << BITS);
	}

	// Return the current value for KEY.  Returns NULL if not yet set,
	// or if k is out of range.
	void* get(Number k) const {
		if ((k >> BITS) > 0) {
			return NULL;
		}
		return array_[k];
	}

	// REQUIRES "k" is in range "[0,2^BITS-1]".
	// REQUIRES "k" has been ensured before.
	//
	// Sets the value 'v' for key 'k'.
	void set(Number k, void* v) {
		array_[k] = v;
	}
};

