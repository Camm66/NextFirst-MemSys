//-----------------------------------------------------------------------------
// Copyright Ed Keenan 2019
// Optimized C++
//----------------------------------------------------------------------------- 

#include "Framework.h"

#include "Used.h"
#include "Free.h"
#include "Block.h"

Free::Free(uint32_t BlockSize)
	:pFreeNext(nullptr),
	pFreePrev(nullptr),
	mBlockSize(BlockSize),
	mType(Block::Free),
	mAboveBlockFree(false),
	pad(0)
{}


// ---  End of File ---------------
