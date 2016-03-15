/*
 * mbn.c
 * dloadtool
 *
 * Created by Joachim Naulet on 3/14/2016
 *
 * Copyright (c) 2016 R&D Innovation. All rights reserved
 */

#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <string.h>

#include "mbn.h"


int mbn_from_file(const char *path, mbn_header_t *h) {

  int fd;
  uint32_t buf;
  
  if((fd = open(path, O_RDONLY)) != -1){
    for(;;){
      /* Seek for .mbn header, 32bit at once */
      if(read(fd, (void*)&buf, sizeof buf) != sizeof(buf))
	goto err;
      /* 32bits read */
      if(buf == MBN_HEAD_CODEWORD){
	/* Now se seek for magic */
	h->codeword = buf;
	//printf("codeword : 0x%08x\n", h->codeword);
	if(read(fd, (void*)&buf, sizeof buf) != sizeof buf)
	  goto err;
	/* Antoher 32bits read*/
	if(buf == MBN_HEAD_MAGIC){
	  /* Found magic */
	  h->magic = buf;
	  //printf("magic : 0x%08x\n", h->magic);
	  int n = read(fd, (void*)&h->image_type, sizeof(*h)-8);
	  if(n != sizeof(*h)-8)
	    goto err;
	  
	  /* Read successfull */
	  //mbn_display_header(h);
	  fprintf(stderr, "Read successful\n");
	  break;
	}
      }
    }
  }

  return fd;

  err:
  /* Can't read data
   * It can be either an I/O problem or the end of the file.
   * We stop */
  close(fd);
  return -1;
}


void *mbn_from_mem(void *buf, size_t size, mbn_header_t *h) {

  int i;
  for(i = 0; i < size; i++){
    mbn_header_t *ptr = (mbn_header_t*)&((uint8_t*)buf)[i];
    if(ptr->codeword == MBN_HEAD_CODEWORD &&
       ptr->magic == MBN_HEAD_MAGIC){
      /* We found start of header, copy */
      memcpy(h, ptr, sizeof(*h));
      /* Read successfull */
      //mbn_display_header(h);
      fprintf(stderr, "Read successful\n");
      break;
    }
  }

  return (i == size ? NULL : h->body);
}

int mbn_display_header(mbn_header_t *h) {

  printf("codeword : 0x%08x\n", h->codeword);
  printf("magic : 0x%08x\n", h->magic);
  printf("Image type : %s\n",
		 (h->image_type == MBN_HEAD_IMG_EHOSTDL ?
		  "eHostDl" : "SBL1"));
  printf("Load address : 0x%08x\n", h->load_address);
  printf("Body length : %u\n", h->body_length);
  printf("Code length : %u\n", h->code_length);
  printf("Signature address : 0x%08x\n", h->signature_address);
  printf("Signature length : %u\n", h->signature_length);
  printf("Certificate address : 0x%08x\n", h->cert_store_address);
  printf("Certificate length : %u\n", h->cert_store_length);
  return 0;
}
