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
#include <stdlib.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "mbn.h"
#include "ihex.h"
#include "util.h"
#include "dload.h"

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

static int dload_action_loadhex(const char *path,
				const char *address, int fd) {

  int i = 0;
  unsigned char *buf;
  unsigned int addr = 0;
  unsigned int size, offset;
  
  if(address != NULL)
    sscanf(address, "%x", &addr);

  dload_get_sw_version(fd);
  dload_get_params(fd);
  
  fprintf(stderr, "Loading file %s...", path);
  if((buf = ihex_raw_from_file(path, &size, &offset))){
    fprintf(stderr, "File size is %u bytes\n", size);
    fprintf(stderr, "Load address is 0x%08x\n", offset);
    do{
      size_t len = ((size < 0x400) ? size : 0x400); /* FIXME */
      if(dload_upload_data(fd, addr + i, &buf[i], len) < 0){
	fprintf(stderr, "Error during upload\n");
	break;
      }
      i += len;
      size -= len;
      /* Show processing */
      fprintf(stderr, ".");
    } while(size > 0);
    /* Don't forget this */
    ihex_free(buf);
  }

  return 0;
}

static int dload_action_infombn(const char *path) {

  int fd;
  mbn_header_t header;
  
  if((fd = mbn_from_file(path, &header)) < 0)
    return -1;

  mbn_display_header(&header);
  close(fd);
  return 0;
}

static int dload_action_loadmbn(const char *path,
				const char *address, int fd) {

  int mbn_fd, n;
  size_t size = 0;
  mbn_header_t header;
  unsigned int addr = 0;
  unsigned char buf[1024]; /* FIXME */
  
  if(address != NULL)
    sscanf(address, "%x", &addr);

  if((mbn_fd = mbn_from_file(path, &header)) < 0)
    return -1;
  
  /* Here the header is correct, start loading */
  fprintf(stderr, "Loading file %s...", path);
  while((n = read(mbn_fd, buf, sizeof(buf))) > 0){
    if(dload_upload_data(fd, addr + header.load_address + size,
			 buf, n) < 0){
      fprintf(stderr, "Error during upload\n");
      size = -1;
      goto out;
    }
    /* Show processing */
    fprintf(stderr, ".");
    size += n;
  }
    
  if(n < 0){
    fprintf(stderr, "Error reading file\n");
    size = -1;
  }

 out:
  close(fd);
  return size;
}

static int dload_action_loadbin(const char *path,
				const char *address, int fd) {
  
  unsigned int addr = 0;
  if(address != NULL)
    sscanf(address, "%x", &addr);
  
  dload_get_sw_version(fd);
  dload_get_params(fd);
  dload_upload_firmware(fd, addr, path);
  //dload_send_execute(fd, addr);

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

static int dload_action_signhex(const char *hex_path,
				const char *sign_path) {

  int ret = -1, n;
  uint8_t *buf, *data;  
  mbn_header_t header;
  
  size_t bytes_left;
  unsigned int offset, size;

  int sign_fd;
  struct stat stat;
  
  /* Open signature file. TODO : Check */
  if((sign_fd = open(sign_path, O_RDONLY)) != -1)
    fstat(sign_fd, &stat);
  else{
    perror("can't find signature file\n");
    return -1;
  }
  
  /* Open intel hex file */
  if((buf = ihex_raw_from_file(hex_path, &size, &offset))){
    data = mbn_from_mem(buf, size, &header);
    /* Prepare data output */
    offset = 0;
    bytes_left = header.body_length;
    /* Modify header to include signature */
    header.body_length += stat.st_size;
    header.signature_length = stat.st_size;
    /* Output modified header */
    write(fileno(stdout), &header, sizeof(header));
    /* Output data */
    while(bytes_left){
      if((n = write(fileno(stdout), &data[offset], bytes_left)) > 0){
	bytes_left -= n;
	offset += n;
      }else
	break;
    }
    /* Append signature. TODO : check */
    while((n = read(sign_fd, buf, 0x100)) > 0)
      write(fileno(stdout), buf, n);
    
    /* That's all, folks */
    close(sign_fd);
    ihex_free(buf);
    ret = 0;
    
  }else
    perror("hex2mbn");
  
  return ret;
}

static int dload_action_signmbn(const char *hex,
				const char *sign) {
  
  return 0;
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
#define DLOAD_COMMAND_INFOMBN 8
#define DLOAD_COMMAND_SIGNHEX 9
#define DLOAD_COMMAND_SIGNMBN 10

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
    if(!strcmp(cmd, "infombn") ||
       !strcmp(cmd, "mbninfo")) return DLOAD_COMMAND_INFOMBN;
    if(!strcmp(cmd, "signhex") ||
       !strcmp(cmd, "hexsign")) return DLOAD_COMMAND_SIGNHEX;
    if(!strcmp(cmd, "mbnsign") ||
       !strcmp(cmd, "signmbn")) return DLOAD_COMMAND_SIGNMBN;
  }
  
  return -1;
}

void usage(char **argv) {

  printf("Usage: %s [-F device] command [args...]\n", argv[0]);
  exit(0);
}

int main(int argc, char **argv) {

  int fd, c, cmd;
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
    case DLOAD_COMMAND_INFOMBN : dload_action_infombn(arg); break;
    case DLOAD_COMMAND_SIGNHEX : dload_action_signhex(arg, arg2); break;
    case DLOAD_COMMAND_SIGNMBN : dload_action_signmbn(arg, arg2); break;
    default : fprintf(stderr, "Unknown command %s\n", argv[optind-2]); break;
    }

    close(fd);
    
  }else
    perror(dev);

  return 0;
}
