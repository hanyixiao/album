//------------------------------------------------------------------------------
// jpg2tga.c
// JPEG to TGA file conversion example program.
// Public domain, Rich Geldreich <richgel99@gmail.com>
// Last updated Nov. 26, 2010
//------------------------------------------------------------------------------
#include "picojpeg.h"
#include "ff.h"
#include "stdint.h"
#include "stb_jpg_load.h"
//------------------------------------------------------------------------------
#ifndef max
#define max(a,b)    (((a) > (b)) ? (a) : (b))
#endif
#ifndef min
#define min(a,b)    (((a) < (b)) ? (a) : (b))
#endif
#define RGB888_RED      0x00ff0000
#define RGB888_GREEN    0x0000ff00
#define RGB888_BLUE     0x000000ff
 
#define RGB565_RED      0xf800
#define RGB565_GREEN    0x07e0
#define RGB565_BLUE     0x001f
/* clang-format on */

static unsigned short RGB888ToRGB565(unsigned int n888Color)
{
   unsigned short n565Color = 0;

   // 获取RGB单色，并截取高位
   unsigned char cRed = (n888Color & RGB888_RED) >> 19;
   unsigned char cGreen = (n888Color & RGB888_GREEN) >> 10;
   unsigned char cBlue = (n888Color & RGB888_BLUE) >> 3;

   // 连接
   n565Color = (cRed << 11) + (cGreen << 5) + (cBlue << 0);
   return n565Color;
}

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
static FIL g_pInFile;
static uint16_t g_nInFileSize;
static uint16_t g_nInFileOfs;
//------------------------------------------------------------------------------
unsigned char my_pjpeg_need_bytes_callback(unsigned char* pBuf, unsigned char buf_size, unsigned char *pBytes_actually_read, void *pCallback_data)
{
   uint16_t n;
   // pCallback_data;
   uint32_t v_ret_len;
   n = min(g_nInFileSize - g_nInFileOfs, buf_size);
   f_read(&g_pInFile,pBuf,n,&v_ret_len);
   if (n && (v_ret_len!= n)){
      printf("[INFO]:read error %d\n",v_ret_len);  
      return PJPG_STREAM_READ_ERROR;
   }

   *pBytes_actually_read = (unsigned char)(n);
   g_nInFileOfs += n;
   return 0;
}
//------------------------------------------------------------------------------
// Loads JPEG image from specified file. Returns NULL on failure.
// On success, the malloc()'d image's width/height is written to *x and *y, and
// the number of components (1 or 3) is written to *comps.
// pScan_type can be NULL, if not it'll be set to the image's pjpeg_scan_type_t.
// Not thread safe.
// If reduce is non-zero, the image will be more quickly decoded at approximately
// 1/8 resolution (the actual returned resolution will depend on the JPEG 
// subsampling factor).
uint8_t *my_pjpeg_load_from_file(const char *pFilename, int *x, int *y, int *comps,int rgb565, pjpeg_scan_type_t *pScan_type, int reduce)
{
   pjpeg_image_info_t image_info;
   int mcu_x = 0;
   int mcu_y = 0;
   uint16_t row_pitch;
   uint8_t *pImage;
   uint8_t status;
   uint16_t decoded_width, decoded_height;
   uint16_t row_blocks_per_mcu, col_blocks_per_mcu;
   FRESULT ret=FR_OK;
   uint8_t one_color_size;
   *x = 0;
   *y = 0;
   *comps = 0;
   if (pScan_type) *pScan_type = PJPG_GRAYSCALE;
   printf("[INFO]:begin to read\n");
   f_open(&g_pInFile,pFilename, FA_READ);
   if (ret !=FR_OK) return 0;

   g_nInFileOfs = 0;
  
   // f_lseek(g_pInFile,
   // fseek(g_pInFile, 0, SEEK_END);
   f_lseek(&g_pInFile,f_size(&g_pInFile));
   g_nInFileSize = f_tell(&g_pInFile);
   // fseek(g_pInFile, 0, SEEK_SET);
   f_lseek(&g_pInFile,0);
   printf("[INFO]:file info:%d\n",g_nInFileSize); 
     
   status = pjpeg_decode_init(&image_info, my_pjpeg_need_bytes_callback, 0, (unsigned char)reduce);
         
   if (status)
   {
      printf("pjpeg_decode_init() failed with status %u\n", status);
      if (status == PJPG_UNSUPPORTED_MODE)
      {
         printf("Progressive JPEG files are not supported.\n");
      }

      f_close(&g_pInFile);
      return 0;
   }
   
   if (pScan_type)
      *pScan_type = image_info.m_scanType;

   if ((image_info.m_comps != 1) && rgb565) //RGB565
      one_color_size = 2;
   else // grayscale or RGB888
      one_color_size = image_info.m_comps;
   printf("[INFO]:one color size is %d\n",one_color_size);
   // In reduce mode output 1 pixel per 8x8 block.
   decoded_width = reduce ? (image_info.m_MCUSPerRow * image_info.m_MCUWidth) / 8 : image_info.m_width;
   decoded_height = reduce ? (image_info.m_MCUSPerCol * image_info.m_MCUHeight) / 8 : image_info.m_height;

   row_pitch = decoded_width * one_color_size;
   pImage = (uint8_t *)malloc(row_pitch * decoded_height);
   if (!pImage)
   {
      f_close(&g_pInFile);
      return 0;
   }

   row_blocks_per_mcu = image_info.m_MCUWidth >> 3;
   col_blocks_per_mcu = image_info.m_MCUHeight >> 3;
   
   for ( ; ; )
   {
      int y, x;
      uint8_t *pDst_row;

      status = pjpeg_decode_mcu();
      
      if (status)
      {
         if (status != PJPG_NO_MORE_BLOCKS)
         {
            printf("pjpeg_decode_mcu() failed with status %u\n", status);

            free(pImage);
            f_close(&g_pInFile);
            return 0;
         }

         break;
      }

      if (mcu_y >= image_info.m_MCUSPerCol)
      {
         free(pImage);
         f_close(&g_pInFile);
         return 0;
      }

      if (reduce)
      {
         // In reduce mode, only the first pixel of each 8x8 block is valid.
         pDst_row = pImage + mcu_y * col_blocks_per_mcu * row_pitch + mcu_x * row_blocks_per_mcu * image_info.m_comps;
         if (image_info.m_scanType == PJPG_GRAYSCALE)
         {
            *pDst_row = image_info.m_pMCUBufR[0];
         }
         else
         {
            uint16_t y, x;
            for (y = 0; y < col_blocks_per_mcu; y++)
            {
               uint16_t src_ofs = (y * 128U);
               for (x = 0; x < row_blocks_per_mcu; x++)
               {
                  if (rgb565)
                  {
                     unsigned short color = RGB888ToRGB565(image_info.m_pMCUBufR[src_ofs] << 16 | image_info.m_pMCUBufG[src_ofs] << 8 | image_info.m_pMCUBufB[src_ofs]);
                     pDst_row[0] = color >> 8 & 0xff;
                     pDst_row[1] = color & 0xff;
                  }
                  else{
                     pDst_row[0] = image_info.m_pMCUBufR[src_ofs];
                     pDst_row[1] = image_info.m_pMCUBufG[src_ofs];
                     pDst_row[2] = image_info.m_pMCUBufB[src_ofs];
                  }
                  pDst_row += one_color_size;
                  src_ofs += 64;
               }

               pDst_row += row_pitch - one_color_size * row_blocks_per_mcu;
            }
         }
      }
      else
      {
         // Copy MCU's pixel blocks into the destination bitmap.
         pDst_row = pImage + (mcu_y * image_info.m_MCUHeight) * row_pitch + (mcu_x * image_info.m_MCUWidth * one_color_size);

         for (y = 0; y < image_info.m_MCUHeight; y += 8)
         {
            const int by_limit = min(8, image_info.m_height - (mcu_y * image_info.m_MCUHeight + y));

            for (x = 0; x < image_info.m_MCUWidth; x += 8)
            {
               uint8_t *pDst_block = pDst_row + x * one_color_size;

               // Compute source byte offset of the block in the decoder's MCU buffer.
               uint16_t src_ofs = (x * 8U) + (y * 16U);
               const uint8_t *pSrcR = image_info.m_pMCUBufR + src_ofs;
               const uint8_t *pSrcG = image_info.m_pMCUBufG + src_ofs;
               const uint8_t *pSrcB = image_info.m_pMCUBufB + src_ofs;

               const int bx_limit = min(8, image_info.m_width - (mcu_x * image_info.m_MCUWidth + x));

               if (image_info.m_scanType == PJPG_GRAYSCALE)
               {
                  int bx, by;
                  for (by = 0; by < by_limit; by++)
                  {
                     uint8_t *pDst = pDst_block;

                     for (bx = 0; bx < bx_limit; bx++)
                        *pDst++ = *pSrcR++;

                     pSrcR += (8 - bx_limit);

                     pDst_block += row_pitch;
                  }
               }
               else
               {
                  int bx, by;
                  for (by = 0; by < by_limit; by++)
                  {
                     uint8_t *pDst = pDst_block;

                     for (bx = 0; bx < bx_limit; bx++)
                     {
                        if (rgb565)
                        {
                           unsigned short color = RGB888ToRGB565(*pSrcR << 16 | *pSrcG << 8 | *pSrcB);
                           pDst[0] = color >> 8 & 0xff;
                           pDst[1] = color & 0xff;
                           // 
                        }
                        else{
                           pDst[0] = *pSrcR;
                           pDst[1] = *pSrcG;
                           pDst[2] = *pSrcB;
                        }
                        pSrcR++;
                        pSrcG++;
                        pSrcB++;
                        pDst += one_color_size;
                     }

                     pSrcR += (8 - bx_limit);
                     pSrcG += (8 - bx_limit);
                     pSrcB += (8 - bx_limit);

                     pDst_block += row_pitch;
                  }
               }
            }

            pDst_row += (row_pitch * 8);
         }
      }

      mcu_x++;
      if (mcu_x == image_info.m_MCUSPerRow)
      {
         mcu_x = 0;
         mcu_y++;
      }
   }

   f_close(&g_pInFile);

   *x = decoded_width;
   *y = decoded_height;
   *comps = image_info.m_comps;

   return pImage;
}
//------------------------------------------------------------------------------
typedef struct image_compare_results_tag
{
   double max_err;
   double mean;
   double mean_squared;
   double root_mean_squared;
   double peak_snr;
} image_compare_results;

static void get_pixel(int* pDst, const uint8_t *pSrc, int luma_only, int num_comps)
{
   int r, g, b;
   if (num_comps == 1)
   {
      r = g = b = pSrc[0];
   }
   else if (luma_only)
   {
      const int YR = 19595, YG = 38470, YB = 7471;
      r = g = b = (pSrc[0] * YR + pSrc[1] * YG + pSrc[2] * YB + 32768) / 65536;
   }
   else
   {
      r = pSrc[0]; g = pSrc[1]; b = pSrc[2];
   }
   pDst[0] = r; pDst[1] = g; pDst[2] = b;
}

// Compute image error metrics.
static void image_compare(image_compare_results *pResults, int width, int height, const uint8_t *pComp_image, int comp_image_comps, const uint8_t *pUncomp_image_data, int uncomp_comps, int luma_only)
{
   double hist[256];
   double sum = 0.0f, sum2 = 0.0f;
   double total_values;
   const uint16_t first_channel = 0, num_channels = 3;
   int x, y;
   uint16_t i;

   memset(hist, 0, sizeof(hist));
   
   for (y = 0; y < height; y++)
   {
      for (x = 0; x < width; x++)
      {
         uint16_t c;
         int a[3]; 
         int b[3]; 

         get_pixel(a, pComp_image + (y * width + x) * comp_image_comps, luma_only, comp_image_comps);
         get_pixel(b, pUncomp_image_data + (y * width + x) * uncomp_comps, luma_only, uncomp_comps);

         for (c = 0; c < num_channels; c++)
            hist[labs(a[first_channel + c] - b[first_channel + c])]++;
      }
   }

   pResults->max_err = 0;
   
   for (i = 0; i < 256; i++)
   {
      double x;
      if (!hist[i])
         continue;
      if (i > pResults->max_err)
         pResults->max_err = i;
      x = i * hist[i];
      sum += x;
      sum2 += i * x;
   }

   // See http://bmrc.berkeley.edu/courseware/cs294/fall97/assignment/psnr.html
   total_values = width * height;

   pResults->mean = sum / total_values;
   pResults->mean_squared = sum2 / total_values;

   pResults->root_mean_squared = sqrt(pResults->mean_squared);

   if (!pResults->root_mean_squared)
      pResults->peak_snr = 1e+10f;
   else
      pResults->peak_snr = log10(255.0f / pResults->root_mean_squared) * 20.0f;
}
//------------------------------------------------------------------------------
// int main(int arg_c, char *arg_v[])
// {
//    int n = 1;
//    const char *pSrc_filename;
//    const char *pDst_filename;
//    int width, height, comps;
//    pjpeg_scan_type_t scan_type;
//    const char* p = "?";
//    uint8_t *pImage;
//    int reduce = 0;
   
//    printf("picojpeg example v1.1, Rich Geldreich <richgel99@gmail.com>, Compiled " __TIME__ " " __DATE__ "\n");

//    if ((arg_c < 3) || (arg_c > 4))
//       return print_usage();
   
//    pSrc_filename = arg_v[n++];
//    pDst_filename = arg_v[n++];
//    if (arg_c == 4)
//       reduce = atoi(arg_v[n++]) != 0;

//    printf("Source file:      \"%s\"\n", pSrc_filename);
//    printf("Destination file: \"%s\"\n", pDst_filename);
//    printf("Reduce during decoding: %u\n", reduce);
   
//    pImage = pjpeg_load_from_file(pSrc_filename, &width, &height, &comps, &scan_type, reduce);
//    if (!pImage)
//    {
//       printf("Failed loading source image!\n");
//       return EXIT_FAILURE;
//    }

//    printf("Width: %i, Height: %i, Comps: %i\n", width, height, comps);
   
//    switch (scan_type)
//    {
//       case PJPG_GRAYSCALE: p = "GRAYSCALE"; break;
//       case PJPG_YH1V1: p = "H1V1"; break;
//       case PJPG_YH2V1: p = "H2V1"; break;
//       case PJPG_YH1V2: p = "H1V2"; break;
//       case PJPG_YH2V2: p = "H2V2"; break;
//    }
//    printf("Scan type: %s\n", p);

//    if (!stbi_write_tga(pDst_filename, width, height, comps, pImage))
//    {
//       printf("Failed writing image to destination file!\n");
//       return EXIT_FAILURE;
//    }

//    printf("Successfully wrote destination file %s\n", pDst_filename);
   
//    // Now load the JPEG file using stb_image.c and compare the decoded pixels vs. picojpeg's.
//    // stb_image.c uses filtered and higher precision chroma upsampling, and a higher precision IDCT vs. picojpeg so some error is to be expected.
//    if (!reduce)
//    {
//       int stb_width = 0, stb_height = 0, stb_actual_comps = 0, stb_req_comps = 0;
//       unsigned char *pSTB_image_data;
//       stb_req_comps = (scan_type == PJPG_GRAYSCALE) ? 1 : 3;
//       pSTB_image_data = stbi_load(pSrc_filename, &stb_width, &stb_height, &stb_actual_comps, stb_req_comps);
//       if (!pSTB_image_data)
//       {
//          printf("Failed decoding using stb_image.c!\n");
//       }
//       else if ((stb_width != width) || (stb_height != height) || (stb_actual_comps != comps))
//       {
//          printf("Image was successfully decompresed using stb_image.c, but the resulting dimensions/comps differ from picojpeg's!\n");
//       }
//       else
//       {
//          image_compare_results results;
//          memset(&results, 0, sizeof(results));

//          image_compare(&results, width, height, pImage, comps, pSTB_image_data, stb_actual_comps, (scan_type == PJPG_GRAYSCALE));
                  
//          printf("picojpeg vs. stb_image:\n");
//          printf("Error Max: %f, Mean: %f, Mean^2: %f, RMSE: %f, PSNR: %f\n", results.max_err, results.mean, results.mean_squared, results.root_mean_squared, results.peak_snr);
//       }
      
//       free(pSTB_image_data);
//    }

//    free(pImage);

//    return EXIT_SUCCESS;
// }
// //------------------------------------------------------------------------------

