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
#include <termios.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "util.h"
#include "dload.h"

#include <cintelhex.h>

typedef enum {
  DLOAD_ACTION_NONE,
  DLOAD_ACTION_SEND,
  DLOAD_ACTION_UPLOAD,
  DLOAD_ACTION_UPLOAD_HEX
} dload_action_t;

static int dload_action_send(int argc, char **argv, int fd) {
  
  int i = 0;
  int v = 0;
  unsigned char input[0x200];
  unsigned char output[0x200];
  
  for(v = optind; v < argc; v++) {
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

static int dload_action_upload_hex(const char *path, int fd) {

  int err;
  ihex_recordset_t *rs;
  ihex_record_t *record;
  unsigned int i = 0, offset;

  if(!(rs = ihex_rs_from_file(path))){
    perror(ihex_error());
    return -1;
  }

  do {
    err = ihex_rs_iterate_data(rs, &i, &record, &offset);
    if (err || record == 0)
      break;
    
    /* send record */
    dload_upload_data(fd, offset + record->ihr_address,
		      record->ihr_data, record->ihr_length);
    
  } while (i > 0);

  /* TODO : Check error */
  dload_send_execute(fd, 0x2a000000); /* FIXME */
  
  ihex_rs_free(rs);
  return 0;
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
  
  dload_get_sw_version(fd);
  dload_get_params(fd);
  
  return 0;
}

int main(int argc, char **argv) {

  int fd, c;
  char config = 0;
  struct termios terminal_data;
  
  unsigned int addr = 0;
  char *dev = NULL, *path = NULL;
  dload_action_t action = DLOAD_ACTION_NONE;
  
  while((c = getopt(argc, argv, "a:u:i:d:ch")) != -1){
    switch(c){
    case 'a' :
      sscanf(optarg, "0x%08x", &addr);
      break;
    case 'u' :
      action = DLOAD_ACTION_UPLOAD;
      path = optarg;
      break;
    case 'i' :
      action = DLOAD_ACTION_UPLOAD_HEX;
      path = optarg;
      break;
    case 'c' :
      action = DLOAD_ACTION_SEND;
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
    //printf("Device %s Opened\n", dev);
    /* Config */
    tcgetattr(fd, &terminal_data);
    cfmakeraw(&terminal_data);
    tcsetattr(fd, TCSANOW, &terminal_data);
    
    switch(action) {
    case DLOAD_ACTION_SEND : dload_action_send(argc, argv, fd); break;
    case DLOAD_ACTION_UPLOAD : dload_action_upload(path, fd); break;
    case DLOAD_ACTION_UPLOAD_HEX : dload_action_upload_hex(path, fd); break;
    case DLOAD_ACTION_NONE : dload_action_none(fd); break;
    }

    //printf("Closing Interface\n");
    close(fd);
    
  }else
    perror(dev);

  return 0;
}
