/*
 * ihex.h
 *
 * (c) 2016 Joachim Naulet <jnaulet@rdinnovation.fr>
 * 
 * Created on 3/15/2016 by Joachim Naulet
 */

#ifndef IHEX_H
#define IHEX_H

void *ihex_raw_from_file(const char *path, unsigned int *size_r, unsigned int *offset_r);
void ihex_free(void *ptr);

#endif
