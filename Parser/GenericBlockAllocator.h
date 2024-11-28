#include <vector>
#include <string>
#include <sstream>

#ifdef _DEBUG

void BlockAllocatorUnitTest();

#endif

/*
* Realistically speaking, this allocator has some flaws that require
* users to utilize it the correct way to prevent errors:
* 1. Does not check if the memory you free was actually provided by the allocator
* 2. Expects users to independently maintain the start and lengths of allocated blocks
* 
* Todo: Consider implementing a system that completely discards freeblocks of a small-enough size,
* to eliminate inefficiency from building up a large number of tiny free blocks?
*/
template <typename T>
class BlockAllocator
{
	public:
	struct FreeBlock {
		T* start;
		T* end;

		size_t length() const {
			return end - start;
		}
	};

	private:
	std::vector<FreeBlock> FreeBlocks; // Contains all free blocks

	std::vector<T*> allBuffers;    // Contains every buffer made by this allocator

	T* buffer = nullptr;    // Active buffer
	size_t max = 0;         // Capacity of the active buffer
	size_t used = 0;        // Number of used elements in the active buffer.
	size_t newBufferLength; // Default length of new buffers

	public:
	BlockAllocator() = delete;
	BlockAllocator(const BlockAllocator<T>& copyFrom) = delete;
	void operator=(const BlockAllocator<T>& copyFrom) = delete;

	BlockAllocator(const size_t p_newBufferLength) : newBufferLength(p_newBufferLength) {}

	~BlockAllocator() 
	{
		for(T* buff : allBuffers)
			delete[] buff; 
	}

	/*
	* For Debugging and Unit Testing
	*/
	const std::vector<FreeBlock>& GetBlocks() { return FreeBlocks; }

	std::string toString(bool includeBlockList)
	{
		std::ostringstream buffer;
		buffer << "Number of Buffers: " << allBuffers.size();
		buffer << "\nActive Buffer Status: " << used << " / " << max << " (used / max)";
		buffer << "\nNew Buffer Sizes: " << newBufferLength;
		buffer << "\nAvailable Free Blocks: " << FreeBlocks.size();

		if (includeBlockList)
		{
			buffer << "\n\nFree Block Log (Addr / Capacity):\n-----";
			for (FreeBlock f : FreeBlocks)
			{
				size_t length = f.end - f.start;
				buffer << '\n' << (void*)f.start << " / " << length;
			}
		}
		buffer << '\n';
		return buffer.str();
	}

	/* Defines a new buffer of a desired capacity as the active buffer */
	void setActiveBuffer(const size_t capacity)
	{
		if (used < max) // If we have unused EOB, make it a FreeBlock
			freeBlock(&buffer[used], max - used);

		buffer = new T[capacity];
		allBuffers.push_back(buffer);
		max = capacity;
		used = 0;
	}

	/* 
	 Reserves a block of memory for a specified number elements. 
	 This will return nullptr if the desired capacity is 0  
	*/
	T* reserveBlock(size_t capacity)
	{
		if(capacity == 0)
			return nullptr;
		T* block = nullptr;
		if (used + capacity > max) // Case 1: Current buffer not good enough
		{
			// Do we have a FreeBlock of sufficient size available? 
				// If yes, use that and return

			for (size_t i = 0, s = FreeBlocks.size(); i < s; i++) 
			{
				FreeBlock fb = FreeBlocks[i];
				size_t length = fb.end - fb.start;
				if (length > capacity)
				{
					block = fb.start;
					FreeBlocks[i].start += capacity;
					return block;
				}
				else if (length == capacity)
				{
					block = fb.start;
					FreeBlocks.erase(FreeBlocks.begin() + i);
					return block;
				}
			}

			// Edge Case: We need a buffer larger than the size
			// of new buffers. Create a specially-sized buffer for that request
			if (capacity > newBufferLength)
			{
				block = new T[capacity];
				allBuffers.push_back(block);
				return block;
			}
				
			// Else: Make EOB a free block if it isn't completely filled 
			// Then Create a new buffer and use the first slots of that
			setActiveBuffer(newBufferLength);
		}
		block = &buffer[used];
		used += capacity;
		return block;
	}

	/*
	 Marks a block of allocated memory as freed and able to be reallocated
	*/
	void freeBlock(T* addr, size_t amount)
	{
		if(amount == 0) return;
		FreeBlock newBlock = {addr, addr + amount};

		/* Binary Search through the FreeBlock array to find a matching block */
		size_t s = FreeBlocks.size(); 
		size_t minimum = 0, maximum = s;
		
		while (minimum < maximum) {
			//size_t midPoint = minimum + (maximum - minimum) / 2; // TODO: (max + min) / 2 (This seems to truncate differently - defer for now)
			size_t midPoint = (minimum + maximum) / 2;
			FreeBlock fb = FreeBlocks[midPoint];
			
			if (fb.start == newBlock.end) {
				// Two Possibilities:
				// 1. The new block perfectly fills the gap between two currently existing blocks
				// 2. Grow the midpoint block leftwards.
				if (midPoint > 0 && FreeBlocks[midPoint - 1].end == newBlock.start) {
					FreeBlocks[midPoint-1].end = fb.end;
					FreeBlocks.erase(FreeBlocks.begin() + midPoint);
				}
				else {
					FreeBlocks[midPoint].start = newBlock.start;
				}
				return;
			}

			if (fb.end == newBlock.start) {
				// Same as above: the new block fills a gap, or extends the midpoint block rightwards
				if (midPoint < maximum - 1 && FreeBlocks[midPoint + 1].start == newBlock.end) {
					FreeBlocks[midPoint + 1].start = fb.start;
					FreeBlocks.erase(FreeBlocks.begin() + midPoint);
				}
				else {
					FreeBlocks[midPoint].end = newBlock.end;
				}
				return;
			}

			if(fb.start > newBlock.end)
				maximum = midPoint;
			else minimum = midPoint + 1; // + 1 should favor an index with a larger value if it exists
		}

		// Minimum will accurately be the first insertable index, maximum will not
		FreeBlocks.insert(FreeBlocks.begin() + minimum, newBlock);
	}
};