#include "palette_merging.h"

#include "safe_mem.h"
#include "log.h"

typedef struct {
    int a, b;
    int score;
} MergeCandidate;

// Compare function for sorting (max-heap)
static int compare_merges(const void *x, const void *y) {
  return ((MergeCandidate *)y)->score - ((MergeCandidate *)x)->score;
}

// Merge palette a into palette b (in place, avoiding duplicates)
static void merge_palettes(Palette *a, Palette *b) {
  for (int i = 0; i < a->count; i++) {
    if (!palette_contains_hash(b, a->hash_set[i])) {
      b->entries[b->count] = a->entries[i];
      b->hash_set[b->count] = a->hash_set[i];
      b->count++;
    }
  }
}

// Reduce palettes efficiently using a priority queue
void reduce_palettes(Palette *palettes[], int palette_count, int *merged) {
  // Max heap for best merge candidates
  MergeCandidate *queue = safe_malloc(palette_count * palette_count * sizeof(MergeCandidate));
  int queue_size = 0;

  // Precompute all valid merges
  for (int a = 0; a < palette_count; a++) {
    if (merged[a] >= 0) continue;
    for (int b = a + 1; b < palette_count; b++) {
      if (merged[b] >= 0) continue;
      int overlap = shared_colors(palettes[a], palettes[b]);
      int size = palettes[a]->count + palettes[b]->count - overlap;
      if (size <= NUM_COLORS) {
        queue[queue_size++] = (MergeCandidate){a, b, overlap};
      }
    }
  }

  // Sort by best score (descending order)
  qsort(queue, queue_size, sizeof(MergeCandidate), compare_merges);

  // Process merges using max-heap
  while (queue_size > 0) {
    int best_a = queue[0].a;
    int best_b = queue[0].b;

    // Remove best merge from queue
    queue_size--;
    for (int i = 0; i < queue_size; i++) queue[i] = queue[i + 1];

    if (merged[best_a] >= 0 || merged[best_b] >= 0 || queue[0].score == 0)
      continue; // Skip already merged palettes

    verbose_log("Best merge: %d + %d (score: %d)\n", best_a, best_b, queue[0].score);
    merged[best_a] = best_b;  // Track merging
    merge_palettes(palettes[best_a], palettes[best_b]);

    // Update affected pairs **only** (avoid recomputing everything)
    // This change can only have made merges including best_b change score, or no longer valid
    // merges inlcuding best_a will be ignored
    for (int i = 0; i < queue_size; i++) {
      if (queue[i].a == best_b || queue[i].b == best_b) {
        int new_overlap = shared_colors(palettes[queue[i].a], palettes[queue[i].b]);
        int new_size = palettes[queue[i].a]->count + palettes[queue[i].b]->count - new_overlap;
        if (new_size <= NUM_COLORS) {
          // Still valid - update score
          queue[i].score = new_overlap;
        } else {
          // No longer a valid merge
          // set zero score to be ignored
          queue[i].score = 0;
        }
      }
    }

    // Resort queue after updating affected pairs
    qsort(queue, queue_size, sizeof(MergeCandidate), compare_merges);
  }
  free(queue);

  // Resolve final mappings
  for (int i = 0; i < palette_count; i++) {
    int index = i;
    while (merged[index] > 0 && merged[index] != index) {
      index = merged[index];
    }
    merged[i] = index;
  }
}
