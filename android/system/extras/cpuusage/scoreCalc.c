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



CPU_SCORE_T astCpuScore[SCORE_POINT_NUM ];

//Data is loaded from Text file

/*
= {
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
*/

THREAD_SCORE_T astThreadScore[SCORE_POINT_NUM ];

/*
= {
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
*/

MEMORY_SCORE_T astMemoryScore[SCORE_POINT_NUM ];

/*
= {
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
*/

int aScoreResult[AVG_SCORE] = {};

RESOURCE_USAGE_T calcDiff(RESOURCE_USAGE_T * stPre, RESOURCE_USAGE_T * stCur);

static RESOURCE_USAGE_T preUsage = {
	.cpuUsage = 20,
	.threadUsage = 2,
	.memoryUsage = 20,
};



//Usage & Score Table format is like below,
// 0 10 20 30 40 50 60 70 80 90 100
//Each data is seperated by space.

//Data file could be made by echo command.
//Ex.  #echo 10 20 30 40 50 60 70 > cpuusage.txt
//Check data by cat command.
//Ex.  #cat cpuusage.txt

int tableFilesCheck(void){

	int i,j;
	FILE *fp ;

	char * file_name[6]  = {
		"/data/cpulog/usage.txt", //CPU Usage Step data
		"/data/cpulog/usagescore.txt", //CPU Usage Score data
		"/data/cpulog/thread.txt", //CPU Thread Step data
		"/data/cpulog/threadscore.txt", //CPU Thread Score data
		"/data/cpulog/mem.txt", //Memory Usage Step data
		"/data/cpulog/memscore.txt" //Memory Usage Score data
	};

	int initValue[6][11] ={
		{0, 10, 20, 30, 40, 50, 60, 70, 80, 90, 100}, //CPU Usage
		{1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 20}, //CPU Usage Score
		{0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10}, //CPU Thread Number
		{1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 20}, //CPU Thread Score
		{0, 10, 20, 30, 40, 50, 60, 70, 80, 90, 100}, //Memory Usage
		{1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 20} //Memory Score
		};

	for (i=0; i<6 ; i++){

		//Text file check.
		if ( 0 == access( file_name[i], F_OK)){
     		printf( "%s file already exists.\n", file_name[i]);
   			}
		
		else{

			//If there is no data text file, the file will be generated with initial value.
			fp = fopen( file_name[i], "wt");
			if(fp == NULL){
				printf("fopen w error\n");
				return 0;
				}


			for (j=0; j<11;j++){

				if ( j == 10)
					fprintf(fp, "%d\n", initValue[i][j]);
				else
					fprintf(fp, "%d ", initValue[i][j]);
				
				}

			printf("%s file is generated. \n", file_name[i]);
			
			fclose(fp);
		
			}
		
		}

	return 0;

}

int loadUsageScoreValue(void){

	FILE *fp ;

	int i;

	int data;

	//Table file check.
	tableFilesCheck();

	//cpuusage.txt file has CPU Usage table data.
	fp = fopen("/data/cpulog/usage.txt", "rt");
	if(fp == NULL){
		printf("fopen open error\n");
		return 0;
		}
	
	while(fscanf(fp, "%d", &data) != EOF){
		astCpuScore[i].diffUsage = data;
		i++;
		}
	
		i = 0;	
	fclose(fp);
	
	//usagescore.txt file has CPU Usage Score data.
	fp = fopen("/data/cpulog/usagescore.txt", "rt");
	if(fp == NULL){
		printf("fopen open error\n");
		return 0;
		}
	
	while(fscanf(fp, "%d", &data) != EOF){
		astCpuScore[i].score = data;
		i++;
		}
							
		i = 0;						
	fclose(fp);

	//thread.txt file has CPU Thread Number table data.
	fp = fopen("/data/cpulog/thread.txt", "rt");
	if(fp == NULL){
		printf("fopen open error\n");
		return 0;
		}
	
	while(fscanf(fp, "%d", &data) != EOF){
		astThreadScore[i].diffUsage = data;
		i++;
		}
							
		i = 0;						
	fclose(fp);

	//threadscore.txt file has CPU Thread Score data.
	fp = fopen("/data/cpulog/threadscore.txt", "rt");
	if(fp == NULL){
		printf("fopen open error\n");
		return 0;
		}
	
	while(fscanf(fp, "%d", &data) != EOF){
		astThreadScore[i].score = data;
		i++;
		}
							
		i = 0;						
	fclose(fp);

	//mem.txt file has Memory Usage table data.
	fp = fopen("/data/cpulog/mem.txt", "rt");
	if(fp == NULL){
		printf("fopen open error\n");
		return 0;
		}
	
	while(fscanf(fp, "%d", &data) != EOF){
		astMemoryScore[i].diffUsage = data;
		i++;
		}
							
		i = 0;						
	fclose(fp);
	
	//memscore.txt file has Memory Score data.
	fp = fopen("/data/cpulog/memscore.txt", "rt");
	if(fp == NULL){
		printf("fopen open error\n");
		return 0;
		}
	
	while(fscanf(fp, "%d", &data) != EOF){
		astMemoryScore[i].score = data;
		i++;
		}
							
		i = 0;						
	fclose(fp);

	

	return 0;


}

SCORE_RESULT_T calcResourceScore(RESOURCE_USAGE_T *stUsage)
{
	int scoreResult = 0;
	int scoreSum = 0;
	float avgScoreResult = 0;
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

/*
	for (i = 0; i < SCORE_POINT_NUM-1; i++)
	{
		if( abs(stDiff.cpuUsage) > astCpuScore[i].diffUsage &&  abs(stDiff.cpuUsage) <= astCpuScore[i+1].diffUsage )
 			cpuScore = astCpuScore[i].score;
		if( abs(stDiff.threadUsage) > astThreadScore[i].diffUsage && abs(stDiff.threadUsage) <= astThreadScore[i+1].diffUsage )
 			threadScore = astThreadScore[i].score;
		if( abs(stDiff.memoryUsage) > astMemoryScore[i].diffUsage &&  abs(stDiff.memoryUsage) <= astMemoryScore[i+1].diffUsage )
 			memoryScore = astMemoryScore[i].score;
	}
*/
	i = SCORE_POINT_NUM-1;
	while (i >= 0)
	{
		if( abs(stDiff.cpuUsage) > astCpuScore[i].diffUsage){
 			cpuScore = astCpuScore[i].score;
			break;
		}
		else{
			i--;
		}
	}

	i = SCORE_POINT_NUM-1;
	while (i >= 0)
	{
		if( abs(stDiff.threadUsage) > astThreadScore[i].diffUsage){
 			threadScore = astThreadScore[i].score;
			break;
		}
		else{
			i--;
		}
	}
	
	i = SCORE_POINT_NUM-1;
	while (i >= 0)
	{
		if( abs(stDiff.memoryUsage) > astMemoryScore[i].diffUsage){
 			memoryScore = astMemoryScore[i].score;
			break;
		}
		else{
			i--;
		}
	}
	
	scoreResult = cpuScore*cpuSign + threadScore*threadSign + memoryScore*memorySign;
	memmove(pScore+1, pScore, sizeof(int) * (AVG_SCORE-1));
	*pScore = scoreResult;

	for (i = 0; i < AVG_SCORE; i++)
	{
		scoreSum += *(pScore+i);
		//printf("Score[%d]=%d", i, *(pScore+i));

		//if(i == AVG_SCORE-1)
			//printf("	sum=[%d]\n", scoreSum);
	}

	avgScoreResult = (float)scoreSum/AVG_SCORE;	
	printf("Score CPU=%d, Thread=%d, Mem=%d, sum=%d, avg=%.2f\n\n", 
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

