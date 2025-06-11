#include "PhysicalMemory.h"
#include "VirtualMemory.h"
# include "MemoryConstants.h"
#include <cassert>
#include <algorithm>

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

uint64_t GetMaxFrame() {
    // Find the maximum frame index that has been used
    uint64_t max_frame = 0;
    word_t value;
    for (uint64_t addr = 0; addr < NUM_FRAMES * PAGE_SIZE; ++addr) {
        PMread(addr, &value);
        uint64_t current_frame = addr / PAGE_SIZE;
        if (value != 0 && current_frame > max_frame) {
            max_frame = current_frame;
        }
    }
    return max_frame;
}


bool ShouldUseMaxFrame(uint64_t MaxFrameIndex){
    return MaxFrameIndex + 1 < NUM_FRAMES;
}

void clearFrame(uint64_t frame) {
    // Clear the frame by writing zeros to all entries
    for (int i = 0; i < PAGE_SIZE; ++i) {
        PMwrite(frame * PAGE_SIZE + i, 0);
    }
}

void ScanUsedFrames(
    uint64_t curr_frame,
    uint64_t depth,
    uint64_t page_path,
    bool used_frames[NUM_FRAMES],
    uint64_t page_per_frame[NUM_FRAMES]
) {
    // Recursively scan the page table to find used frames and
    // their corresponding pages
    used_frames[curr_frame] = true;

    if (depth == TABLES_DEPTH) {
        page_per_frame[curr_frame] = page_path;
        return;
    }

    for (uint64_t offset = 0; offset < PAGE_SIZE; ++offset) {
        word_t next;
        PMread(curr_frame * PAGE_SIZE + offset, &next);

        if (next != 0) {
            // Recursively scan the child frame
            uint64_t next_page_path = (page_path << OFFSET_WIDTH) | offset;
            ScanUsedFrames(next, depth + 1, next_page_path, used_frames, page_per_frame);
        }
    }
}

uint64_t CyclicalDistance(uint64_t a, uint64_t b) {
    int64_t diff = (int64_t)a - (int64_t)b;
    uint64_t abs_diff = std::abs(diff);
    return std::min(NUM_PAGES - abs_diff, abs_diff);
}

// Function to remove reference from parent table
void RemoveReference(uint64_t frame_to_remove, uint64_t curr_frame, uint64_t depth) {
    if (depth == TABLES_DEPTH) {
        return; // This is a leaf page, no children to check
    }

    for (uint64_t offset = 0; offset < PAGE_SIZE; ++offset) {
        word_t next;
        PMread(curr_frame * PAGE_SIZE + offset, &next);
        if (next == frame_to_remove) {
            // Found the reference, remove it
            PMwrite(curr_frame * PAGE_SIZE + offset, 0);
            return;
        } else if (next != 0) {
            // Recursively search in child tables
            RemoveReference(frame_to_remove, next, depth + 1);
        }
    }
}

uint64_t AllocateFrame(uint64_t page_to_swap_in, uint64_t parent_frame, uint64_t parent_offset, bool protected_frames[NUM_FRAMES]) {
    // Allocate a new frame for the given page, either by finding an empty table or evicting an existing one
    uint64_t max_frame = GetMaxFrame();
    // if there is an empty table, we can use it
    for (uint64_t f = 1; f <= max_frame; ++f) {
        if (CheckEmptyTable(f) && !protected_frames[f]) {
            PMwrite(parent_frame * PAGE_SIZE + parent_offset, f);
            protected_frames[f] = true;
            return f;
        }
    }

    // there's no empty table, maybe we can use the next unused frame
    if (ShouldUseMaxFrame(max_frame)) {
        uint64_t new_frame = max_frame + 1;
        clearFrame(new_frame);
        PMwrite(parent_frame * PAGE_SIZE + parent_offset, new_frame);
        protected_frames[new_frame] = true;
        return new_frame;
    }

    // no empty table, no unused frame, so we need to evict a page
    bool used_frames[NUM_FRAMES] = {false};
    uint64_t page_per_frame[NUM_FRAMES] = {0};
    // scan the tree to find used frames and their pages
    ScanUsedFrames(0, 0, 0, used_frames, page_per_frame);

    // find the frame based on max cyclical distance
    uint64_t max_distance = 0;
    uint64_t frame_to_evict = 0;

    for (uint64_t f = 1; f < NUM_FRAMES; ++f) {
        if (!used_frames[f] || protected_frames[f]) continue;

        uint64_t p = page_per_frame[f];
        uint64_t cyclical_dist = CyclicalDistance(page_to_swap_in, p);

        if (cyclical_dist > max_distance) {
            max_distance = cyclical_dist;
            frame_to_evict = f;
        }
    }

    // Evict the chosen frame
    PMevict(frame_to_evict, page_per_frame[frame_to_evict]);
    clearFrame(frame_to_evict);
    RemoveReference(frame_to_evict, 0, 0); // Remove references to the evicted frame
    PMwrite(parent_frame * PAGE_SIZE + parent_offset, frame_to_evict);
    assert(frame_to_evict < NUM_FRAMES);
    return frame_to_evict;
}

uint64_t ResolveAddress(uint64_t virtualAddress, bool allocate_if_missing, bool protected_frames[NUM_FRAMES]) {
    uint64_t page_index, offset;
    SplitOffsetPage(virtualAddress, &page_index, &offset);

    uint64_t level_indices[TABLES_DEPTH];
    SplitPageIndexByLevels(page_index, level_indices);

    uint64_t current_frame = 0;
    for (int depth = 0; depth < TABLES_DEPTH; ++depth) {
        uint64_t idx = level_indices[depth];
        word_t next_frame;
        PMread(current_frame * PAGE_SIZE + idx, &next_frame);

        if (next_frame == 0) {
            if (!allocate_if_missing) return UINT64_MAX; // If allocation is not allowed, return an invalid address
            next_frame = AllocateFrame(page_index, current_frame, idx, protected_frames);
        }
        current_frame = next_frame;
    }
    printf("Resolved virtual address %llu to physical frame %llu with offset %llu\n", virtualAddress, current_frame, offset);
    assert(current_frame < NUM_FRAMES);
    assert(offset < PAGE_SIZE);
    return current_frame * PAGE_SIZE + offset;
}

int VMread(uint64_t virtualAddress, word_t* value){
    assert(virtualAddress < VIRTUAL_MEMORY_SIZE);
    bool protected_frames[NUM_FRAMES] = {false};
    uint64_t phys_addr = ResolveAddress(virtualAddress, false, protected_frames);
    if (phys_addr == UINT64_MAX) return 0;
    PMread(phys_addr, value);
    return 1;
}

int VMwrite(uint64_t virtualAddress, word_t value){
    assert(virtualAddress < VIRTUAL_MEMORY_SIZE);
    bool protected_frames[NUM_FRAMES] = {false};
    uint64_t phys_addr = ResolveAddress(virtualAddress, true, protected_frames);
    if (phys_addr == UINT64_MAX) return 0;
    PMwrite(phys_addr, value);
    return 1;
}