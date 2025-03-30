#ifndef GLOBAL_H
#define GLOBAL_H
#include "zmcaux.h"
///存储重新映射的数组////
extern int MotorMap[10];
extern int MotorMapbuckup[10];
extern float fAxisNum;

extern bool AllRecordStart;
extern float downForce;

// 声明全局变量
extern ZMC_HANDLE g_handle;

#endif // GLOBAL_H
