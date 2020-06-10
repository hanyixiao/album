#include "ff.h"
#include "rgb2bmp.h"
#include "sdcard.h"

int rgb565tobmp(char *buf, int width, int height, const char *filename)
{
    FIL file;
    FRESULT ret = FR_OK;
    uint32_t ret_len = 0;
    uint32_t i;
    uint16_t temp;
    uint16_t *ptr;

    BitMapFileHeader bmfHdr; /* file header */
    BitMapInfoHeader bmiHdr; /* information header */
    RgbQuad bmiClr[3];       /* color palette */

    bmiHdr.biSize = sizeof(BitMapInfoHeader);
    bmiHdr.biWidth = width;
    bmiHdr.biHeight = height;
    bmiHdr.biPlanes = 1;
    bmiHdr.biBitCount = 16;
    bmiHdr.biCompression = BI_BITFIELDS;
    bmiHdr.biSizeImage = (width * height * 2);
    bmiHdr.biXPelsPerMeter = 0;
    bmiHdr.biYPelsPerMeter = 0;
    bmiHdr.biClrUsed = 0;
    bmiHdr.biClrImportant = 0;

    /* rgb565 mask */
    bmiClr[0].rgbBlue = 0;
    bmiClr[0].rgbGreen = 0xF8;
    bmiClr[0].rgbRed = 0;
    bmiClr[0].rgbReserved = 0;

    bmiClr[1].rgbBlue = 0xE0;
    bmiClr[1].rgbGreen = 0x07;
    bmiClr[1].rgbRed = 0;
    bmiClr[1].rgbReserved = 0;

    bmiClr[2].rgbBlue = 0x1F;
    bmiClr[2].rgbGreen = 0;
    bmiClr[2].rgbRed = 0;
    bmiClr[2].rgbReserved = 0;

    bmfHdr.bfType = 0x4D42;
    bmfHdr.bfSize = sizeof(BitMapFileHeader) + sizeof(BitMapInfoHeader) + sizeof(RgbQuad) * 3 + bmiHdr.biSizeImage;
    bmfHdr.bfReserved1 = 0;
    bmfHdr.bfReserved2 = 0;
    bmfHdr.bfOffBits = sizeof(BitMapFileHeader) + sizeof(BitMapInfoHeader) + sizeof(RgbQuad) * 3;

    ptr = (uint16_t *)buf;

    for(i = 0; i < width * height; i += 2)
    {
        temp = ptr[i];
        ptr[i] = ptr[i + 1];
        ptr[i + 1] = temp;
    }

    if((ret = f_open(&file, filename, FA_CREATE_ALWAYS | FA_WRITE)) != FR_OK)
    {
        printf("create file %s err[%d]\n", filename, ret);
        return ret;
    }
    printf("[INFO] open file OK\n");
    
    f_write(&file, &bmfHdr, sizeof(BitMapFileHeader), &ret_len);
    f_sync(&file);//同步数据写入
    f_write(&file, &bmiHdr, sizeof(BitMapInfoHeader), &ret_len);
    f_sync(&file);//同步数据写入
    f_write(&file, &bmiClr, 3 * sizeof(RgbQuad), &ret_len);
    f_sync(&file);//同步数据写入
    printf("[INFO] write header OK\n");
    printf("[INFO] write byte %d",ret_len);
    
    //printf("[INFO] buf is %s",buf);
   // printf("[INFO] sizeof buf %ld",sizeof(buf));
   
   // f_write(&file,(char *)buf, bmiHdr.biSizeImage, &ret_len);
    uint16_t remain=bmiHdr.biSizeImage;
    for(int i=0;i<bmiHdr.biSizeImage;i+=512)
    {
        f_write(&file,(char *)buf,512, &ret_len);
        buf+=512;
        remain-=512;
    }
    f_write(&file,(char *)buf,remain,&ret_len);
    // f_sync(&file);//同步数据写入
    printf("[INFO] write bmp %s OK\n",filename);
    f_close(&file);

    return 0;
}
FRESULT sd_write_test(TCHAR *path,char *src)
{
    FIL file;
    FRESULT ret = FR_OK;
   // printf("/*******************sd write test*******************/\r\n");
    uint32_t v_ret_len = 0;

    FILINFO v_fileinfo;
    if ((ret = f_stat(path, &v_fileinfo)) == FR_OK)
    {
       // printf("%s length is %lld\r\n", path, v_fileinfo.fsize);
    }
    else
    {
        printf("%s fstat err [%d]\r\n", path, ret);
    }

    if ((ret = f_open(&file, path, FA_READ)) == FR_OK)
    {
        char v_buf[64] = {0};
        ret = f_read(&file, (void *)v_buf, 64, &v_ret_len);
        if (ret != FR_OK)
        {
           // printf("Read %s err[%d]\r\n", path, ret);
        }
        else
        {
         //   printf("Read :> %s %d bytes lenth\r\n", v_buf, v_ret_len);
        }
        f_close(&file);
    }

    if ((ret = f_open(&file, path, FA_CREATE_ALWAYS | FA_WRITE)) != FR_OK)
    {
        printf("open file %s err[%d]\r\n", path, ret);
        return ret;
    }
    else
    {
        printf("Open %s ok\r\n", path);
    }
    // ret = f_write(&file, hello, sizeof(hello), &v_ret_len);
    uint16_t* ptr = (uint16_t *)src;
    uint16_t temp;
    for(int i = 0; i < 320 * 240; i += 2)
    {
        temp = ptr[i];
        ptr[i] = ptr[i + 1];
        ptr[i + 1] = temp;
    }
    ret = f_write(&file,(char *)src,320*240*2,&v_ret_len);
    f_sync(&file);//同步数据写入
    f_write(&file,(char *)src,320*240*2,&v_ret_len);
    if (ret != FR_OK)
    {
        printf("Write %s err[%d]\r\n", path, ret);
    }
    else
    {
        printf("Write %d bytes to %s ok\r\n", v_ret_len, path);
    }
    f_close(&file);
    return ret;
}