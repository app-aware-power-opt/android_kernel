//for aplication aware score calc
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "scoreCalc.h"


RESOURCE_USAGE_T calcDiff(RESOURCE_USAGE_T * stPre, RESOURCE_USAGE_T * stCur);

static RESOURCE_USAGE_T preUsage = {
	.cpuUsage = 20,
	.threadUsage = 2,
	.memoryUsage = 20,
};


int calcResourceScore(RESOURCE_USAGE_T *stUsage)
{
	int scoreResult = 0;

	RESOURCE_USAGE_T diffUsage;

	diffUsage = calcDiff(&preUsage, stUsage);
	
	preUsage = *stUsage;






	return scoreResult;

};

RESOURCE_USAGE_T calcDiff(RESOURCE_USAGE_T * stPre, RESOURCE_USAGE_T * stCur)
{
	RESOURCE_USAGE_T stDiff;
	stDiff.cpuUsage = stCur->cpuUsage - stPre->cpuUsage;
	stDiff.threadUsage = stCur->threadUsage - stPre->threadUsage;
	stDiff.memoryUsage = stCur->memoryUsage - stPre->memoryUsage;

	printf("curCPU=%4lu.%4lu, curThread=%d, curMem=%4.2f\n", stCur->cpuUsage/100, stCur->cpuUsage%100, stCur->threadUsage, stCur->memoryUsage);

	printf("diffCPU=%4lu.%4lu, diffThread=%d, diffMem=%4.2f\n", stDiff.cpuUsage/100, stCur->cpuUsage%100, stDiff.threadUsage, stDiff.memoryUsage);

	
	return stDiff;
};

