#include "cache.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include "cpu.h"
#include "lru.h"

char *make_block(int block_size) {
  // TODO:
  //   Make and initialize a block of memory given the block_size.
  //   Note that the memory should be initialized with zeros.
  //
   char *blk = (char *)calloc(block_size, sizeof(char));
   return blk;  

}

Line *make_lines(int line_count, int block_size) {
  // TODO:
  //   Make and initialize the lines given the line count.
  //   Then make and initialize the blocks using make_block function.
  //
    Line *new_lines = (Line *)malloc(sizeof(Line) * line_count);

    if (new_lines == NULL) return NULL;

	int i = 0;
    while (i < line_count) {
    	new_lines[i].valid = 0;
    	new_lines[i].tag = 0u;
    	new_lines[i].block_size = block_size;
    	new_lines[i].block = make_block(block_size);
    	if (new_lines[i].block == NULL) return NULL;
    	i++;
    }
    return new_lines;
}


Set *make_sets(int set_count, int line_count, int block_size) {
  // TODO:
  //   Make and initialize the sets given the set count. Then
  //   make and initialize the lines and blocks.
  //
  Set *new_set = (Set *)malloc(sizeof(Set) * set_count);
if (new_set == NULL) return NULL;

for (int i = 0; i < set_count; i++) {
  new_set[i].line_count = line_count;
  new_set[i].lru_queue = NULL;
  new_set[i].lines = make_lines(line_count, block_size);
  if (new_set[i].lines == NULL) return NULL;
}
return new_set;
 }

Cache *make_cache(int set_bits, int line_count, int block_bits) {
  Cache *cache = NULL;
  // TODO:
  //   Make and initialize the cache, sets, lines, and blocks.
  //   You should use the `exp2` function to determine the
  //   set_count and block_count from the set_bits and block_bits
  //   respectively (use `man exp2` from the command line).
  //
  // ADD YOUR CODE HERE:

// 1) alloc cache
// 2) set count = set_bits ** 2 (exp_2)
// 3) block size ** 2
// 4) add to alloc cache, if NULL free cache and set to nULL
///----------------------
  cache = (Cache *)malloc(sizeof(Cache));

  if (cache == NULL) {return NULL; }
  int set_count = (int)exp2(set_bits);
  int block_size = (int)exp2(block_bits);

  cache->sets = make_sets(set_count, line_count, block_size);
  cache->set_count = set_count;
  cache->line_count = line_count;
  cache->block_size = block_size;
  cache->set_bits = set_bits;
  cache->block_bits = block_bits;
  if (cache->sets == NULL) { free(cache); cache = NULL; }




  // END TODO

  // Create LRU queues for sets:
  if (cache != NULL) {
    lru_init(cache);
  }

  return cache;
}

void delete_block(char *accessed) { free(accessed); }

void delete_lines(Line *lines, int line_count) {
  for (int i = 0; i < line_count; i++) {
    delete_block(lines[i].block);
  }
  free(lines);
}

void delete_sets(Set *sets, int set_count) {
  for (int i = 0; i < set_count; i++) {
    delete_lines(sets[i].lines, sets[i].line_count);
  }
  free(sets);
}

void delete_cache(Cache *cache) {
  lru_destroy(cache);
  delete_sets(cache->sets, cache->set_count);
  free(cache);
}

SearchInfo get_bits(Cache *cache, address_type address) {
  SearchInfo result;

  // TODO:
  //  Extract the set bits, tag bits, and block bits from a 32-bit address into
  //    result.
  //
  //1) store extracted bits in result
  // 2) extract bits into tag set bloc offset from most to least sig bit
unsigned int offset_mask; // keeps block_bit bits
if (cache->block_bits == 0) {
  offset_mask = 0u;
} else {
  offset_mask = (1u << cache->block_bits) - 1u;
}

unsigned int set_mask; //same 
if (cache->set_bits == 0) {
  set_mask = 0u;
} else {
  set_mask = (1u << cache->set_bits) - 1u;
}

 result.offset = (int)(address & offset_mask);
//tag | set | block format
  result.set_id = (int)((address >> cache->block_bits) & set_mask);
  result.tag    = (int)(address >> (cache->block_bits + cache->set_bits));
  return result;
}

AccessResult cache_access(Cache *cache, TraceLine *trace_line) {
  SearchInfo bits = get_bits(cache, trace_line->address);
  unsigned int s = bits.set_id;
  unsigned int t = bits.tag;
  unsigned int b = bits.offset;

  // Get the set:
  Set *set = &cache->sets[s];

  // Get the line:
  LRUResult result;
  lru_fetch(set, t, &result);
  Line *line = result.line;

  // If it was a miss we will clear the accessed bits:
  if (result.access != HIT) {
    for (int i = 0; i < cache->block_size; i++) {
      line->block[i] = 0;
    }
  }

  // Then set the accessed byte to 1:
  line->block[b] = 1;

  return result.access;
}
