//
//  util.h
//  usblogger
//
//  Created by Joshua Hill on 1/29/13.
//  Modified by Joachim Naulet on 3/10/16
//
//

#ifndef usblogger_util_h
#define usblogger_util_h

#include <stdint.h>
#include <sys/types.h>

typedef uint8_t boolean;
#ifndef TRUE
#define TRUE ((uint8_t) 1)
#endif
#ifndef FALSE
#define FALSE ((uint8_t) 0)
#endif

void hexdump (unsigned char *data, unsigned int amount);

uint16_t flip_endian16(uint16_t value);
uint32_t flip_endian32(uint32_t value);

uint16_t dm_crc16 (const char *buffer, size_t len);


#endif
