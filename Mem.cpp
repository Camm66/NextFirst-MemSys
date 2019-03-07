#include <malloc.h>
#include <new>

#include "Framework.h"

#include "Mem.h"
#include "Heap.h"
#include "Block.h"

#define STUB_PLEASE_REPLACE(x) (x)

// To help with coalescing... not required
struct SecretPtr
{
	Free *pFree;
};

void Mem::initialize()
{
	// Create a FreeHdr and place it on the Heap
	Heap *pHeapTmp = this->getHeap();

	// Free header start
	Free *pFreeStart = (Free *)(pHeapTmp + 1);

	// Free header End
	Free *pFreeEnd = pFreeStart + 1;

	// Free block size
	uint32_t blockSize = (uint32_t)pHeapTmp->mStats.heapBottomAddr - (uint32_t)pFreeEnd;

	// Create the free header.
	Free *pFree = placement_new(pFreeStart, Free, blockSize);

	// Add secret pointer
	this->addSecretPtr(pFree);

	// Connect Free header to the Heap
	pHeapTmp->pFreeHead = pFree;
	pHeapTmp->pNextFit = pFree;

	// Set up the heap stats
	Heap::Stats *pStats = &pHeapTmp->mStats;

	pStats->currFreeMem = blockSize;
	pStats->currNumFreeBlocks = 1;
}

void *Mem::malloc(const uint32_t size)
{
	// Find an allocation, is there a free block big enough?
	Free *pFree = findFreeBlock(size);

	// Init empty used block pointers
	void *pUsedBlock = nullptr;
	Used *pUsed = nullptr;

	// Yes? - allocate it
	if (pFree != nullptr) {
		if (pFree->mBlockSize == size) {
			// On exact hit
			this->removeFreeBlock(pFree); // Remove from free list
			// Update Next Fit pointer
			if (this->pHeap->pNextFit->pFreeNext != nullptr) {
				this->pHeap->pNextFit = this->pHeap->pNextFit->pFreeNext;
			}
			else {
				this->pHeap->pNextFit = this->pHeap->pFreeHead;
			}
			pUsed = allocateFreeBlock(pFree); // Allocate block
		}
		else {
			// On split
			Free *pSplit = this->splitFreeBlock(pFree, size); // Split current free block
			pUsed = allocateFreeBlock(pSplit); // Allocate split block
			// On a split, we update the next fit pointer inside the splitFree block function
		}
		// Get the Start of Used Block (after Used Header)
		pUsedBlock = this->getBlockPtr(pUsed);
		assert(pUsedBlock != nullptr);
	}
	else
	{
		// no free block big enough
	}
	return pUsedBlock;
}

Free* Mem::findFreeBlock(const uint32_t size) const
// Here we iterate through the initialized heap in the typical linked list fashion.
// If a block with a BlockSize >= the size passed in is found, the function returns 
// a pointer to that block, otherwise it returns a nullptr.
{
	Free *pFreeTmp = this->pHeap->pNextFit;
	// First check if the initial next fit is valid 
	if (pFreeTmp == nullptr || pFreeTmp->mBlockSize < size) {
		// Check if we need to reset to the pFreeHead
		if(pFreeTmp->pFreeNext == nullptr)
		{
			pFreeTmp = this->pHeap->pFreeHead;
		}
		else 
		{
			pFreeTmp = pFreeTmp->pFreeNext;
		}
		// Iterate through the remaining Free list
		while (pFreeTmp != this->pHeap->pNextFit) {
			if (pFreeTmp->mBlockSize >= size) {
				break;
			}
			if (pFreeTmp->pFreeNext == nullptr) {
				pFreeTmp = this->pHeap->pFreeHead;
			}
			else {
				pFreeTmp = pFreeTmp->pFreeNext;
			}
		}
	}
	return pFreeTmp;
}

Free* Mem::splitFreeBlock(Free *pFree, uint32_t blockSize)
// This method splits the current free heap by removing the free block
// that needs to be allocated. First we calculate the chunk of space required
// for the block to be allocated. Then we create two blocks free blocks, one
// that is to be allocated, and one that points to the remainder of the 
// free heap space.
{
	// Track position of block to be split
	Free* tmpNext = pFree->pFreeNext;
	Free* tmpPrev = pFree->pFreePrev;

	// Get block sizes for each new block and allocate
	uint32_t newBlock = blockSize + this->getFreeHeaderSize(pFree); // Size of the new block
	uint32_t diff = pFree->mBlockSize - newBlock; // Offset for the rest
	Free *pFreeTmp = (Free*)((char*)pFree + newBlock);
	Free *p1 = placement_new(pFree, Free, blockSize); // New block
	Free *p2 = placement_new(pFreeTmp, Free, diff); // Remaining free block

	// Update pointer for split Free block
	p2->pFreeNext = tmpNext;
	p2->pFreePrev = tmpPrev;
	if (tmpPrev != nullptr) {
		tmpPrev->pFreeNext = p2;
	}
	if (tmpNext != nullptr) {
		tmpNext->pFreePrev = p2;
	}

	// Check if heap freeHead pointer needs to be relocated
	if (this->pHeap->pFreeHead == p1) {
		this->pHeap->pFreeHead = p2;
	}

	// Update Next Fit pointer
	if (this->pHeap->pNextFit == p1) {
		this->pHeap->pNextFit = p2;
	}

	// Add secret pointer to split free block
	this->addSecretPtr(p2);

	// Update stats for extra free block
	this->pHeap->mStats.currFreeMem -= this->getFreeHeaderSize(pFree);; // Account for extra free header
	this->pHeap->mStats.currNumFreeBlocks += 1; // Account for extra free block

	return p1;
}

void Mem::removeFreeBlock(const Free *pFree) const
// Here we remove the Free block from the free list by updating the relevant
// pointers. Including pFreeHead, and the next/prev headers for the affected blocks.
{
	assert(pFree != nullptr);
	if (pFree->pFreeNext == nullptr && pFree->pFreePrev == nullptr) {
		this->pHeap->pFreeHead = nullptr;
	}
	else if (pFree->pFreeNext == nullptr && pFree->pFreePrev != nullptr) {
		pFree->pFreePrev->pFreeNext = nullptr;
	}
	else if (pFree->pFreeNext != nullptr && pFree->pFreePrev == nullptr) {
		pFree->pFreePrev->pFreeNext = pFree->pFreeNext;
	}
	else {
		pFree->pFreePrev->pFreeNext = pFree->pFreeNext;
		pFree->pFreeNext->pFreePrev = pFree->pFreePrev;
	}
}

Used* Mem::allocateFreeBlock(Free* pFree) const {
	// Update requisite stats for free block removal 
	this->removeFreeAdjustStats(pFree);

	// Convert free block to used block
	Used *pUsed = placement_new(pFree, Used, pFree->mBlockSize);

	// Add new used block to the front of used list
	this->addUsedToFront(pUsed);

	// Update requisite stats for used block addition
	this->addUsedAdjustStats(pUsed);

	return pUsed;
}

void Mem::addUsedToFront(Used* pUsed) const
// This method adds a new used block to the front of the Used Block list
{
	assert(pUsed != nullptr);

	// Get current head & set previous pointer
	Used *tmpUsed = this->pHeap->pUsedHead;
	if (tmpUsed != nullptr)
	{
		tmpUsed->pUsedPrev = pUsed;
	}
	// Set pointers on the new head
	pUsed->pUsedNext = tmpUsed;
	pUsed->pUsedPrev = nullptr;
	this->pHeap->pUsedHead = pUsed;
}

void Mem::removeFreeAdjustStats(const Free *pFree) const
// This method updates the relevant stats for a newly allocated free block
{
	assert(pFree != nullptr);
	this->pHeap->mStats.currFreeMem -= pFree->mBlockSize;
	this->pHeap->mStats.currNumFreeBlocks--;
}

void Mem::addUsedAdjustStats(const Used* pUsed) const
// This method updates the relevant stats for a newly converted Used block
{
	assert(pUsed != nullptr);
	this->pHeap->mStats.currUsedMem += pUsed->mBlockSize;
	this->pHeap->mStats.currNumUsedBlocks += 1;
	if (this->pHeap->mStats.peakUsedMemory < this->pHeap->mStats.currUsedMem) {
		this->pHeap->mStats.peakUsedMemory = this->pHeap->mStats.currUsedMem;
	}
	if (this->pHeap->mStats.peakNumUsed < this->pHeap->mStats.currNumUsedBlocks) {
		this->pHeap->mStats.peakNumUsed = this->pHeap->mStats.currNumUsedBlocks;
	}
}

void* Mem::getBlockPtr(const Used* pUsed) const
{
	assert(pUsed != nullptr);
	void *p = (void *)(pUsed + 1);
	return p;
}

void Mem::free(void * const data)
{
	assert(data != nullptr);
	// First, backtrack to the position of the Used Header
	Used *pUsed = (Used *)((uint32_t *)data - sizeof(this->pHeap->pUsedHead));

	bool aboveBlockFree = pUsed->mAboveBlockFree;
	
	// Remove used block from used list
	this->removeUsedBlock(pUsed);

	// Convert used block to a free block
	Free *pFree = placement_new(pUsed, Free, pUsed->mBlockSize);

	// Update requisite stats
	this->removeUsedAdjustStats(pUsed);

	// Add free block back to the free list
	pFree = this->addFreeBlock(pFree, aboveBlockFree);

	this->setFreeAboveFlag(pUsed);
	this->addSecretPtr(pFree);
}

void Mem::removeUsedBlock(const Used *pUsed) const
// This method removes a Used block from the Used Heap by updating
// the necessary pointers.
{
	assert(pUsed != nullptr);
	// Check if Used block is the only one on the Used list
	if (pUsed->pUsedNext == nullptr && pUsed->pUsedPrev == nullptr)
	{
		this->pHeap->pUsedHead = nullptr;
	}
	// Check if Used block is the first item on the Used list
	else if (pUsed->pUsedNext != nullptr && pUsed->pUsedPrev == nullptr)
	{
		this->pHeap->pUsedHead = pUsed->pUsedNext;
		pUsed->pUsedNext->pUsedPrev = nullptr;
	}
	// Check if Used block is the last item on the Used list
	else if (pUsed->pUsedNext == nullptr && pUsed->pUsedPrev != nullptr)
	{
		pUsed->pUsedPrev->pUsedNext = nullptr;
	}
	else {
		pUsed->pUsedNext->pUsedPrev = pUsed->pUsedPrev;
		pUsed->pUsedPrev->pUsedNext = pUsed->pUsedNext;
	}
}

void Mem::removeUsedAdjustStats(const Used *pUsed) const
// This method updates the relevant stats for a newly deallocated used block.
{
	assert(pUsed != nullptr);
	this->pHeap->mStats.currUsedMem -= pUsed->mBlockSize;
	this->pHeap->mStats.currFreeMem += pUsed->mBlockSize;
	this->pHeap->mStats.currNumUsedBlocks--;
	this->pHeap->mStats.currNumFreeBlocks++;
}

Free* Mem::addFreeBlock(Free *pFree, bool aboveBlockFree)
// This method is used to add a newly deallocated Free block back onto the free list.
// The relevant pointers are updated.
{
	assert(pFree != nullptr);

	// Check if this block consists of the whole free heap
	Free *pHead = this->pHeap->pFreeHead;
	if (pHead == nullptr)
	{
		this->pHeap->pFreeHead = pFree;
		this->pHeap->pNextFit = pFree;
	}
	else
	{
		// Coalesce
		// Caluculate next free block
		uint32_t diff = (pFree->mBlockSize + this->getFreeHeaderSize(pFree));
		Free *nextFreeBlock = (Free *)((char *)pFree + diff);

		// Calculate previous free block
		uint32_t *prevFreeBlock = (uint32_t *)((char *)pFree - (char*)4);
		Free * prevFreeHdr = (Free *)*prevFreeBlock;

		if (nextFreeBlock->mType == Block::Free || aboveBlockFree == true) {
			if (nextFreeBlock->mType == Block::Free)
			{
				pFree = this->mergeBlocks(nextFreeBlock, pFree);
			}
			if (aboveBlockFree == true) {
				pFree = this->mergeBlocks(prevFreeHdr, pFree);
			}
		}
		else
		{
			// If blocks are not adjacent, we only link them
			pFree = this->dontMergeFreeBlock(pFree);
		}
	}
	return pFree;
}

Free* Mem::dontMergeFreeBlock(Free* pFree) 
{
	Free* tmpHead = this->pHeap->pFreeHead;
	if (tmpHead > pFree)
	{
		this->pHeap->pFreeHead = pFree;
		pFree->pFreeNext = tmpHead;
		tmpHead->pFreePrev = pFree;
	}
	else
	{
		Free* currentBlock = tmpHead;
		while (currentBlock != nullptr) {
			if (currentBlock > pFree) {
				pFree->pFreeNext = currentBlock;
				pFree->pFreePrev = currentBlock->pFreePrev;
				currentBlock->pFreePrev->pFreeNext = pFree;
				currentBlock->pFreePrev = pFree;
				break;
			}
			else if (currentBlock->pFreeNext == nullptr) {
				currentBlock->pFreeNext = pFree;
				pFree->pFreePrev = currentBlock;
				break;
			}
			currentBlock = currentBlock->pFreeNext;
		}
	}
	return pFree;
}

Free* Mem::mergeBlocks(Free* pHead, Free* pNew) const
// This method serves as our coalesce function. It takes two pointers to free blocks
// and merges them into a single free block
{
	// Calculate new total block size
	uint32_t headerSize = this->getFreeHeaderSize(pNew);
	uint32_t totalBlockSize = pHead->mBlockSize + pNew->mBlockSize + headerSize;

	// Create the new Free block
	Free* pFree = nullptr;
	bool head = false;
	if (this->pHeap->pFreeHead == pHead)
	{
		head = true;
	}
	if (pNew < pHead) {
		pFree = placement_new(pNew, Free, totalBlockSize);
		pFree->pFreeNext = pHead->pFreeNext;
		pFree->pFreePrev = pHead->pFreePrev;
		if (pHead->pFreeNext != nullptr) {
			pHead->pFreeNext->pFreePrev = pFree;
		}
		if (pHead->pFreePrev != nullptr) {
			pHead->pFreePrev->pFreeNext = pFree;
		}
	}
	else {
		Free* tmpNext = pHead->pFreeNext;
		Free* tmpPrev = pHead->pFreePrev;
		pFree = placement_new(pHead, Free, totalBlockSize);
		pFree->pFreePrev = tmpPrev;
		if (tmpNext != pNew) {
			pFree->pFreeNext = tmpNext;
		}
	}
	// Update the Head pointers
	if (head == true){
		this->pHeap->pFreeHead = pFree;
	}

	// Update Next fit pointer if we merged to the next fit block
	if (this->pHeap->pNextFit == pNew || this->pHeap->pNextFit == pHead) {
		this->pHeap->pNextFit = pFree;
	}

	// Update relevant stats
	this->pHeap->mStats.currNumFreeBlocks--; // 2 blocks == 1 block
	this->pHeap->mStats.currFreeMem += headerSize; // Account for extraneous header

	return pFree;
}

uint32_t Mem::getFreeHeaderSize(const Free* pFree) const {
	uint32_t headersize = (uint32_t)((char*)(pFree + 1) - (char *)pFree);
	return headersize;
}

void Mem::addSecretPtr(const Free* pFree) {
	uint32_t blockSize = pFree->mBlockSize + this->getFreeHeaderSize(pFree);
	uint32_t* endBlock = (uint32_t *)((char *)pFree + blockSize - (char *)4);
	*endBlock = (uint32_t)pFree;
}

void Mem::setFreeAboveFlag(Used* hdr)
{
	uint32_t headerSize = this->getFreeHeaderSize((Free *)hdr);
	Used *nextPtr = (Used *)((char *)hdr + hdr->mBlockSize + headerSize);

	if (nextPtr != nullptr && nextPtr->mType == Block::Used) 
	{
		nextPtr->mAboveBlockFree = true;
	}
}
// ---  End of File ---------------
