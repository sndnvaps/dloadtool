//
//  main.cpp
//  dloadtool
//
//  Created by Joshua Hill on 1/30/13.
//  Modified by Joachim Naulet on 3/10/2016.
//
//  Copyright (c) 2013 __MyCompanyName__. All rights reserved.
//

#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "util.h"
#include "dload.h"

typedef enum {
  DLOAD_ACTION_NONE,
  DLOAD_ACTION_SEND,
  DLOAD_ACTION_UPLOAD,
} dload_action_t;

static int dload_action_send(int argc, char **argv, int fd) {
  
  int i = 0;
  int v = 0;
  unsigned char input[0x200];
  unsigned char output[0x200];
  
  for(v = 2; v < argc; v++) {
    const char* arg = (const char*) argv[v];
    unsigned int size = strlen(arg) / 2;
    memset(output,'\0', sizeof(output));
    memset(input, '\0', sizeof(input));
    for(i = 0; i < size; i++) {
      unsigned int byte = 0;
      sscanf(arg, "%02x", &byte);
      output[i] = byte;
      arg += 2;
    }
    
    dload_write(fd, output, size);
    dload_read(fd, input, sizeof(input));
  }
}

static int dload_action_upload(const char *path, int fd) {
  
  unsigned int addr = 0x20012000;
  
  dload_get_params(fd);
  dload_get_sw_version(fd);
  dload_upload_firmware(fd, addr, path);
  dload_send_execute(fd, addr);

  return 0;
}

static int dload_action_none(int fd) {
  
  unsigned int addr = 0x20012000;
  
  dload_get_params(fd);
  dload_get_sw_version(fd);
  dload_upload_firmware(fd, addr,
			"/usr/local/standalone/firmware/Trek/dbl.mbn");
  
  dload_send_execute(fd, addr);
  
  return 0;
}

int main(int argc, char **argv) {

  int fd, c;
  char config = 0;

  unsigned int addr = 0;
  char *dev = NULL, *path = NULL;
  dload_action_t action = DLOAD_ACTION_NONE;
  
  printf("Starting DLOADTool\n");
  
  while((c = getopt(argc, argv, "a:f:d:h")) != -1){
    switch(c){
    case 'a' :
      sscanf(optarg, "0x%08x", &addr);
      break;
    case 'f' :
      action = DLOAD_ACTION_UPLOAD;
      path = optarg;
      break;
    case 'd' :
      dev = optarg;
      break;
    default :
      printf("Usage: %s [-f firmware -a address] " \
	     "[-c command1 command2 ... commandN]\n", argv[0]);
      return -1;
    }
  }
      
  // Vendor ID: 0x5c6
  // Product ID: 0x9006
  // ttyUSBx
  if((fd = open(dev, O_RDWR)) != -1){
    printf("Device %s Opened\n", dev);
    /* TODO : config */
    switch(action) {
    case DLOAD_ACTION_SEND : dload_action_send(argc, argv, fd); break;
    case DLOAD_ACTION_UPLOAD : dload_action_upload(path, fd); break;
    case DLOAD_ACTION_NONE : dload_action_none(fd); break;
    }

    printf("Closing Interface\n");
    close(fd);
    
  }else
    fprintf(stderr, "Couldn't open device interface\n");

  return 0;
}
