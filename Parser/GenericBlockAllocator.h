#include <vector>
#include <string>
#include <sstream>

/*
* Realistically speaking, this allocator has some flaws that require
* users to utilize it the correct way to prevent errors:
* 1. Does not check if the memory you free was actually provided by the allocator
* 2. Expects users to independently maintain the start and lengths of allocated blocks
*/
template <typename T>
class BlockAllocator
{
	private:
	std::vector<T*> blockStarts; // Sorted starting addresses of deallocated blocks
	std::vector<T*> blockEnds;   // Sorted ending addresses of deallocated blocks

	std::vector<T*> allBuffers; // Contains every buffer made by this allocator
	const size_t NEW_BUFFER_LENGTH;

	T* buffer; // Active buffer
	size_t max; // Capacity of the active buffer
	size_t used = 0; // Number of used elements in the active buffer.

	public:
	BlockAllocator(const size_t newCapacity) : buffer(nullptr), max(0), NEW_BUFFER_LENGTH(newCapacity) {}

	BlockAllocator(const size_t initialCapacity, const size_t newCapacity) : NEW_BUFFER_LENGTH(newCapacity)
	{
		setActiveBuffer(initialCapacity);
	}

	~BlockAllocator() 
	{
		for(T* buff : allBuffers)
			delete[] buff; 
	}

	std::string toString(bool includeBlockList)
	{
		std::ostringstream buffer;
		buffer << "Number of Buffers: " << allBuffers.size();
		buffer << "\nActive Buffer Status: " << used << " / " << max << " (used / max)";
		buffer << "\nNew Buffer Sizes: " << NEW_BUFFER_LENGTH;
		buffer << "\nAvailable Free Blocks: " << blockStarts.size();

		if (includeBlockList)
		{
			buffer << "\n\nFree Block Log (Addr / Capacity):\n-----";
			for (size_t i = 0, s = blockStarts.size(); i < s; i++)
			{
				size_t length = blockEnds[i] - blockStarts[i];
				buffer << '\n' << (void*)blockStarts[i] << " / " << length;
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

			for (size_t i = 0, s = blockStarts.size(); i < s; i++) 
			{
				size_t length = blockEnds[i] - blockStarts[i];
				if (length > capacity)
				{
					block = blockStarts[i];
					blockStarts[i] += capacity;
					return block;
				}
				else if (length == capacity)
				{
					block = blockStarts[i];
					blockStarts.erase(blockStarts.begin() + i);
					blockEnds.erase(blockEnds.begin() + i);
					return block;
				}
			}

			// Edge Case: We need a buffer larger than the size
			// of new buffers. Create a specially-sized buffer for that request
			if (capacity > NEW_BUFFER_LENGTH)
			{
				block = new T[capacity];
				allBuffers.push_back(block);
				return block;
			}
				
			// Else: Make EOB a free block if it isn't completely filled 
			// Then Create a new buffer and use the first slots of that
			setActiveBuffer(NEW_BUFFER_LENGTH);
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
		T* endAddr = addr + amount;

		size_t s = blockStarts.size();
		size_t startIndex = 0, endIndex = 0;
		bool hasStart = binarySearch(blockEnds, addr, 0, s, startIndex);
			// startIndex gives a lower bound for where the endAddr may be
		bool hasEnd = binarySearch(blockStarts, endAddr, startIndex, s, endIndex);

		// Simplified rules based on our assumption of
		// safe allocation/deallocation practices by users of this class:
		// If it contains that border value, we erase it
		// If it doesn't contain that border value, we add it
		// This if/else chain swaps the value when we have one but not the other
		if (hasStart) {
			if (hasEnd)
			{
				blockEnds.erase(blockEnds.begin() + startIndex);
				blockStarts.erase(blockStarts.begin() + endIndex);
			}
			else blockEnds[startIndex] = endAddr;
		}
		else if (hasEnd) {
			blockStarts[endIndex] = addr;
		}
		else {
			blockEnds.insert(blockEnds.begin() + startIndex, endAddr);
			blockStarts.insert(blockStarts.begin() + startIndex, addr);
		}
	}

	private:
	/*
	* Modified binary search algorithm. Finds index an element is located at. If the element is not present,
	* it finds the index it can be inserted at.
	* @param list
	* @param target Element to find
	* @param minimum Smallest index to search at
	* @param maximum Largest index+1 to search at (up to the size of the vector)
	* @param finalPosition The location/insertion index is placed here when found.
	* 
	* @returns true if the target is already inside the vector, otherwise false
	*/
	bool binarySearch(std::vector<T*> &list, T* target, size_t minimum, size_t maximum, size_t &finalPosition)
	{
		while (minimum < maximum) {
			size_t midPoint = minimum + (maximum - minimum) / 2; // Truncation means the left value is favored.
			if (list[midPoint] == target)
			{
				finalPosition = midPoint;
				return true;
			}
				
			if (list[midPoint] > target)
				maximum = midPoint;
			else minimum = midPoint + 1; // +1 should favor an index with a larger value if it exists
		}
		finalPosition = minimum; // Minimum will accurately be first insertable index, maximum will not
		return false;
	}
};