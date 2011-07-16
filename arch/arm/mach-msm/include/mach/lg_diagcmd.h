#ifndef LG_DIAGCMD_H
#define LG_DIAGCMD_H
/* 
This file comes from vendor/qcom-proprietary/diag/src/diagcmd.h
Don't change previous defines and add new id at the end   
*/

/*--------------------------------------------------------------------------

  Command Codes between the Diagnostic Monitor and the mobile. Packets
  travelling in each direction are defined here, while the packet templates
  for requests and responses are distinct.  Note that the same packet id
  value can be used for both a request and a response.  These values
  are used to index a dispatch table in diag.c, so 

  DON'T CHANGE THE NUMBERS ( REPLACE UNUSED IDS WITH FILLERS ). NEW IDs
  MUST BE ASSIGNED AT THE END.
  
----------------------------------------------------------------------------*/

/* Version Number Request/Response            */
#define DIAG_VERNO_F    0

/* Mobile Station ESN Request/Response        */
#define DIAG_ESN_F      1

/* Peek byte Request/Response                 */
#define DIAG_PEEKB_F    2
  
/* Peek word Request/Response                 */
#define DIAG_PEEKW_F    3

/* Peek dword Request/Response                */
#define DIAG_PEEKD_F    4  

/* Poke byte Request/Response                 */
#define DIAG_POKEB_F    5  

/* Poke word Request/Response                 */
#define DIAG_POKEW_F    6  

/* Poke dword Request/Response                */
#define DIAG_POKED_F    7  

/* Byte output Request/Response               */
#define DIAG_OUTP_F     8

/* Word output Request/Response               */
#define DIAG_OUTPW_F    9  

/* Byte input Request/Response                */
#define DIAG_INP_F      10 

/* Word input Request/Response                */
#define DIAG_INPW_F     11 

/* DMSS status Request/Response               */
#define DIAG_STATUS_F   12 

/* 13-14 Reserved */

/* Set logging mask Request/Response          */
#define DIAG_LOGMASK_F  15 

/* Log packet Request/Response                */
#define DIAG_LOG_F      16 

/* Peek at NV memory Request/Response         */
#define DIAG_NV_PEEK_F  17 

/* Poke at NV memory Request/Response         */
#define DIAG_NV_POKE_F  18 

/* Invalid Command Response                   */
#define DIAG_BAD_CMD_F  19 

/* Invalid parmaeter Response                 */
#define DIAG_BAD_PARM_F 20 

/* Invalid packet length Response             */
#define DIAG_BAD_LEN_F  21 

/* 22-23 Reserved */

/* Packet not allowed in this mode 
   ( online vs offline )                      */
#define DIAG_BAD_MODE_F     24
                            
/* info for TA power and voice graphs         */
#define DIAG_TAGRAPH_F      25 

/* Markov statistics                          */
#define DIAG_MARKOV_F       26 

/* Reset of Markov statistics                 */
#define DIAG_MARKOV_RESET_F 27 

/* Return diag version for comparison to
   detect incompatabilities                   */
#define DIAG_DIAG_VER_F     28 
                            
/* Return a timestamp                         */
#define DIAG_TS_F           29 

/* Set TA parameters                          */
#define DIAG_TA_PARM_F      30 

/* Request for msg report                     */
#define DIAG_MSG_F          31 

/* Handset Emulation -- keypress              */
#define DIAG_HS_KEY_F       32 

/* Handset Emulation -- lock or unlock        */
#define DIAG_HS_LOCK_F      33 

/* Handset Emulation -- display request       */
#define DIAG_HS_SCREEN_F    34 

/* 35 Reserved */

/* Parameter Download                         */
#define DIAG_PARM_SET_F     36 

/* 37 Reserved */

/* Read NV item                               */
#define DIAG_NV_READ_F  38 
/* Write NV item                              */
#define DIAG_NV_WRITE_F 39 
/* 40 Reserved */

/* Mode change request                        */
#define DIAG_CONTROL_F    41 

/* Error record retreival                     */
#define DIAG_ERR_READ_F   42 

/* Error record clear                         */
#define DIAG_ERR_CLEAR_F  43 

/* Symbol error rate counter reset            */
#define DIAG_SER_RESET_F  44 

/* Symbol error rate counter report           */
#define DIAG_SER_REPORT_F 45 

/* Run a specified test                       */
#define DIAG_TEST_F       46 

/* Retreive the current dip switch setting    */
#define DIAG_GET_DIPSW_F  47 

/* Write new dip switch setting               */
#define DIAG_SET_DIPSW_F  48 

/* Start/Stop Vocoder PCM loopback            */
#define DIAG_VOC_PCM_LB_F 49 

/* Start/Stop Vocoder PKT loopback            */
#define DIAG_VOC_PKT_LB_F 50 

/* 51-52 Reserved */

/* Originate a call                           */
#define DIAG_ORIG_F 53 
/* End a call                                 */
#define DIAG_END_F  54 
/* 55-57 Reserved */

/* Switch to downloader                       */
#define DIAG_DLOAD_F 58 
/* Test Mode Commands and FTM commands        */
#define DIAG_TMOB_F  59 
/* Test Mode Commands and FTM commands        */
#define DIAG_FTM_CMD_F  59 
/* 60-62 Reserved */

#ifdef FEATURE_HWTC
#define DIAG_TEST_STATE_F 61
#endif /* FEATURE_HWTC */

/* Return the current state of the phone      */
#define DIAG_STATE_F        63 

/* Return all current sets of pilots          */
#define DIAG_PILOT_SETS_F   64 

/* Send the Service Prog. Code to allow SP    */
#define DIAG_SPC_F          65 

/* Invalid nv_read/write because SP is locked */
#define DIAG_BAD_SPC_MODE_F 66 

/* get parms obsoletes PARM_GET               */
#define DIAG_PARM_GET2_F    67 

/* Serial mode change Request/Response        */
#define DIAG_SERIAL_CHG_F   68 

/* 69 Reserved */

/* Send password to unlock secure operations  
   the phone to be in a security state that
   is wasn't - like unlocked.                 */
#define DIAG_PASSWORD_F     70 

/* An operation was attempted which required  */
#define DIAG_BAD_SEC_MODE_F 71 

/* Write Preferred Roaming list to the phone. */
#define DIAG_PR_LIST_WR_F   72 

/* Read Preferred Roaming list from the phone.*/
#define DIAG_PR_LIST_RD_F   73 

/* 74 Reserved */

/* Subssytem dispatcher (extended diag cmd)   */
#define DIAG_SUBSYS_CMD_F   75 

/* 76-80 Reserved */

/* Asks the phone what it supports            */
#define DIAG_FEATURE_QUERY_F   81 

/* 82 Reserved */

/* Read SMS message out of NV                 */
#define DIAG_SMS_READ_F        83 

/* Write SMS message into NV                  */
#define DIAG_SMS_WRITE_F       84 

/* info for Frame Error Rate          
   on multiple channels                       */
#define DIAG_SUP_FER_F         85 

/* Supplemental channel walsh codes           */
#define DIAG_SUP_WALSH_CODES_F 86 

/* Sets the maximum # supplemental 
   channels                                   */
#define DIAG_SET_MAX_SUP_CH_F  87 

/* get parms including SUPP and MUX2: 
   obsoletes PARM_GET and PARM_GET_2          */
#define DIAG_PARM_GET_IS95B_F  88 

/* Performs an Embedded File System
   (EFS) operation.                           */
#define DIAG_FS_OP_F           89 

/* AKEY Verification.                         */
#define DIAG_AKEY_VERIFY_F     90 

/* Handset emulation - Bitmap screen          */
#define DIAG_BMP_HS_SCREEN_F   91 

/* Configure communications                   */
#define DIAG_CONFIG_COMM_F        92 

/* Extended logmask for > 32 bits.            */
#define DIAG_EXT_LOGMASK_F        93 

/* 94-95 reserved */

/* Static Event reporting.                    */
#define DIAG_EVENT_REPORT_F       96 

/* Load balancing and more!                   */
#define DIAG_STREAMING_CONFIG_F   97 

/* Parameter retrieval                        */
#define DIAG_PARM_RETRIEVE_F      98 

 /* A state/status snapshot of the DMSS.      */
#define DIAG_STATUS_SNAPSHOT_F    99
 
/* Used for RPC                               */
#define DIAG_RPC_F               100 

/* Get_property requests                      */
#define DIAG_GET_PROPERTY_F      101 

/* Put_property requests                      */
#define DIAG_PUT_PROPERTY_F      102 

/* Get_guid requests                          */
#define DIAG_GET_GUID_F          103 

/* Invocation of user callbacks               */
#define DIAG_USER_CMD_F          104 

/* Get permanent properties                   */
#define DIAG_GET_PERM_PROPERTY_F 105 

/* Put permanent properties                   */
#define DIAG_PUT_PERM_PROPERTY_F 106 

/* Permanent user callbacks                   */
#define DIAG_PERM_USER_CMD_F     107 

/* GPS Session Control                        */
#define DIAG_GPS_SESS_CTRL_F     108 

/* GPS search grid                            */
#define DIAG_GPS_GRID_F          109 

/* GPS Statistics                             */
#define DIAG_GPS_STATISTICS_F    110 

/* Packet routing for multiple instances of diag */
#define DIAG_ROUTE_F             111 

/* IS2000 status                              */
#define DIAG_IS2000_STATUS_F     112

/* RLP statistics reset                       */
#define DIAG_RLP_STAT_RESET_F    113

/* (S)TDSO statistics reset                   */
#define DIAG_TDSO_STAT_RESET_F   114

/* Logging configuration packet               */
#define DIAG_LOG_CONFIG_F        115

/* Static Trace Event reporting */
#define DIAG_TRACE_EVENT_REPORT_F 116

/* SBI Read */
#define DIAG_SBI_READ_F           117

/* SBI Write */
#define DIAG_SBI_WRITE_F          118

/* SSD Verify */
#define DIAG_SSD_VERIFY_F         119

/* Log on Request */
#define DIAG_LOG_ON_DEMAND_F      120

/* Request for extended msg report */
#define DIAG_EXT_MSG_F            121 

/* ONCRPC diag packet */
#define DIAG_ONCRPC_F             122

/* Diagnostics protocol loopback. */
#define DIAG_PROTOCOL_LOOPBACK_F  123

/* Extended build ID text */
#define DIAG_EXT_BUILD_ID_F       124

/* Request for extended msg report */
#define DIAG_EXT_MSG_CONFIG_F     125

/* Extended messages in terse format */
#define DIAG_EXT_MSG_TERSE_F      126

/* Translate terse format message identifier */
#define DIAG_EXT_MSG_TERSE_XLATE_F 127

/* Subssytem dispatcher Version 2 (delayed response capable) */
#define DIAG_SUBSYS_CMD_VER_2_F    128

/* Get the event mask */
#define DIAG_EVENT_MASK_GET_F      129

/* Set the event mask */
#define DIAG_EVENT_MASK_SET_F      130

/* RESERVED CODES: 131-139 */

/* Command Code for Changing Port Settings */
#define DIAG_CHANGE_PORT_SETTINGS  140

/* Country network information for assisted dialing */
#define DIAG_CNTRY_INFO_F          141

/* Send a Supplementary Service Request */
#define DIAG_SUPS_REQ_F            142

/* Originate SMS request for MMS */
#define DIAG_MMS_ORIG_SMS_REQUEST_F 143

/* Change measurement mode*/
#define DIAG_MEAS_MODE_F           144

/* Request measurements for HDR channels */
#define DIAG_MEAS_REQ_F            145

/* Send Optimized F3 messages */
#define DIAG_QSR_EXT_MSG_TERSE_F   146

#define DIAG_LGF_SCREEN_SHOT_F     150

#define DIAG_LGF_SCREEN_PARTSHOT_F     151 

#define DIAG_MTC_F              240
#define DIAG_TEST_MODE_F          250  
#define DIAG_UDM_SMS_MODE			252
#define DIAG_LCD_Q_TEST_F         253

#define DIAG_MAX_F                 255

typedef enum {
  DIAG_SUBSYS_OEM                = 0,       /* Reserved for OEM use */
  DIAG_SUBSYS_ZREX               = 1,       /* ZREX */
  DIAG_SUBSYS_SD                 = 2,       /* System Determination */
  DIAG_SUBSYS_BT                 = 3,       /* Bluetooth */
  DIAG_SUBSYS_WCDMA              = 4,       /* WCDMA */
  DIAG_SUBSYS_HDR                = 5,       /* 1xEvDO */
  DIAG_SUBSYS_DIABLO             = 6,       /* DIABLO */
  DIAG_SUBSYS_TREX               = 7,       /* TREX - Off-target testing environments */
  DIAG_SUBSYS_GSM                = 8,       /* GSM */
  DIAG_SUBSYS_UMTS               = 9,       /* UMTS */
  DIAG_SUBSYS_HWTC               = 10,      /* HWTC */
  DIAG_SUBSYS_FTM                = 11,      /* Factory Test Mode */
  DIAG_SUBSYS_REX                = 12,      /* Rex */
  DIAG_SUBSYS_OS                 = DIAG_SUBSYS_REX,
  DIAG_SUBSYS_GPS                = 13,      /* Global Positioning System */
  DIAG_SUBSYS_WMS                = 14,      /* Wireless Messaging Service (WMS, SMS) */
  DIAG_SUBSYS_CM                 = 15,      /* Call Manager */
  DIAG_SUBSYS_HS                 = 16,      /* Handset */
  DIAG_SUBSYS_AUDIO_SETTINGS     = 17,      /* Audio Settings */
  DIAG_SUBSYS_DIAG_SERV          = 18,      /* DIAG Services */
  DIAG_SUBSYS_FS                 = 19,      /* File System - EFS2 */
  DIAG_SUBSYS_PORT_MAP_SETTINGS  = 20,      /* Port Map Settings */
  DIAG_SUBSYS_MEDIAPLAYER        = 21,      /* QCT Mediaplayer */
  DIAG_SUBSYS_QCAMERA            = 22,      /* QCT QCamera */
  DIAG_SUBSYS_MOBIMON            = 23,      /* QCT MobiMon */
  DIAG_SUBSYS_GUNIMON            = 24,      /* QCT GuniMon */
  DIAG_SUBSYS_LSM                = 25,      /* Location Services Manager */
  DIAG_SUBSYS_QCAMCORDER         = 26,      /* QCT QCamcorder */
  DIAG_SUBSYS_MUX1X              = 27,      /* Multiplexer */
  DIAG_SUBSYS_DATA1X             = 28,      /* Data */
  DIAG_SUBSYS_SRCH1X             = 29,      /* Searcher */
  DIAG_SUBSYS_CALLP1X            = 30,      /* Call Processor */
  DIAG_SUBSYS_APPS               = 31,      /* Applications */
  DIAG_SUBSYS_SETTINGS           = 32,      /* Settings */
  DIAG_SUBSYS_GSDI               = 33,      /* Generic SIM Driver Interface */
  DIAG_SUBSYS_UIMDIAG            = DIAG_SUBSYS_GSDI,
  DIAG_SUBSYS_TMC                = 34,      /* Task Main Controller */
  DIAG_SUBSYS_USB                = 35,      /* Universal Serial Bus */
  DIAG_SUBSYS_PM                 = 36,      /* Power Management */
  DIAG_SUBSYS_DEBUG              = 37,
  DIAG_SUBSYS_QTV                = 38,
  DIAG_SUBSYS_CLKRGM             = 39,      /* Clock Regime */
  DIAG_SUBSYS_DEVICES            = 40,
  DIAG_SUBSYS_WLAN               = 41,      /* 802.11 Technology */
  DIAG_SUBSYS_PS_DATA_LOGGING    = 42,      /* Data Path Logging */
  DIAG_SUBSYS_PS                 = DIAG_SUBSYS_PS_DATA_LOGGING,
  DIAG_SUBSYS_MFLO               = 43,      /* MediaFLO */
  DIAG_SUBSYS_DTV                = 44,      /* Digital TV */
  DIAG_SUBSYS_RRC                = 45,      /* WCDMA Radio Resource Control state */
  DIAG_SUBSYS_PROF               = 46,      /* Miscellaneous Profiling Related */
  DIAG_SUBSYS_TCXOMGR            = 47,
  DIAG_SUBSYS_NV                 = 48,      /* Non Volatile Memory */
  DIAG_SUBSYS_AUTOCONFIG         = 49,
  DIAG_SUBSYS_PARAMS             = 50,      /* Parameters required for debugging subsystems */
  DIAG_SUBSYS_MDDI               = 51,      /* Mobile Display Digital Interface */
  DIAG_SUBSYS_DS_ATCOP           = 52,
  DIAG_SUBSYS_L4LINUX            = 53,      /* L4/Linux */
  DIAG_SUBSYS_MVS                = 54,      /* Multimode Voice Services */
  DIAG_SUBSYS_CNV                = 55,      /* Compact NV */
  DIAG_SUBSYS_APIONE_PROGRAM     = 56,      /* apiOne */
  DIAG_SUBSYS_HIT                = 57,      /* Hardware Integration Test */
  DIAG_SUBSYS_DRM                = 58,      /* Digital Rights Management */
  DIAG_SUBSYS_DM                 = 59,      /* Device Management */
  DIAG_SUBSYS_FC                 = 60,      /* Flow Controller */
  DIAG_SUBSYS_MEMORY             = 61,      /* Malloc Manager */
  DIAG_SUBSYS_FS_ALTERNATE       = 62,      /* Alternate File System */
  DIAG_SUBSYS_REGRESSION         = 63,      /* Regression Test Commands */
  DIAG_SUBSYS_SENSORS            = 64,      /* The sensors subsystem */
  DIAG_SUBSYS_FLUTE              = 65,      /* FLUTE */
  DIAG_SUBSYS_ANALOG             = 66,      /* Analog die subsystem */
  DIAG_SUBSYS_APIONE_PROGRAM_MODEM = 67,    /* apiOne Program On Modem Processor */
  DIAG_SUBSYS_LTE                = 68,      /* LTE */
  DIAG_SUBSYS_BREW               = 69,      /* BREW */
  DIAG_SUBSYS_PWRDB              = 70,      /* Power Debug Tool */
  DIAG_SUBSYS_CHORD              = 71,      /* Chaos Coordinator */
  DIAG_SUBSYS_SEC                = 72,      /* Security */
  DIAG_SUBSYS_TIME               = 73,      /* Time Services */
  DIAG_SUBSYS_Q6_CORE            = 74,		  /* Q6 core services */

  DIAG_SUBSYS_LAST,

  /* Subsystem IDs reserved for OEM use */
  DIAG_SUBSYS_RESERVED_OEM_0     = 250,
  DIAG_SUBSYS_RESERVED_OEM_1     = 251,
  DIAG_SUBSYS_RESERVED_OEM_2     = 252,
  DIAG_SUBSYS_RESERVED_OEM_3     = 253,
  DIAG_SUBSYS_RESERVED_OEM_4     = 254,
  DIAG_SUBSYS_LEGACY             = 255
} diagpkt_subsys_cmd_enum_type;

#define LG_DIAG_CMD_LINE_LEN 256

struct lg_diag_cmd_dev {
	char	*name;
	struct device *dev;
	int 	index;
	int 	state;
};

#if 1 //LG_FW_MTC_GISELE
#define BUFFER_MAX_SIZE 4096

struct mtc_data_buffer
{
  int data_length;
  char data[BUFFER_MAX_SIZE];
};
#endif

#define DIAG_IOCTL_BULK_DATA    10

extern void* lg_diag_mtc_req_pkt_ptr;
extern unsigned short lg_diag_mtc_req_pkt_length;
extern unsigned char g_diag_mtc_check;

//keycode define in surf_keypad.kl
#define KERNELHOMEKEY 0xE6
#define KERNELBACKKEY 0x9E
#define KERNELPPOWERKEY 0x6B
#define KERNELPVOLUPKEY 0x73
#define KERNELVOLDNKEY 0x72
#define KERNELCAMERAKEY 0xD4
#define KERNELMENUKEY 0x8B
#define KERNELFOCUSKEY 0xF7

#define APPHOMEKEY 0x94
#define APPBACKKEY 0x92
#define APPPOWERKEY 0x97
#define APPVOLUPKEY 0x8F
#define APPVOLDNKEY 0x90
#define APPCAMERAKEY 0x96
#define APPMENUKEY 0x93
#define KEY_STAR 0xE3
#define KEY_SHARP 0xE4
//LG_FW_MTC_GISELE

#endif /* LG_DIAGCMD_H */
