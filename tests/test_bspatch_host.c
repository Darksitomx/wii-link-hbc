#include "bspatch.h"
#include <stdio.h>
int main(int argc,char**argv){
 if(argc!=4)return 2;
 int r=bspatch_file(argv[1],argv[2],argv[3],NULL,NULL);
 if(r)fprintf(stderr,"%s\n",bspatch_last_error());
 return r?1:0;
}
