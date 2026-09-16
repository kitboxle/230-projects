#include "lru.h"
#include <stdio.h>
#include <stdlib.h>
#include "cache.h"

void lru_init_queue(Set *set) {
  LRUNode *s = NULL;
  LRUNode **pp = &s;  // place to chain in the next node
  for (int i = 0; i < set->line_count; i++) {
    Line *line = &set->lines[i];
    LRUNode *node = (LRUNode *)(malloc(sizeof(LRUNode)));
    node->line = line;
    node->next = NULL;
    (*pp) = node;
    pp = &((*pp)->next);
  }
  set->lru_queue = s;
}

void lru_init(Cache *cache) {
  Set *sets = cache->sets;
  for (int i = 0; i < cache->set_count; i++) {
    lru_init_queue(&sets[i]);
  }
}

void lru_destroy(Cache *cache) {
  Set *sets = cache->sets;
  for (int i = 0; i < cache->set_count; i++) {
    LRUNode *p = sets[i].lru_queue;
    LRUNode *n = p;
    while (p != NULL) {
      p = p->next;
      free(n);
      n = p;
    }
    sets[i].lru_queue = NULL;
  }
}

void lru_fetch(Set *set, unsigned int tag, LRUResult *result) {
  // TODO:
  // Implement the LRU algorithm to determine which line in
  // the cache should be accessed.
  //
//steps:
//get lru_queue from set and iterate --
//HIT = line found in cache
//COLD_MISS = l not found or empty
//CONFLICT_MISS=  line not found bc valid line was removed

//set->line_count is how many lines are in that array.


LRUNode *MRU = set->lru_queue;
LRUNode *q_head = MRU;
LRUNode *prev = NULL;

// walk through the linked list to check for a hit
while (q_head != NULL) {
    Line *traverser = q_head->line;

    // check if the line is valid and the tag matches
    if (traverser->valid && traverser->tag == tag) {
        // if its not already at the front move it to the front
        if (q_head != MRU) {
            prev->next = q_head->next;
            q_head->next = MRU;
            set->lru_queue = q_head;
        }
        result->line = traverser;
        result->access = HIT;
        return;
    }
    prev = q_head;
    q_head = q_head->next;
}
//-------------------------------------------------------------------------------------
// didnt find it so its a miss
// go to the end of the list
prev = NULL;
q_head = MRU;
while (q_head != NULL && q_head->next != NULL) {
    prev = q_head;
    q_head = q_head->next;
}

Line *LRU = q_head->line;
AccessResult miss_type = COLD_MISS;
if (LRU->valid) {
    miss_type = CONFLICT_MISS; //evict valid so conflict miss
}
//--------------------------------------------------------------------------------------------------
// update and move to front
LRU->valid = 1;
LRU->tag = tag;

if (q_head != MRU) {
    prev->next = NULL;
    q_head->next = MRU;
    set->lru_queue = q_head;
}

result->line = LRU;
result->access = miss_type;
}
