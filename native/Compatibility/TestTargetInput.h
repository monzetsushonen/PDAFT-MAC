#pragma once
/* Offline test input scheduling only; original game's judgments are untouched. */
static int p2TargetKey(int type,int hit,float left){
 if(hit!=21||left==0.0f||!(left<=.023f&&left>=-.040f))return -1;
 if(type>=0&&type<=7){const int map[4]={'W','D','S','A'};return map[type%4];}
 if(type>=18&&type<=21){const int map[4]={'W','D','S','A'};return map[type-18];}
 if(type==12||type==15||type==23)return 'Q';
 if(type==13||type==16||type==24)return 'O';
 return -2;
}
