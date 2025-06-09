#include "PhysicalMemory.h"
#include "VirtualMemory.h"
# include "MemoryConstants.h"


void VMinitialize() {
    // Initialize the virtual memory
    for (uint64_t i = 0; i < PAGE_SIZE; ++i) {
        PMwrite(i, 0);
    }
}

void SplitOffsetPage(uint64_t virtualAddress, uint64_t* pageIndex, uint64_t* offset) {
    // Split the virtual address into page index and offset
    *pageIndex = virtualAddress >> OFFSET_WIDTH;
    *offset = virtualAddress & (PAGE_SIZE - 1);
}

void ComputeBitsPerLevel(uint64_t bitsPerLevel[TABLES_DEPTH]) {
    // Compute the number of bits to be used for each level of the page table
    int totalBits = VIRTUAL_ADDRESS_WIDTH - OFFSET_WIDTH;

    for (int level = 0; level < TABLES_DEPTH; ++level) {
        int remainingLevels = TABLES_DEPTH - level;
        if (totalBits < OFFSET_WIDTH) {
            bitsPerLevel[level] = totalBits;
        } else {
            bitsPerLevel[level] = OFFSET_WIDTH;
            totalBits -= OFFSET_WIDTH;
        }
    }
}

void SplitPageIndexByLevels(uint64_t pageIndex, uint64_t levelIndices[TABLES_DEPTH]) {
    // Split the page index into indices for each level of the page table
    uint64_t bitsPerLevel[TABLES_DEPTH];
    ComputeBitsPerLevel(bitsPerLevel);

    int bitOffset = 0;
    for (int level = TABLES_DEPTH - 1; level >= 0; --level) {
        uint64_t mask = (1ULL << bitsPerLevel[level]) - 1;
        levelIndices[level] = (pageIndex >> bitOffset) & mask;
        bitOffset += bitsPerLevel[level];
    }
}


bool CheckEmptyTable(uint64_t frameIndex) {
    for (int i = 0; i < PAGE_SIZE; ++i) {
        word_t value;
        PMread(frameIndex * PAGE_SIZE + i, &value);
        if (value != 0) {
            return false;  // Found a non-zero entry → table not empty
        }
    }
    return true;  // All entries are 0 → table is empty
}


bool ShouldUseMaxFrame(uint64_t MaxFrameIndex){
    return MaxFrameIndex + 1 < NUM_FRAMES;
}


int choose_frame_to_swap(uint64_t page_to_swap_in, uint64_t frames[NUM_FRAMES]) {
    int frame_to_swap = -1;
    uint64_t max_distance = 0;

    for (int i = 0; i < NUM_FRAMES; ++i) {
        uint64_t p = frames[i];
        uint64_t abs_distance = std::abs((int64_t)page_to_swap_in - (int64_t)p);

        uint64_t cyclical_distance = std::min(NUM_PAGES - abs_distance, abs_distance);

//        std::cout << "Frame " << i << " contains page " << p
//                  << " -> cyclical distance to page " << page_to_swap_in
//                  << " is " << cyclical_distance << std::endl;

        if (cyclical_distance > max_distance) {
            max_distance = cyclical_distance;
            frame_to_swap = i;
        }
    }
    return frame_to_swap;
}


void clearFrame(uint64_t frame) {
    // Clear the frame by writing zeros to all entries
    for (int i = 0; i < PAGE_SIZE; ++i) {
        PMwrite(frame * PAGE_SIZE + i, 0);
    }
}

uint64_t traversePageTable(){
    // Traverse from root to the leaf frame
}



void GetNextFrame{
}


int VMread(uint64_t virtualAddress, word_t* value){
    assert(virtualAddress < VIRTUAL_MEMORY_SIZE);
}


int VMwrite(uint64_t virtualAddress, word_t value){
    assert(virtualAddress < VIRTUAL_MEMORY_SIZE);
}