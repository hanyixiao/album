#ifndef __STB_JPG_LOAD_H
#define __STB_JPG_LOAD_H

#include "stdint.h"
#include "picojpeg.h"
#include "ff.h"
#include "stdint.h"
#include "stdlib.h"
#include <memory.h>
#include <math.h>
#include <assert.h>
#include <stdio.h>
typedef unsigned char (*my_pjpeg_need_bytes_callback_t)(unsigned char* pBuf, unsigned char buf_size, unsigned char *pBytes_actually_read, void *pCallback_data);

// unsigned char pjpeg_need_bytes_callback(unsigned char* pBuf, unsigned char buf_size, unsigned char *pBytes_actually_read, void *pCallback_data);
uint8_t *my_pjpeg_load_from_file(const char *pFilename, int *x, int *y, int *comps,int rgb565, pjpeg_scan_type_t *pScan_type, int reduce);

#endif 