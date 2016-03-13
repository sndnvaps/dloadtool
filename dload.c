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

#define BUFSIZE 256

unsigned char magic[] =
  "\x01QCOM fast download protocol host\x02\x02\x01";
int dload_send_magic(int fd) {

  uint8_t response[BUFSIZE];
  uint8_t request = DLOAD_WRITE;
  
  dload_write(fd, magic, sizeof(magic) - 1);
  dload_read(fd, response, sizeof response);
  if(response[0] == 0x2){
    fprintf(stderr, "%s\n", &response[1]);
    return 0;
  }

  return -1;
}

int dload_send_reset(int fd) {

  uint8_t response[BUFSIZE];
  uint8_t request = DLOAD_RESET;
 
  dload_write(fd, &request, sizeof(request));
  dload_read(fd, response, sizeof response);
  if(response[0] == 0x2)
    return 0;

  return -1;
}

int dload_get_params(int fd) {
  
  uint8_t output[BUFSIZE];
  dload_params *response = NULL;
  uint8_t request = DLOAD_PARAM_REQ;
  
  //memset(output, '\0', sizeof(output));
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
    
  }else{
    fprintf(stderr, "Error receiving software parameters!!\n");
    return -1;
  }
  
  return 0;
}

int dload_get_sw_version(int fd) {
  
  uint8_t output[BUFSIZE];
  dload_sw_version *response;
  uint8_t request = DLOAD_SW_VER_REQ;
    
  //memset(output, '\0', sizeof(output));
  dload_write(fd, &request, sizeof(request));
  dload_read(fd, output, sizeof(output));
  if(output[0] == DLOAD_SW_VERS_RESP) {
    response = (dload_sw_version*)output;
    printf("Software Version: %s\n", response->version);
    
  }else{
    fprintf(stderr, "Error receiving software version!!\n");
    return -1;
  }
  
  return 0;
}

int dload_send_unlock(int fd, uint64_t key) {

  dload_unlock request;
  uint8_t response[BUFSIZE];  
  
  request.code = DLOAD_UNLOCK;
  request.security_key = flip_endian64(key);
  
  dload_write(fd, (uint8_t*)&request, sizeof(request));
  dload_read(fd, response, sizeof(response));
  if(response[0] == 0x2)
    return 0;
  
  return -1;
}

//Frame      Code      Address                Size
//           0x0f       0x20012000            0x100
//7e         0x05       0x20012000
unsigned char done[] = "\x7e\x05\x20\x01\x20\x00\x9f\x1f\x7e";
int dload_upload_firmware(int fd, uint32_t addr, const char *path) {
  
  FILE *fp = NULL;
  uint8_t input[BUFSIZE];
  uint8_t output[BUFSIZE];

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
        
	dload_write(fd, (uint8_t*)packet, sizeof(dload_write_addr) + len);
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
		      const char *data, size_t len) {
  
  uint8_t buf[BUFSIZE];
  dload_write_addr *packet = (dload_write_addr*)buf;
  
  memcpy(packet->buffer, data, len);
  packet->code = DLOAD_WRITE_ADDR;
  packet->size = flip_endian16(len);
  packet->address = flip_endian32(addr);
    
  dload_write(fd, (uint8_t*)packet, sizeof(dload_write_addr) + len);
  dload_read(fd, buf, sizeof(buf));
  if(buf[0] != 0x2){
    fprintf(stderr, "Error 0x%hhx!!!\n", buf[0]);
    return -1;
  }
  
  return 0;
}

int dload_send_execute(int fd, uint32_t address) {
  
  uint8_t response[BUFSIZE];
  dload_execute request;
  
  request.code = DLOAD_EXECUTE;
  request.address = flip_endian32(address);
    
  dload_write(fd, (uint8_t*)&request, sizeof(dload_execute));
  dload_read(fd, response, sizeof(response));

  if(response[0] == 0x2)
    return 0;
  
  return -1;
}

int dload_read(int fd, uint8_t *buffer, uint32_t size) {
  
  uint32_t insize = 0;
  uint32_t outsize = 0;
  uint8_t* inbuf = NULL;
  uint8_t* outbuf = NULL;
    
  insize = size;
  inbuf = (uint8_t*)malloc(size);
  
  //fprintf(stderr, "< ");
  insize = read(fd, inbuf, size);
  if(insize > 0){
    dload_response(inbuf, insize, &outbuf, &outsize);
    if(outsize <= size) {
      hexdump(outbuf, outsize);
      memcpy(buffer, outbuf, outsize);
    }
  }
    
  free(inbuf);
  free(outbuf);
  return outsize;
}

int dload_write(int fd, uint8_t *buffer, uint32_t size) {
  
  uint32_t outsize = 0;
  uint8_t* outbuf = NULL;
  
  //fprintf(stderr, "> ");
  dload_request(buffer, size, &outbuf, &outsize);
  if(outsize > 0) {
    outsize = write(fd, outbuf, outsize);
    //hexdump(buffer, size);
  }
  
  free(outbuf);
  return outsize;
}

int dload_request(uint8_t *input, uint32_t insize,
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

int dload_response(uint8_t *input, uint32_t insize,
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
