/* drivers/video/msm/src/panel/mddi/mddi_novatek_hvga.c
 *
 * Copyright (C) 2008 QUALCOMM Incorporated.
 * Copyright (c) 2008 QUALCOMM USA, INC.
 * 
 * All source code in this file is licensed under the following license
 * except where indicated.
 * 
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, you can find it at http://www.fsf.org
 */


#include "msm_fb.h"
#include "mddihost.h"
#include "mddihosti.h"
#include <asm/gpio.h>
#include <mach/vreg.h>

#include <mach/board_lge.h>


#define PANEL_DEBUG 0

/* set gamma */
//#define GAMMA_CURRENT
//#define GAMMA_1_DOT_8
//#define GAMMA_1_DOT_9
//#define GAMMA_2_DOT_0
//#define GAMMA_2_DOT_1
#define GAMMA_2_DOT_2



#define LCD_CONTROL_BLOCK_BASE	0x110000
#define INTFLG		LCD_CONTROL_BLOCK_BASE|(0x18)
#define INTMSK		LCD_CONTROL_BLOCK_BASE|(0x1c)
#define VPOS		LCD_CONTROL_BLOCK_BASE|(0xc0)

static boolean is_lcd_on = -1;

/* The comment from AMSS codes:
 * Dot clock (10MHz) / pixels per row (320) = rows_per_second
 * Rows Per second, this number arrived upon empirically 
 * after observing the timing of Vsync pulses
 * XXX: TODO: change this values for INNOTEK PANEL */
static uint32 mddi_novatek_rows_per_second = 31250;
static uint32 mddi_novatek_rows_per_refresh = 480;
extern boolean mddi_vsync_detect_enabled;

static msm_fb_vsync_handler_type mddi_novatek_vsync_handler = NULL;
static void *mddi_novatek_vsync_handler_arg;
static uint16 mddi_novatek_vsync_attempts;

#if defined(CONFIG_FB_MSM_MDDI_NOVATEK_HITACHI_HVGA)
extern int g_mddi_lcd_probe;
#endif

static struct msm_panel_novatek_pdata *mddi_novatek_pdata;

static int mddi_novatek_lcd_on(struct platform_device *pdev);
static int mddi_novatek_lcd_off(struct platform_device *pdev);

static int mddi_novatek_lcd_init(void);
static void mddi_novatek_lcd_panel_poweron(void);
static void mddi_novatek_lcd_panel_poweroff(void);

#define DEBUG 1
#if DEBUG
#define EPRINTK(fmt, args...) printk(fmt, ##args)
#else
#define EPRINTK(fmt, args...) do { } while (0)
#endif

struct display_table {
    unsigned reg;
    unsigned char count;
    unsigned val_list[256];
};

#define REGFLAG_DELAY             0XFFFE
#define REGFLAG_END_OF_TABLE      0xFFFF   // END OF REGISTERS MARKER

static struct display_table mddi_novatek_position_table[] = {
	// set horizontal address 
	{0x2a00, 1, {0x0000}}, // XSA
	{0x2a01, 1, {0x0000}}, // XSA
	{0x2a02, 1, {0x0000}}, // XEA
	{0x2a03, 1, {0x013f}}, // XEA, 320-1
	// set vertical address 
	{0x2b00, 1, {0x0000}}, // YSA
	{0x2b01, 1, {0x0000}}, // YSA
	{0x2b02, 1, {0x0000}}, // YEA
	{0x2b03, 1, {0x01df}}, // YEA, 480-1
	{REGFLAG_END_OF_TABLE, 0x00, {}}
};

#if 0
static struct display_table mddi_novatek_display_on[] = {
	// Display on sequence
	//{0x1100, 1, {0x0000}}, // sleep out
	//{REGFLAG_DELAY, 150, {}},
	{0x2c00, 1, {0x0000}},
	{0x3800, 1, {0x0000}}, // Set Idle Mode Off
	{0x2900, 1, {0x0000}}, // Display On
	{0x2c00, 1, {0x0000}},
	{REGFLAG_END_OF_TABLE, 0x00, {}}
};
#endif

#if 0
static struct display_table2 mddi_novatek_img[] = {
	{0x2c, 16384, {}},
};
static struct display_table mddi_novatek_img_end[] = {
	{0x00, 0, {}},
	{REGFLAG_END_OF_TABLE, 0x00, {}}
};
#endif
#if 0
static struct display_table mddi_novatek_display_off[] = {
	// Display off sequence
	{0x3900, 1, {0x0000}}, // Set Idle mode On
	{0x2800, 1, {0x0000}}, // Display Off
	{REGFLAG_DELAY, 50, {}},
	{0x1000, 1, {0x0000}},
	{REGFLAG_DELAY, 100, {}},
	{REGFLAG_END_OF_TABLE, 0x00, {}}
};
#endif
static struct display_table mddi_novatek_sleep_mode_on_data[] = {
	// Display off sequence
	{0x3900, 1, {0x0000}}, // Set Idle mode On
	{0x2800, 1, {0x0000}},
	{REGFLAG_DELAY, 50, {}},
	{0x1000, 4, {0x0000}},
	{REGFLAG_DELAY, 100, {}},
	{REGFLAG_END_OF_TABLE, 0x00, {}}
};

static struct display_table mddi_novatek_initialize[] = {
	// Display on sequence
	{0x3900, 1, {0x0000}}, // Set Idle Mode on
	{0x1100, 1, {0x0000}}, // sleep out
	{REGFLAG_DELAY, 130, {}}, // change 100->130 for stability
	{0xF300, 1, {0x00AA}}, // unlock cmd2
	{0xF280, 1, {0x0002}}, // check cmd status
	{0x0280, 1, {0x0011}}, // power ctrl
	{0x0380, 1, {0x0000}}, // power ctrl

  /* Frame Frequency */
  /* = 5M(+-5%)/(RTN*(Lines+BP+FP) Hz */
  #if 1
  /* Frame Frq = 67.2 Hz */
	{0x0480, 1, {0x0056}}, // set LTPS timing : 150 clks
	{0x0580, 1, {0x0056}}, // set LTPS timing : 150 clks
	{0x0680, 1, {0x0056}}, // set LTPS timing : 150 clks
	#else
	/* Frame Frq = 65.0 Hz */
	{0x0480, 1, {0x005b}}, // set LTPS timing : 155 clks
	{0x0580, 1, {0x005b}}, // set LTPS timing : 155 clks
	{0x0680, 1, {0x005b}}, // set LTPS timing : 155 clks
  #endif
	
	{0x0780, 1, {0x0000}}, // power ctrl
	{0x0880, 1, {0x0033}}, // power ctrl
	{0x0980, 1, {0x0043}}, // power ctrl
	{0x0A80, 1, {0x0030}}, // power ctrl
	{0x0B80, 1, {0x0044}}, // power ctrl
	{0x0C80, 1, {0x0054}}, // power ctrl
	{0x0D80, 1, {0x0030}}, // power ctrl
	{0x0E80, 1, {0x0033}}, // power ctrl
	{0x0F80, 1, {0x0043}}, // power ctrl
	{0x1080, 1, {0x0030}}, // power ctrl
	{0x1180, 1, {0x0000}}, // power ctrl
	{0x1280, 1, {0x000C}}, // VDDGR
	{0x1380, 1, {0x0004}}, // VG Control
	{0x1480, 1, {0x0058}}, // Set GVDD=5.0V
	{0x1680, 1, {0x0070}}, // Set VCOMDC1=2.1V
	{0x1780, 1, {0x00CC}}, // VCOM Control
	{0x1880, 1, {0x0080}}, // VCOM Control
	{0x1980, 1, {0x0000}}, // VCOM Mode=0x00 for DC VCOM Mode 1
	{0x1A80, 1, {0x0078}}, // VCOM Control
	{0x1B80, 1, {0x0050}}, // Set VCOMMH=3.5V
	{0x1C80, 1, {0x0080}}, // VCOM Control
	{0x9480, 1, {0x0017}}, // Set LTPS timing : 23 clks
	{0x9580, 1, {0x0017}}, // Set LTPS timing : 33 clks
	{0x9680, 1, {0x0005}}, // Set LTPS timing : 5 clks
	{0x9780, 1, {0x000C}}, // Set LTPS timing : 12 clks
	{0x9880, 1, {0x0072}}, // Set LTPS timing : 114 clks
	{0x9980, 1, {0x0012}}, // Set LTPS timing : 18 clks
	{0x9A80, 1, {0x0084}}, // Set LTPS timing : 132 clks
	{0x9B80, 1, {0x0001}}, // Set LTPS timing : 1 clks
	{0x9C80, 1, {0x0005}}, // Set LTPS timing : 5 clks
	{0x9D80, 1, {0x0016}}, // Set LTPS timing : 22 clks
	{0x9E80, 1, {0x0000}}, // Set LTPS timing
	{0x9F80, 1, {0x0000}}, // Set LTPS timing
	{0xA380, 1, {0x00F8}}, // Set LTPS timing
	{0xA480, 1, {0x003F}}, // Set LTPS timing
	{0xA680, 1, {0x0008}}, // Set LTPS timing
	//{0x3600, 1, {0x0008}}, // Set RGB	
	#if defined(GAMMA_CURRENT)
	{0x2880, 1, {0x0009}}, // Set Gamma R
	{0x2980, 1, {0x001E}}, // Set Gamma R
	{0x2A80, 1, {0x0045}}, // Set Gamma R
	{0x2B80, 1, {0x0063}}, // Set Gamma R
	{0x2C80, 1, {0x000D}}, // Set Gamma R
	{0x2D80, 1, {0x002E}}, // Set Gamma R
	{0x2E80, 1, {0x0061}}, // Set Gamma R
	{0x2F80, 1, {0x0063}}, // Set Gamma R
	{0x3080, 1, {0x0020}}, // Set Gamma R
	{0x3180, 1, {0x0026}}, // Set Gamma R
	{0x3280, 1, {0x00A5}}, // Set Gamma R
	{0x3380, 1, {0x001E}}, // Set Gamma R
	{0x3480, 1, {0x0048}}, // Set Gamma R
	{0x3580, 1, {0x0067}}, // Set Gamma R
	{0x3680, 1, {0x0078}}, // Set Gamma R
	{0x3780, 1, {0x0088}}, // Set Gamma R
	{0x3880, 1, {0x0025}}, // Set Gamma R
	{0x3980, 1, {0x0053}}, // Set Gamma R
	{0x3A80, 1, {0x0009}}, // Set Gamma R
	{0x3B80, 1, {0x0037}}, // Set Gamma R
	{0x3C80, 1, {0x0056}}, // Set Gamma R
	{0x3D80, 1, {0x0068}}, // Set Gamma R
	{0x3E80, 1, {0x0018}}, // Set Gamma R
	{0x3F80, 1, {0x0037}}, // Set Gamma R
	{0x4080, 1, {0x0061}}, // Set Gamma R
	{0x4180, 1, {0x0040}}, // Set Gamma R
	{0x4280, 1, {0x0019}}, // Set Gamma R
	{0x4380, 1, {0x001F}}, // Set Gamma R
	{0x4480, 1, {0x0085}}, // Set Gamma R
	{0x4580, 1, {0x001E}}, // Set Gamma R
	{0x4680, 1, {0x0051}}, // Set Gamma R
	{0x4780, 1, {0x0072}}, // Set Gamma R
	{0x4880, 1, {0x0087}}, // Set Gamma R
	{0x4980, 1, {0x00A6}}, // Set Gamma R
	{0x4A80, 1, {0x004D}}, // Set Gamma R
	{0x4B80, 1, {0x0062}}, // Set Gamma R
	{0x4C80, 1, {0x003D}}, // Set Gamma G
	{0x4D80, 1, {0x0050}}, // Set Gamma G
	{0x4E80, 1, {0x006E}}, // Set Gamma G
	{0x4F80, 1, {0x007E}}, // Set Gamma G
	{0x5080, 1, {0x0009}}, // Set Gamma G
	{0x5180, 1, {0x0028}}, // Set Gamma G
	{0x5280, 1, {0x005C}}, // Set Gamma G
	{0x5380, 1, {0x006A}},
	{0x5480, 1, {0x001F}},
	{0x5580, 1, {0x0026}},
	{0x5680, 1, {0x00AA}},
	{0x5780, 1, {0x001D}},
	{0x5880, 1, {0x0048}},
	{0x5980, 1, {0x0065}},
	{0x5A80, 1, {0x007A}},
	{0x5B80, 1, {0x008A}},
	{0x5C80, 1, {0x0026}},
	{0x5D80, 1, {0x0053}},
	{0x5E80, 1, {0x0009}},
	{0x5F80, 1, {0x0036}},
	{0x6080, 1, {0x0053}},
	{0x6180, 1, {0x0066}},
	{0x6280, 1, {0x001A}},
	{0x6380, 1, {0x0037}},
	{0x6480, 1, {0x0062}},
	{0x6580, 1, {0x003B}},
	{0x6680, 1, {0x0019}},
	{0x6780, 1, {0x0020}},
	{0x6880, 1, {0x007E}},
	{0x6980, 1, {0x0023}},
	{0x6A80, 1, {0x0057}},
	{0x6B80, 1, {0x0076}},
	{0x6C80, 1, {0x006C}},
	{0x6D80, 1, {0x007C}},
	{0x6E80, 1, {0x001A}},
	{0x6F80, 1, {0x002D}},
	{0x7080, 1, {0x0009}}, // Set Gamma B
	{0x7180, 1, {0x0023}},
	{0x7280, 1, {0x004F}},
	{0x7380, 1, {0x0069}},
	{0x7480, 1, {0x0015}},
	{0x7580, 1, {0x003E}},
	{0x7680, 1, {0x0069}},
	{0x7780, 1, {0x0074}},
	{0x7880, 1, {0x0020}},
	{0x7980, 1, {0x0026}},
	{0x7A80, 1, {0x00AD}},
	{0x7B80, 1, {0x001E}},
	{0x7C80, 1, {0x004E}},
	{0x7D80, 1, {0x0067}},
	{0x7E80, 1, {0x0079}},
	{0x7F80, 1, {0x0086}},
	{0x8080, 1, {0x0028}},
	{0x8180, 1, {0x0053}},
	{0x8280, 1, {0x0009}},
	{0x8380, 1, {0x0034}},
	{0x8480, 1, {0x0058}},
	{0x8580, 1, {0x0067}},
	{0x8680, 1, {0x0018}},
	{0x8780, 1, {0x0031}},
	{0x8880, 1, {0x0061}},
	{0x8980, 1, {0x0038}},
	{0x8A80, 1, {0x0019}},
	{0x8B80, 1, {0x001F}},
	{0x8C80, 1, {0x0074}},
	{0x8D80, 1, {0x0016}},
	{0x8E80, 1, {0x0041}},
	{0x8F80, 1, {0x006A}},
	{0x9080, 1, {0x0081}},
	{0x9180, 1, {0x009B}},
	{0x9280, 1, {0x0048}},
	{0x9380, 1, {0x0062}},
#elif defined(GAMMA_1_DOT_8)
	{0x2880, 1, {0x0009}}, // Set Gamma R
	{0x2980, 1, {0x001D}}, // Set Gamma R
	{0x2A80, 1, {0x0041}}, // Set Gamma R
	{0x2B80, 1, {0x005A}}, // Set Gamma R
	{0x2C80, 1, {0x000D}}, // Set Gamma R
	{0x2D80, 1, {0x002F}}, // Set Gamma R
	{0x2E80, 1, {0x0060}}, // Set Gamma R
	{0x2F80, 1, {0x0056}}, // Set Gamma R
	{0x3080, 1, {0x001F}}, // Set Gamma R
	{0x3180, 1, {0x0025}}, // Set Gamma R
	{0x3280, 1, {0x009E}}, // Set Gamma R
	{0x3380, 1, {0x001E}}, // Set Gamma R
	{0x3480, 1, {0x0048}}, // Set Gamma R
	{0x3580, 1, {0x0068}}, // Set Gamma R
	{0x3680, 1, {0x0076}}, // Set Gamma R
	{0x3780, 1, {0x0089}}, // Set Gamma R
	{0x3880, 1, {0x0026}}, // Set Gamma R
	{0x3980, 1, {0x0053}}, // Set Gamma R
	{0x3A80, 1, {0x000A}}, // Set Gamma R
	{0x3B80, 1, {0x003C}}, // Set Gamma R
	{0x3C80, 1, {0x0055}}, // Set Gamma R
	{0x3D80, 1, {0x006A}}, // Set Gamma R
	{0x3E80, 1, {0x0017}}, // Set Gamma R
	{0x3F80, 1, {0x0036}}, // Set Gamma R
	{0x4080, 1, {0x0060}}, // Set Gamma R
	{0x4180, 1, {0x0049}}, // Set Gamma R
	{0x4280, 1, {0x0017}}, // Set Gamma R
	{0x4380, 1, {0x0021}}, // Set Gamma R
	{0x4480, 1, {0x0092}}, // Set Gamma R
	{0x4580, 1, {0x001E}}, // Set Gamma R
	{0x4680, 1, {0x0051}}, // Set Gamma R
	{0x4780, 1, {0x0071}}, // Set Gamma R
	{0x4880, 1, {0x008E}}, // Set Gamma R
	{0x4980, 1, {0x00A1}}, // Set Gamma R
	{0x4A80, 1, {0x004B}}, // Set Gamma R
	{0x4B80, 1, {0x0063}}, // Set Gamma R
	{0x4C80, 1, {0x003D}}, // Set Gamma G
	{0x4D80, 1, {0x004D}}, // Set Gamma G
	{0x4E80, 1, {0x006F}}, // Set Gamma G
	{0x4F80, 1, {0x0074}}, // Set Gamma G
	{0x5080, 1, {0x0009}}, // Set Gamma G
	{0x5180, 1, {0x0029}}, // Set Gamma G
	{0x5280, 1, {0x005B}}, // Set Gamma G
	{0x5380, 1, {0x005D}},
	{0x5480, 1, {0x001E}},
	{0x5580, 1, {0x0025}},
	{0x5680, 1, {0x009E}},
	{0x5780, 1, {0x001D}},
	{0x5880, 1, {0x0048}},
	{0x5980, 1, {0x0066}},
	{0x5A80, 1, {0x0073}},
	{0x5B80, 1, {0x008B}},
	{0x5C80, 1, {0x0027}},
	{0x5D80, 1, {0x0053}},
	{0x5E80, 1, {0x0009}},
	{0x5F80, 1, {0x0033}},
	{0x6080, 1, {0x0053}},
	{0x6180, 1, {0x006C}},
	{0x6280, 1, {0x0016}},
	{0x6380, 1, {0x003A}},
	{0x6480, 1, {0x0061}},
	{0x6580, 1, {0x0048}},
	{0x6680, 1, {0x001A}},
	{0x6780, 1, {0x0022}},
	{0x6880, 1, {0x008A}},
	{0x6980, 1, {0x0024}},
	{0x6A80, 1, {0x0055}},
	{0x6B80, 1, {0x0072}},
	{0x6C80, 1, {0x0078}},
	{0x6D80, 1, {0x007B}},
	{0x6E80, 1, {0x001C}},
	{0x6F80, 1, {0x002F}},
	{0x7080, 1, {0x0009}}, // Set Gamma B
	{0x7180, 1, {0x0020}},
	{0x7280, 1, {0x0050}},
	{0x7380, 1, {0x0067}},
	{0x7480, 1, {0x0015}},
	{0x7580, 1, {0x003F}},
	{0x7680, 1, {0x0068}},
	{0x7780, 1, {0x006C}},
	{0x7880, 1, {0x001F}},
	{0x7980, 1, {0x0025}},
	{0x7A80, 1, {0x009F}},
	{0x7B80, 1, {0x001E}},
	{0x7C80, 1, {0x004E}},
	{0x7D80, 1, {0x0068}},
	{0x7E80, 1, {0x006D}},
	{0x7F80, 1, {0x0084}},
	{0x8080, 1, {0x0027}},
	{0x8180, 1, {0x0053}},
	{0x8280, 1, {0x0009}},
	{0x8380, 1, {0x0034}},
	{0x8480, 1, {0x0058}},
	{0x8580, 1, {0x0073}},
	{0x8680, 1, {0x0016}},
	{0x8780, 1, {0x0033}},
	{0x8880, 1, {0x0062}},
	{0x8980, 1, {0x0046}},
	{0x8A80, 1, {0x001B}},
	{0x8B80, 1, {0x0021}},
	{0x8C80, 1, {0x007C}},
	{0x8D80, 1, {0x0017}},
	{0x8E80, 1, {0x003E}},
	{0x8F80, 1, {0x0061}},
	{0x9080, 1, {0x0087}},
	{0x9180, 1, {0x009A}},
	{0x9280, 1, {0x0048}},
	{0x9380, 1, {0x0062}},
#elif defined(GAMMA_1_DOT_9)
	{0x2880, 1, {0x0009}}, // Set Gamma R
	{0x2980, 1, {0x001E}}, // Set Gamma R
	{0x2A80, 1, {0x0045}}, // Set Gamma R
	{0x2B80, 1, {0x005D}}, // Set Gamma R
	{0x2C80, 1, {0x000C}}, // Set Gamma R
	{0x2D80, 1, {0x002E}}, // Set Gamma R
	{0x2E80, 1, {0x005F}}, // Set Gamma R
	{0x2F80, 1, {0x005A}}, // Set Gamma R
	{0x3080, 1, {0x001D}}, // Set Gamma R
	{0x3180, 1, {0x0026}}, // Set Gamma R
	{0x3280, 1, {0x00A1}}, // Set Gamma R
	{0x3380, 1, {0x001E}}, // Set Gamma R
	{0x3480, 1, {0x0048}}, // Set Gamma R
	{0x3580, 1, {0x0067}}, // Set Gamma R
	{0x3680, 1, {0x0077}}, // Set Gamma R
	{0x3780, 1, {0x0088}}, // Set Gamma R
	{0x3880, 1, {0x0025}}, // Set Gamma R
	{0x3980, 1, {0x0053}}, // Set Gamma R
	{0x3A80, 1, {0x0009}}, // Set Gamma R
	{0x3B80, 1, {0x0037}}, // Set Gamma R
	{0x3C80, 1, {0x0056}}, // Set Gamma R
	{0x3D80, 1, {0x006A}}, // Set Gamma R
	{0x3E80, 1, {0x0017}}, // Set Gamma R
	{0x3F80, 1, {0x0037}}, // Set Gamma R
	{0x4080, 1, {0x0061}}, // Set Gamma R
	{0x4180, 1, {0x0044}}, // Set Gamma R
	{0x4280, 1, {0x001A}}, // Set Gamma R
	{0x4380, 1, {0x0023}}, // Set Gamma R
	{0x4480, 1, {0x008D}}, // Set Gamma R
	{0x4580, 1, {0x0022}}, // Set Gamma R
	{0x4680, 1, {0x0051}}, // Set Gamma R
	{0x4780, 1, {0x0071}}, // Set Gamma R
	{0x4880, 1, {0x0090}}, // Set Gamma R
	{0x4980, 1, {0x00A6}}, // Set Gamma R
	{0x4A80, 1, {0x004D}}, // Set Gamma R
	{0x4B80, 1, {0x0062}}, // Set Gamma R
	{0x4C80, 1, {0x003D}}, // Set Gamma G
	{0x4D80, 1, {0x0050}}, // Set Gamma G
	{0x4E80, 1, {0x006E}}, // Set Gamma G
	{0x4F80, 1, {0x0078}}, // Set Gamma G
	{0x5080, 1, {0x0008}}, // Set Gamma G
	{0x5180, 1, {0x0028}}, // Set Gamma G
	{0x5280, 1, {0x005A}}, // Set Gamma G
	{0x5380, 1, {0x0061}},
	{0x5480, 1, {0x001C}},
	{0x5580, 1, {0x0026}},
	{0x5680, 1, {0x00A1}},
	{0x5780, 1, {0x001D}},
	{0x5880, 1, {0x0048}},
	{0x5980, 1, {0x0065}},
	{0x5A80, 1, {0x0074}},
	{0x5B80, 1, {0x008A}},
	{0x5C80, 1, {0x0026}},
	{0x5D80, 1, {0x0053}},
	{0x5E80, 1, {0x0009}},
	{0x5F80, 1, {0x0036}},
	{0x6080, 1, {0x0053}},
	{0x6180, 1, {0x006B}},
	{0x6280, 1, {0x001A}},
	{0x6380, 1, {0x0037}},
	{0x6480, 1, {0x0061}},
	{0x6580, 1, {0x0045}},
	{0x6680, 1, {0x0018}},
	{0x6780, 1, {0x0022}},
	{0x6880, 1, {0x0087}},
	{0x6980, 1, {0x0020}},
	{0x6A80, 1, {0x004A}},
	{0x6B80, 1, {0x0067}},
	{0x6C80, 1, {0x0087}},
	{0x6D80, 1, {0x0085}},
	{0x6E80, 1, {0x001D}},
	{0x6F80, 1, {0x002D}},
	{0x7080, 1, {0x0009}}, // Set Gamma B
	{0x7180, 1, {0x0023}},
	{0x7280, 1, {0x004F}},
	{0x7380, 1, {0x006B}},
	{0x7480, 1, {0x0014}},
	{0x7580, 1, {0x003E}},
	{0x7680, 1, {0x0067}},
	{0x7780, 1, {0x0070}},
	{0x7880, 1, {0x001D}},
	{0x7980, 1, {0x0026}},
	{0x7A80, 1, {0x00A2}},
	{0x7B80, 1, {0x001E}},
	{0x7C80, 1, {0x004E}},
	{0x7D80, 1, {0x0067}},
	{0x7E80, 1, {0x006E}},
	{0x7F80, 1, {0x0083}},
	{0x8080, 1, {0x0026}},
	{0x8180, 1, {0x0053}},
	{0x8280, 1, {0x0009}},
	{0x8380, 1, {0x0036}},
	{0x8480, 1, {0x005A}},
	{0x8580, 1, {0x0073}},
	{0x8680, 1, {0x0018}},
	{0x8780, 1, {0x0031}},
	{0x8880, 1, {0x0061}},
	{0x8980, 1, {0x0044}},
	{0x8A80, 1, {0x0019}},
	{0x8B80, 1, {0x0023}},
	{0x8C80, 1, {0x0077}},
	{0x8D80, 1, {0x0018}},
	{0x8E80, 1, {0x003D}},
	{0x8F80, 1, {0x0064}},
	{0x9080, 1, {0x008A}},
	{0x9180, 1, {0x009F}},
	{0x9280, 1, {0x0048}},
	{0x9380, 1, {0x0062}},
#elif defined(GAMMA_2_DOT_0)
	{0x2880, 1, {0x0009}}, // Set Gamma R
	{0x2980, 1, {0x001E}}, // Set Gamma R
	{0x2A80, 1, {0x0045}}, // Set Gamma R
	{0x2B80, 1, {0x005D}}, // Set Gamma R
	{0x2C80, 1, {0x000D}}, // Set Gamma R
	{0x2D80, 1, {0x002E}}, // Set Gamma R
	{0x2E80, 1, {0x005F}}, // Set Gamma R
	{0x2F80, 1, {0x005A}}, // Set Gamma R
	{0x3080, 1, {0x001C}}, // Set Gamma R
	{0x3180, 1, {0x0026}}, // Set Gamma R
	{0x3280, 1, {0x00A1}}, // Set Gamma R
	{0x3380, 1, {0x001E}}, // Set Gamma R
	{0x3480, 1, {0x0048}}, // Set Gamma R
	{0x3580, 1, {0x0067}}, // Set Gamma R
	{0x3680, 1, {0x0077}}, // Set Gamma R
	{0x3780, 1, {0x0088}}, // Set Gamma R
	{0x3880, 1, {0x0025}}, // Set Gamma R
	{0x3980, 1, {0x0053}}, // Set Gamma R
	{0x3A80, 1, {0x0009}}, // Set Gamma R
	{0x3B80, 1, {0x0037}}, // Set Gamma R
	{0x3C80, 1, {0x0056}}, // Set Gamma R
	{0x3D80, 1, {0x006A}}, // Set Gamma R
	{0x3E80, 1, {0x0018}}, // Set Gamma R
	{0x3F80, 1, {0x0037}}, // Set Gamma R
	{0x4080, 1, {0x0061}}, // Set Gamma R
	{0x4180, 1, {0x0044}}, // Set Gamma R
	{0x4280, 1, {0x001A}}, // Set Gamma R
	{0x4380, 1, {0x0024}}, // Set Gamma R
	{0x4480, 1, {0x008D}}, // Set Gamma R
	{0x4580, 1, {0x0022}}, // Set Gamma R
	{0x4680, 1, {0x0051}}, // Set Gamma R
	{0x4780, 1, {0x0072}}, // Set Gamma R
	{0x4880, 1, {0x0090}}, // Set Gamma R
	{0x4980, 1, {0x00A6}}, // Set Gamma R
	{0x4A80, 1, {0x004D}}, // Set Gamma R
	{0x4B80, 1, {0x0062}}, // Set Gamma R
	{0x4C80, 1, {0x003D}}, // Set Gamma G
	{0x4D80, 1, {0x0050}}, // Set Gamma G
	{0x4E80, 1, {0x006E}}, // Set Gamma G
	{0x4F80, 1, {0x0078}}, // Set Gamma G
	{0x5080, 1, {0x0009}}, // Set Gamma G
	{0x5180, 1, {0x0028}}, // Set Gamma G
	{0x5280, 1, {0x005A}}, // Set Gamma G
	{0x5380, 1, {0x0061}},
	{0x5480, 1, {0x001B}},
	{0x5580, 1, {0x0026}},
	{0x5680, 1, {0x00A1}},
	{0x5780, 1, {0x001D}},
	{0x5880, 1, {0x0048}},
	{0x5980, 1, {0x0065}},
	{0x5A80, 1, {0x0074}},
	{0x5B80, 1, {0x008A}},
	{0x5C80, 1, {0x0026}},
	{0x5D80, 1, {0x0053}},
	{0x5E80, 1, {0x0009}},
	{0x5F80, 1, {0x0036}},
	{0x6080, 1, {0x0053}},
	{0x6180, 1, {0x006B}},
	{0x6280, 1, {0x001A}},
	{0x6380, 1, {0x0037}},
	{0x6480, 1, {0x0062}},
	{0x6580, 1, {0x0045}},
	{0x6680, 1, {0x0019}},
	{0x6780, 1, {0x0023}},
	{0x6880, 1, {0x0087}},
	{0x6980, 1, {0x0021}},
	{0x6A80, 1, {0x004B}},
	{0x6B80, 1, {0x0067}},
	{0x6C80, 1, {0x0087}},
	{0x6D80, 1, {0x0085}},
	{0x6E80, 1, {0x001D}},
	{0x6F80, 1, {0x002D}},
	{0x7080, 1, {0x0009}}, // Set Gamma B
	{0x7180, 1, {0x0023}},
	{0x7280, 1, {0x004F}},
	{0x7380, 1, {0x006B}},
	{0x7480, 1, {0x0015}},
	{0x7580, 1, {0x003E}},
	{0x7680, 1, {0x0067}},
	{0x7780, 1, {0x0070}},
	{0x7880, 1, {0x001C}},
	{0x7980, 1, {0x0026}},
	{0x7A80, 1, {0x00A2}},
	{0x7B80, 1, {0x001E}},
	{0x7C80, 1, {0x004E}},
	{0x7D80, 1, {0x0067}},
	{0x7E80, 1, {0x006E}},
	{0x7F80, 1, {0x0083}},
	{0x8080, 1, {0x0026}},
	{0x8180, 1, {0x0053}},
	{0x8280, 1, {0x0009}},
	{0x8380, 1, {0x0036}},
	{0x8480, 1, {0x005A}},
	{0x8580, 1, {0x0073}},
	{0x8680, 1, {0x0018}},
	{0x8780, 1, {0x0031}},
	{0x8880, 1, {0x0061}},
	{0x8980, 1, {0x0044}},
	{0x8A80, 1, {0x0019}},
	{0x8B80, 1, {0x0023}},
	{0x8C80, 1, {0x0077}},
	{0x8D80, 1, {0x0018}},
	{0x8E80, 1, {0x003D}},
	{0x8F80, 1, {0x0065}},
	{0x9080, 1, {0x008A}},
	{0x9180, 1, {0x009F}},
	{0x9280, 1, {0x0048}},
	{0x9380, 1, {0x0062}},
#elif defined(GAMMA_2_DOT_1)
	{0x2880, 1, {0x0009}}, // Set Gamma R
	{0x2980, 1, {0x001E}}, // Set Gamma R
	{0x2A80, 1, {0x0045}}, // Set Gamma R
	{0x2B80, 1, {0x0063}}, // Set Gamma R
	{0x2C80, 1, {0x000D}}, // Set Gamma R
	{0x2D80, 1, {0x002E}}, // Set Gamma R
	{0x2E80, 1, {0x0060}}, // Set Gamma R
	{0x2F80, 1, {0x0061}}, // Set Gamma R
	{0x3080, 1, {0x001D}}, // Set Gamma R
	{0x3180, 1, {0x0026}}, // Set Gamma R
	{0x3280, 1, {0x00A4}}, // Set Gamma R
	{0x3380, 1, {0x001D}}, // Set Gamma R
	{0x3480, 1, {0x0048}}, // Set Gamma R
	{0x3580, 1, {0x0067}}, // Set Gamma R
	{0x3680, 1, {0x0078}}, // Set Gamma R
	{0x3780, 1, {0x0088}}, // Set Gamma R
	{0x3880, 1, {0x0025}}, // Set Gamma R
	{0x3980, 1, {0x0053}}, // Set Gamma R
	{0x3A80, 1, {0x0009}}, // Set Gamma R
	{0x3B80, 1, {0x0037}}, // Set Gamma R
	{0x3C80, 1, {0x0056}}, // Set Gamma R
	{0x3D80, 1, {0x0068}}, // Set Gamma R
	{0x3E80, 1, {0x0018}}, // Set Gamma R
	{0x3F80, 1, {0x0037}}, // Set Gamma R
	{0x4080, 1, {0x0063}}, // Set Gamma R
	{0x4180, 1, {0x0041}}, // Set Gamma R
	{0x4280, 1, {0x0019}}, // Set Gamma R
	{0x4380, 1, {0x0021}}, // Set Gamma R
	{0x4480, 1, {0x0087}}, // Set Gamma R
	{0x4580, 1, {0x0021}}, // Set Gamma R
	{0x4680, 1, {0x0051}}, // Set Gamma R
	{0x4780, 1, {0x0072}}, // Set Gamma R
	{0x4880, 1, {0x0087}}, // Set Gamma R
	{0x4980, 1, {0x00A6}}, // Set Gamma R
	{0x4A80, 1, {0x004D}}, // Set Gamma R
	{0x4B80, 1, {0x0062}}, // Set Gamma R
	{0x4C80, 1, {0x003D}}, // Set Gamma G
	{0x4D80, 1, {0x0050}}, // Set Gamma G
	{0x4E80, 1, {0x006E}}, // Set Gamma G
	{0x4F80, 1, {0x0079}}, // Set Gamma G
	{0x5080, 1, {0x0009}}, // Set Gamma G
	{0x5180, 1, {0x0028}}, // Set Gamma G
	{0x5280, 1, {0x005B}}, // Set Gamma G
	{0x5380, 1, {0x0063}},
	{0x5480, 1, {0x001C}},
	{0x5580, 1, {0x0026}},
	{0x5680, 1, {0x00A4}},
	{0x5780, 1, {0x001C}},
	{0x5880, 1, {0x0048}},
	{0x5980, 1, {0x0065}},
	{0x5A80, 1, {0x0075}},
	{0x5B80, 1, {0x008A}},
	{0x5C80, 1, {0x0026}},
	{0x5D80, 1, {0x0053}},
	{0x5E80, 1, {0x0009}},
	{0x5F80, 1, {0x0036}},
	{0x6080, 1, {0x0053}},
	{0x6180, 1, {0x006A}},
	{0x6280, 1, {0x001A}},
	{0x6380, 1, {0x0037}},
	{0x6480, 1, {0x0066}},
	{0x6580, 1, {0x0040}},
	{0x6680, 1, {0x001B}},
	{0x6780, 1, {0x0022}},
	{0x6880, 1, {0x0085}},
	{0x6980, 1, {0x0025}},
	{0x6A80, 1, {0x0057}},
	{0x6B80, 1, {0x0076}},
	{0x6C80, 1, {0x006F}},
	{0x6D80, 1, {0x007C}},
	{0x6E80, 1, {0x001A}},
	{0x6F80, 1, {0x002D}},
	{0x7080, 1, {0x0009}}, // Set Gamma B
	{0x7180, 1, {0x0023}},
	{0x7280, 1, {0x004F}},
	{0x7380, 1, {0x0064}},
	{0x7480, 1, {0x0015}},
	{0x7580, 1, {0x003E}},
	{0x7680, 1, {0x0068}},
	{0x7780, 1, {0x006D}},
	{0x7880, 1, {0x001D}},
	{0x7980, 1, {0x0026}},
	{0x7A80, 1, {0x00A9}},
	{0x7B80, 1, {0x001D}},
	{0x7C80, 1, {0x004B}},
	{0x7D80, 1, {0x0067}},
	{0x7E80, 1, {0x0074}},
	{0x7F80, 1, {0x0086}},
	{0x8080, 1, {0x0028}},
	{0x8180, 1, {0x0053}},
	{0x8280, 1, {0x0009}},
	{0x8380, 1, {0x0034}},
	{0x8480, 1, {0x0058}},
	{0x8580, 1, {0x006B}},
	{0x8680, 1, {0x0018}},
	{0x8780, 1, {0x0035}},
	{0x8880, 1, {0x0063}},
	{0x8980, 1, {0x003C}},
	{0x8A80, 1, {0x0019}},
	{0x8B80, 1, {0x0023}},
	{0x8C80, 1, {0x007A}},
	{0x8D80, 1, {0x0019}},
	{0x8E80, 1, {0x0041}},
	{0x8F80, 1, {0x006A}},
	{0x9080, 1, {0x0085}},
	{0x9180, 1, {0x009B}},
	{0x9280, 1, {0x0048}},
	{0x9380, 1, {0x0062}},
#elif defined(GAMMA_2_DOT_2)
	{0x2880, 1, {0x0009}}, // Set Gamma R
	{0x2980, 1, {0x001E}}, // Set Gamma R
	{0x2A80, 1, {0x0045}}, // Set Gamma R
	{0x2B80, 1, {0x005E}}, // Set Gamma R
	{0x2C80, 1, {0x000D}}, // Set Gamma R
	{0x2D80, 1, {0x002E}}, // Set Gamma R
	{0x2E80, 1, {0x0061}}, // Set Gamma R
	{0x2F80, 1, {0x005F}}, // Set Gamma R
	{0x3080, 1, {0x0020}}, // Set Gamma R
	{0x3180, 1, {0x0026}}, // Set Gamma R
	{0x3280, 1, {0x00A2}}, // Set Gamma R
	{0x3380, 1, {0x0022}}, // Set Gamma R
	{0x3480, 1, {0x004A}}, // Set Gamma R
	{0x3580, 1, {0x0067}}, // Set Gamma R
	{0x3680, 1, {0x0073}}, // Set Gamma R
	{0x3780, 1, {0x0088}}, // Set Gamma R
	{0x3880, 1, {0x0025}}, // Set Gamma R
	{0x3980, 1, {0x0053}}, // Set Gamma R
	{0x3A80, 1, {0x0009}}, // Set Gamma R
	{0x3B80, 1, {0x0037}}, // Set Gamma R
	{0x3C80, 1, {0x0056}}, // Set Gamma R
	{0x3D80, 1, {0x006C}}, // Set Gamma R
	{0x3E80, 1, {0x0018}}, // Set Gamma R
	{0x3F80, 1, {0x0037}}, // Set Gamma R
	{0x4080, 1, {0x0060}}, // Set Gamma R
	{0x4180, 1, {0x0042}}, // Set Gamma R
	{0x4280, 1, {0x001B}}, // Set Gamma R
	{0x4380, 1, {0x0021}}, // Set Gamma R
	{0x4480, 1, {0x0089}}, // Set Gamma R
	{0x4580, 1, {0x001E}}, // Set Gamma R
	{0x4680, 1, {0x0051}}, // Set Gamma R
	{0x4780, 1, {0x0072}}, // Set Gamma R
	{0x4880, 1, {0x008D}}, // Set Gamma R
	{0x4980, 1, {0x00A6}}, // Set Gamma R
	{0x4A80, 1, {0x004D}}, // Set Gamma R
	{0x4B80, 1, {0x0062}}, // Set Gamma R
	{0x4C80, 1, {0x003D}}, // Set Gamma G
	{0x4D80, 1, {0x0050}}, // Set Gamma G
	{0x4E80, 1, {0x006E}}, // Set Gamma G
	{0x4F80, 1, {0x0079}}, // Set Gamma G
	{0x5080, 1, {0x0009}}, // Set Gamma G
	{0x5180, 1, {0x0028}}, // Set Gamma G
	{0x5280, 1, {0x005C}}, // Set Gamma G
	{0x5380, 1, {0x0066}},
	{0x5480, 1, {0x001F}},
	{0x5580, 1, {0x0026}},
	{0x5680, 1, {0x00A6}},
	{0x5780, 1, {0x0021}},
	{0x5880, 1, {0x004A}},
	{0x5980, 1, {0x0065}},
	{0x5A80, 1, {0x0075}},
	{0x5B80, 1, {0x008A}},
	{0x5C80, 1, {0x0026}},
	{0x5D80, 1, {0x0053}},
	{0x5E80, 1, {0x0009}},
	{0x5F80, 1, {0x0036}},
	{0x6080, 1, {0x0053}},
	{0x6180, 1, {0x006A}},
	{0x6280, 1, {0x001A}},
	{0x6380, 1, {0x0035}},
	{0x6480, 1, {0x0060}},
	{0x6580, 1, {0x003F}},
	{0x6680, 1, {0x001B}},
	{0x6780, 1, {0x0022}},
	{0x6880, 1, {0x0081}},
	{0x6980, 1, {0x0027}},
	{0x6A80, 1, {0x0057}},
	{0x6B80, 1, {0x0076}},
	{0x6C80, 1, {0x006F}},
	{0x6D80, 1, {0x007C}},
	{0x6E80, 1, {0x001A}},
	{0x6F80, 1, {0x002D}},
	{0x7080, 1, {0x0009}}, // Set Gamma B
	{0x7180, 1, {0x0023}},
	{0x7280, 1, {0x004F}},
	{0x7380, 1, {0x0069}},
	{0x7480, 1, {0x0015}},
	{0x7580, 1, {0x003E}},
	{0x7680, 1, {0x0069}},
	{0x7780, 1, {0x0075}},
	{0x7880, 1, {0x0020}},
	{0x7980, 1, {0x0026}},
	{0x7A80, 1, {0x00AE}},
	{0x7B80, 1, {0x0022}},
	{0x7C80, 1, {0x0050}},
	{0x7D80, 1, {0x0067}},
	{0x7E80, 1, {0x0079}},
	{0x7F80, 1, {0x0086}},
	{0x8080, 1, {0x0028}},
	{0x8180, 1, {0x0053}},
	{0x8280, 1, {0x0009}},
	{0x8380, 1, {0x0034}},
	{0x8480, 1, {0x0058}},
	{0x8580, 1, {0x0067}},
	{0x8680, 1, {0x0018}},
	{0x8780, 1, {0x002F}},
	{0x8880, 1, {0x005F}},
	{0x8980, 1, {0x0037}},
	{0x8A80, 1, {0x001A}},
	{0x8B80, 1, {0x0020}},
	{0x8C80, 1, {0x0073}},
	{0x8D80, 1, {0x0016}},
	{0x8E80, 1, {0x0041}},
	{0x8F80, 1, {0x006A}},
	{0x9080, 1, {0x0081}},
	{0x9180, 1, {0x009B}},
	{0x9280, 1, {0x0048}},
	{0x9380, 1, {0x0062}},
#endif
#if defined(USE_TENTATIVE_COMMAND)
	{0x2780, 1, {0x0033}},
#endif
	{0x1580, 1, {0x00AA}}, // Lock CMD2
	{0xF200, 1, {0x0001}}, // Cehck CMD status
	{REGFLAG_DELAY, 100, {}},
#if defined(USE_TENTATIVE_COMMAND)
	//{0x1100, 1, {0x0000}}, // sleep out
	//{REGFLAG_DELAY, 100, {}},
#endif
	{0x5100, 1, {0x0000}}, // Output LEDPWM=0% Duty
	{0x5300, 1, {0x002C}}, // Output LEDPWM=50% Duty

	{0x2a00, 1, {0x0000}}, // XSA
	{0x2a01, 1, {0x0000}}, // XSA
	{0x2a02, 1, {0x0000}}, // XEA
	{0x2a03, 1, {0x013f}}, // XEA, 320-1
	// set vertical address 
	{0x2b00, 1, {0x0000}}, // YSA
	{0x2b01, 1, {0x0000}}, // YSA
	{0x2b02, 1, {0x0000}}, // YEA
	{0x2b03, 1, {0x01df}}, // YEA, 480-1

	{0x3600, 1, {0x0008}}, // Set RGB
	{0x3A00, 1, {0x0055}}, // Set RGB565
	{0x3800, 1, {0x0000}}, // Set Idle Mode Off
	{0x3500, 1, {0x0000}}, // TE On, Vsync On
	{0x2900, 1, {0x0000}}, // Display On
	{REGFLAG_END_OF_TABLE, 0x00, {}}
};

void display_table(struct display_table *table, unsigned int count)
{
	unsigned int i;

    for(i = 0; i < count; i++) {
		
        unsigned reg;
        reg = table[i].reg;
		
        switch (reg) {
			
            case REGFLAG_DELAY :
                msleep(table[i].count);
				        EPRINTK("%s() : delay %d msec\n", __func__, table[i].count);
                break;
				
            case REGFLAG_END_OF_TABLE :
                break;
				
            default:
                mddi_host_register_cmds_write32(reg, table[i].count, table[i].val_list, 1, 0, 0);
                //EPRINTK("%s: reg : 0x%04X, val : 0x%04X\n", __func__, reg, table[i].val_list[0]);
       	}
    }
	
}

static void mddi_novatek_vsync_set_handler(msm_fb_vsync_handler_type handler,	
					 void *arg)
{
	boolean error = FALSE;
	unsigned long flags;

	printk("%s : handler = %x\n", 
			__func__, (unsigned int)handler);

	/* Disable interrupts */
	spin_lock_irqsave(&mddi_host_spin_lock, flags);
	/* INTLOCK(); */

	if (mddi_novatek_vsync_handler != NULL) {
		error = TRUE;
	} else {
		/* Register the handler for this particular GROUP interrupt source */
		mddi_novatek_vsync_handler = handler;
		mddi_novatek_vsync_handler_arg = arg;
	}
	
	/* Restore interrupts */
	spin_unlock_irqrestore(&mddi_host_spin_lock, flags);
	/* MDDI_INTFREE(); */
	if (error) {
		printk("MDDI: Previous Vsync handler never called\n");
	} else {
		/* Enable the vsync wakeup */
		/* mddi_queue_register_write(INTMSK, 0x0000, FALSE, 0); */
		mddi_novatek_vsync_attempts = 1;
		mddi_vsync_detect_enabled = TRUE;
	}
}

static void mddi_novatek_lcd_vsync_detected(boolean detected)
{
	mddi_vsync_detect_enabled = TRUE;;
}

#ifdef CONFIG_MACH_MSM7X27_THUNDERC_SPRINT
extern int ts_set_vreg(unsigned char onoff);
#endif

static int mddi_novatek_lcd_on(struct platform_device *pdev)
{
	EPRINTK("%s: started.\n", __func__);

	if(is_lcd_on == -1) {
		is_lcd_on = TRUE;
		return 0;
	}
	if (system_state == SYSTEM_BOOTING && mddi_novatek_pdata->initialized) {
		is_lcd_on = TRUE;
	}

#ifdef CONFIG_MACH_MSM7X27_THUNDERC_SPRINT
	ts_set_vreg(1);
#endif

	// LCD HW Reset
	mddi_novatek_lcd_panel_poweron();	
	display_table(mddi_novatek_initialize, sizeof(mddi_novatek_initialize)/sizeof(struct display_table));
	// display_table(mddi_novatek_display_on, sizeof(mddi_novatek_display_on) / sizeof(struct display_table));
	is_lcd_on = TRUE;
	return 0;
}

static int mddi_novatek_lcd_off(struct platform_device *pdev)
{
	display_table(mddi_novatek_sleep_mode_on_data, sizeof(mddi_novatek_sleep_mode_on_data)/sizeof(struct display_table));
	mddi_novatek_lcd_panel_poweroff();
	is_lcd_on = FALSE;
	return 0;
}

ssize_t mddi_novatek_lcd_show_onoff(struct device *dev, struct device_attribute *attr,
		char *buf)
{
	EPRINTK("%s : strat\n", __func__);
	return 0;
}

ssize_t mddi_novatek_lcd_store_onoff(struct device *dev, struct device_attribute *attr, 
		const char *buf, size_t count)
{
	int onoff;
	struct platform_device *pdev = to_platform_device(dev); 

	sscanf(buf, "%d", &onoff);

	EPRINTK("%s: onoff : %d\n", __func__, onoff);
	
	if(onoff) {
		mddi_novatek_lcd_on(pdev);
	}
	else {
		mddi_novatek_lcd_off(pdev);
	}

	return count;
}

int mddi_novatek_position(void)
{
	display_table(mddi_novatek_position_table, 
			ARRAY_SIZE(mddi_novatek_position_table));
	return 0;
}
EXPORT_SYMBOL(mddi_novatek_position);

DEVICE_ATTR(lcd_onoff, 0666, mddi_novatek_lcd_show_onoff, mddi_novatek_lcd_store_onoff);

struct msm_fb_panel_data novatek_panel_data0 = {
	.on = mddi_novatek_lcd_on,
	.off = mddi_novatek_lcd_off,
	.set_backlight = NULL,
	.set_vsync_notifier = mddi_novatek_vsync_set_handler,
};

static struct platform_device this_device_0 = {
	.name   = "mddi_novatek_hvga",
	.id	= MDDI_LCD_NOVATEK_NT35451,
	.dev	= {
		.platform_data = &novatek_panel_data0,
	}
};

static int __init mddi_novatek_lcd_probe(struct platform_device *pdev)
{
	int ret;
	EPRINTK("%s: started.\n", __func__);

	if (pdev->id == 0) {
		mddi_novatek_pdata = pdev->dev.platform_data;
		return 0;
	}

	msm_fb_add_device(pdev);

	ret = device_create_file(&pdev->dev, &dev_attr_lcd_onoff);

	return 0;
}

static struct platform_driver this_driver = {
	.probe  = mddi_novatek_lcd_probe,
	.driver = {
		.name   = "mddi_novatek_hvga",
	},
};

static int mddi_novatek_lcd_init(void)
{
	int ret;
	struct msm_panel_info *pinfo;

#ifdef CONFIG_FB_MSM_MDDI_AUTO_DETECT
	u32 id;
	id = mddi_get_client_id();

	/* TODO: Check client id */
#endif

#if defined(CONFIG_FB_MSM_MDDI_NOVATEK_HITACHI_HVGA)
	gpio_tlmm_config(GPIO_CFG(101, 0, GPIO_INPUT, GPIO_PULL_DOWN, GPIO_2MA), 
			  GPIO_ENABLE);
	gpio_configure(101, GPIOF_INPUT);
	if (gpio_get_value(101) != 1)
		return -ENODEV;
	g_mddi_lcd_probe = 1;
#endif

	ret = platform_driver_register(&this_driver);
	if (!ret) {
		pinfo = &novatek_panel_data0.panel_info;
		EPRINTK("%s: setting up panel info.\n", __func__);
		pinfo->xres = 320;
		pinfo->yres = 480;
		pinfo->type = MDDI_PANEL;
		pinfo->pdest = DISPLAY_1;
		pinfo->mddi.vdopkt = MDDI_DEFAULT_PRIM_PIX_ATTR;
		pinfo->wait_cycle = 0;
		pinfo->bpp = 16;
	
		// vsync config
		pinfo->lcd.vsync_enable = TRUE;
		pinfo->lcd.refx100 = (mddi_novatek_rows_per_second * 100) /
                        		mddi_novatek_rows_per_refresh;

		pinfo->lcd.v_back_porch = 200;
		pinfo->lcd.v_front_porch = 200;
		pinfo->lcd.v_pulse_width = 30;

		pinfo->lcd.hw_vsync_mode = TRUE;
		pinfo->lcd.vsync_notifier_period = (1 * HZ);

		pinfo->bl_max = 4;
		pinfo->bl_min = 1;

		pinfo->clk_rate = 122880000;
		pinfo->clk_min =   120000000;
		pinfo->clk_max =   130000000;
		pinfo->fb_num = 2;

		ret = platform_device_register(&this_device_0);
		if (ret) {
			EPRINTK("%s: this_device_0 register success\n", __func__);
			platform_driver_unregister(&this_driver);
		}
	}

	if(!ret) {
		mddi_lcd.vsync_detected = mddi_novatek_lcd_vsync_detected;
	}

	return ret;
}

extern unsigned fb_width;
extern unsigned fb_height;

static void mddi_novatek_lcd_panel_poweron(void)
{
	struct msm_panel_novatek_pdata *pdata = mddi_novatek_pdata;

	EPRINTK("%s: started.\n", __func__);

	fb_width = 320;
	fb_height = 480;

	if(pdata && pdata->gpio) {
		gpio_set_value(pdata->gpio, 1);
		mdelay(10);
		gpio_set_value(pdata->gpio, 0);
		mdelay(15); /* wait for more than 10ms */
		gpio_set_value(pdata->gpio, 1);
		mdelay(30); /* wait for more than 20ms */
	}
}

static void mddi_novatek_lcd_panel_poweroff(void)
{
	struct msm_panel_novatek_pdata *pdata = mddi_novatek_pdata;

	EPRINTK("%s: started.\n", __func__);

	fb_width = 320;
	fb_height = 480;

	if(pdata && pdata->gpio) {
		gpio_set_value(pdata->gpio, 0);
		mdelay(10);
	}
}

module_init(mddi_novatek_lcd_init);
