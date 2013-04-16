/*
 * Copyright 2013 10gen Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */


#include <string.h>

#include "bson-memory.h"
#include "bson-utf8.h"


static BSON_INLINE void
bson_utf8_get_sequence (const char   *utf8,
                        bson_uint8_t *seq_length,
                        bson_uint8_t *first_mask)
{
   unsigned char c = *(const unsigned char *)utf8;
   bson_uint8_t m;
   bson_uint8_t n;

   /*
    * See the following[1] for a description of what the given multi-byte
    * sequences will be based on the bits set of the first byte. We also need
    * to mask the first byte based on that.  All subsequent bytes are masked
    * against 0x3F.
    *
    * [1] http://www.joelonsoftware.com/articles/Unicode.html
    */

   if ((c & 0x80) == 0) {
      n = 1;
      m = 0x7F;
   } else if ((c & 0xE0) == 0xC0) {
      n = 2;
      m = 0x1F;
   } else if ((c & 0xF0) == 0xE0) {
      n = 3;
      m = 0x0F;
   } else if ((c & 0xF8) == 0xF0) {
      n = 4;
      m = 0x07;
   } else if ((c & 0xFC) == 0xF8) {
      n = 5;
      m = 0x03;
   } else if ((c & 0xFE) == 0xFC) {
      n = 6;
      m = 0x01;
   } else {
      n = 0;
      m = 0;
   }

   *seq_length = n;
   *first_mask = m;
}


bson_bool_t
bson_utf8_validate (const char *utf8,
                    size_t      utf8_len,
                    bson_bool_t allow_null)
{
   bson_uint8_t first_mask;
   bson_uint8_t seq_length;
   int i;
   int j;

   bson_return_val_if_fail(utf8, FALSE);

   for (i = 0; i < utf8_len; i += seq_length) {
      bson_utf8_get_sequence(&utf8[i], &seq_length, &first_mask);
      if (!seq_length) {
         return FALSE;
      }
      /*
       * TODO: Might make sense to validate each of the trailing bytes
       *       based on the sequence length.
       */
      if (!allow_null) {
         for (j = 0; j < seq_length; j++) {
            if (((i + j) > utf8_len) || !utf8[i + j]) {
               return FALSE;
            }
         }
      }
   }

   return TRUE;
}


char *
bson_utf8_escape_for_json (const char *utf8,
                           ssize_t     utf8_len)
{
   unsigned int i = 0;
   unsigned int o = 0;
   bson_uint8_t seq_len;
   bson_uint8_t mask;
   char *ret;

   bson_return_val_if_fail(utf8, NULL);

   if (utf8_len < 0) {
      utf8_len = strlen(utf8);
   }

   ret = bson_malloc0((utf8_len * 2) + 1);

   while (i < utf8_len) {
      bson_utf8_get_sequence(&utf8[i], &seq_len, &mask);

      if ((i + seq_len) > utf8_len) {
         bson_free(ret);
         return NULL;
      }

      switch (utf8[i]) {
      case '"':
      case '\\':
         ret[o++] = '\\';
      default:
         memcpy(&ret[o], &utf8[i], seq_len);
         o += seq_len;
         break;
      }

      i += seq_len;
   }

   return ret;
}


bson_unichar_t
bson_utf8_get_char (const char *utf8)
{
   bson_unichar_t c;
   bson_uint8_t mask;
   bson_uint8_t num;
   int i;

   bson_return_val_if_fail(utf8, -1);

   bson_utf8_get_sequence(utf8, &num, &mask);
   c = (*utf8) & mask;
   for (i = 1; i < num; i++) {
      c = (c << 6) | (utf8[i] & 0x3F);
   }

   return c;
}


const char *
bson_utf8_next_char (const char *utf8)
{
   bson_uint8_t mask;
   bson_uint8_t num;

   bson_return_val_if_fail(utf8, NULL);

   bson_utf8_get_sequence(utf8, &num, &mask);

   return utf8 + num;
}
