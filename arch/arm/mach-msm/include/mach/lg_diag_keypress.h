#ifndef LG_DIAG_KEYPRESS_H
#define LG_DIAG_KEYPRESS_H
// LG_FW_DIAG_KERNEL_SERVICE

#include "lg_comdef.h"
/*********************** BEGIN PACK() Definition ***************************/
#if defined __GNUC__
  #define PACK(x)       x __attribute__((__packed__))
  #define PACKED        __attribute__((__packed__))
#elif defined __arm
  #define PACK(x)       __packed x
  #define PACKED        __packed
#else
  #error No PACK() macro defined for this compiler
#endif
/********************** END PACK() Definition *****************************/

typedef struct DIAG_HS_KEY_F_req_tag
{
  unsigned char command_code;	// cmd code
  unsigned char hold;		// If true, diag witholds key release
  unsigned char key;		// enumerated key, e.g. HS_DOWN_K
}PACKED DIAG_HS_KEY_F_req_type;

typedef DIAG_HS_KEY_F_req_type DIAG_HS_KEY_F_rsp_type;

#endif /* LG_DIAG_KEYPRESS_H */
