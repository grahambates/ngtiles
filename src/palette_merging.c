#include <stdlib.h>

#include "palette_merging.h"

#include "consts.h"
#include "safe_mem.h"
#include "log.h"

typedef struct {
  int a, b;
  int score;
} MergeCandidate;

// Priority Queue (Max-Heap) for Merge Candidates
typedef struct {
  MergeCandidate *data;
  int size;
} PriorityQueue;

static void heapify_down(PriorityQueue *pq, int i) {
  int largest = i, left = 2 * i + 1, right = 2 * i + 2;
  if (left < pq->size && pq->data[left].score > pq->data[largest].score)
    largest = left;
  if (right < pq->size && pq->data[right].score > pq->data[largest].score)
    largest = right;
  if (largest != i) {
    MergeCandidate temp = pq->data[i];
    pq->data[i] = pq->data[largest];
    pq->data[largest] = temp;
    heapify_down(pq, largest);
  }
}

static MergeCandidate heap_extract_max(PriorityQueue *pq) {
  MergeCandidate max = pq->data[0];
  pq->data[0] = pq->data[--pq->size];
  heapify_down(pq, 0);
  return max;
}

static void heap_insert(PriorityQueue *pq, MergeCandidate candidate) {
  int i = pq->size++;
  while (i > 0 && pq->data[(i - 1) / 2].score < candidate.score) {
    pq->data[i] = pq->data[(i - 1) / 2];
    i = (i - 1) / 2;
  }
  pq->data[i] = candidate;
}

// Find root of a set with path compression
static int find_root(int x, int merged[]) {
  if (merged[x] != x) {
    merged[x] = find_root(merged[x], merged);  // Path compression
  }
  return merged[x];
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
  for (int i = 0; i < palette_count; i++) {
    merged[i] = i;
  }

  PriorityQueue pq = {
    .data = safe_malloc(palette_count * palette_count * sizeof(MergeCandidate)),
    .size = 0
  };

  // Precompute all valid merges
  for (int a = 0; a < palette_count; a++) {
    for (int b = a + 1; b < palette_count; b++) {
      // The number of shared colours is the score we use to prioritise the queue
      int overlap = shared_colors(palettes[a], palettes[b]);
      // Only insert combinations that can be merged with a combined colour count of <= 16
      int size = palettes[a]->count + palettes[b]->count - overlap;
      if (size <= NUM_COLORS) {
        heap_insert(&pq, (MergeCandidate){a, b, overlap});
      }
    }
  }

  // Process items in queue
  while (pq.size > 0) {
    MergeCandidate best = heap_extract_max(&pq);
    int root_a = find_root(best.a, merged);
    int root_b = find_root(best.b, merged);

    if (root_a == root_b) continue;  // Skip if already merged

    // Do the merge
    verbose_log("Best merge: %d + %d (score: %d)\n", root_a, root_b, best.score);
    merged[root_a] = root_b;
    merge_palettes(palettes[root_a], palettes[root_b]);

    // Update any pairs in the queue that would be affected by this merge
    for (int i = 0; i < pq.size; i++) {
      int a = pq.data[i].a;
      int b = pq.data[i].b;

      // Check if this pair is affected by the merge
      if (a != root_a && b != root_a && a != root_b && b != root_b) continue;

      // Find new roots
      int new_root_a = find_root(a, merged);
      int new_root_b = find_root(b, merged);

      // Remove if already merged
      if (new_root_a == new_root_b) {
        pq.data[i] = pq.data[--pq.size];
        heapify_down(&pq, i);
        i--;
        continue;
      }

      // Recalculate overlap and size
      int new_overlap = shared_colors(palettes[new_root_a], palettes[new_root_b]);
      int new_size = palettes[new_root_a]->count + palettes[new_root_b]->count - new_overlap;

      // Remove invalid merge from heap
      if (new_size > NUM_COLORS) {
        pq.data[i] = pq.data[--pq.size];
        heapify_down(&pq, i);
        i--;
        continue;
      }

      // Update valid pair
      pq.data[i].a = new_root_a;
      pq.data[i].b = new_root_b;
      pq.data[i].score = new_overlap;
    }
  }

  free(pq.data);
}
