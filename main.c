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
  size = dload_read(fd, input, sizeof(input));
  hexdump(input, size); /* Dump */
  if(input[0] == 2)
    return 0;

  return -1;
}

static int dload_action_read(const char *address,
			     const char *length, int fd) {

  unsigned int addr, len;

  if(address && length){
    sscanf(address, "%x", &addr);
    sscanf(length, "%x", &len);
  }else{
    fprintf(stderr, "read : missing parameter(s) !\n");
    return -1;
  }

  if(dload_memory_read_req(fd, addr, len) < 0)
    fprintf(stderr, "0x3 error - %s (%d)",
	    dload_strerror(nak_errno), nak_errno);

  return 0;
}

static int dload_action_erase(const char *address,
			      const char *length, int fd) {

  unsigned int addr, len;
  
  if(address && length){
    sscanf(address, "%x", &addr);
    sscanf(length, "%x", &len);
  }else{
    fprintf(stderr, "erase : missing parameter(s) !\n");
    return -1;
  }
  
  if(dload_send_erase(fd, addr, len) < 0)
    fprintf(stderr, "0x3 error - %s (%d)",
	    dload_strerror(nak_errno), nak_errno);

  return 0;
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
  
  fprintf(stderr, "Loading file %s...\n", path);
  if((buf = ihex_raw_from_file(path, &size, &offset))){
    fprintf(stderr, "File size is %u bytes\n", size);
    fprintf(stderr, "Load address is 0x%08x\n", offset);
    do{
      size_t len = ((size < 0x400) ? size : 0x400); /* FIXME */
      if(dload_upload_data(fd, addr + offset + i, &buf[i], len) < 0){
	fprintf(stderr, "0x3 - upload failed : %s (%d)\n",
		dload_strerror(nak_errno), nak_errno);
	break;
      }
      i += len;
      size -= len;
      /* Show processing */
      fprintf(stderr, ".");
    } while(size > 0);
    /* Don't forget this */
    ihex_free(buf);
    fprintf(stderr, " Done\n");
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
  uint32_t base_addr = (header.load_address - header.header_length) + addr;
  fprintf(stderr, "Loading file %s at addr 0x%08x...", path, base_addr);
  /* TODO : check */
  dload_upload_data(fd, base_addr, &header, header.header_length);
  base_addr += header.header_length;
  
  while((n = read(mbn_fd, buf, sizeof(buf))) > 0){
    if(dload_upload_data(fd, base_addr + size,
			 buf, n) < 0){
      fprintf(stderr, "0x3 - upload failed : %s (%d)\n",
	      dload_strerror(nak_errno), nak_errno);
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
  }else
    fprintf(stderr, " Done\n");

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
  
  if(dload_upload_firmware(fd, addr, path) < 0)
    fprintf(stderr, "Upload failed : %s (%d)\n",
	    dload_strerror(nak_errno), nak_errno);
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
  if(sscanf(address, "%x", &addr)){
    if(dload_send_execute(fd, addr) < 0)
      fprintf(stderr, "0x3 - can't execute : %s (%d)\n",
	      dload_strerror(nak_errno), nak_errno);
    else
      fprintf(stderr, "Done\n");
  }else
    fprintf(stderr, "execute : missing parameter(s)\n");
  
  return 0;
}

static int dload_action_signhex(const char *hex_path,
				const char *sign_path,
				const char *cert_path) {

  int ret = -1, n;
  uint8_t *buf, *data;  
  mbn_header_t header;
  
  size_t bytes_left;
  unsigned int offset, size;

  int sign_fd, cert_fd;
  struct stat sign_stat, cert_stat;
  
  /* Open signature file. TODO : Check */
  if((sign_fd = open(sign_path, O_RDONLY)) != -1)
    fstat(sign_fd, &sign_stat);
  else{
    perror("can't find signature file\n");
    return -1;
  }

  /* Open certificate file. TODO : Check */
  if((cert_fd = open(cert_path, O_RDONLY)) != -1)
    fstat(cert_fd, &cert_stat);
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
    header.signature_address = header.load_address +
      header.header_length + header.body_length;
    header.signature_length = sign_stat.st_size;
    header.cert_store_address = header.signature_address +
      header.signature_length;
    header.cert_store_length = cert_stat.st_size;
    /* Correct body length */
    header.body_length += (sign_stat.st_size + cert_stat.st_size);
    /* Output modified header */
    write(STDOUT_FILENO, &header, sizeof(header));
    /* Output data */
    while(bytes_left){
      if((n = write(STDOUT_FILENO, &data[offset], bytes_left)) > 0){
	bytes_left -= n;
	offset += n;
      }else{
	perror("write");
	break;
      }
    }
    /* Append signature. TODO : check */
    while((n = read(sign_fd, buf, 0x100)) > 0)
      write(STDOUT_FILENO, buf, n);
    /* Append certificate. TODO : check */
    while((n = read(cert_fd, buf, 0x100)) > 0)
      write(STDOUT_FILENO, buf, n);
    
    /* That's all, folks */
    close(sign_fd);
    ihex_free(buf);
    ret = 0;
    
  }else
    perror("hex2mbn");
  
  return ret;
}

static int dload_action_signmbn(const char *mbn_path,
				const char *sign_path) {
  
  char buf[4096];
  struct stat stat;
  mbn_header_t header;
  int mbn_fd, sign_fd, n;
  
  /* Open signature file. TODO : Check */
  if((sign_fd = open(sign_path, O_RDONLY)) != -1)
    fstat(sign_fd, &stat);
  else{
    perror("can't find signature file\n");
    return -1;
  }
  
  if((mbn_fd = mbn_from_file(mbn_path, &header)) < 0)
    return -1;

  /* Prepare data output */
  off_t offset = 0;
  size_t bytes_left = header.body_length;
  /* Modify header to include signature */
  header.body_length += stat.st_size;
  header.signature_length = stat.st_size;
  /* Output modified header */
  write(STDOUT_FILENO, &header, sizeof(header));
  /* Output data */
  while(bytes_left){
    if((n = read(mbn_fd, buf, sizeof(buf))) > 0){
      /* TODO : check */
      write(STDOUT_FILENO, buf, n);
      bytes_left -= n;
      offset += n;
    }else
      break;
  }
  
  if(bytes_left)
    fprintf(stderr, "Warning, some data might be missing\n");
  
  /* Append signature. TODO : check */
  while((n = read(sign_fd, buf, 0x100)) > 0)
    write(STDOUT_FILENO, buf, n);
  
  /* That's all, folks */
  close(sign_fd);
  close(mbn_fd);
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
#define DLOAD_COMMAND_READ    11
#define DLOAD_COMMAND_ERASE   12

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
       !strcmp(cmd, "exec")    ||
       !strcmp(cmd, "go"))      return DLOAD_COMMAND_EXECUTE;
    if(!strcmp(cmd, "infombn") ||
       !strcmp(cmd, "mbninfo")) return DLOAD_COMMAND_INFOMBN;
    if(!strcmp(cmd, "signhex") ||
       !strcmp(cmd, "hexsign")) return DLOAD_COMMAND_SIGNHEX;
    if(!strcmp(cmd, "mbnsign") ||
       !strcmp(cmd, "signmbn")) return DLOAD_COMMAND_SIGNMBN;
    if(!strcmp(cmd, "read"))    return DLOAD_COMMAND_READ;
    if(!strcmp(cmd, "erase"))   return DLOAD_COMMAND_ERASE;
  }
  
  return -1;
}

void usage(char **argv) {

  printf("Usage: %s [-F device] command [args...]\n", argv[0]);
  exit(0);
}

#define MAX_ARG 3

int main(int argc, char **argv) {

  int fd, c, cmd, i;
  struct termios terminal_data;

  int argn = 0;
  char *arg[MAX_ARG] = { 0 };
  char *dev = "/dev/ttyUSB0";
  
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
  for(i = optind + 1; i < argc; i++)
    arg[argn++] = argv[i];
  
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
    case DLOAD_COMMAND_SEND : dload_action_send(arg[0], fd); break;
    case DLOAD_COMMAND_LOADHEX :
      dload_action_loadhex(arg[0], arg[1], fd); break;
    case DLOAD_COMMAND_LOADMBN :
      dload_action_loadmbn(arg[0], arg[1], fd); break;
    case DLOAD_COMMAND_LOADBIN :
      dload_action_loadbin(arg[0], arg[1], fd); break;
    case DLOAD_COMMAND_EXECUTE : dload_action_execute(arg[0], fd); break;
    case DLOAD_COMMAND_INFOMBN : dload_action_infombn(arg[0]); break;
    case DLOAD_COMMAND_SIGNHEX :
      dload_action_signhex(arg[0], arg[1], arg[2]); break;
    case DLOAD_COMMAND_SIGNMBN : dload_action_signmbn(arg[0], arg[1]); break;
    case DLOAD_COMMAND_READ : dload_action_read(arg[0], arg[1], fd); break;
    case DLOAD_COMMAND_ERASE : dload_action_erase(arg[0], arg[1], fd); break;
      
    default :
      fprintf(stderr, "Unknown command %s\n", argv[optind]);
      break;
    }

    close(fd);
    
  }else
    perror(dev);

  return 0;
}
