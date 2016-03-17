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

extern int ack_errno;

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

/* NACK codes - Only for information, no use at the moment 
 * Data is from Gassan Idriss' openPST Project
 */
#define DLOAD_NAK_INVALID_FRAME_FCS           0x01
#define DLOAD_NAK_INVALID_DESTINATION_ADDRESS 0x02
#define DLOAD_NAK_INVALID_LENGTH              0x03
#define DLOAD_NAK_UNEXPECTED_END_OF_PACKET    0x04
#define DLOAD_NAK_DATA_LENGTH_TOO_LARGE       0x05
#define DLOAD_NAK_INVALID_COMMAND             0x06
#define DLOAD_NAK_OPERATION_FAILT             0x07
#define DLOAD_NAK_WRONG_FLASH_INTELLIGENT_ID  0x08
#define DLOAD_NAK_BAD_PROGRAMMING_VOLTAGE     0x09
#define DLOAD_NAK_WRITE_VERIFY_FAILED         0x0A
#define DLOAD_NAK_UNLOCK_REQUIRED             0x0B
#define DLOAD_NAK_INCORRECT_SECURITY_CODE     0x0C
#define DLOAD_NAK_CANNOT_POWER_DOWN_PHONE     0x0D
#define DLOAD_NAK_OPERATION_NOT_PERMITTED     0x0E
#define DLOAD_NAK_INVALID_READ_ADDRESS        0x0F
/* To Be continued ? */

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
    uint16_t size;
} __attribute__((packed)) dload_read_req;
#define dload_erase dload_read_req

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

typedef struct {
  uint8_t code;
  uint64_t security_key;
} __attribute__((packed)) dload_unlock;

typedef struct {
  uint8_t code;
  uint16_t errno;
} __attribute__((packed)) dload_ack;

extern int nak_errno;

int dload_send_magic(int fd);
int dload_send_reset(int fd);
int dload_send_unlock(int fd, uint64_t key);
int dload_get_params(int fd);
int dload_get_sw_version(int fd);
int dload_send_execute(int fd, uint32_t address);
int dload_upload_firmware(int fd, uint32_t address, const char* path);
int dload_upload_data(int fd, uint32_t addr, const void *data, size_t len);
int dload_memory_read_req(int fd, uint32_t address, size_t len);
int dload_send_erase(int fd, uint32_t address, size_t len);
  
int dload_read(int fd, void* buffer, uint32_t size);
int dload_write(int fd, void* buffer, uint32_t size);

int dload_request(void* input, uint32_t insize,
		  uint8_t** output, uint32_t* outsize);
int dload_response(void* input, uint32_t insize,
		   uint8_t** output, uint32_t* outsize);
int dload_escape(uint8_t* input, uint32_t insize,
		 uint8_t** output, uint32_t* outsize);
int dload_unescape(uint8_t* input, uint32_t insize,
		   uint8_t** output, uint32_t* outsize);

#endif /* defined(__dloadtool__dload__) */
