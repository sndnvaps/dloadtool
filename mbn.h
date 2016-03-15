/*
 * mbn.h
 * dloadtool
 *
 * Created by Joachim Naulet on 3/14/2016
 *
 * Copyright (c) 2016 R&D Innovation. All rights reserved
 */

#ifndef MBN_H
#define MBN_H

#include <stdint.h>

/* 32-bit, little endian */
typedef struct mbn_header {
  uint32_t codeword;         /* d1 dC 4b 84 */
  uint32_t magic;            /* 34 10 d7 73 */
  uint32_t image_type;       /* see below */
  uint32_t padding[2];       /* FFFFFFFF FFFFFFFF */
  uint32_t head_length;      /* should be 50 */
  uint32_t load_address;     /* load addr / start addr */
  uint32_t body_length;      /* code + signature + certificater store size */
  uint32_t code_length;
  uint32_t signature_address;
  uint32_t signature_length; /* 256 bytes */
  uint32_t cert_store_address;
  uint32_t cert_store_length;
  uint8_t  body[0];
} __attribute__ ((packed)) mbn_header_t;

/* Numbers */
#define MBN_HEAD_CODEWORD 0x844bdcd1 /* little-endian */
#define MBN_HEAD_MAGIC    0x73d71034 /* little-endian */

/* Image type */
#define MBN_HEAD_IMG_EHOSTDL 0x0d
#define MBN_HEAD_IMG_SBL1    0x15

/* Functions */
int mbn_from_file(const char *path, mbn_header_t *h);
void *mbn_from_mem(void *buf, size_t size, mbn_header_t *h);

int mbn_display_header(mbn_header_t *h);

#endif
