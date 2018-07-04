/**
 * This file is part of the CernVM File System.
 */

#include "chunk_detector.h"

#include <algorithm>
#include <cassert>
#include <cstdio>
#include <limits>

uint64_t ChunkDetector::FindNextCutMark(BlockItem *block) {
  uint64_t result = DoFindNextCutMark(block);
  if (result == 0)
    offset_ += block->size();
  return result;
}


//------------------------------------------------------------------------------



// This defines the center of the interval where the xor32 rolling checksum is
// queried. You should never change this number, since it affects the definition
// of cut marks.
const int32_t Xor32Detector::kMagicNumber =
  std::numeric_limits<uint32_t>::max() / 2;


Xor32Detector::Xor32Detector(const uint64_t minimal_chunk_size,
                             const uint64_t average_chunk_size,
                             const uint64_t maximal_chunk_size)
  : minimal_chunk_size_(minimal_chunk_size)
  , average_chunk_size_(average_chunk_size)
  , maximal_chunk_size_(maximal_chunk_size)
  , threshold_((average_chunk_size > 0)
               ? (std::numeric_limits<uint32_t>::max() / average_chunk_size)
               : 0)
  , xor32_ptr_(0)
  , xor32_(0)
{
  assert((average_chunk_size_ == 0) || (minimal_chunk_size_ > 0));
  if (minimal_chunk_size_ > 0) {
    assert(minimal_chunk_size_ >= kXor32Window);
    assert(minimal_chunk_size_ < average_chunk_size_);
    assert(average_chunk_size_ < maximal_chunk_size_);
  }
}


uint64_t Xor32Detector::DoFindNextCutMark(BlockItem *buffer) {
  assert(minimal_chunk_size_ > 0);
  const unsigned char *data = buffer->data();

  // Get the offset where the next xor32 computation needs to be continued
  // Note: this could be after collecting at least kMinChunkSize bytes in the
  //       current chunk, or directly at the beginning of the buffer, when a
  //       cut mark is currently searched
  const uint64_t global_offset =
    std::max(
           last_cut() +
           static_cast<uint64_t>(minimal_chunk_size_ - kXor32Window),
           xor32_ptr_);

  // Check if the next xor32 computation is taking place in the current buffer
  if (global_offset >= offset() + static_cast<uint64_t>(buffer->size())) {
    return NoCut(global_offset);
  }

  // get the byte offset in the current buffer
  uint64_t internal_offset = global_offset - offset();
  assert(internal_offset < static_cast<uint64_t>(buffer->size()));

  // Precompute the xor32 rolling checksum for finding the next cut mark
  // Note: this might be skipped, if the precomputation was already performed
  //       for the current rolling checksum
  //       (internal_precompute_end will be negative --> loop is not entered)
  const uint64_t precompute_end = last_cut() + minimal_chunk_size_;
  const int64_t internal_precompute_end =
    std::min(static_cast<int64_t>(precompute_end - offset()),
             static_cast<int64_t>(buffer->size()));
  assert(internal_precompute_end - static_cast<int64_t>(internal_offset) <=
         static_cast<int64_t>(kXor32Window));
  for (; static_cast<int64_t>(internal_offset) < internal_precompute_end;
       ++internal_offset)
  {
    xor32(data[internal_offset]);
  }

  // Do the actual computation and try to find a xor32 based cut mark
  // Note: this loop is bound either by kMaxChunkSize or by the size of the
  //       current buffer, thus the computation would continue later
  const uint64_t internal_max_chunk_size_end =
    last_cut() + maximal_chunk_size_ - offset();
  const uint64_t internal_compute_end =
    std::min(internal_max_chunk_size_end,
             static_cast<uint64_t>(buffer->size()));
  for (; internal_offset < internal_compute_end; ++internal_offset) {
    xor32(data[internal_offset]);

    // check if we found a cut mark
    if (CheckThreshold()) {
      return DoCut(internal_offset + offset());
    }
  }

  // Check if the loop was exited because we reached kMaxChunkSize and do a
  // hard cut in this case. If not, it exited because we ran out of data in this
  // buffer --> continue computation with the next buffer
  if (internal_offset == internal_max_chunk_size_end) {
    return DoCut(internal_offset + offset());
  } else {
    return NoCut(internal_offset + offset());
  }
}


int main(int argc, char **argv) {
  if (argc < 2)
    return 1;

  printf("reading %s\n", argv[1]);
  uint32_t kBufSize = 10 * 1024 * 1024;  // 10M
  unsigned char *buffer = reinterpret_cast<unsigned char *>(malloc(kBufSize));
  assert(buffer != NULL);
  FILE *f = fopen(argv[1], "r");
  size_t nbytes = fread(buffer, 1, kBufSize, f);
  assert(nbytes < kBufSize);
  fclose(f);
  printf("read %lu bytes\n", nbytes);

  BlockItem item(buffer, nbytes);

  Xor32Detector xor32_detector(512000, 2*512000, 4*512000);
  uint64_t cut_mark;
  while ((cut_mark = xor32_detector.FindNextCutMark(&item)) != 0)
    printf("New cutmark: %lu\n", cut_mark);

  return 0;
}
