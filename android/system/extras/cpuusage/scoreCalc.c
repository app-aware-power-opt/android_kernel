//for aplication aware score calc
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <math.h>
#include "scoreCalc.h"

#define SCORE_POINT_NUM 11
#define AVG_SCORE 3

typedef struct {
	int diffUsage;
	int score;
}CPU_SCORE_T;

typedef struct {
	int diffUsage;
	int score;
}THREAD_SCORE_T;

typedef struct {
	int diffUsage;
	int score;
}MEMORY_SCORE_T;



CPU_SCORE_T astCpuScore[SCORE_POINT_NUM ] = {
	{0, 1},
	{10, 2},
	{20, 3},
	{30, 4},
	{40, 5},
	{50, 6},
	{60, 7},
	{70, 8},
	{80, 9},
	{90, 10},
	{100, 20},
};

THREAD_SCORE_T astThreadScore[SCORE_POINT_NUM ] = {
	{0, 1},
	{1, 2},
	{2, 3},
	{3, 4},
	{4, 5},
	{5, 6},
	{6, 7},
	{7, 8},
	{8, 9},
	{9, 10},
	{10, 20},
};

MEMORY_SCORE_T astMemoryScore[SCORE_POINT_NUM ] = {
	{0, 1},
	{10, 2},
	{20, 3},
	{30, 4},
	{40, 5},
	{50, 6},
	{60, 7},
	{70, 8},
	{80, 9},
	{90, 10},
	{100, 20},
};

int aScoreResult[AVG_SCORE] = {};

RESOURCE_USAGE_T calcDiff(RESOURCE_USAGE_T * stPre, RESOURCE_USAGE_T * stCur);

static RESOURCE_USAGE_T preUsage = {
	.cpuUsage = 20,
	.threadUsage = 2,
	.memoryUsage = 20,
};


SCORE_RESULT_T calcResourceScore(RESOURCE_USAGE_T *stUsage)
{
	int scoreResult = 0;
	int avgScoreResult = 0;
	int cpuScore = 0;
	int threadScore = 0;
	int memoryScore =0;
	int i = 0, cpuSign = 1, threadSign = 1, memorySign = 1;
	RESOURCE_USAGE_T stDiff;
	SCORE_RESULT_T stResult = {};

	static int * pScore = NULL;
	static int fInitlBuffer = 1;
	
	//TO DO : buffer free
	if(fInitlBuffer)
	{
		pScore = (int *)calloc( AVG_SCORE, sizeof(int) );
		memset(pScore, 0, AVG_SCORE);
		fInitlBuffer = 0;
	}

	stDiff = calcDiff(&preUsage, stUsage);

	preUsage = *stUsage;

	//printf("copy preCPU=%4.2f, preThread=%d, preMem=%4.2f\n", preUsage.cpuUsage, preUsage.threadUsage, preUsage.memoryUsage);

	if(stDiff.cpuUsage < 0)	cpuSign = -1;
	if(stDiff.threadUsage < 0)	threadSign = -1;
	if(stDiff.memoryUsage < 0)	memorySign = -1;


	for (i = 0; i < SCORE_POINT_NUM-1; i++)
	{
		if( abs(stDiff.cpuUsage) > astCpuScore[i].diffUsage &&  abs(stDiff.cpuUsage) <= astCpuScore[i+1].diffUsage )
 			cpuScore = astCpuScore[i].score;
		if( abs(stDiff.threadUsage) > astThreadScore[i].diffUsage && abs(stDiff.threadUsage) <= astThreadScore[i+1].diffUsage )
 			threadScore = astThreadScore[i].score;
		if( abs(stDiff.memoryUsage) > astMemoryScore[i].diffUsage &&  abs(stDiff.memoryUsage) <= astMemoryScore[i+1].diffUsage )
 			memoryScore = astMemoryScore[i].score;
	}

	scoreResult = cpuScore*cpuSign + threadScore*threadSign + memoryScore*memorySign;
	memmove(pScore+1, pScore, sizeof(int) * (AVG_SCORE-1));
	*pScore = scoreResult;

	for (i = 0; i < AVG_SCORE; i++)
	{
		avgScoreResult =+ *(pScore+i);
		avgScoreResult = avgScoreResult/AVG_SCORE;
		//printf("Score[%d]=%d", i, *(pScore+i));
	}
	
	printf("Score CPU=%d, Thread=%d, Mem=%d, sum=%d, avg=%d\n\n", 
		cpuScore*cpuSign, threadScore*threadSign, memoryScore*memorySign, scoreResult, avgScoreResult);

/*
	for (i = 0; i < AVG_SCORE; i++)
	{
		printf("pointer Score[%d]=%d\n ", i, *(pScore+i));
	}
*/
	stResult.score = scoreResult;
	stResult.avgScore = avgScoreResult;

	return stResult;

};

RESOURCE_USAGE_T calcDiff(RESOURCE_USAGE_T * stPre, RESOURCE_USAGE_T * stCur)
{
	RESOURCE_USAGE_T stDiff;
	stDiff.cpuUsage = stCur->cpuUsage - stPre->cpuUsage;
	stDiff.threadUsage = stCur->threadUsage - stPre->threadUsage;
	stDiff.memoryUsage = stCur->memoryUsage - stPre->memoryUsage;

	printf("curCPU=%4.2f, curThread=%d, curMem=%4.2f\n", stCur->cpuUsage, stCur->threadUsage, stCur->memoryUsage);
	printf("preCPU=%4.2f, preThread=%d, preMem=%4.2f\n", stPre->cpuUsage, stPre->threadUsage, stPre->memoryUsage);
	printf("diffCPU=%4.2f, diffThread=%d, diffMem=%4.2f\n", stDiff.cpuUsage, stDiff.threadUsage, stDiff.memoryUsage);

	
	return stDiff;
};

