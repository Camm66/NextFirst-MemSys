//-----------------------------------------------------------------------------
// Copyright Ed Keenan 2019
// Optimized C++
//----------------------------------------------------------------------------- 

#ifndef MEM_H
#define MEM_H

#include "Heap.h"

class Mem
{
public:
	static const unsigned int HEAP_SIZE = (50 * 1024);

public:
	Mem();	
	Mem(const Mem &) = delete;
	Mem & operator = (const Mem &) = delete;
	~Mem();

	Heap *getHeap();
	void dump();

	// implement these functions
	void free( void * const data );
	void *malloc( const uint32_t size );
	void initialize( );


private:
	// Helper functions

	Free* findFreeBlock(const uint32_t size) const;
	Free* addFreeBlock(Free *pFree);
	Free* splitFreeBlock(Free *pFree, uint32_t blockSize);
	Used* allocateFreeBlock(Free* pFree) const;

	Free* mergeBlocks(Free* pHead, Free* pNew) const;
	Free* dontMergeFreeBlock(Free* pFree, Free* pHead);

	void addUsedToFront(Used* pUsed) const;

	void removeFreeBlock(const Free* pFree) const;
	void removeUsedBlock(const Used* pUsed) const;

	void removeFreeAdjustStats(const Free* pFree) const;
	void removeUsedAdjustStats(const Used* pUsed) const;
	void addUsedAdjustStats(const Used* pUsed) const;

	uint32_t getFreeHeaderSize(const Free* pFree) const;
	void setFreeAboveFlag(Used* hdr);

	void* getBlockPtr(const Used* pUsed) const;
	void addSecretPtr(const Free* pFree);

	Heap	*pHeap;
	void	*pRawMem;
};



#endif 

// ---  End of File ---------------
