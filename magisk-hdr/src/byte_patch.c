/* SPDX-License-Identifier: MIT
 * Bounded byte recipe application to a COPY. Installer separately pins the
 * complete original and resulting SHA256; every edited byte is checked here.
 */
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
static int digit(char c){return c>='0'&&c<='9'?c-'0':c>='a'&&c<='f'?c-'a'+10:-1;}
static int decode(const char *s,uint8_t *out){size_t n=strlen(s);if(n%2||n>1024)return -1;for(size_t i=0;i<n/2;i++){int a=digit(s[i*2]),b=digit(s[i*2+1]);if(a<0||b<0)return -1;out[i]=(a<<4)|b;}return n/2;}
int main(int argc,char **argv){
 if(argc!=4||!strcmp(argv[1],argv[2]))return 2;
 FILE *f=fopen(argv[1],"rb");if(!f)return 3;if(fseek(f,0,SEEK_END)){fclose(f);return 3;}
 long size=ftell(f);if(size<=0||size>32*1024*1024){fclose(f);return 3;}
 uint8_t *b=malloc(size);if(!b){fclose(f);return 3;}rewind(f);
 if(fread(b,1,size,f)!=(size_t)size){fclose(f);free(b);return 3;}fclose(f);
 f=fopen(argv[3],"r");if(!f){free(b);return 3;}
 char old[1025],next[1025];unsigned long offset;unsigned count=0;int fields;
 while((fields=fscanf(f,"%lu %1024s %1024s",&offset,old,next))==3){
  uint8_t before[512],after[512];int n=decode(old,before),m=decode(next,after);
  if(++count>32||n<=0||m!=n||offset>(unsigned long)size||(unsigned)n>(unsigned long)size-offset||memcmp(b+offset,before,n)){fclose(f);free(b);return 4;}
  memcpy(b+offset,after,n);
 }
 fclose(f);if(fields!=EOF||!count){free(b);return 4;}
 f=fopen(argv[2],"wb");if(!f){free(b);return 5;}int ok=fwrite(b,1,size,f)==(size_t)size;
 if(fclose(f))ok=0;free(b);return ok?0:5;
}
