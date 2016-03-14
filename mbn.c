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

#include "mbn.h"


int mbn_open(const char *path, mbn_head_t *head) {

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
	head->codeword = buf;
	printf("codeword : 0x%08x\n", head->codeword);
	if(read(fd, (void*)&buf, sizeof buf) != sizeof buf)
	  goto err;
	/* Antoher 32bits read*/
	if(buf == MBN_HEAD_MAGIC){
	  /* Found magic */
	  head->magic = buf;
	  printf("magic : 0x%08x\n", head->magic);
	  int n = read(fd, (void*)&head->image_type, sizeof(*head)-8);
	  if(n != sizeof(*head)-8)
	    goto err;
	  
	  /* Read successfull */
	  printf("Image type : %s\n",
		 (head->image_type == MBN_HEAD_IMG_EHOSTDL ?
		  "eHostDl" : "SBL1"));

	  printf("Load address : 0x%08x\n", head->load_address);
	  printf("Body length : %u\n", head->body_length);
	  printf("Code length : %u\n", head->code_length);
	  printf("Signature address : 0x%08x\n", head->signature_address);
	  printf("Signature length : %u\n", head->signature_length);
	  printf("Certificate address : 0x%08x\n", head->cert_store_address);
	  printf("Certificate length : %u\n", head->cert_store_length);
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
