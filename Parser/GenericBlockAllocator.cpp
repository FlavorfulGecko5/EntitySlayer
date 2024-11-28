#ifdef _DEBUG
#include "GenericBlockAllocator.h"
#include <cassert>

void BlockAllocatorUnitTest()
{
	{
		BlockAllocator<int> alloc(100);
		const std::vector<BlockAllocator<int>::FreeBlock>& blocks = alloc.GetBlocks();

		int* nums = alloc.reserveBlock(20);
		assert(blocks.size() == 0);

		// Test Simple Free Block System
		alloc.freeBlock(nums + 10, 5);
		assert(blocks.size() == 1);
		assert(blocks[0].length() == 5);
		assert(blocks[0].start == nums + 10);

		alloc.freeBlock(nums + 5, 2);
		assert(blocks.size() == 2);
		assert(blocks[0].length() == 2);

		alloc.freeBlock(nums + 8, 1);
		assert(blocks.size() == 3);
		assert(blocks[1].length() == 1);

		alloc.freeBlock(nums + 9, 1);
		assert(blocks.size() == 2);
		assert(blocks[1].length() == 7);

		alloc.freeBlock(nums + 15, 2);
		assert(blocks[1].length() == 9);

		alloc.freeBlock(nums, 5);
		assert(blocks.size() == 2);
		assert(blocks[0].length() == 7);

		// Reserve more than what remains in the buffer
		int* moreNums = alloc.reserveBlock(81);
		assert(blocks.size() == 3);
		assert(blocks[2].length() == 80);

		// Reserve rest of buffer to start reserving from free blocks
		int* consumeBuffer = alloc.reserveBlock(19);
		int* a = alloc.reserveBlock(1);
		assert(blocks[0].length() == 6);

		// Reserve more than the first free block can give, forcing allocation from block 2
		int* b = alloc.reserveBlock(8);
		assert(blocks[1].length() == 1);

		// Reserve the entirety of block 1
		int* c = alloc.reserveBlock(6);
		assert(blocks.size() == 2);
		assert(blocks[0].length() == 1);
	}
	{
		BlockAllocator<int> alloc(100);
		const std::vector<BlockAllocator<int>::FreeBlock>& blocks = alloc.GetBlocks();

		int* nums = alloc.reserveBlock(100);
		assert(blocks.size() == 0);

		// Ensure binary insertion is working properly
		alloc.freeBlock(nums + 50, 1);
		alloc.freeBlock(nums + 25, 1);
		alloc.freeBlock(nums + 75, 1);
		alloc.freeBlock(nums + 40, 1);
		alloc.freeBlock(nums + 80, 1);
		alloc.freeBlock(nums + 30, 1);
		alloc.freeBlock(nums, 1);
		alloc.freeBlock(nums + 65, 1);
		alloc.freeBlock(nums + 67, 1);
		alloc.freeBlock(nums + 23, 1);

		assert(blocks.size() == 10);

		int* last = nullptr;
		for (BlockAllocator<int>::FreeBlock f : blocks) {
			assert(f.start > last);
			assert(f.start < f.end);
			last = f.end;
		}
	}
}

#endif