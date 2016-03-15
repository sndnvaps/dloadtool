/*
 * ihex.c
 *
 * (c) 2016 Joachim Naulet <jnaulet@rdinnovation.fr>
 * 
 * Created on 3/15/2016 by Joachim Naulet
 *
 */

#include "ihex.h"

#include <stdio.h>
#include <string.h>
#include <cintelhex.h>

void *ihex_raw_from_file(const char *path,
			 unsigned int *size_r,
			 unsigned int *offset_r) {

  int err;
  char *buf;
  uint32_t offset;
  unsigned int n, i = 0;

  ihex_recordset_t *rs;
  ihex_record_t *record;
  
  if(!(rs = ihex_rs_from_file(path))){
    fprintf(stderr, "%s\n", ihex_error());
    return NULL;
  }

  /* TODO : check */
  size_t size = ihex_rs_get_size(rs);
  if((buf = (char*)malloc(size))){
    for(n = 0;;){
      if((err = ihex_rs_iterate_data(rs, &i, &record, &offset)))
	goto out;
      if(!record)
	break;
      /* Ignore offset */
      memcpy(buf + n, record->ihr_data, record->ihr_length);
      n += record->ihr_length;
    }
    /* Copy offset just in case */
    *offset_r = offset;
    *size_r = size;
  }
  
 out:
  ihex_rs_free(rs);
  return buf;
}

void ihex_free(void *ptr) {

  free(ptr);
}
