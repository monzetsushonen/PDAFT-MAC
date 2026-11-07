#include <stdio.h>
#include <math.h>
#include "TestTargetInput.h"
int main(void){int n=0;
#define CHECK(x) do{if(!(x)){fprintf(stderr,"failed line%d\n",__LINE__);return 1;}++n;}while(0)
 CHECK(p2TargetKey(0,21,0)==-1);CHECK(p2TargetKey(0,0,.01f)==-1);CHECK(p2TargetKey(0,8,.01f)==-1);
 CHECK(p2TargetKey(0,21,NAN)==-1);CHECK(p2TargetKey(0,21,INFINITY)==-1);CHECK(p2TargetKey(0,21,-INFINITY)==-1);
 CHECK(p2TargetKey(0,21,.024f)==-1);CHECK(p2TargetKey(0,21,-.041f)==-1);
 CHECK(p2TargetKey(0,21,.023f)=='W');CHECK(p2TargetKey(1,21,-.040f)=='D');CHECK(p2TargetKey(2,21,.01f)=='S');CHECK(p2TargetKey(3,21,-.01f)=='A');
 CHECK(p2TargetKey(4,21,.01f)=='W');CHECK(p2TargetKey(7,21,.01f)=='A');CHECK(p2TargetKey(18,21,.01f)=='W');CHECK(p2TargetKey(21,21,.01f)=='A');
 CHECK(p2TargetKey(12,21,.01f)=='Q');CHECK(p2TargetKey(15,21,.01f)=='Q');CHECK(p2TargetKey(23,21,.01f)=='Q');CHECK(p2TargetKey(13,21,.01f)=='O');CHECK(p2TargetKey(16,21,.01f)=='O');CHECK(p2TargetKey(24,21,.01f)=='O');
 CHECK(p2TargetKey(11,21,.01f)==-2);CHECK(p2TargetKey(90,21,.01f)==-2);
 printf("PASS %d timing/inactive-slot/nonfinite/type-routing boundaries; no HOLD/full-game claim\n",n);return 0;
}
