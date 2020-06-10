/* Copyright 2018 Canaan Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#include <stdio.h>
#include <string.h>
#include "dvp.h"
#include "fpioa.h"
#include "lcd.h"
#include "ov5640.h"
#include "ov2640.h"
#include "gc0328.h"
#include "cambus.h"
#include "plic.h"
#include "sysctl.h"
#include "uarths.h"
#include "nt35310.h"
#include "board_config.h"
#include "sleep.h"
#include "sdcard.h"
#include "ff.h"
#include "i2s.h"
#include "rgb2bmp.h"
#include <stdlib.h>
#include "picojpeg.h"
#include "picojpeg_util.h"
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#define STB_IMAGE_RESIZE_IMPLEMENTATION
#include "stb_image_resize.h"

/**
 *  lcd_cs	    36
 *  lcd_rst	37
 *  lcd_dc	    38
 *  lcd_wr 	39
 * */

static void io_mux_init(void)
{

#if BOARD_LICHEEDAN
    /* Init DVP IO map and function settings */

    /* Init SPI IO map and function settings */
    fpioa_set_function(37, FUNC_GPIOHS0 + RST_GPIONUM);
    fpioa_set_function(38, FUNC_GPIOHS0 + DCX_GPIONUM);
    fpioa_set_function(36, FUNC_SPI0_SS3);
    fpioa_set_function(39, FUNC_SPI0_SCLK);
    
    sysctl_set_spi0_dvp_data(1);
    /* Init sdcard IO map and function setttings */
    //io28--mosi
    //io26--miso
    //io29--cs
    //io27--clk
    // fpioa_set_function(27, FUNC_SPI0_SCLK); //clk
    // fpioa_set_function(28, FUNC_SPI0_D0);   //MOSI
    // fpioa_set_function(26, FUNC_SPI0_D1);   //MISO
    // fpioa_set_function(29, FUNC_GPIOHS7);   //cs
    fpioa_set_function(27, FUNC_SPI1_SCLK); //clk
    fpioa_set_function(28, FUNC_SPI1_D0);   //MOSI
    fpioa_set_function(26, FUNC_SPI1_D1);   //MISO
    fpioa_set_function(29, FUNC_GPIOHS7);   //cs
    // fpioa_set_function(29, FUNC_SPI0_SS3);

    // fpioa_set_function(32, FUNC_I2S0_OUT_D0);
    // fpioa_set_function(34, FUNC_I2S0_SCLK);
    // fpioa_set_function(31, FUNC_I2S0_WS);
#else
    /* Init SPI IO map and function settings */
    fpioa_set_function(8, FUNC_GPIOHS0 + DCX_GPIONUM);
    fpioa_set_function(6, FUNC_SPI0_SS3);
    fpioa_set_function(7, FUNC_SPI0_SCLK);

    sysctl_set_spi0_dvp_data(1);

#endif
}

static void io_set_power(void)
{
#if BOARD_LICHEEDAN
    /* Set dvp and spi pin to 1.8V */
    sysctl_set_power_mode(SYSCTL_POWER_BANK6, SYSCTL_POWER_V18);
    sysctl_set_power_mode(SYSCTL_POWER_BANK7, SYSCTL_POWER_V18);
#else
    /* Set dvp and spi pin to 1.8V */
    sysctl_set_power_mode(SYSCTL_POWER_BANK1, SYSCTL_POWER_V18);
    sysctl_set_power_mode(SYSCTL_POWER_BANK2, SYSCTL_POWER_V18);
#endif
}
static int sdcard_test(void)
{
    uint8_t status;

    printf("/******************sdcard test*****************/\r\n");
    status = sd_init();
    printf("sd init %d\r\n", status);
    if (status != 0)
    {
        return status;
    }

    printf("card info status %d\r\n", status);
    printf("CardCapacity:%ld\r\n", cardinfo.CardCapacity);
    printf("CardBlockSize:%d\r\n", cardinfo.CardBlockSize);
    return 0;
}

static int fs_test(void)
{
    static FATFS sdcard_fs;
    FRESULT status;
    DIR dj;
    FILINFO fno;

    printf("/********************fs test*******************/\r\n");
    status = f_mount(&sdcard_fs, _T("0:"), 1);
    printf("mount sdcard:%d\r\n", status);
    if (status != FR_OK)
        return status;

    printf("printf filename\r\n");
    status = f_findfirst(&dj, &fno, _T("0:"), _T("*"));
    while (status == FR_OK && fno.fname[0])
    {
        if (fno.fattrib & AM_DIR)
            printf("dir:%s\r\n", fno.fname);
        else
            printf("file:%s\r\n", fno.fname);
        status = f_findnext(&dj, &fno);
    }
    f_closedir(&dj);
    return 0;
}
FRESULT sd_read_img(TCHAR *path,uint8_t *src_img,int len)
{
    FIL file;
    FRESULT ret=FR_OK;
    uint32_t v_ret_len;
    if((ret=f_open(&file,path,FA_READ))==FR_OK)
    {
        ret = f_read(&file,(void *)src_img,len,&v_ret_len);
        if(ret !=FR_OK)
        {
            printf("[ERROR]:read file error\n");
        }
        else 
        {
            //printf("[INFO]:read file is %s",src_img);
        }
        f_close(&file);
    }
    return ret;
}

int main(void)
{
    uint64_t core_id = current_coreid();

    if (core_id == 0)
    {
        /* Set CPU and dvp clk */
        sysctl_pll_set_freq(SYSCTL_PLL0, 800000000UL);
        sysctl_pll_set_freq(SYSCTL_PLL1, 300000000UL);
        sysctl_pll_set_freq(SYSCTL_PLL2, 45158400UL);
        uarths_init();

        io_mux_init();
       // dmac_init();
        io_set_power();
        plic_init();
        sysctl_enable_irq();

        /* LCD init */
        printf("LCD init\n");
        lcd_init();
#if OV5640
        lcd_set_direction(DIR_YX_RLUD);
#elif OV2640
        lcd_set_direction(DIR_YX_RLDU);
#else 
        printf("[INFO] change the direction\n");
        lcd_set_direction(DIR_YX_LRUD);
#endif

        lcd_clear(WHITE);
        lcd_draw_string(LCD_X_MAX/2,LCD_Y_MAX/2-40,"LCD_INIT",RED);
        while(sdcard_test())
        {
            printf("SD card err\r\n");
            return -1;
        }
        if (fs_test())
        {
            printf("FAT32 err\r\n");
            return -1;
        }
        lcd_clear(WHITE);
        lcd_draw_string(LCD_X_MAX/2,LCD_Y_MAX/2-40,"GC0328 Init",RED);
        msleep(200);
        printf("[INFO]:begin to read jpeg\n");
        uint64_t tm= sysctl_get_time_us();
        uint8_t *img=(uint8_t *)malloc(43756);
        sd_read_img("0:deshakase.jpg",img,43756);   
        jpeg_image_t *jpeg = pico_jpeg_decode(img,43756);
        printf("[INFO]:decode use time %ld ms\n",(sysctl_get_time_us()-tm)/1000);
        lcd_rgb_draw(0,0,320,240,jpeg->img_data);
        msleep(5000);
        printf("[info]:test the code \n");
        /* system start */
        printf("system start\r\n");
        msleep(200);
        free(jpeg->img_data);
        free(jpeg);
        while (1)
        {
            int img_width,img_height,img_channel;
            static int number=0;
            char img_name[1024];
            lcd_clear(BLACK);
            sprintf(img_name,"0:img%d.jpg",number);
            uint8_t *stbi_img=stbi_load(img_name,&img_width,&img_height,&img_channel,3);
            printf("[INFO]:img_read width%d height%d channel%d\n",img_width,img_height,img_channel);
            int re_width=320,re_height=240;
            stbi_uc *resize_img=malloc(re_width*re_height*3);
            stbir_resize_uint8(stbi_img,img_width,img_height,0,resize_img,re_width,re_height,0,img_channel);
            uint8_t *stbi_img2=stbi_888_to_565(resize_img,re_width,re_height);
            lcd_draw_picture(0,0,re_width,re_height,(uint32_t *)stbi_img2);
            msleep(5000);
            stbi_image_free(stbi_img);
            free(resize_img);
            stbi_image_free(stbi_img2);
            number++;
            if(number==10) number=0; 
          
        }
    }
    while (1)
        ;

    return 0;
}
