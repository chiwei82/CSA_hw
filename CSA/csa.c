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
   // If any of the pointers is NULL, 
   // or if the cell is unset, returns false.
   if (c == NULL || val == NULL) {
      return false;
   }
   // 1. check if block exists
   int bit_index = idx % MSKLEN;
   int bp_index = findBlock(c, idx);
   if (bp_index == BLOCK_NOT_FOUND) {
      return false;
   }
   block *bp = &c->b[bp_index];
   // 2. check if value exists
   bool isExist = bp->msk & ((mask_t)1 << bit_index);
   if (!isExist) {
      return false;
   }
   // 3. get value
   int pos = findPosition(bp, bit_index);
   *val = bp->vals[pos];
   return true;
}

bool csa_set(csa* c, int idx, int val)
{
   if (c == NULL) {
      return false;
   } //
   // 1. if not block exists do block increment
   int bp_index = findBlock(c, idx);
   if (bp_index == BLOCK_NOT_FOUND) {
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
   for (int i = 0; i < c->n; i++) {
      
      // close '{'
      strcat(s, "{");
   
      int count = findPosition(&c->b[i], MSKLEN);
      snprintf(tmp, BIGSTR , "%d|", count);
      strcat(s, tmp);
      
      int cur_val_index = 0;
      for (int bit_index = 0; bit_index < MSKLEN; bit_index++) {
         if (c->b[i].msk & ((mask_t)1 << bit_index)) {
            int full_index = c->b[i].offset + bit_index;
            int ram_val = c->b[i].vals[cur_val_index];
            snprintf(tmp, BIGSTR, "[%d]=%d", full_index, ram_val);
            strcat(s, tmp);
            if (cur_val_index != count - 1){
               strcat(s, ":");
            }
            cur_val_index++;
         }
      }

      // close '}'
      strcat(s, "}");
   }
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

   csa_tostring(c, str);
   assert(strcmp(str, "")==0);
   // init
   c = csa_init();
   assert(!csa_get(c, 0, &n));
   assert(!csa_get(c, 2, &n));
   // setting test
   assert(csa_set(c, 2,  25));
   // Add csa[3]=30
   assert(csa_set(c, 3,  30));
   // Add csa[63]=100
   assert(csa_set(c, 100, 100));
   // Getters
   assert(csa_get(c,  2, &n));
   assert(n==25);
   assert(csa_get(c,  3, &n));
   assert(n==30);
   assert(csa_get(c, 100, &n));
   assert(n==100);
   csa_tostring(c, str);

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
   int bp_index = findBlock(c, idx);
   if (bp_index == BLOCK_NOT_FOUND) {
      return false;
   }
   
   block *bp = &c->b[bp_index];
   // find which pos inside block
   int bit_index = idx % MSKLEN;
   int pos = findPosition(bp, bit_index);
   // delete
   // bp is pointed to desired block
   // bit_index is the index to unset inside block msk << ?
   // pos is the index to unset inside block val[pos]
   bool succ = delete(c, bp_index, bit_index, pos);
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
   int bit_index = idx % MSKLEN;
   // update block
   int bp_index = findBlock(c, idx);
   block *bp = &c->b[bp_index];
   // 1. find size and position
   int counts = findPosition(bp, MSKLEN); // total occpuied
   int pos = findPosition(bp, bit_index); // insert position
   // 2. realloc and move values if needed
   bool val_existed = bp->msk & ((mask_t)1 << bit_index);
   if (!val_existed) {
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
   bp->msk |= ((mask_t)1 << bit_index);
}

int findBlock(csa* c, int idx)
{
   int block_index = idx / MSKLEN;
   unsigned int offset_key = block_index * MSKLEN;
   for (int i = 0; i < c->n; i++){
      if (c->b[i].offset == offset_key) {
         return i;
      }
   }
   return BLOCK_NOT_FOUND;
}

int findPosition(block *bp, int bit_index)
{
   int pos = 0; // == how many occpuied before pos
   for (int i = 0; i < bit_index; i++) {
      if (bp->msk & ((mask_t)1 << i)) {
         pos++;
      }
   }
   return pos;
}

void move_and_insert(csa *c, unsigned int offset)
{
   // noted: n has been updated
   block temp = c->b[c->n-1];
   int pos = 0;
   for (int i = 0; i < c->n-1; i++){
      if (c->b[i].offset < offset) {
         pos++;
      }
   }
   for (int i = c->n-1; i > pos; i--) {
      c->b[i] = c->b[i-1];
   }
   c->b[pos] = temp;
}

bool delete(csa *c, int bp_index, int bit_index, int pos)
{
   if (c == NULL) {
      return false;
   }

   block *bp = &c->b[bp_index];
   if (bp == NULL) {
      return false;
   }

   bool isExist = bp->msk & ((mask_t)1 << bit_index);
   if (!isExist) {
      return false;
   }
   // 1. free block val
   // 1.a set to 0
   int counts = findPosition(bp, MSKLEN);
   // 1.b move one index to the left
   for (int i = pos; i < counts-1 ; i++) {
      bp->vals[i] = bp->vals[i+1];
   }
   // 1.c realloc 1 index
   int new_len = counts - 1;
   bp->vals = realloc(bp->vals, sizeof(int) * (new_len));

   // 2. update mask
   bp->msk &= ~((mask_t)1 << bit_index);

   // 3. if whole block is empty, free it from csa ?
   if (new_len == 0){
      // 3.a move the remaining blocks to the left
      for (int i = bp_index; i < c->n-1; i++) {
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
   return true;
}

// TODO?
// (free)(realloc)(move)(insert)
// csa increment/decrement -> arr b, n 
// block increment/decrement -> vals, msk
