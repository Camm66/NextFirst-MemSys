//-----------------------------------------------------------------------------
// Copyright Ed Keenan 2019
// Optimized C++
//----------------------------------------------------------------------------- 

#include "Framework.h"

#include "Free.h"
#include "Used.h"

Used::Used(const uint32_t blockSize)
	:pUsedNext(nullptr),
	pUsedPrev(nullptr),
	mBlockSize(blockSize),
	mType(Block::Used),
	mAboveBlockFree(false),
	pad(0)
{
}

// ---  End of File ---------------
