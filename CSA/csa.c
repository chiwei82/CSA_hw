#include "csa.h"
#include "mydefs.h"

csa* csa_init(void)
{
   csa *c = malloc(sizeof(csa));
   c->b = NULL;
   c->n = 0;
   return c;
}

bool csa_get(csa* c, int idx, int* val)
{
   // If any of the pointers is NULL, return false
   if (c == NULL || val == NULL) {
      return false;
   }
   // 1. check if block exists
   int bp_idx = findBlock(c, idx);
   if (bp_idx == BLOCK_NOT_FOUND) {
      return false;
   }
   block *bp = &c->b[bp_idx];

   // 2. check if value exists
   int bit_idx = idx % MSKLEN;
   if (!isValExist(bp->msk, bit_idx)) {
      return false;
   }
   // 3. get value and copied into val
   int pos = findPosition(bp, bit_idx);
   *val = bp->vals[pos];
   return true;
}

bool csa_set(csa* c, int idx, int val)
{
   if (c == NULL) {
      return false;
   } //
   // 1. if not block exists do block increment
   int bp_idx = findBlock(c, idx);
   if (bp_idx == BLOCK_NOT_FOUND) {
      incrementBlock(c, idx);
   }
   // 2. set value
   update_block(c, idx, val);
   return true;
}

void csa_tostring(csa* c, char* s)
{
   // init string
   char tmp[BIGSTR];
   s[0] = '\0';
   if (c == NULL || s == NULL) {
      return;
   }

   if (c->n == 0) {
      strcat(s, "0 blocks");
      return;
   }

   // concat blocks
   if (c->n == 1) {
      strcat(s, "1 block");
   } else {
      snprintf(tmp, BIGSTR , "%d blocks", c->n);
      strcat(s, tmp);
   }
   strcat(s, " ");

   // concat data part
   concat_data(c, s);
}

void csa_free(csa** l)
{
   if (*l == NULL) {
      return;
   }
   for (int i = 0; i < (*l)->n; i++) {
      free((*l)->b[i].vals);
   }
   free((*l)->b);
   free(*l);
   *l = NULL;
}

void test(void)
{
   csa* c = NULL;
   char str[BIGSTR];
   int n = 1;

   // csa_init
   csa_tostring(c, str);
   assert(strcmp(str, "")==0);
   
   c = csa_init();
   csa_tostring(c, str);
   assert(strcmp(str, "0 blocks")==0);
   // csa_set
   assert(csa_set(c, 2,  25));
   assert(csa_set(c, 3,  30));
   // csa_get
   assert(!csa_get(c, 0, &n));
   assert(csa_get(c,  2, &n));
   // csa_tostring
   csa_tostring(c, str);
   assert(strcmp(str, "1 block {2|[2]=25:[3]=30}")==0);

   // findBlock
   assert(findBlock(c, 2) == 0);
   assert(findBlock(c, 64) == BLOCK_NOT_FOUND);
   assert(findBlock(c, 256) == BLOCK_NOT_FOUND);
   // findPosition
   assert(findPosition(&c->b[0], 0) == 0);
   assert(findPosition(&c->b[0], 3) == 1);
   assert(findPosition(&c->b[0], 5) == 2);
   // isValExist
   assert(!isValExist(c->b[0].msk, 63));
   assert(isValExist(c->b[0].msk, 2));
   assert(isValExist(c->b[0].msk, 3));
   // void incrementBlock(inside csa_set)
   // so we test if incrementBlock works as expected 
   // by testing after csa_set possibly call incrementBlock
   assert(csa_set(c, 67,  10));
   assert(findBlock(c, 67) == 1);
   assert(csa_set(c, 257,  10));
   assert(findBlock(c, 257) == 2);
   // void update_block(inside csa_set)
   // if we can retrieve the previously set value
   // it means update_block works
   assert(csa_get(c,  67, &n));
   assert(csa_get(c,  257, &n));
   csa_free(&c);
   
   // move_and_insert
   // desired: if we insert block with lower offset
   // it will still stay ordered by offset
   c = csa_init();
   assert(csa_set(c, 257,  25));
   assert(c->b[0].offset == 256);
   assert(csa_set(c, 66,  30));
   assert(c->b[0].offset == 64);
   assert(c->b[1].offset == 256);
   assert(csa_set(c, 2,  10));
   assert(c->b[0].offset == 0);
   assert(c->b[1].offset == 64);
   assert(c->b[2].offset == 256);
   // concat_data
   str[0] = '\0'; // reset str
   concat_data(c, str);
   assert(strcmp(str, "{1|[2]=10}{1|[66]=30}{1|[257]=25}")==0);

   #ifdef EXT
   // csa_foreach
   // csa_delete
   // delete
   // additional_free
   #endif

   // clean up: for LeakSanitizer
   csa_free(&c);
}

#ifdef EXT
void csa_foreach(void (*func)(int* p, int* ac), csa* c, int* ac)
{
   if (c == NULL) {
      return;
   }
   for (int i=0 ; i<c->n ; i++) {
      int counts = findPosition(&c->b[i], MSKLEN);
      for (int j=0 ; j<counts ; j++) {
         func(&c->b[i].vals[j], ac);
      }
   }
}

bool csa_delete(csa* c, int idx)
{
   if (c == NULL) {
      return false;
   }
   // find which block
   int bp_idx = findBlock(c, idx);
   if (bp_idx == BLOCK_NOT_FOUND) {
      return false;
   }
   block *bp = &c->b[bp_idx];
   
   // find which pos inside block
   int bit_idx = idx % MSKLEN;
   int pos = findPosition(bp, bit_idx);
   // delete
   bool succ = delete(c, bp_idx, bit_idx, pos);
   if (succ) {
      return true;
   }
   return false;
}
#endif

// helpers
void incrementBlock(csa* c, int idx)
{
   int offset_unit = idx / MSKLEN;
   
   // create new block
   block new_block;
   new_block.vals = NULL;
   new_block.msk = 0;
   new_block.offset = offset_unit * MSKLEN;
   
   // update csa
   c->b = realloc(c->b, sizeof(block) * (c->n + 1));
   c->b[c->n] = new_block;
   c->n++;
   // linear move and insert (keep ordered by offset)
   move_and_insert(c, new_block.offset);
}

void update_block(csa* c, int idx, int val)
{
   // indexes
   int bit_idx = idx % MSKLEN;
   // update block
   int bp_idx = findBlock(c, idx);
   block *bp = &c->b[bp_idx];
   // 1. find size and position
   int counts = findPosition(bp, MSKLEN); // total occpuied
   int pos = findPosition(bp, bit_idx);   // insert position
   // 2. realloc and move values if needed
   if (!isValExist(bp->msk, bit_idx)) {
      int new_len = counts + 1;
      bp->vals = realloc(bp->vals, sizeof(int) * (new_len));
      // linear move one index to the right
      for (int i = counts; i > pos; i--) {
         bp->vals[i] = bp->vals[i-1];
      }
   }
   // 3. update vals(array)
   bp->vals[pos] = val;
   // 4. upadte mask
   bp->msk |= ((mask_t)1 << bit_idx);
}

int findBlock(csa* c, int idx)
{
   int offset_unit = idx / MSKLEN;
   unsigned int offset_key = offset_unit * MSKLEN;
   for (int i = 0; i < c->n; i++){
      if (c->b[i].offset == offset_key) {
         return i;
      }
   }
   return BLOCK_NOT_FOUND;
}

int findPosition(block *bp, int bit_idx)
{
   int pos = 0; // == how many occpuied before pos
   for (int i = 0; i < bit_idx; i++) {
      if (bp->msk & ((mask_t)1 << i)) {
         pos++;
      }
   }
   return pos;
}

void move_and_insert(csa *c, unsigned int offset)
{
   // noted: n has been updated
   int last_idx = c->n-ZERO_INDEX_OFFSET;
   block temp = c->b[last_idx];
   int pos = 0;
   for (int i = 0; i < last_idx; i++){
      if (c->b[i].offset < offset) {
         pos++;
      }
   }
   for (int i = last_idx; i > pos; i--) {
      c->b[i] = c->b[i-1];
   }
   c->b[pos] = temp;
}

bool delete(csa *c, int bp_idx, int bit_idx, int pos)
{
   if (c == NULL) {
      return false;
   }

   block *bp = &c->b[bp_idx];
   // check if val exists
   if (!isValExist(bp->msk, bit_idx)) {
      return false;
   }
   // 1. free block val
   // 1.a set to 0
   int counts = findPosition(bp, MSKLEN);
   // 1.b move one index to the left
   for (int i = pos; i < counts-ZERO_INDEX_OFFSET ; i++) {
      bp->vals[i] = bp->vals[i+1];
   }
   // 1.c realloc 1 index
   int new_len = counts-DESIRED_RESIZE;
   bp->vals = realloc(bp->vals, sizeof(int) * (new_len));
   bp->msk &= ~((mask_t)1 << bit_idx);

   // 3. if whole block is empty, free it from csa ?
   additional_free(c, bp_idx, new_len);
   return true;
}

void concat_data(csa *c, char* s)
{
   char tmp[BIGSTR];
   for (int i = 0; i < c->n; i++) {
      
      // open '{'
      strcat(s, "{");
   
      int count = findPosition(&c->b[i], MSKLEN);
      snprintf(tmp, BIGSTR , "%d|", count);
      strcat(s, tmp);
      
      int val_idx = 0;
      for (int bit_idx = 0; bit_idx < MSKLEN; bit_idx++) {
         if (isValExist(c->b[i].msk, bit_idx)) {
            int full_idx = c->b[i].offset + bit_idx;
            int val = c->b[i].vals[val_idx];
            snprintf(tmp, BIGSTR, "[%d]=%d", full_idx, val);
            strcat(s, tmp);
            if (val_idx != count - 1){
               strcat(s, ":");
            }
            val_idx++;
         }
      }

      // close '}'
      strcat(s, "}");
   }
}

bool isValExist(mask_t msk, int bit_idx)
{
   return (msk & ((mask_t)1 << bit_idx)) != NONE_OVERLAP;
}

void additional_free(csa *c, int bp_idx, int new_len)
{
   if (new_len == 0){
      // 3.a move the remaining blocks to the left
      for (int i = bp_idx; i < c->n-1; i++) {
         c->b[i] = c->b[i+1];
      }
      c->n--;
      c->b = realloc(c->b, sizeof(block) * (c->n));
   }

   // if csa is empty
   if (c->n == 0) {
      free(c->b);
      c->b = NULL;
   }
}
