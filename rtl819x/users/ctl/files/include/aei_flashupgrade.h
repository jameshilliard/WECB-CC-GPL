#ifndef __AEI_FLASHUPGRADE_H__
#define __AEI_FLASHUPGRADE_H__


#include "../../../../../boa/apmib/apmib.h"
#include "../../../../../boa/apmib/mibtbl.h"



enum
{
    IMAGE_BANK1 = 1,
    IMAGE_BANK2 = 2,
};


/*
Function:	aei_get_image_bank
Decriptions:	Get the image bank number
Parameters:	
        image: (IN) BOOT_IMAGE or BACK_IMAGE
Return Value: 
        <0	- Fail
        ==0	- Success
*/
int aei_get_image_bank(int image);


/*
Function:	aei_upgrade_image
Decriptions:	Upgrade the image
Parameters:	
        buf: point to the image
        file_len: the image len
        image: (IN) BOOT_IMAGE or BACK_IMAGE
        write_bootcode: (IN) upgrade the bootcode or not
Return Value: 
        <0	- Fail
        ==0	- Success
*/
int aei_upgrade_image(const char *buf, int file_len, int image, int write_bootcode);


/*
Function:	aei_get_current_default_bank
Decriptions:	Get the current boot image bank number
Parameters:	
Return Value: 
        <0	- Fail
        IMAGE_BANK1	- The bank1 is boot image
        IMAGE_BANK2 - The bank2 is boot image
*/
int aei_get_current_default_bank(void);


/*
Function:	aei_get_image_version
Decriptions:	Get the image version from flash
Parameters:	
        buf: 	(OUT) Point to buf for save image version
        len:    (IN) The len of buf
        image: (IN) BACK_IMAGE or BOOT_IMAGE
Return Value: 
        <0	- Fail
        ==0	- Success
*/
int aei_get_image_version(char *buf, int len, int image);


/*
Function:	aei_set_default_bootup_bank
Decriptions:	Set default boot image bank
Parameters:	
        bank: IMAGE_BANK1 or IMAGE_BANK2
Return Value: 
        <0	- Fail
        ==0	- Success
*/
int aei_set_default_bootup_bank(int bank);

#endif
