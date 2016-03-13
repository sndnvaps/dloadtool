//
//  dload.h
//  dloadtool
//
//  Created by Joshua Hill on 1/31/13.
//  Modified by Joachim Naulet on 3/10/16
//
//

#ifndef __dloadtool__dload__
#define __dloadtool__dload__

#include <stdint.h>
  
//Start      Code      Address            Size           CRC         End
//7e         0f        20 02 fe 00        01 00          ?? ??       7e
//7e         05        20 01 20 00                       9f 1f       7e

#define DLOAD_WRITE                       0x01
#define DLOAD_ACK                         0x02
#define DLOAD_NAK                         0x03
#define DLOAD_ERASE                       0x04
#define DLOAD_EXECUTE                     0x05
#define DLOAD_NOP                         0x06
#define DLOAD_PARAM_REQ                   0x07
#define DLOAD_PARAM_RESP                  0x08
#define DLOAD_MEMORY_DUMP                 0x09
#define DLOAD_RESET                       0x0A
#define DLOAD_UNLOCK                      0x0B
#define DLOAD_SW_VER_REQ                  0x0C
#define DLOAD_SW_VERS_RESP                0x0D
#define DLOAD_POWERDOWN                   0x0E
#define DLOAD_WRITE_ADDR                  0x0F
#define DLOAD_MEMORY_DEBUG_QUERY          0x10  //Memory Debug Query
#define DLOAD_MEMORY_DEBUG_INFO           0x11  //Memory Debug Info
#define DLOAD_MEMORY_READ_REQ             0x12  //Memory Read Request
#define DLOAD_MEMORY_READ                 0x13  //Memory Read
#define DLOAD_MEMOY_UNFRAMED_READ_REQ     0x14  //Memory Unframed Read Request
#define DLOAD_SERIAL_NUMBER_READ_REQ      0x14  //Serial Number Read Request
#define DLOAD_SERIAL_NUMBER_READ_RESP     0x14  //Serial Number Read Response
#define DLOAD_MEMORY_UNFRAMED_READ_RESP   0x15  //Memory Unframed Read Response
//#define DLOAD_SERIAL_NUMBER_READ_REQ      0x16  //Serial Number Read Request
//#define DLOAD_SERIAL_NUMBER_READ_RESP     0x16

typedef struct {
    uint16_t code;
    uint16_t sequence;
    uint8_t unknown;
    uint16_t size;
} __attribute__((packed)) dload_firmware_header;

typedef struct {
    uint8_t code;
    uint32_t address;
    uint16_t size;
    uint8_t buffer[0];
} __attribute__((packed)) dload_write_addr;

typedef struct {
    uint8_t code;
    uint32_t address;
} __attribute__((packed)) dload_execute;

typedef struct {
    uint8_t code;
    uint8_t length;
    uint8_t version[0];
} __attribute__((packed)) dload_sw_version;

typedef struct {
    uint8_t code;
    uint8_t version;
    uint8_t min_version;
    uint16_t max_write;
    uint8_t model;
    uint8_t device_size;
    uint8_t device_type;
} __attribute__((packed)) dload_params;

int dload_get_params(int fd);
int dload_get_sw_version(int fd);
int dload_send_execute(int fd, uint32_t address);
int dload_upload_firmware(int fd, uint32_t address, const char* path);
int dload_upload_data(int fd, uint32_t addr, const char *data, size_t len);

int dload_read(int fd, uint8_t* buffer, uint32_t size);
int dload_write(int fd, uint8_t* buffer, uint32_t size);

int dload_request(uint8_t* input, uint32_t insize,
		  uint8_t** output, uint32_t* outsize);
int dload_response(uint8_t* input, uint32_t insize,
		   uint8_t** output, uint32_t* outsize);
int dload_escape(uint8_t* input, uint32_t insize,
		 uint8_t** output, uint32_t* outsize);
int dload_unescape(uint8_t* input, uint32_t insize,
		   uint8_t** output, uint32_t* outsize);

#endif /* defined(__dloadtool__dload__) */
