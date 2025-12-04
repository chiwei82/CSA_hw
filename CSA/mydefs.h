#pragma once
#include <string.h>

#define BIGSTR 100000
#define BLOCK_NOT_FOUND -1
#define ZERO_INDEX_OFFSET 1
#define DESIRED_RESIZE 1
#define NONE_OVERLAP 0

// Prototypes for other "private" functions etc.
int findBlock(csa* c, int idx);
int findPosition(block *bp, int bit_index);
bool delete(csa *c, int bp_index, int bit_index, int pos);
bool isValExist(mask_t msk, int bit_idx);
void additional_free(csa *c, int bp_idx, int new_len);
void incrementBlock(csa* c, int idx);
void update_block(csa* c, int idx, int val);
void move_and_insert(csa *c, unsigned int offset);
void concat_data(csa *c, char* s);
