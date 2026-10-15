// SPDX-License-Identifier: Apache-2.0
// Read only. The stock debug status starts these static fields one byte early.
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
int main(int argc,char **argv) {
    const char *path=argc>1?argv[1]:"/sys/kernel/debug/dw-hdmi0/ctrl";
    FILE *f=fopen(path,"r");if(!f){perror("HDMI register snapshot");return 1;}
    uint32_t registers[8]={0};unsigned found=0;char line[512];
    while(fgets(line,sizeof(line),f)){
        unsigned offset,v[4];int count=sscanf(line,">>>hdmi_ctl %x: %x %x %x %x",&offset,v,v+1,v+2,v+3)-1;
        for(int i=0;i<count;++i){unsigned reg=offset+4*i;if(reg>=0xc60 && reg<=0xc7c && !(reg&3)){
            unsigned index=(reg-0xc60)/4;registers[index]=v[i];found|=1u<<index;
        }}
    }
    fclose(f);if(found!=255){fputs("Incomplete RK3588 HDMI-QP register snapshot\n",stderr);return 2;}
    uint8_t bytes[28];for(unsigned i=0;i<28;++i)bytes[i]=(registers[1+i/4]>>(8*(i%4)))&255;
    unsigned version=(registers[0]>>8)&255,length=(registers[0]>>16)&255;
    unsigned sum=0x87+version+length;
    for(unsigned i=0;i<27;++i)sum+=bytes[i];
    if(version!=1 || length!=26 || (sum&255)){fputs("Invalid static HDR InfoFrame header or checksum\n",stderr);return 3;}
    printf("{\"register_packet_checksum_valid\":true,\"eotf\":%u,\"metadata_descriptor\":%u,\"fields\":[",bytes[1],bytes[2]);
    for(unsigned i=0;i<12;++i)printf("%s%u",i?",":"",bytes[3+2*i]|(unsigned)bytes[4+2*i]<<8);
    printf("],\"hdr_static_info_hex\":\"");
    for(unsigned i=2;i<27;++i)printf("%02x",bytes[i]);
    puts("\",\"wire_measurement\":false}");return 0;
}
