#ifdef AEI_WECB

#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdlib.h>
#include <sys/types.h>
#ifdef WIN32
#include <io.h>
#else
#include <unistd.h>
#endif
#define __mips__

#include "apmib.h"

static unsigned short calculateChecksum(char *buf, int len, unsigned short oldv)
{
    int i, j;
    unsigned short sum=oldv, tmp;

    j = (len/2)*2;

    for (i=0; i<j; i+=2) {
        tmp = *((unsigned short *)(buf + i));
        sum += WORD_SWAP(tmp);
    }

    if ( len % 2 ) {
        tmp = buf[len-1];
        sum += WORD_SWAP(tmp);
    }
    return sum;
}


static int change_act_bin_hw_version(const char *input, const char *output, unsigned short hw_version)
{
    FW_HEADER_T fw_hdr;
    unsigned short fw_chksum = 0;
    unsigned int fw_total = 0;
    char fw_buf[4096];
    int fh_out, fh_in;
    int ret = 0;

    if (!input[0] || !output[0])
    {
        goto L0;
    }

#ifdef WIN32
    fh_in = open(input, O_RDONLY|O_BINARY);
#else
    fh_in = open(input, O_RDONLY,S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH|S_IWOTH);
#endif
    if(fh_in < 0)
    {
        perror("Open bootFile error");
        goto L0;
    }

    if(read(fh_in, &fw_hdr, sizeof(fw_hdr)) != sizeof(fw_hdr))
    {
        printf("Read %s error\n", input);
        goto L1;
    }

    fw_hdr.hw_version = WORD_SWAP(hw_version);

#ifdef WIN32
    _chmod(output, S_IREAD|S_IWRITE);
    fh_out = open(output, O_RDWR|O_CREAT|O_TRUNC|O_BINARY);
#else
    chmod(output, S_IREAD|S_IWRITE);
    fh_out = open(output, O_RDWR|O_CREAT|O_TRUNC);
#endif
    if(fh_out < 0)
    {
        perror("Create fh_out error");
        goto L1;
    }

    write(fh_out, &fw_hdr, sizeof(fw_hdr));
    fw_total = sizeof(fw_hdr);

    lseek(fh_out, sizeof(fw_hdr), SEEK_SET);

    while((ret = read(fh_in, &fw_buf, sizeof(fw_buf))) > 0)
    {
        fw_chksum = calculateChecksum((char *)&fw_buf, ret, fw_chksum);
        if(write(fh_out, fw_buf, ret) != ret)
        {
            printf("build %s error\n", output);
            goto L2;
        }
        fw_total += ret;
    }
    if (ret < 0)
    {
        printf("fw_chksum error\n");
        goto L2;
    }

    fw_hdr.force_upgrade = 1;
    fw_hdr.hdr_chksum = 0;
    fw_hdr.chksum = 0;

    fw_hdr.len = DWORD_SWAP(fw_total);
    fw_chksum = calculateChecksum((char *)&fw_hdr, sizeof(fw_hdr), fw_chksum);
    fw_hdr.chksum = WORD_SWAP((~fw_chksum+1));

    fw_chksum = calculateChecksum((char *)&fw_hdr, sizeof(fw_hdr), 0);
    fw_hdr.hdr_chksum = WORD_SWAP((~fw_chksum+1));
    lseek(fh_out, 0, SEEK_SET);
    write(fh_out, &fw_hdr, sizeof(fw_hdr));

    close(fh_out);

#ifdef WIN32
    _chmod(output, S_IREAD);
#else
    chmod(output, DEFFILEMODE);
#endif
    return 0;
L2:
    close(fh_out);
L1:
    close(fh_in);
L0:
    return -1;

}


/*
    Describe:
        This command used for change act.bin/act_boot.bin file header hw_version, then can upgrade from Comcast FW to NCS FW.

    How to use it:
        run command: mgbin act_ncs.bin act_comcast.bin <Comcast boardID>
        Use act_comcast.bin and upgrade by GUI at Comcast FW. After upgrade, the new FW is for NCS
*/
int main(int argc, char *argv[])
{
    if(argc != 4)
    {
        printf("Error argument count\n");
        printf("help:\n");
        printf("\tmgbin <input file> <output file> <New boardID>\n");
        return -1;
    }

    return change_act_bin_hw_version(argv[1], argv[2], atoi(argv[3]));
}

#endif

