/*
 *  synaptics_reflash.h :
 */
#ifdef _cplusplus
extern "C" {
#endif

#ifndef _SYNA_REPLASH_LAYER_H
#define _SYNA_REPLSAH_LAYER_H

#include <linux/i2c.h>

void SynaConvertErrCodeToStr(unsigned int errCode, char * errCodeStr, int len);
void SynaCheckIfFatalError(unsigned int errCode);
unsigned int SynaIsExpectedRegFormat();
void SynaReadFirmwareInfo();
void SynaReadConfigInfo();
void SynaSetFlashAddrForDifFormat();
void SynaReadPageDescriptionTable();
int SynaInitialize(struct i2c_client *syn_touch);
void SynaEnableFlashing();
void SynaSpecialCopyEndianAgnostic(unsigned char *dest, unsigned short src) ;
unsigned int SynaReadBootloadID();
unsigned int SynaWriteBootloadID();
void SynaWaitATTN(int errorCount);
void SynaProgramFirmware();
int SynaFlashFirmwareWrite();
unsigned int SynaDoReflash(struct i2c_client *syn_touch, int fw_revision);
void RMI4CheckIfFatalError(int errCode);
int SynaFinalizeFlash();

#endif /* _SYNA_REPLASH_LAYER_H */

#ifdef _cplusplus
}
#endif

