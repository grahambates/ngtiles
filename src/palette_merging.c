#include "palette_merging.h"

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
  PriorityQueue pq = {.data = safe_malloc(palette_count * palette_count * sizeof(MergeCandidate)), .size = 0};

  // Precompute all valid merges
  for (int a = 0; a < palette_count; a++) {
    for (int b = a + 1; b < palette_count; b++) {
      int overlap = shared_colors(palettes[a], palettes[b]);
      int size = palettes[a]->count + palettes[b]->count - overlap;
      if (size <= NUM_COLORS) {
        heap_insert(&pq, (MergeCandidate){a, b, overlap});
      }
    }
  }

  // Process merges using max-heap
  while (pq.size > 0) {
    MergeCandidate best = heap_extract_max(&pq);

    if (merged[best.a] >= 0 || merged[best.b] >= 0 || best.score == 0)
      continue; // Skip already merged palettes

    verbose_log("Best merge: %d + %d (score: %d)\n", best.a, best.b, best.score);
    merged[best.a] = best.b;  // Track merging
    merge_palettes(palettes[best.a], palettes[best.b]);

    // Update affected pairs **only** (avoid recomputing everything)
    // This change can only have made merges including best.b change score, or no longer valid
    // merges including best.a will be ignored
    for (int i = 0; i < pq.size; i++) {
      if (pq.data[i].a == best.b || pq.data[i].b == best.b) {
        int new_overlap = shared_colors(palettes[pq.data[i].a], palettes[pq.data[i].b]);
        int new_size = palettes[pq.data[i].a]->count + palettes[pq.data[i].b]->count - new_overlap;

        if (new_size <= NUM_COLORS) {
          // Still valid - update score
          int old_score = pq.data[i].score;
          pq.data[i].score = new_overlap;

          // Reorder in heap
          if (new_overlap > old_score) {
            // Score increased, move up the heap
            int j = i;
            while (j > 0 && pq.data[(j - 1) / 2].score < pq.data[j].score) {
              MergeCandidate temp = pq.data[j];
              pq.data[j] = pq.data[(j - 1) / 2];
              pq.data[(j - 1) / 2] = temp;
              j = (j - 1) / 2;  // Move up
            }
          } else if (new_overlap < old_score) {
            // Score decreased, move down the heap
            heapify_down(&pq, i);
          }
        } else {
          // No longer a valid merge, mark as zero score to be skipped
          pq.data[i].score = 0;
        }
      }
    }
  }
  free(pq.data);

  // Resolve final mappings
  for (int i = 0; i < palette_count; i++) {
    int index = i;
    while (merged[index] > 0 && merged[index] != index) {
      index = merged[index];
    }
    merged[i] = index;
  }
}
