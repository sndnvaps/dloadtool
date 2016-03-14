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

#include "mbn.h"
#include "util.h"
#include "dload.h"

#include <cintelhex.h>

int verbose = 0; /* FIXME */

static int dload_action_send(const char *data, int fd) {
  
  int byte;
  size_t size = 0;
  unsigned char input[256];
  unsigned char output[256];

  while(*data){
    if(sscanf(data, "%02x", &byte)){
      output[size++] = 0xff & byte;
      data += 2;
    }
  }
  
  dload_write(fd, output, size);
  dload_read(fd, input, sizeof(input));
  if(input[0] == 2)
    return 0;

  return -1;
}

static int dload_action_crack(int fd) {

  unsigned long long code = 0;
  while(dload_send_unlock(fd, code) < 0)
    code++;

  printf("***Unlock code is %016x\n", code);
  return 0;
}

static int dload_action_loadhex(const char *path,
				const char *address, int fd) {

  int err;
  ihex_recordset_t *rs;
  ihex_record_t *record;
  unsigned int i = 0, offset;

  unsigned int addr = 0;
  if(address != NULL)
    sscanf(address, "%x", &addr);

  if(!(rs = ihex_rs_from_file(path))){
    fprintf(stderr, "%s\n", ihex_error());
    return -1;
  }
  
  dload_get_sw_version(fd);
  dload_get_params(fd);
  
  fprintf(stderr, "Loading file %s...", path);
  
  do {
    if((err = ihex_rs_iterate_data(rs, &i, &record, &offset)))
      goto out;

    if(!record)
      break;
    
    /* send record */
    if(dload_upload_data(fd, addr + offset + record->ihr_address,
			 record->ihr_data, record->ihr_length) < 0){
      fprintf(stderr, "Error during upload\n");
      goto out;
    }

    /* Show processing */
    fprintf(stderr, ".");
    
  } while (i > 0);

  /* FIXME : Use last record */
  //if(dload_send_execute(fd, 0x2a000000) < 0)
  //  fprintf(stderr, "Can't execute software\n");

  fprintf(stderr, "\n");
  ihex_rs_free(rs);
  return 0;
  
 out:
  ihex_rs_free(rs);
  return -1;
}

static int dload_action_loadmbn(const char *path,
				const char *address, int fd) {

  int mbn_fd, n;
  size_t size = 0;
  mbn_head_t header;
  unsigned int addr = 0;
  unsigned char buf[1024]; /* FIXME */
  
  if(address != NULL)
    sscanf(address, "%x", &addr);

  if((mbn_fd = mbn_open(path, &header)) < 0)
    goto err;
  
  /* Here's the header is correct, start loading */
  fprintf(stderr, "Loading file %s...", path);
  while((n = read(mbn_fd, buf, sizeof(buf))) > 0){
    if(dload_upload_data(fd,
			 addr + header.load_address + size,
			 buf, n) < 0){
      fprintf(stderr, "Error during upload\n");
      goto err;
    }
    /* Show processing */
    fprintf(stderr, ".");
    size += n;
  }
    
  if(n < 0){
    fprintf(stderr, "Error reading file\n");
    goto err;
  }
  
  return size;
  
 err:
  return -1;
}

static int dload_action_loadbin(const char *path,
				const char *address, int fd) {
  
  unsigned int addr = 0;
  if(address != NULL)
    sscanf(address, "%x", &addr);
  
  dload_get_sw_version(fd);
  dload_get_params(fd);
  dload_upload_firmware(fd, addr, path);
  dload_send_execute(fd, addr);

  return 0;
}

static int dload_action_info(int fd) {
  
  dload_get_sw_version(fd);
  dload_get_params(fd);
  
  return 0;
}

static int dload_action_execute(const char *address, int fd) {

  unsigned int addr;
  if(sscanf(address, "%x", &addr))
    return dload_send_execute(fd, addr);

  return -1;
}

/* Simple parser */

#define DLOAD_COMMAND_INFO    0
#define DLOAD_COMMAND_RESET   1
#define DLOAD_COMMAND_MAGIC   2
#define DLOAD_COMMAND_SEND    3
#define DLOAD_COMMAND_LOADHEX 4
#define DLOAD_COMMAND_LOADMBN 5
#define DLOAD_COMMAND_LOADBIN 6
#define DLOAD_COMMAND_EXECUTE 7

int dload_parse_command(const char *cmd) {

  if(cmd != NULL){
    /* TODO : add limit */
    if(!strcmp(cmd, "info"))    return DLOAD_COMMAND_INFO;
    if(!strcmp(cmd, "reset"))   return DLOAD_COMMAND_RESET;
    if(!strcmp(cmd, "magic"))   return DLOAD_COMMAND_MAGIC;
    if(!strcmp(cmd, "send"))    return DLOAD_COMMAND_SEND;
    if(!strcmp(cmd, "loadhex")) return DLOAD_COMMAND_LOADHEX;
    if(!strcmp(cmd, "loadmbn")) return DLOAD_COMMAND_LOADMBN;
    if(!strcmp(cmd, "loadbin")) return DLOAD_COMMAND_LOADBIN;
    if(!strcmp(cmd, "execute") ||
       !strcmp(cmd, "exec"))    return DLOAD_COMMAND_EXECUTE;
  }
  
  return -1;
}

void usage(char **argv) {

  printf("Usage: %s [-F device] command [args...]\n", argv[0]);
  exit(0);
}

int main(int argc, char **argv) {

  int fd, c, cmd;
  char config = 0;
  struct termios terminal_data;

  char *dev = "/dev/ttyUSB0";
  char *arg = NULL, *arg2 = NULL;
  
  while((c = getopt(argc, argv, "F:vh")) != -1){
    switch(c){
    case 'F': dev = optarg; break;
    case 'v': verbose = 1; break;
    case 'h': usage(argv);
    }
  }

  if((cmd = dload_parse_command(argv[optind])) < 0)
    usage(argv);

  /* Get cmd argument (two max for the moment) */
  arg = argv[++optind];
  arg2 = argv[++optind];
  
  /* Vendor ID: 0x5c6
   * Product ID: 0x9006 | 0x9008
   * /dev/ttyUSB0 by default */
  if((fd = open(dev, O_RDWR)) != -1){
    /* TTY Config */
    tcgetattr(fd, &terminal_data);
    cfmakeraw(&terminal_data);
    tcsetattr(fd, TCSANOW, &terminal_data);
    
    switch(cmd) {
    case DLOAD_COMMAND_INFO : dload_action_info(fd); break;
    case DLOAD_COMMAND_RESET : dload_send_reset(fd); break;
    case DLOAD_COMMAND_MAGIC : dload_send_magic(fd); break;
    case DLOAD_COMMAND_SEND : dload_action_send(arg, fd); break;
    case DLOAD_COMMAND_LOADHEX : dload_action_loadhex(arg, arg2, fd); break;
    case DLOAD_COMMAND_LOADMBN : dload_action_loadmbn(arg, arg2, fd); break;
    case DLOAD_COMMAND_LOADBIN : dload_action_loadbin(arg, arg2, fd); break;
    case DLOAD_COMMAND_EXECUTE : dload_action_execute(arg, fd); break;
    }

    close(fd);
    
  }else
    perror(dev);

  return 0;
}
