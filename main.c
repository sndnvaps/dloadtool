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

static int dload_action_crack(int fd) {

  unsigned long long code = 0;
  while(dload_send_unlock(fd, code) < 0)
    code++;

  printf("***Unlock code is %016x\n", code);
  return 0;
}

static int dload_action_loadhex(const char *path, int fd) {

  int err;
  ihex_recordset_t *rs;
  ihex_record_t *record;
  unsigned int i = 0, offset;

  if(!(rs = ihex_rs_from_file(path))){
    perror(ihex_error());
    return -1;
  }
  
  dload_get_sw_version(fd);
  dload_get_params(fd);
  
  do {
    if((err = ihex_rs_iterate_data(rs, &i, &record, &offset)))
      goto out;

    if(!record)
      break;
    
    /* send record */
    if(dload_upload_data(fd, offset + record->ihr_address,
			 record->ihr_data, record->ihr_length) < 0){
      fprintf(stderr, "Error during upload\n");
      goto out;
    }  
  } while (i > 0);

  /* FIXME : Use last record */
  //if(dload_send_execute(fd, 0x2a000000) < 0)
  //  fprintf(stderr, "Can't execute software\n");
  
  ihex_rs_free(rs);
  return 0;
  
 out:
  ihex_rs_free(rs);
  return -1;
}							    

static int dload_action_loadbin(const char *path, int fd) {
  
  unsigned int addr = 0x0;
  
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
#define DLOAD_COMMAND_LOADBIN 5
#define DLOAD_COMMAND_EXECUTE 6

int dload_parse_command(const char *cmd) {

  if(cmd != NULL){
    /* TODO : add limit */
    if(!strcmp(cmd, "info"))    return DLOAD_COMMAND_INFO;
    if(!strcmp(cmd, "reset"))   return DLOAD_COMMAND_RESET;
    if(!strcmp(cmd, "magic"))   return DLOAD_COMMAND_MAGIC;
    if(!strcmp(cmd, "send"))    return DLOAD_COMMAND_SEND;
    if(!strcmp(cmd, "loadhex")) return DLOAD_COMMAND_LOADHEX;
    if(!strcmp(cmd, "loadbin")) return DLOAD_COMMAND_LOADBIN;
    if(!strcmp(cmd, "execute")) return DLOAD_COMMAND_EXECUTE;
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

  char *arg = NULL;
  char *dev = "/dev/ttyUSB0";
  
  while((c = getopt(argc, argv, "F:h")) != -1){
    switch(c){
    case 'F': dev = optarg; break;
    case 'h' : usage(argv);
    }
  }

  if((cmd = dload_parse_command(argv[optind])) < 0)
    usage(argv);

  /* Get cmd argument (only one for the moment) */
  arg = argv[++optind];
  
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
    case DLOAD_COMMAND_SEND : dload_action_send(argc, argv, fd); break;
    case DLOAD_COMMAND_LOADHEX : dload_action_loadhex(arg, fd); break;
    case DLOAD_COMMAND_LOADBIN : dload_action_loadbin(arg, fd); break;
    case DLOAD_COMMAND_EXECUTE : dload_action_execute(arg, fd); break;
    }

    close(fd);
    
  }else
    perror(dev);

  return 0;
}
