#include "PhysicalMemory.h"
#include "VirtualMemory.h"
# include "MemoryConstants.h"
#include <cassert>
#include <algorithm>

struct VMState {
    bool protected_frames[NUM_FRAMES] = {false};
};

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
  int totalBits = VIRTUAL_ADDRESS_WIDTH - OFFSET_WIDTH;
  int remainder = totalBits % OFFSET_WIDTH;
  bitsPerLevel[0] = remainder;
  for (int i = 1; i < TABLES_DEPTH; ++i) {
    bitsPerLevel[i] = OFFSET_WIDTH;
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

void clearFrame(uint64_t frame) {
  // Clear the frame by writing zeros to all entries
  for (int i = 0; i < PAGE_SIZE; ++i) {
    PMwrite(frame * PAGE_SIZE + i, 0);
  }
}
//
//void FindMaxFrame(
//    uint64_t curr_frame,
//    uint64_t depth,
//    uint64_t* max_frame_seen
//) {
//  if (curr_frame > *max_frame_seen) {
//    *max_frame_seen = curr_frame;
//  }
//  if (depth == TABLES_DEPTH - 1) {
//    return;
//  }
//  for (uint64_t offset = 0; offset < PAGE_SIZE; ++offset) {
//    word_t next;
//    PMread(curr_frame * PAGE_SIZE + offset, &next);
//    if (next != 0) {
//      FindMaxFrame(next, depth + 1, max_frame_seen);
//    }
//  }
//}

#include <stack>
//
//void FindMaxFrame(uint64_t root_frame, uint64_t* max_frame_seen) {
//  using FrameInfo = std::pair<uint64_t, uint64_t>; // (frame, depth)
//  std::stack<FrameInfo> stack;
//  stack.push({root_frame, 0});
//
//  while (!stack.empty()) {
//    auto [curr_frame, depth] = stack.top();
//    stack.pop();
//
//    if (curr_frame > *max_frame_seen) {
//      *max_frame_seen = curr_frame;
//    }
//
//    // תמיד ממשיכים לסרוק, גם בעומק האחרון, כדי לא לפספס פריימים שמכילים דפים
//    for (uint64_t offset = 0; offset < PAGE_SIZE; ++offset) {
//      word_t next;
//      uint64_t addr = curr_frame * PAGE_SIZE + offset;
//
//      if (addr >= NUM_FRAMES * PAGE_SIZE) {
//        // הגנה על זיכרון – לא להיכנס לתחום לא חוקי
//        continue;
//      }
//
//      PMread(addr, &next);
//      if (next != 0) {
//        stack.push({next, depth + 1});
//      }
//    }
//  }
//}

#include <unordered_set>

void FindMaxFrame(uint64_t root_frame, uint64_t* max_frame_seen) {
  using FrameInfo = std::pair<uint64_t, uint64_t>; // (frame, depth)
  std::stack<FrameInfo> stack;
  std::unordered_set<uint64_t> visited;

  stack.push({root_frame, 0});

  while (!stack.empty()) {
    auto [curr_frame, depth] = stack.top();
    stack.pop();

    if (visited.count(curr_frame)) {
      continue;
    }
    visited.insert(curr_frame);

    // לא נחשב את frame 0 כ-max
    if (curr_frame != 0 && curr_frame > *max_frame_seen) {
      *max_frame_seen = curr_frame;
    }

    for (uint64_t offset = 0; offset < PAGE_SIZE; ++offset) {
      uint64_t addr = curr_frame * PAGE_SIZE + offset;
      if (addr >= NUM_FRAMES * PAGE_SIZE) {
        continue; // הגנה על גישה לא חוקית
      }

      word_t next;
      PMread(addr, &next);
      if (next != 0) {
        stack.push({next, depth + 1});
      }
    }
  }
}


//
//void ScanUsedFramesForEvict(
//    uint64_t curr_frame,
//    uint64_t depth,
//    uint64_t page_path,
//    bool used_frames[NUM_FRAMES],
//    uint64_t page_per_frame[NUM_FRAMES]
//) {
//  used_frames[curr_frame] = true;
//
//  if (depth == TABLES_DEPTH - 1) {
//    page_per_frame[curr_frame] = page_path;
//    printf("ScanUsedFrames: frame %llu now maps to page %llu\n", curr_frame, page_path);
//    return;
//  }
//
//  for (uint64_t offset = 0; offset < PAGE_SIZE; ++offset) {
//    word_t next;
//    PMread(curr_frame * PAGE_SIZE + offset, &next);
//    if (next != 0) {
//      ScanUsedFramesForEvict(next, depth + 1, (page_path << OFFSET_WIDTH) | offset,
//                             used_frames, page_per_frame);
//    }
//  }
//}

#include <stack>
#include <unordered_set>


void ScanUsedFramesForEvict(
    uint64_t root_frame,
    bool used_frames[NUM_FRAMES],
    uint64_t page_per_frame[NUM_FRAMES]
) {
  printf("here?");
  using FrameState = std::tuple<uint64_t, uint64_t, uint64_t>; // (frame, depth, page_path)
  std::stack<FrameState> stack;
  std::unordered_set<uint64_t> visited;
  stack.push({root_frame, 0, 0});

  while (!stack.empty()) {
    auto [curr_frame, depth, page_path] = stack.top();
    stack.pop();

    if (visited.count(curr_frame)) {
      continue;
    }
    visited.insert(curr_frame);

    used_frames[curr_frame] = true;

    if (depth == TABLES_DEPTH - 1) {
      page_per_frame[curr_frame] = page_path;
      printf("ScanUsedFrames: frame %llu now maps to page %llu\n", curr_frame, page_path);
      continue;
    }

    for (uint64_t offset = 0; offset < PAGE_SIZE; ++offset) {
      uint64_t addr = curr_frame * PAGE_SIZE + offset;
      if (addr >= NUM_FRAMES * PAGE_SIZE) {
        continue; // הגנה על זיכרון
      }

      word_t next;
      PMread(addr, &next);
      if (next != 0) {
        uint64_t new_page_path = (page_path << OFFSET_WIDTH) | offset;
        stack.push({next, depth + 1, new_page_path});
      }
    }
  }
}


uint64_t CyclicalDistance(uint64_t a, uint64_t b) {
  uint64_t d = (a > b) ? a - b : b - a;
  return std::min(d, NUM_PAGES - d);
}

void RemoveReference(uint64_t frame_to_remove, uint64_t curr_frame, uint64_t depth, bool visited[NUM_FRAMES]) {
  if (visited[curr_frame]) return;
  visited[curr_frame] = true;
  if (depth == TABLES_DEPTH - 1) {
    return;
  }
  for (uint64_t offset = 0; offset < PAGE_SIZE; ++offset) {
    word_t next;
    PMread(curr_frame * PAGE_SIZE + offset, &next);
    if ((uint64_t)next == frame_to_remove) {
      PMwrite(curr_frame * PAGE_SIZE + offset, 0);
    }
    if (next != 0 && (uint64_t)next != frame_to_remove) {
      RemoveReference(frame_to_remove, next, depth + 1, visited);
    }
  }
}


uint64_t AllocateFrame(uint64_t page_to_swap_in, uint64_t parent_frame,
                       uint64_t parent_offset, VMState& state) {
  // Allocate a new frame for the given page, either by finding an empty table or evicting an existing one

  uint64_t max_frame = 0;
  FindMaxFrame(0, &max_frame);
  printf("AllocateFrame: max frame seen is %llu\n", max_frame);

  // First, try to find an empty table that is not protected
  for (uint64_t f = 1; f <= max_frame; ++f) {
    if (CheckEmptyTable(f) && !state.protected_frames[f]) {
      printf("bool for protected is: %d\n", state.protected_frames[f]);
      printf("i use now frame %llu:\n", f);
      return f;
    }
  }

  // If there's space for a new frame
  if (ShouldUseMaxFrame(max_frame)) {
    uint64_t new_frame = max_frame + 1;
    printf("AllocateFrame: using new frame %llu\n", new_frame);
    clearFrame(new_frame);
    return new_frame;
  }

  // Else, find a frame to evict
  uint64_t max_distance = 0;
  uint64_t frame_to_evict = 0;
  bool used_frames[NUM_FRAMES] = {false};
  uint64_t page_per_frame[NUM_FRAMES] = {0};
  ScanUsedFramesForEvict(0, used_frames, page_per_frame);

  for (uint64_t f = 1; f < NUM_FRAMES; ++f) {
    if (!used_frames[f] || state.protected_frames[f] || page_per_frame[f] == 0) {
      continue;
    }

    uint64_t p = page_per_frame[f];
    uint64_t cyclical_dist = CyclicalDistance(page_to_swap_in, p);

    if (cyclical_dist > max_distance) {
      max_distance = cyclical_dist;
      frame_to_evict = f;
    }
  }

  PMevict(frame_to_evict, page_per_frame[frame_to_evict]);
  printf("frame to evict %llu: \n", frame_to_evict);

  PMwrite(parent_frame * PAGE_SIZE + parent_offset, 0);
  clearFrame(frame_to_evict);
  assert(frame_to_evict < NUM_FRAMES);

  return frame_to_evict;
}



uint64_t ResolveAddress(uint64_t virtualAddress, VMState& state) {
  uint64_t page_index, offset;
  SplitOffsetPage(virtualAddress, &page_index, &offset);

  uint64_t level_indices[TABLES_DEPTH];
  SplitPageIndexByLevels(page_index, level_indices);
  printf("page index: %llu ", page_index);
  printf("offset: %llu \n", offset);

  uint64_t current_frame = 0;

  for (int depth = 0; depth < TABLES_DEPTH; ++depth) {
    uint64_t idx = level_indices[depth];
    word_t next_frame;
    PMread(current_frame * PAGE_SIZE + idx, &next_frame);

    if (next_frame == 0) {
      next_frame = AllocateFrame(page_index, current_frame, idx, state);
      state.protected_frames[next_frame] = true;
      PMwrite(current_frame * PAGE_SIZE + idx, next_frame);

      if (depth == TABLES_DEPTH - 1) {
        PMrestore(next_frame, page_index);
      }
    }

    current_frame = next_frame;

    assert(current_frame < NUM_FRAMES);
    assert(offset < PAGE_SIZE);
  }

  return current_frame * PAGE_SIZE + offset;
}

int VMread(uint64_t virtualAddress, word_t* value){
  assert(virtualAddress < VIRTUAL_MEMORY_SIZE);
  static VMState state;
  uint64_t phys_addr = ResolveAddress(virtualAddress, state);
  if (phys_addr == UINT64_MAX) return 0;
  PMread(phys_addr, value);
  printf ("vm read physical address: %llu ", phys_addr);
  printf ("vm read value: %d \n", *value);
  return 1;
}

int VMwrite(uint64_t virtualAddress, word_t value){
  assert(virtualAddress < VIRTUAL_MEMORY_SIZE);
  static VMState state;
  uint64_t phys_addr = ResolveAddress(virtualAddress, state);
  if (phys_addr == UINT64_MAX) return 0;
  PMwrite(phys_addr, value);
  printf ("vm write physical address: %llu ", phys_addr);
  printf ("vm write value: %d \n", value);
  return 1;
}