/* OP_STR1.C
 * String handling opcodes
 * Copyright (c) 2007 Ladybridge Systems, All Rights Reserved
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3, or (at your option)
 * any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 * 
 * START-HISTORY:
 * 31 Dec 23 SD launch - prior history suppressed
 * 02 Jul 24 mab max string size test in op_str, op_cat, op_append 
 * END-HISTORY
 *
 * START-DESCRIPTION:
 *
 *    op_cat         CAT
 *    op_append      APPEND
 *    op_char        CHAR
 *    op_dncase      DNCASE
 *    op_fold        FOLD
 *    op_len         LEN
 *    op_remove      REMOVE
 *    op_rmvf        RMVF
 *    op_seq         SEQ
 *    op_space       SPACE
 *    op_str         STR
 *    op_substr      SUBSTR
 *    op_substra     SUBSTRA      Substring assignment
 *    op_substre     SUBSTRE
 *    op_upcase      UPCASE
 *
 * END-DESCRIPTION
 *
 * START-CODE
 */

#include "sd.h"

Private void substra(bool pick_style);
Private void form_substr(int32_t substr_start, int32_t substr_len);
Private STRING_CHUNK* copy_substr(STRING_CHUNK* src_str,
                                  STRING_CHUNK* tgt_chunk,
                                  int32_t substr_start,
                                  int32_t substr_len);

/* ======================================================================
   op_cat()  -  Concatenate string 1 to string 2                          */

void op_cat() {
  /* Stack:

     |=============================|=============================|
     |            BEFORE           |           AFTER             |
     |=============================|=============================|
 top |  String to append           | Result string               |
     |-----------------------------|-----------------------------| 
     |  String to which to append  |                             |
     |=============================|=============================|
 */

  DESCRIPTOR* arg1;
  int32_t len1; /* Length of arg1 */
  STRING_CHUNK* str1;

  DESCRIPTOR* arg2;
  int32_t len2; /* Length of arg2 */
  STRING_CHUNK* str2;

  DESCRIPTOR result;
  int32_t s_len;   /* Length of concatenated string */
  int64_t s_len64; /* Length of concatenated string MAX_STRING test */

  STRING_CHUNK* tgt; /* Ptr to target string */
  int16_t actual_size;

  /* Find arg1, the string to be concatenated to string 2 */

  arg1 = e_stack - 1;
  k_get_string(arg1);
  str1 = arg1->data.str.saddr;

  /* Find arg2, the string to which we are to concatenate string 1 */

  arg2 = e_stack - 2;
  k_get_string(arg2);
  str2 = arg2->data.str.saddr;

  if (str1 == NULL) {
    /* Concatenating a null string - often done for type conversion */
    k_dismiss(); /* Simply cast off the arg1 descriptor */
    arg2->flags = 0;
  } else if (str2 == NULL) {
    /* Concatenating to a null string */
    /* Move arg1 to arg2 and drop the stack */

    k_release(arg2);
    *arg2 = *arg1;
    arg2->flags = 0;
    e_stack--;
  } else /* Really have some work to do */
  {
    /* Establish size of concatenated strings */

    len1 = str1->string_len;
    len2 = str2->string_len;

    /* 020240702 mab Max String Size test */
    s_len64 = len1 + len2;
    if (s_len64 > MAX_STRING_SIZE){
      k_error(sysmsg(10004));   /* Operation exceeds MAX_STRING_SIZE */
    }

    /* ok to cast to 32 bit */
    s_len = (int32_t)s_len64;

    /* Create a target descriptor in our C stack space */

    result.type = STRING;
    result.flags = 0;

    /* Allocate first chunk of target string */

    tgt = s_alloc(s_len, &actual_size);
    result.data.str.saddr = tgt;
    tgt->string_len = s_len;
    tgt->ref_ct = 1;

    tgt = copy_substr(str2, tgt, 0L, len2); /* Copy first part of string */
    (void)copy_substr(str1, tgt, 0L, len1); /* Append second part of string */

    k_dismiss(); /* Remove source strings */
    k_dismiss();

    *(e_stack++) = result; /* Put result on stack */
  }
}

/* ======================================================================
   op_append()  -  Concatenate string 1 to string 2 in situ              */

void op_append() {
  /* Stack:

     |=============================|=============================|
     |            BEFORE           |           AFTER             |
     |=============================|=============================|
 top |  String to append           |                             |
     |-----------------------------|-----------------------------| 
     |  ADDR of target string      |                             |
     |=============================|=============================|
 */

  DESCRIPTOR* src_descr;
  int32_t src_len;
  STRING_CHUNK* src;

  DESCRIPTOR* tgt_descr;
  int32_t tgt_len;
  STRING_CHUNK* tgt;
  STRING_CHUNK* prev_tgt;

  STRING_CHUNK* new_head;
  int32_t new_len;
  STRING_CHUNK* new_str;
  int16_t chunk_size;

  int32_t s_len;   /* Length of concatenated string */
  int64_t s_len64; /* Length of concatenated string MAX_STRING test */
  /* Find the source string */

  src_descr = e_stack - 1;
  k_get_string(src_descr);
  src = src_descr->data.str.saddr;

  /* Find the target string */

  tgt_descr = e_stack - 2;
  while (tgt_descr->type == ADDR)
    tgt_descr = tgt_descr->data.d_addr;
  GetString(tgt_descr);
  tgt = tgt_descr->data.str.saddr;

  /* Ensure we do not leave a remove pointer active and mark descriptor
    as changed for debugger.                                           */

  tgt_descr->flags &= ~(DF_REMOVE | DF_CHANGE);

  if (src == NULL) {
    /* Concatenating a null string - often done for type conversion */
    k_dismiss(); /* Simply cast off the source... */
    k_pop(1);    /* ...and target descriptors */
  } else if (tgt == NULL) {
    /* Concatenating to a null string */
    /* Move src to the target descriptor and drop the stack */

    tgt_descr->data.str.saddr = src;
    k_pop(2); /* Do not release */
  } else      /* Really have some work to do */
  {
    /* Establish size of concatenated strings */

    src_len = src->string_len;
    tgt_len = tgt->string_len;
    
    /* 020240702 mab Max String Size test */
    s_len64 = src_len + tgt_len;
    if (s_len64 > MAX_STRING_SIZE){
      k_error(sysmsg(10004));   /* Operation exceeds MAX_STRING_SIZE */
    }

    /* ok to cast to 32 bit */
    s_len = (int32_t)s_len64;


    /* If the target string has a reference count of one we can optimise
      by appending string1 to it without copying any earlier chunks.     */

    if (tgt->ref_ct == 1) {
      /* Update the total string length while this chunk is mapped */

      tgt->string_len = s_len;

      /* Find the final chunk of the string */

      prev_tgt = NULL;
      while (tgt->next != NULL) {
        prev_tgt = tgt;
        tgt = tgt->next;
      }

      if (tgt->alloc_size >= s_len) {
        /* The entire result will fit in the allocated size of the target
          string. Append the new data.                                   */

        do {
          memcpy(tgt->data + tgt->bytes, src->data, src->bytes);
          tgt->bytes += src->bytes;
          src = src->next;
        } while (src != NULL);

        k_dismiss(); /* Remove source... */
        k_pop(1);    /* ...and target descriptors */
        goto exit_op_append;
      }

      if (tgt->bytes < MAX_STRING_CHUNK_SIZE) {
        if (tgt->bytes == tgt->alloc_size) {
          /* Allocate a replacement allowing some slack space for potential
            future growth.                                                 */

          new_len = src_len + tgt->bytes + (tgt->alloc_size / 2);
          new_str = s_alloc(new_len, &chunk_size);

          /* Copy old data and rechain chunks, freeing old one */

          memcpy(new_str->data, tgt->data, tgt->bytes);
          new_str->bytes = tgt->bytes;

          if (prev_tgt == NULL) /* First chunk */
          {
            new_str->string_len = tgt->string_len;
            new_str->ref_ct = 1;

            tgt_descr->data.str.saddr = new_str;
          } else {
            prev_tgt->next = new_str;
          }

          s_free(tgt);
          tgt = new_str;
        } else {
          chunk_size = (int16_t)(src_len + tgt->bytes);
        }
      } else
        chunk_size = (int16_t)min(src_len, MAX_STRING_CHUNK_SIZE);

      /* Append string 1 to string 2 */

      while (src != NULL) {
        tgt = copy_string(tgt, &(src_descr->data.str.saddr), src->data,
                          src->bytes, &chunk_size);
        src = src->next;
      }

      k_dismiss(); /* Remove source... */
      k_pop(1);    /* ...and target descriptors */
      goto exit_op_append;
    }

    /* Allocate first chunk of target string */

    new_head = s_alloc(s_len, &chunk_size);
    new_str = new_head;
    new_str->string_len = s_len;
    new_str->ref_ct = 1;

    /* Copy first part of string */

    new_str = copy_substr(tgt, new_head, 0L, tgt_len);

    /* Append second part of string */

    (void)copy_substr(src, new_str, 0L, src_len);

    k_release(tgt_descr);
    InitDescr(tgt_descr, STRING);
    tgt_descr->data.str.saddr = new_head;
    k_dismiss(); /* Remove source and... */
    k_pop(1);    /* ...target descriptors */
  }

exit_op_append:
  return;
}

/* ======================================================================
   op_char()  -  Form ASCII character from collating sequence value       */

void op_char() {
  DESCRIPTOR* seq_descr;
  STRING_CHUNK* str;
  int16_t n;
  char c;

  seq_descr = e_stack - 1;
  GetInt(seq_descr);
  c = (char)(seq_descr->data.value & 0xFF);

  InitDescr(seq_descr, STRING);
  str = seq_descr->data.str.saddr = s_alloc(1L, &n);
  str->ref_ct = 1;
  str->string_len = 1;
  str->bytes = 1;
  str->data[0] = c;
}

/* ======================================================================
   op_dncase()  -  Convert string to lowercase                            */

void op_dncase() {
  set_case(FALSE);
}

/* ======================================================================
   op_fold()  -  Fold string into fixed width fragments                   */

void op_fold() {
  /* Stack:

     |=============================|=============================|
     |            BEFORE           |           AFTER             |
     |=============================|=============================|
 top |  Width                      | Result string               |
     |-----------------------------|-----------------------------|
     |  Source string              |                             |
     |=============================|=============================|
 */

  k_put_string("\xfe", 1, e_stack++);
  k_recurse(pcode_fold, 3); /* Execute recursive code */
}

/* ======================================================================
   op_fold3()  -  Fold string (3 argument format)                         */

void op_fold3() {
  /* Stack:

     |=============================|=============================|
     |            BEFORE           |           AFTER             |
     |=============================|=============================|
 top |  Delimiter                  | Result string               |
     |-----------------------------|-----------------------------|
     |  Width                      |                             |
     |-----------------------------|-----------------------------|
     |  Source string              |                             |
     |=============================|=============================|
 */

  k_recurse(pcode_fold, 3); /* Execute recursive code */
}

/* ======================================================================
   op_len()  -  Return length of string                                   */

void op_len() {
  DESCRIPTOR* descr;
  int32_t s_len;
  STRING_CHUNK* str;

  descr = e_stack - 1;
  k_get_string(descr);

  str = descr->data.str.saddr;
  if (str == NULL)
    s_len = 0;
  else
    s_len = str->string_len;

  k_release(descr); /* Cast off string chunks */

  InitDescr(descr, INTEGER);
  descr->data.value = s_len;
}

/* ======================================================================
   op_remove()  -  Remove delimited substring from source                 */

void op_remove() {
  /* Stack:

     |=============================|=============================|
     |            BEFORE           |           AFTER             |
     |=============================|=============================|
 top |  ADDR to variable to        | Substring                   |
     |  receive delimiter code     |                             |
     |-----------------------------|-----------------------------| 
     |  ADDR to source string      |                             |
     |=============================|=============================|
 */

  DESCRIPTOR* delim_descr; /* Descriptor to receive delimiter code */

  DESCRIPTOR* src_descr; /* Ptr to source string descriptor */
  STRING_CHUNK* src;
  int16_t offset; /* Offset into current source chunk */
  int16_t bytes_remaining;
  int16_t len;

  STRING_CHUNK* tgt;
  STRING_CHUNK* new_tgt;
  int16_t actual_len;

  char* p;
  char* q;
  int32_t bytes;
  bool done;

  /* Follow ADDR chain to leave delim_descr pointing to descriptor to
    receive the delimiter code.                                         */

  delim_descr = e_stack - 1;
  do {
    delim_descr = delim_descr->data.d_addr;
  } while (delim_descr->type == ADDR);
  k_pop(1); /* Dismiss ADDR */

  k_release(delim_descr);
  InitDescr(delim_descr, INTEGER);
  delim_descr->data.value = 0; /* For end of string paths */

  /* Get source string. For this opcode, we must have a pointer to the actual
    string descriptor rather than a copy of it as we may need to set up the
    remove pointer. The source descriptor is always a ADDR; literals and
    expressions are not allowed with REMOVE.                                 */

  src_descr = e_stack - 1;
  while (src_descr->type == ADDR)
    src_descr = src_descr->data.d_addr;
  k_pop(1); /* Dismiss ADDR */
  if (src_descr->type != STRING)
    k_get_string(src_descr); /* Ensure that it is a string */

  /* Set up a string descriptor to receive the result */

  InitDescr(e_stack, STRING);
  e_stack->data.str.saddr = NULL;

  if (src_descr->data.str.saddr == NULL) /* Null source */
  {
    goto exit_remove;
  }

  if ((src_descr->flags & DF_REMOVE)) /* Remove pointer exists */
  {
    offset = src_descr->n1;
    src = src_descr->data.str.rmv_saddr;
  } else /* No remove pointer */
  {
    src = src_descr->data.str.saddr;
    offset = 0;
    src_descr->flags |= DF_REMOVE;
  }

  bytes_remaining = src->bytes - offset;
  if (bytes_remaining < 0) /* Points off end of string (must be last chunk) */
  {
    goto update_remove_table; /* End of string */
  }

  /* Copy portion of the source string up to the next delimiter to the
    result string. We do this in chunks of up to the size of the current
    source chunk.                                                        */

  bytes = 0;
  done = FALSE;
  do {
    /* Is there any data left in this chunk? */

    if (bytes_remaining > 0) {
      /* Look for delimiter in the current chunk */

      p = src->data + offset;

      for (q = p; bytes_remaining-- > 0; q++) {
        if (IsMark(*q)) {
          delim_descr->data.value = 256 - (u_char)*q;
          len = q - p;
          offset += len + 1;
          done = TRUE;
          break;
        }
      }

      /* Copy substring */

      if (!done)
        len = q - p;

      if (len > 0) {
        new_tgt = s_alloc((int32_t)len, &actual_len);
        if (bytes == 0) /* First chunk */
        {
          e_stack->data.str.saddr = new_tgt;
        } else /* Appending subsequent chunk */
        {
          tgt->next = new_tgt;
        }
        tgt = new_tgt;

        tgt->bytes = len;
        memcpy(tgt->data, p, len);
        bytes += len;
      }
    }

    if (!done) /* Find next chunk */
    {
      if (src->next == NULL) {
        offset = src->bytes + 1; /* Leave off end of string */
        done = TRUE;
      } else {
        src = src->next;
        bytes_remaining = src->bytes;
        offset = 0;
      }
    }
  } while (!done);

  /* Go back and set total string length in first chunk of result */

  if (bytes > 0) {
    tgt = e_stack->data.str.saddr;
    tgt->string_len = bytes;
    tgt->ref_ct = 1;
  }

update_remove_table:
  /* Update remove pointer */

  src_descr->n1 = offset;
  src_descr->data.str.rmv_saddr = src;

exit_remove:
  e_stack++;
}

/* ======================================================================
   op_rmvf()  -  Remove field(s) from source                              */

void op_rmvf() {
  /* Stack:

     |=============================|=============================|
     |            BEFORE           |           AFTER             |
     |=============================|=============================|
 top |  Number of fields to remove | Substring                   |
     |-----------------------------|-----------------------------| 
     |  ADDR to source string      |                             |
     |=============================|=============================|

     STATUS() = 0   ok
              = 1   null string
              = 2   end of string
 */

  DESCRIPTOR* descr;
  int32_t no_fields; /* Number of fields to remove */

  int16_t offset; /* Offset into current source chunk */

  STRING_CHUNK* str_hdr;
  STRING_CHUNK* str;
  char* src;
  int16_t bytes_remaining;
  char* p;
  int16_t n;
  bool is_sellist;

  process.status = 0;

  /* Get number of fields */

  descr = e_stack - 1;
  GetInt(descr);
  no_fields = descr->data.value;
  k_pop(1);
  if (no_fields < 1)
    no_fields = 1;

  /* Get source string. For this opcode, we must have a pointer to the actual
    string descriptor rather than a copy of it as we may need to set up the
    remove pointer. The source descriptor is always an ADDR; literals and
    expressions are not allowed with REMOVEF.                                */

  descr = e_stack - 1;
  while (descr->type == ADDR)
    descr = descr->data.d_addr;
  k_pop(1); /* Dismiss ADDR */
  if ((descr->type != STRING) && (descr->type != SELLIST)) {
    k_get_string(descr); /* Ensure that it is a string */
  }
  is_sellist = (descr->type == SELLIST);

  /* Set up a string descriptor to receive the result. The e-stack pointer is
    not incremented until we are about to return as we need to update this
    descriptor later.                                                        */

  InitDescr(e_stack, STRING);
  e_stack->data.str.saddr = NULL;
  ts_init(&(e_stack->data.str.saddr), 32);

  if ((str_hdr = descr->data.str.saddr) == NULL) /* Null string */
  {
    process.status = 1;
    goto exit_removef;
  }

  /* Find or create remove pointer */

  if ((descr->flags & DF_REMOVE)) /* Remove pointer exists */
  {
    offset = descr->n1;
    str = descr->data.str.rmv_saddr;
  } else /* No remove pointer */
  {
    str = descr->data.str.saddr;
    offset = 0;
    descr->flags |= DF_REMOVE;
  }

  /* Find current position in source string */

  bytes_remaining = str->bytes - offset;
  if (bytes_remaining < 0) /* Points off end of string (must be last chunk) */
  {
    process.status = 2;
    goto update_remove_table; /* End of string */
  }

  src = str->data + offset;

  /* Copy source string up to desired end point */

  while (no_fields) {
    if (bytes_remaining == 0)
      p = NULL;
    else
      p = (char*)memchr(src, FIELD_MARK, bytes_remaining);

    if (p != NULL) /* Mark found */
    {
      n = (p - src) + 1;
      offset += n;

      if (--no_fields == 0) /* Final field */
      {
        if (n > 1)
          ts_copy(src, n - 1); /* Copy, excluding mark */
        if (is_sellist)
          str_hdr->offset--; /* Used to hold count */
        break;
      }

      ts_copy(src, n); /* Copy, including mark */
      src += n;
      bytes_remaining -= n;
    } else /* No mark in this chunk */
    {
      ts_copy(src, bytes_remaining);

      if (str->next == NULL) /* End of string */
      {
        offset = str->bytes + 1; /* Leave positioned off end of string */
        if (is_sellist)
          str_hdr->offset--; /* Used to hold count */
        break;
      }

      str = str->next;
      offset = 0;
      src = str->data;
      bytes_remaining = str->bytes;
    }
  }

  (void)ts_terminate();

update_remove_table:
  /* Update remove pointer */

  descr->n1 = offset;
  descr->data.str.rmv_saddr = str;

exit_removef:
  e_stack++;
  return;
}

/* ======================================================================
   op_seq()  -  Form collating sequence value of ASCII character          */

void op_seq() {
  DESCRIPTOR* seq_descr;
  u_int16_t n;
  STRING_CHUNK* str;

  seq_descr = e_stack - 1;
  k_get_string(seq_descr);

  str = seq_descr->data.str.saddr;
  if (str == NULL)
    n = 0; /* Null string */
  else
    n = (u_char)(str->data[0]);

  k_release(seq_descr);

  InitDescr(seq_descr, INTEGER);
  seq_descr->data.value = n;
}

/* ======================================================================
   op_space()  -  Generate space filled string                           */

void op_space() {
  DESCRIPTOR* descr;
  int32_t n;
  int32_t total_len;
  int16_t len;
  STRING_CHUNK* str;
  STRING_CHUNK* new_str;

  descr = e_stack - 1;
  GetInt(descr);
  total_len = n = descr->data.value;
  /* Do not dismiss as e-stack top is INTEGER and we will overlay it */

  descr->type = STRING;
  descr->data.str.saddr = NULL;

  while (n > 0) {
    new_str = s_alloc(n, &len);
    if (len > n)
      len = (int16_t)n;

    if (descr->data.str.saddr == NULL) {
      descr->data.str.saddr = new_str;
      new_str->ref_ct = 1;
      new_str->string_len = total_len;
    } else
      str->next = new_str;

    str = new_str;

    str->bytes = len;
    memset(str->data, ' ', len);

    n -= len;
  }
}

/* ======================================================================
   op_str()  -  Generate repeated string                                  */

void op_str() {
  DESCRIPTOR* descr;
  int32_t repeats;

  STRING_CHUNK* src;
  int32_t src_len;

  int16_t len;
  STRING_CHUNK* tgt;
  STRING_CHUNK* new_tgt;
  int16_t tgt_bytes_remaining;
  char* p;

  DESCRIPTOR result;
  int32_t total_len;
  int64_t total_len64;

  /* Get repeat count */

  descr = e_stack - 1;
  GetInt(descr);
  repeats = descr->data.value;
  k_pop(1);

  /* Get source string */

  descr = e_stack - 1;
  k_get_string(descr);
  src = descr->data.str.saddr;

  result.type = STRING;
  result.flags = 0;
  result.data.str.saddr = NULL;

  if (repeats == 1)
    goto same_as_source;
  if ((repeats <= 0) || (src == NULL))
    goto done;

  src_len = src->string_len;

  /* 020240702 mab Max String Size test */
  total_len64 = repeats * src_len;
  if (total_len64 > MAX_STRING_SIZE){
    k_error(sysmsg(10004));   /* Operation exceeds MAX_STRING_SIZE */
  }

  /* ok to cast to 32 bit */
  total_len = (int32_t)total_len64;

  if (src_len == 1) /* Optimise single character case */
  {
    while (repeats > 0) {
      new_tgt = s_alloc(repeats, &len);
      if (len > repeats)
        len = (int16_t)repeats;

      if (result.data.str.saddr == NULL) {
        result.data.str.saddr = new_tgt;
        new_tgt->ref_ct = 1;
        new_tgt->string_len = total_len;
      } else
        tgt->next = new_tgt;
      tgt = new_tgt;

      tgt->bytes = len;
      memset(tgt->data, src->data[0], len);

      repeats -= len;
    }
  } else /* Source string length > 1 */
  {
    /* Allocate initial target chunk */

    tgt = s_alloc(total_len, &tgt_bytes_remaining);
    result.data.str.saddr = tgt;

    tgt->ref_ct = 1;
    tgt->string_len = total_len;

    while (repeats-- > 0) {
      while (src != NULL) {
        src_len = src->bytes;
        p = src->data;

        if (src_len > tgt_bytes_remaining) /* Overflows current target chunk */
        {
          memcpy(tgt->data + tgt->bytes, p, tgt_bytes_remaining);
          p += tgt_bytes_remaining;
          src_len -= tgt_bytes_remaining;
          tgt->bytes += tgt_bytes_remaining;
          total_len -= tgt_bytes_remaining;

          /* Allocate a new chunk */

          new_tgt = s_alloc(total_len, &tgt_bytes_remaining);
          tgt->next = new_tgt;
          tgt = new_tgt;
        }

        memcpy(tgt->data + tgt->bytes, p, src_len);
        tgt->bytes += (int16_t)src_len;
        total_len -= src_len;
        tgt_bytes_remaining -= (int16_t)src_len;

        src = src->next;
      }

      src = descr->data.str.saddr; /* Reset source position */
    }
  }

done:
  k_dismiss(); /* Dismiss source string */
  *(e_stack++) = result;

same_as_source:
  return;
}

/* ======================================================================
   op_substr()  -  Substring extraction                                   */

void op_substr() {
  DESCRIPTOR* len_descr;
  DESCRIPTOR* start_descr;
  int32_t substr_start;
  int32_t substr_len;

  len_descr = e_stack - 1;
  GetInt(len_descr);
  substr_len = len_descr->data.value;
  if (substr_len < 0)
    substr_len = 0;

  start_descr = e_stack - 2;
  GetInt(start_descr);
  substr_start = start_descr->data.value - 1; /* From zero internally */
  if (substr_start < 0)
    substr_start = 0;

  k_pop(2);

  form_substr(substr_start, substr_len);
}

/* ======================================================================
   op_substra()  -  Substring assignment                                  */

void op_substra() {
  substra(FALSE);
}

void op_psubstra() {
  substra(TRUE);
}

Private void substra(bool pick_style) {
  /* Stack:

     |==============================|=============================|
     |            BEFORE            |           AFTER             |
     |==============================|=============================|
 top |  Replacement substring       |                             |
     |------------------------------|-----------------------------| 
     |  Substring region length     |                             |
     |------------------------------|-----------------------------| 
     |  Substring region offset     |                             |
     |------------------------------|-----------------------------| 
     |  ADDR to string to overlay   |                             |
     |==============================|=============================|
 */

  DESCRIPTOR* substr_descr;
  STRING_CHUNK* src_hdr;
  int16_t src_bytes_remaining;
  char* src;
  bool src_end;

  DESCRIPTOR* descr;

  DESCRIPTOR* tgt_descr;
  STRING_CHUNK* tgt_hdr;
  int32_t tgt_len;              /* Total length of string */
  int16_t tgt_bytes_remaining; /* Bytes left in current chunk */
  char* tgt;
  STRING_CHUNK* tgt_prev;
  STRING_CHUNK* tgt_tail;

  int32_t substr_start;
  int32_t substr_len;
  int32_t add_bytes;
  int16_t n;
  int16_t len;
  STRING_CHUNK* str;
  STRING_CHUNK* new_str;

  /* Get substring pointer */

  substr_descr = e_stack - 1;
  k_get_string(substr_descr);

  /* Get length */

  descr = e_stack - 2;
  GetInt(descr);
  substr_len = descr->data.value;
  if (substr_len <= 0)
    goto exit_op_substra; /* No action if length = 0 */

  /* Get offset */

  descr = e_stack - 3;
  GetInt(descr);
  substr_start = descr->data.value - 1; /* From zero internally */
  if (substr_start < 0)
    substr_start = 0;

  /* Get target string. We do not want to copy it as this will bump the
    reference count and makes our check for multiple references more
    complicated. Instead, just follow the ADDR chain (it must be an
    indirect reference) until we find the string itself.                */

  tgt_descr = e_stack - 4;
  do {
    tgt_descr = tgt_descr->data.d_addr;
  } while (tgt_descr->type == ADDR);
  if (tgt_descr->type != STRING)
    k_get_string(tgt_descr);

  tgt_hdr = tgt_descr->data.str.saddr;
  if (tgt_hdr == NULL) {
    if (!pick_style)
      goto exit_op_substra;
    tgt_len = 0;
  } else {
    tgt_len = tgt_hdr->string_len;
  }

  /* For Pick style substring assignment, we may need to add space filled
    chunks to the target string to bring it up to the correct size.      */

  if (pick_style) {
    add_bytes = substr_start + substr_len - tgt_len - 1; /* Size to add */
  } else {
    add_bytes = 0;

    if (tgt_len <= substr_start)
      goto exit_op_substra;

    /* Truncate replacement length if it over-runs the target string */

    if (substr_len > tgt_len - substr_start) {
      substr_len = tgt_len - substr_start;
    }
  }

  /* We now know that we are going to update the string. If the string has
    a reference count greater than one, we must copy it.                  */

  if ((tgt_hdr != NULL) && (tgt_hdr->ref_ct > 1)) {
    tgt_hdr->ref_ct--;
    tgt_hdr = k_copy_string(tgt_descr->data.str.saddr);
    tgt_descr->data.str.saddr = tgt_hdr;
  }

  if (add_bytes > 0) {
    if (tgt_hdr == NULL) /* Target is a null string */
    {
      while (add_bytes > 0) {
        new_str = s_alloc(add_bytes, &len);
        if (len > add_bytes)
          len = (int16_t)add_bytes;

        if (tgt_hdr == NULL) /* First chunk */
        {
          tgt_hdr = new_str;
          new_str->ref_ct = 1;
          new_str->string_len = add_bytes;
        } else
          str->next = new_str;

        str = new_str;
        str->bytes = len;
        memset(str->data, ' ', len);

        add_bytes -= len;
        tgt_len += len;
      }
      tgt_descr->data.str.saddr = tgt_hdr;
    } else /* Target has at least one chunk */
    {
      /* Find the final chunk */

      tgt_prev = NULL;
      tgt_tail = tgt_hdr;
      while (tgt_tail->next != NULL) {
        tgt_prev = tgt_tail;
        tgt_tail = tgt_tail->next;
      }

      /* If this chunk can be extended, replace it */

      if (tgt_tail->bytes < MAX_STRING_CHUNK_SIZE) {
        new_str = s_alloc(tgt_tail->bytes + add_bytes, &len);

        /* s_alloc() may return a chunk larger than we ask for. Ensure
          that len does not exceed the desired space count.           */

        if (len > tgt_tail->bytes + add_bytes)
          len = (int16_t)(tgt_tail->bytes + add_bytes);

        n = len - tgt_tail->bytes; /* Actual number of new bytes added */

        memcpy(new_str->data, tgt_tail->data, tgt_tail->bytes);
        memset(new_str->data + tgt_tail->bytes, ' ', n);
        new_str->bytes = len;

        if (tgt_prev == NULL) /* This is the only chunk */
        {
          new_str->string_len = tgt_hdr->string_len + n;
          new_str->ref_ct = 1;
          tgt_descr->data.str.saddr = new_str;
          tgt_hdr = new_str;
        } else /* Replace last chunk */
        {
          tgt_hdr->string_len += n;
          tgt_prev->next = new_str;
        }

        s_free(tgt_tail); /* Free replaced chunk */

        tgt_tail = new_str;

        tgt_len += n;
        add_bytes -= n; /* Still need this many more spaces */
      }

      /* Now add the remainder as new chunks */

      while (add_bytes) {
        new_str = s_alloc(add_bytes, &len);
        if (len > add_bytes)
          len = (int16_t)add_bytes;

        new_str->bytes = len;
        memset(new_str->data, ' ', len);

        tgt_tail->next = new_str;
        tgt_tail = new_str;

        add_bytes -= len;
        tgt_len += len;
        tgt_hdr->string_len += len;
      }
    }
  }

  /* Clear any hint */

  tgt_hdr->field = 0;

  tgt_descr->flags &= ~DF_CHANGE;

  /* Find position for replacement */

  while (substr_start >= tgt_hdr->bytes) {
    substr_start -= tgt_hdr->bytes;
    tgt_hdr = tgt_hdr->next;
  }

  tgt_bytes_remaining = (int16_t)(tgt_hdr->bytes - substr_start);
  tgt = tgt_hdr->data + substr_start;

  /* Find insertion string */

  src_hdr = substr_descr->data.str.saddr;
  if (src_hdr == NULL) {
    src_bytes_remaining = 0;
    src_end = TRUE;
  } else {
    src_bytes_remaining = src_hdr->bytes;
    src = src_hdr->data;
    src_end = FALSE;
  }

  /* Perform replacement */

  if (substr_len == 1) /* Optimised path for single byte replacement */
  {
    *tgt = (src_end) ? ' ' : *src;
    goto exit_op_substra;
  }

  do {
    if (src_bytes_remaining == 0) {
      if (!src_end) {
        if (src_hdr->next == NULL)
          src_end = TRUE;
        else {
          src_hdr = src_hdr->next;
          src_bytes_remaining = src_hdr->bytes;
          src = src_hdr->data;
        }
      }
    }

    if (tgt_bytes_remaining == 0) {
      if (tgt_hdr->next == NULL)
        break; /* End of string */

      tgt_hdr = tgt_hdr->next;
      tgt_bytes_remaining = tgt_hdr->bytes;
      tgt = tgt_hdr->data;
    }

    n = tgt_bytes_remaining;
    if (substr_len < n)
      n = (int16_t)substr_len;

    if (src_end) /* Space filling */
    {
      memset(tgt, ' ', n);
    } else {
      if (src_bytes_remaining < n)
        n = src_bytes_remaining;
      memcpy(tgt, src, n);
      src += n;
      src_bytes_remaining -= n;
    }

    tgt += n;
    tgt_bytes_remaining -= n;
    substr_len -= n;
  } while (substr_len);

exit_op_substra:

  k_dismiss(); /* Replacement string */
  k_pop(3);    /* Start offset, length, target string ADDR */
}

/* ======================================================================
   Full D3 style Pick substring assignment                                */

void op_psubstrb() {
  /* Stack:

     |==============================|=============================|
     |            BEFORE            |           AFTER             |
     |==============================|=============================|
 top |  Replacement substring       |                             |
     |------------------------------|-----------------------------| 
     |  Substring region length     |                             |
     |------------------------------|-----------------------------| 
     |  Substring region offset     |                             |
     |------------------------------|-----------------------------| 
     |  ADDR to string to overlay   |                             |
     |==============================|=============================|

     S[A,B] = C
 */

  DESCRIPTOR* descr;

  /* Substring region start (A) */
  int32_t substr_start;

  /* Substring region length (B) */
  int32_t substr_len;

  /* New substring (C) */
  DESCRIPTOR* new_descr;
  int32_t new_len;              /* Total length */
  STRING_CHUNK* new_str;         /* Current chunk... */
  char* newptr;                  /* ...position within it and... */
  int16_t new_bytes_remaining; /* ...bytes left in chunk */

  /* Source string (S) management */
  DESCRIPTOR* src_descr;
  STRING_CHUNK* src_hdr;
  int32_t src_len;              /* Total length of original string */
  STRING_CHUNK* src_str;         /* Current chunk... */
  char* src;                     /* ...position within it and... */
  int16_t src_bytes_remaining; /* ...bytes left in chunk */

  /* Result string */
  STRING_CHUNK* result;

  int32_t bytes;
  int16_t n;

  /* Get new substring pointer (C) */

  new_descr = e_stack - 1;
  k_get_string(new_descr);
  new_str = new_descr->data.str.saddr;
  if (new_str != NULL) {
    new_len = new_str->string_len;
    newptr = new_str->data;
    new_bytes_remaining = new_str->bytes;
  } else {
    new_len = 0;
    newptr = NULL;
    new_bytes_remaining = 0;
  }

  /* Get length (B) */

  descr = e_stack - 2;
  GetInt(descr);
  substr_len = descr->data.value;
  if (substr_len < 0)
    substr_len = 0;

  /* Get substring region start (A) */

  descr = e_stack - 3;
  GetInt(descr);
  substr_start = descr->data.value;

  /* Get source string (S). We do not want to copy it as this will bump the
    reference count and makes our check for multiple references more
    complicated. Instead, just follow the ADDR chain (it must be an
    indirect reference) until we find the string itself.                */

  src_descr = e_stack - 4;
  do {
    src_descr = src_descr->data.d_addr;
  } while (src_descr->type == ADDR);
  if (src_descr->type != STRING)
    k_get_string(src_descr);

  src_hdr = src_descr->data.str.saddr;
  src_len = (src_hdr == NULL) ? 0 : (src_hdr->string_len);
  src_str = src_hdr;

  /* If our substitution is totally within the existing string and it
    has a reference count of 1, we can optimise this by simply updating
    the string in-situ.                                                  */

  if ((substr_start > 0) && (substr_len != 0) && (substr_len == new_len) &&
      ((substr_start + substr_len) <= src_len + 1) && (src_str != NULL) &&
      (src_str->ref_ct == 1)) {
    /* Easy - Do the substitution in-situ */

    src_hdr->field = 0; /* Clear any hint */
    src_descr->flags &= ~DF_CHANGE;

    /* Find position for replacement */

    while (substr_start > src_str->bytes) {
      substr_start -= src_str->bytes;
      src_str = src_str->next;
    }

    src_bytes_remaining = (int16_t)(src_str->bytes - substr_start - 1);
    src = src_str->data + substr_start - 1;

    /* Perform replacement */

    if (substr_len == 1) /* Optimised path for single byte replacement */
    {
      *src = (newptr == NULL) ? ' ' : *newptr;
      goto exit_op_substrb;
    }

    do {
      bytes = min(new_bytes_remaining, src_bytes_remaining);
      memcpy(newptr, src, bytes);

      newptr += bytes;
      new_bytes_remaining -= (int16_t)bytes;
      if (new_bytes_remaining == 0) {
        new_str = new_str->next;
        if (new_str != NULL) {
          newptr = new_str->data;
          new_bytes_remaining = new_str->bytes;
        }
      }

      src += bytes;
      src_bytes_remaining -= (int16_t)bytes;
      if (src_bytes_remaining == 0) {
        src_str = src_str->next;
        if (src_str != NULL) {
          src = src_str->data;
          src_bytes_remaining = src_str->bytes;
        }
      }

      substr_len -= bytes;
    } while (substr_len);
  } else {
    /* We need to build a new string */

    if (src_str != NULL) {
      src = src_str->data;
      src_bytes_remaining = src_str->bytes;
    }

    ts_init(&result,
            abs(substr_start) + max(src_len - substr_len, 0) + new_len);
    result = NULL;

    if (substr_start >= 0) {
      /* Copy A-1 bytes */

      bytes = substr_start - 1;
      while ((src_str != NULL) && (bytes > 0)) {
        n = (int16_t)min(src_bytes_remaining, bytes);
        ts_copy(src, n);
        src += n;
        src_bytes_remaining -= n;
        bytes -= n;

        if (src_bytes_remaining == 0) /* Move to next chunk */
        {
          src_str = src_str->next;
          if (src_str != NULL) {
            src = src_str->data;
            src_bytes_remaining = src_str->bytes;
          }
        }
      }

      /* Add spaces if we ran out of characters to copy */

      if (bytes > 0)
        ts_fill(' ', bytes);

      /* Skip B characters from source */

      bytes = substr_len;
      while ((src_str != NULL) && (bytes > 0)) {
        n = (int16_t)min(src_bytes_remaining, bytes);
        src += n;
        src_bytes_remaining -= n;
        bytes -= n;

        if (src_bytes_remaining == 0) /* Move to next chunk */
        {
          src_str = src_str->next;
          if (src_str != NULL) {
            src = src_str->data;
            src_bytes_remaining = src_str->bytes;
          }
        }
      }

      /* Copy new data C */

      while (new_str != NULL) {
        ts_copy(new_str->data, new_str->bytes);
        new_str = new_str->next;
      }

      /* Copy remainder of original string */

      while (src_str != NULL) {
        ts_copy(src, src_bytes_remaining);
        src += src_bytes_remaining;

        src_str = src_str->next;
        if (src_str != NULL) {
          src = src_str->data;
          src_bytes_remaining = src_str->bytes;
        }
      }
    } else /* A < 0 */
    {
      /* Copy new data C */

      while (new_str != NULL) {
        ts_copy(new_str->data, new_str->bytes);
        new_str = new_str->next;
      }

      /* Insert abs(A) spaces */

      ts_fill(' ', -substr_start);

      /* Skip B characters from source */

      bytes = substr_len;
      while ((src_str != NULL) && (bytes > 0)) {
        n = (int16_t)min(src_bytes_remaining, bytes);
        src += n;
        src_bytes_remaining -= n;
        bytes -= n;

        if (src_bytes_remaining == 0) /* Move to next chunk */
        {
          src_str = src_str->next;
          if (src_str != NULL) {
            src = src_str->data;
            src_bytes_remaining = src_str->bytes;
          }
        }
      }

      /* Copy remainder of original string */

      while ((src_str != NULL)) {
        ts_copy(src, src_bytes_remaining);
        src += src_bytes_remaining;

        src_str = src_str->next;
        if (src_str != NULL) {
          src = src_str->data;
          src_bytes_remaining = src_str->bytes;
        }
      }
    }

    ts_terminate();

    if (src_hdr != NULL)
      k_deref_string(src_hdr);
    src_descr->data.str.saddr = result;
  }

exit_op_substrb:
  k_dismiss(); /* Replacement string */
  k_pop(3);    /* Start offset, length, target string ADDR */
}

/* ======================================================================
   op_substre()  -  Trailing substring extraction                         */

void op_substre() {
  DESCRIPTOR* len_descr;
  int32_t substr_len;

  len_descr = e_stack - 1;
  GetInt(len_descr);
  substr_len = len_descr->data.value;
  if (substr_len < 0)
    substr_len = 0;
  k_pop(1);

  form_substr(-1L, substr_len);
}

/* ======================================================================
   form_substr()  -  Common path for SUBSTR and SUBSTRE opcodes           */

Private void form_substr(int32_t substr_start, int32_t substr_len) {
  DESCRIPTOR* string_descr;
  STRING_CHUNK* src_str; /* Current chunk of source string */
  int32_t src_len;      /* Total length of source string */
  STRING_CHUNK* tgt_str; /* First chunk of target string */
  int16_t actual_len;

  /* Although we could optimise for the case of a reference count of one, this
    is an extremely unlikely case as it represents a substr of a literal.    */

  string_descr = e_stack - 1;
  k_get_string(string_descr);

  /* For the case of a null source string, we take no action as the source
    is also the result.                                                    */

  if ((src_str = string_descr->data.str.saddr) != NULL) /* Not a null string */
  {
    src_len = src_str->string_len;

    if (substr_start < 0) /* Trailing substring (from op_substre()) */
    {
      substr_start = src_len - substr_len;
      if (substr_start < 0)
        substr_start = 0;
    }

    if ((substr_start + substr_len) > src_len) /* Limit to end of source */
    {
      substr_len = src_len - substr_start;
    }

    if ((substr_start >= src_len) || (substr_len == 0)) {
      /* Return a null string */

      string_descr->data.str.saddr = NULL;
      k_deref_string(src_str); /* Decrement ref ct of source */
    } else if ((substr_start > 0) || (substr_len < src_len)) {
      /* Not returning entire source string */

      /* Allocate first chunk of target string and hook onto existing
        e-stack descriptor, setting known final size.                */

      string_descr->data.str.saddr = s_alloc(substr_len, &actual_len);
      tgt_str = string_descr->data.str.saddr;
      tgt_str->string_len = substr_len;
      tgt_str->ref_ct = 1;
      copy_substr(src_str, string_descr->data.str.saddr, substr_start,
                  substr_len);
      k_deref_string(src_str); /* Decrement ref ct of source */
    }
  }
}

/* ====================================================================== */

Private STRING_CHUNK* copy_substr(
    /* Returns STRING_CHUNK * of final target chunk used */
    STRING_CHUNK* src_str, /* First chunk of source */
    STRING_CHUNK* tgt_str, /* Chunk of target to append */
    int32_t substr_start,
    int32_t substr_len) {
  int32_t offset;       /* Offset into source string */
  int16_t chunk_space; /* Space available in target string chunk */
  char* tgt_ptr;         /* Byte ptr into target string */
  int16_t bytes;
  STRING_CHUNK* new_tgt;

  /* Walk up source string to chunk containing start of substring */

  offset = substr_start;
  while (offset >= src_str->bytes) {
    offset -= src_str->bytes;
    src_str = src_str->next;
  }

  chunk_space = tgt_str->alloc_size - tgt_str->bytes;
  tgt_ptr = tgt_str->data + tgt_str->bytes; /* Set pointer to target area */

  bytes = (int16_t)(src_str->bytes -
                      offset); /* Bytes remaining in source chunk */
  if (bytes > substr_len)
    bytes = (int16_t)substr_len;

  while (substr_len > 0) {
    if (bytes <= chunk_space) /* All fits current target chunk */
    {
      if (bytes != 0) {
        memcpy(tgt_ptr, src_str->data + offset, bytes);
        tgt_str->bytes += bytes;
        tgt_ptr += bytes;
        substr_len -= bytes;
        chunk_space -= bytes;
      }

      if (substr_len == 0)
        break;

      /* Move to next source chunk */

      src_str = src_str->next;
      bytes = src_str->bytes;
      if (bytes > substr_len)
        bytes = (int16_t)substr_len;
      offset = 0;
    } else /* Remaining source data will overflow target chunk */
    {
      /* Copy to end of current target chunk then allocate new chunk */

      memcpy(tgt_ptr, src_str->data + offset, chunk_space);
      tgt_str->bytes += chunk_space;
      offset += chunk_space;
      substr_len -= chunk_space;
      bytes -= chunk_space;

      new_tgt = s_alloc(substr_len, &chunk_space);
      if (chunk_space > substr_len)
        chunk_space = (int16_t)substr_len;

      tgt_str->next = new_tgt;
      tgt_str = new_tgt;
      tgt_ptr = tgt_str->data;
    }
  }

  return tgt_str;
}

/* ======================================================================
   op_upcase()  -  Convert string to uppercase                            */

void op_upcase() {
  set_case(TRUE);
}

/* ======================================================================
   set_case()  -  Common path for op_dncase() and op_upcase()
   Public function, also used by iconv and oconv                          */

void set_case(bool upcase) {
  DESCRIPTOR* descr;
  STRING_CHUNK* src;
  int16_t bytes;
  char* p;
  DESCRIPTOR result;

  /* Get source string */

  descr = e_stack - 1;
  k_get_string(descr);
  src = descr->data.str.saddr;

  InitDescr(&result, STRING);
  result.data.str.saddr = NULL;
  if (src == NULL)
    goto done;

  ts_init(&result.data.str.saddr, src->string_len);

  do {
    bytes = src->bytes;
    p = src->data;

    if (upcase) {
      while (bytes-- > 0) {
        ts_copy_byte(UpperCase(*p));
        p++;
      }
    } else {
      while (bytes-- > 0) {
        ts_copy_byte(LowerCase(*p));
        p++;
      }
    }
  } while ((src = src->next) != NULL);

  ts_terminate();

done:
  k_dismiss();
  *(e_stack++) = result;
  return;
}

/* END-CODE */
