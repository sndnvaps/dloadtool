//
//  dload.cpp
//  dloadtool
//
//  Created by Joshua Hill on 1/31/13.
//  Modified by Joachim Naulet on 3/10/2016.
//
//

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>

#include "util.h"
#include "dload.h"

#define BUFSIZE 4096

extern int verbose; /* FIXME */
int nak_errno = 0;

unsigned char magic[] =
  "\x01QCOM fast download protocol host\x02\x02\x01";
int dload_send_magic(int fd) {

  dload_ack ack;
  //uint8_t request = DLOAD_WRITE;
  
  dload_write(fd, magic, sizeof(magic) - 1);
  dload_read(fd, &ack, sizeof ack);
  if(ack.code == DLOAD_ACK)
    return 0;

  nak_errno = ack.errno;
  return -1;
}

int dload_send_reset(int fd) {

  dload_ack ack;
  uint8_t request = DLOAD_RESET; 
  dload_write(fd, &request, sizeof(request));
  dload_read(fd, &ack, sizeof ack);
  if(ack.code == DLOAD_ACK)
    return 0;
  
  nak_errno = ack.errno;
  return -1;
}

int dload_get_params(int fd) {
  
  uint8_t output[BUFSIZE];
  dload_params *response = NULL;
  uint8_t request = DLOAD_PARAM_REQ;
  
  dload_write(fd, &request, sizeof(request));
  dload_read(fd, output, sizeof(output));
  if(output[0] == DLOAD_PARAM_RESP){
    response = (dload_params*) output;
    printf("Protocol Version: 0x%hhx\n", response->version);
    printf("Min Protocol Version: 0x%hhx\n", response->min_version);
    printf("Max Write Size: 0x%hx\n", flip_endian16(response->max_write));
    printf("Model: 0x%hhx\n", response->model);
    printf("Device Size: 0x%hhx\n", response->device_size);
    printf("Device Type: 0x%hhx\n", response->device_type);
    return 0;
  }
  
  nak_errno = ((dload_ack*)response)->errno;
  fprintf(stderr, "Error receiving software parameters!!\n");
  return -1;
}

int dload_get_sw_version(int fd) {
  
  uint8_t output[BUFSIZE];
  dload_sw_version *response;
  uint8_t request = DLOAD_SW_VER_REQ;
    
  dload_write(fd, &request, sizeof(request));
  dload_read(fd, output, sizeof(output));
  if(output[0] == DLOAD_SW_VERS_RESP) {
    response = (dload_sw_version*)output;
    printf("Software Version: %.16s\n", response->version);
    return 0;
  }
  
  nak_errno = ((dload_ack*)output)->errno;
  fprintf(stderr, "Error receiving software version!!\n");
  return -1;
}

int dload_send_unlock(int fd, uint64_t key) {

  dload_ack ack;
  dload_unlock request;
  
  request.code = DLOAD_UNLOCK;
  request.security_key = flip_endian64(key);
  
  dload_write(fd, &request, sizeof(request));
  dload_read(fd, &ack, sizeof(ack));
  if(ack.code == DLOAD_ACK)
    return 0;

  nak_errno = ack.errno;
  return -1;
}

//Frame      Code      Address                Size
//           0x0f       0x20012000            0x100
//7e         0x05       0x20012000
unsigned char done[] = "\x7e\x05\x20\x01\x20\x00\x9f\x1f\x7e";
int dload_upload_firmware(int fd, uint32_t addr, const char *path) {
  
  FILE *fp = NULL;
  uint8_t input[0x100];
  uint8_t output[0x100];

  dload_write_addr *packet = (dload_write_addr*)
    malloc(sizeof(dload_write_addr) + sizeof(output));
  
  if((fp = fopen(path, "rb")) != NULL) {
    printf("Opened firmware at %s\n", path);
    while(!feof(fp)) {
      memset(input, '\0', sizeof(input));
      memset(output, '\0', sizeof(output));
      size_t len = fread(output, 1, sizeof(output), fp);
      printf("Read buffer of size %zd\n", len);
      if(len != 0) {
	memset(packet, '\0', sizeof(dload_write_addr) + sizeof(output));
	memcpy(packet->buffer, output, sizeof(output));
	packet->code = DLOAD_WRITE_ADDR;
	packet->size = flip_endian16(len);
	packet->address = flip_endian32(addr);
        
	dload_write(fd, packet, sizeof(dload_write_addr) + len);
	dload_read(fd, input, sizeof(input));
	if(input[0] != 0x2) {
	  fprintf(stderr, "Error 0x%hhx!!!\n", input[0]);
	  free(packet);
	  return -1;
	}
	addr += len;
      }
    }

    fclose(fp);
  }
  
  free(packet);
  return 0;
}

int dload_upload_data(int fd, uint32_t addr,
		      const void *data, size_t len) {
  
  dload_ack ack;
  uint8_t buf[BUFSIZE];
  dload_write_addr *packet = (dload_write_addr*)buf;
  
  memcpy(packet->buffer, data, len);
  packet->code = DLOAD_WRITE_ADDR;
  packet->size = flip_endian16(0xffff & len);
  packet->address = flip_endian32(addr);
    
  dload_write(fd, packet, sizeof(dload_write_addr) + len);
  dload_read(fd, &ack, sizeof(ack));
  if(ack.code == DLOAD_ACK)
    return 0;
  
  nak_errno = ack.errno;
  return -1;
}

int dload_send_execute(int fd, uint32_t address) {
  
  dload_ack ack;
  dload_execute request;
  
  request.code = DLOAD_EXECUTE;
  request.address = flip_endian32(address);
    
  dload_write(fd, &request, sizeof(dload_execute));
  dload_read(fd, &ack, sizeof(ack));
  if(ack.code == DLOAD_ACK)
    return 0;

  nak_errno = ack.errno;
  return -1;
}

int dload_memory_read_req(int fd, uint32_t address, size_t len) {

  int outsize;
  dload_read_req request;
  uint8_t outbuf[BUFSIZE];
  dload_ack *ack = (dload_ack*)outbuf;

  request.code = DLOAD_MEMORY_READ_REQ;
  request.address = flip_endian32(address);
  request.size = flip_endian16(0xffff & len);

  dload_write(fd, &request, sizeof(request));
  outsize = dload_read(fd, outbuf, sizeof(outbuf));
  if(ack->code == DLOAD_MEMORY_READ){
    hexdump(outbuf, outsize);
    return 0;
  }

  nak_errno = ack->errno;
  return -1;
}

int dload_send_erase(int fd, uint32_t address, size_t len) {
  
  dload_ack ack;
  dload_erase request;

  request.code = DLOAD_ERASE;
  /* Compute 20-bit address */
  uint16_t segment = 0xffff & (address >> 16);
  uint32_t addr = (segment << 4) + (0xffff & address);
  /* Set message content */
  request.address[0] = 0x0f & (addr >> 16);
  request.address[1] = 0xff & (addr >> 8);
  request.address[2] = 0xff & (addr >> 0);
  /* Compute 24-bit size */
  request.size[0] = 0xff & (len >> 16);
  request.size[1] = 0xff & (len >> 8);
  request.size[2] = 0xff & (len);
  
  dload_write(fd, &request, sizeof(request));
  dload_read(fd, &ack, sizeof(ack));
  if(ack.code == DLOAD_ACK)
    return 0;

  nak_errno = ack.errno;
  return -1;
}

/* Deeper function */

int dload_read(int fd, void *buffer, uint32_t size) {
  
  size_t insize;
  uint32_t outsize;
  uint8_t inbuf[BUFSIZE];
  uint8_t *outbuf = NULL;
    
  fprintf(stderr, "< ");
  insize = read(fd, inbuf, sizeof(inbuf));
  if(insize > 0){
    dload_response(inbuf, insize, &outbuf, &outsize);
    if(outsize <= size) {
      if(verbose) hexdump(outbuf, outsize);
      memcpy(buffer, outbuf, outsize);
    }
  }
  
  free(outbuf);
  return outsize;
}

int dload_write(int fd, void *buffer, uint32_t size) {
  
  uint32_t outsize = 0;
  uint8_t* outbuf = NULL;
  
  //fprintf(stderr, "> ");
  dload_request(buffer, size, &outbuf, &outsize);
  if(outsize > 0) {
    outsize = write(fd, outbuf, outsize);
    if(verbose) hexdump(buffer, size);
  }
  
  free(outbuf);
  return outsize;
}

int dload_request(void *input, uint32_t insize,
		  uint8_t **output, uint32_t *outsize) {
  
  uint16_t crc = 0;
  uint32_t size = 0;
  uint8_t* inbuf = NULL;
  uint8_t* outbuf = NULL;
  uint8_t* buffer = NULL;
    
  inbuf = (uint8_t*)malloc(insize + 2); /* plus 2 for the crc */
  memset(inbuf, '\0', insize + 2);
  /* copy the original data into our buffer with the crc */
  memcpy(inbuf, input, insize);

  /* perform the crc or the original data */
  crc = dm_crc16((const char*) input, insize);
  inbuf[insize] = crc & 0xFF; /* add first byte of crc */
  inbuf[insize+1] = (crc >> 8) & 0xFF; /* add second byte of crc */
  /* escape all control and escape characters */
  dload_escape(inbuf, insize+2, &outbuf, &size);
  /* plus 2 for start and end control characters */
  buffer = (uint8_t*)malloc(size + 2);
  memset(buffer, '\0', size+2);
  /* copy our crc'd and escaped characters into final buffer */
  memcpy(&buffer[1], outbuf, size);
    
  buffer[0] = 0x7E; /* Add our beginning control character */
  buffer[size+1] = 0x7E; /* Add out ending control character */
    
  free(inbuf); /* We don't need this anymore */
  free(outbuf); /* We don't need this anymore */
    
  *output = buffer;
  *outsize = size + 2;
  
  return 0;
}

int dload_response(void *input, uint32_t insize,
		   uint8_t **output, uint32_t *outsize) {
  
  uint32_t size = 0;
  uint8_t* outbuf = NULL;
  dload_unescape(input, insize, &outbuf, &size);
  
  unsigned short crc = dm_crc16((const char*)&outbuf[1], size-4);
  unsigned short chk = *((unsigned short*)&outbuf[size-3]);
  if(crc != chk){
    fprintf(stderr, "Invalid CRC!!!\n");
    size += 2;
  }
  
  uint8_t* buffer = (uint8_t*)malloc(size-4);
  memset(buffer, '\0', size-4);
  memcpy(buffer, &outbuf[1], size-4);
  free(outbuf);
  
  *output = buffer;
  *outsize = size-4;
  return 0;
}

int dload_escape(uint8_t *input, uint32_t insize,
		 uint8_t **output, uint32_t *outsize) {
  
  int i = 0;
  unsigned int size = 0;
  
  for(i = 0; i < insize; i++){
    if(input[i] == 0x7E || input[i] == 0x7D)
      size++;
    
    size++;
  }
  
  int o = 0;
  unsigned char* buffer = NULL;
  buffer = (unsigned char*)malloc(size);
  memset(buffer, '\0', size);
  
  for(i = 0; i < insize; i++){
    if(input[i] == 0x7E){
      buffer[o] = 0x7D;
      buffer[o+1] = 0x7E ^ 0x20;
      o++;
    }else if(input[i] == 0x7D){
      buffer[o] = 0x7D;
      buffer[o+1] = 0x7D ^ 0x20;
      o++;
    }else{
      buffer[o] = input[i];
    }
    o++;
  }
  
  *outsize = size;
  *output = buffer;
  return 0;
}

int dload_unescape(uint8_t *input, uint32_t insize,
		   uint8_t **output, uint32_t *outsize) {
  
  int i = 0;
  unsigned int size = insize;
  for(i = insize; i >= 0; i--){
    if(input[i] == 0x7D) size--;
  }
    
  int o = 0;
  unsigned char* buffer = NULL;
  buffer = (unsigned char*) malloc(size);

  memset(buffer, '\0', size);
  for(i = 0; i <= insize; i++){
    if(input[i] == 0x7D){
      buffer[o] = input[i+1] ^ 0x20;
      i++;
    }else{
      buffer[o] = input[i];
    }
    o++;
  }
  
  *outsize = size;
  *output = buffer;
  return 0;
}

const char *dload_strerror(int errno) {

  switch(errno) {
  case DLOAD_NAK_INVALID_FRAME_FCS :
    return "Invalid frame FCS";
  case DLOAD_NAK_INVALID_DESTINATION_ADDRESS :
    return "Invalid destination address";
  case DLOAD_NAK_INVALID_LENGTH :
    return "Invalid length";
  case DLOAD_NAK_UNEXPECTED_END_OF_PACKET :
    return "Unexpected end of packet";
  case DLOAD_NAK_DATA_LENGTH_TOO_LARGE :
    return "Data length too large";
  case DLOAD_NAK_INVALID_COMMAND :
    return "Invalid command";
  case DLOAD_NAK_OPERATION_FAILT :
    return "Operation failed";
  case DLOAD_NAK_WRONG_FLASH_INTELLIGENT_ID :
    return "Wrong flash intelligent ID";
  case DLOAD_NAK_BAD_PROGRAMMING_VOLTAGE :
    return "Bad programming voltage";
  case DLOAD_NAK_WRITE_VERIFY_FAILED :
    return "Write verify failed";
  case DLOAD_NAK_UNLOCK_REQUIRED :
    return "Unlock requied";
  case DLOAD_NAK_INCORRECT_SECURITY_CODE :
    return "Incorrect security code";
  case DLOAD_NAK_CANNOT_POWER_DOWN_PHONE :
    return "Cannot power down phone";
  case DLOAD_NAK_OPERATION_NOT_PERMITTED :
    return "Operation not permitted";
  case DLOAD_NAK_INVALID_READ_ADDRESS :
    return "Invalid read address";
  default :
    return "To be continued...";
  }
}
