// SPDX-License-Identifier: Apache-2.0
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
static int hex(char c){if(c>='0'&&c<='9')return c-'0';if(c>='a'&&c<='f')return c-'a'+10;if(c>='A'&&c<='F')return c-'A'+10;return -1;}
int main(int argc,char**argv){
 if(argc!=5)return 2;FILE*in=fopen(argv[2],"rb");if(!in)return 3;if(fseek(in,0,SEEK_END))return 4;long n=ftell(in);if(n<=0||n>16*1024*1024)return 5;rewind(in);unsigned char*data=malloc(n);if(!data)return 6;if(fread(data,1,n,in)!=(size_t)n)return 7;fclose(in);
 unsigned char*outdata=data;size_t outsize=n;
 if(!strcmp(argv[1],"binary")){
  FILE*recipe=fopen(argv[4],"r");if(!recipe)return 8;unsigned long long off;char old[257],next[257];int count=0,fields;
  while((fields=fscanf(recipe,"%llu %256s %256s",&off,old,next))==3){size_t len=strlen(old);if(len%2||len!=strlen(next)||off>(unsigned long long)n||len/2>(unsigned long long)n-off)return 9;
   for(size_t i=0;i<len;i+=2){int a=hex(old[i]),b=hex(old[i+1]),c=hex(next[i]),d=hex(next[i+1]);if(a<0||b<0||c<0||d<0||data[off+i/2]!=(unsigned char)(a*16+b))return 10;}
   for(size_t i=0;i<len;i+=2)data[off+i/2]=(hex(next[i])*16+hex(next[i+1]));count++;
  }
  fclose(recipe);if(fields!=EOF||!count)return 11;
 }else if(!strcmp(argv[1],"xml")){
  const char*closing="</Decoders>";size_t marker=strlen(closing),at=0;int matches=0;for(size_t i=0;i+marker<=(size_t)n;i++)if(!memcmp(data+i,closing,marker)){at=i;matches++;}if(matches!=1)return 12;
  FILE*extra=fopen(argv[4],"rb");if(!extra)return 13;fseek(extra,0,SEEK_END);long m=ftell(extra);if(m<=0||m>65536)return 14;rewind(extra);outsize=n+m;outdata=malloc(outsize);if(!outdata)return 15;memcpy(outdata,data,at);if(fread(outdata+at,1,m,extra)!=(size_t)m)return 16;fclose(extra);memcpy(outdata+at+m,data+at,n-at);
 }else return 17;
 int fd=open(argv[3],O_WRONLY|O_CREAT|O_EXCL,0644);if(fd<0)return 18;size_t at=0;while(at<outsize){ssize_t w=write(fd,outdata+at,outsize-at);if(w<0&&errno==EINTR)continue;if(w<=0){close(fd);unlink(argv[3]);return 19;}at+=w;}if(fsync(fd)||close(fd)){unlink(argv[3]);return 20;}if(outdata!=data)free(outdata);free(data);return 0;
}
