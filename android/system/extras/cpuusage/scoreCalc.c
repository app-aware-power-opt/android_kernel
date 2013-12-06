//for aplication aware score calc
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <math.h>
#include "scoreCalc.h"

#define MEM_SCORE_POINT_NUM 11
#define FREQ_SCORE_POINT_NUM 20
#define THREAD_SCORE_POINT_NUM 10
#define AVG_SCORE 3

typedef struct {
	int cpuFreq;
	int score;
}CPU_SCORE_T;

typedef struct {
	int threadNum;
	int score;
}THREAD_SCORE_T;

typedef struct {
	int diffUsage;
	int score;
}MEMORY_SCORE_T;



CPU_SCORE_T astCpuScore[FREQ_SCORE_POINT_NUM ];

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

THREAD_SCORE_T astThreadScore[THREAD_SCORE_POINT_NUM ];

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

MEMORY_SCORE_T astMemoryScore[MEM_SCORE_POINT_NUM ];

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

int plusThreshold;
int minusThreshold;
int continueThreshold;
int plusContinueCount = 0;
int minusContinueCount = 0;
int aScoreResult[AVG_SCORE] = {};

RESOURCE_USAGE_T calcDiff(RESOURCE_USAGE_T * stPre, RESOURCE_USAGE_T * stCur);

static RESOURCE_USAGE_T preUsage = {
	.cpuUsage = 20,
	.threadUsage = 2,
	.memoryUsage = 8,
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
		"/data/cpulog/freq.txt", //CPU Usage Step data
		"/data/cpulog/freqscore.txt", //CPU Usage Score data
		"/data/cpulog/thread.txt", //CPU Thread Step data
		"/data/cpulog/threadscore.txt", //CPU Thread Score data
		"/data/cpulog/mem.txt", //Memory Usage Step data
		"/data/cpulog/memscore.txt" //Memory Usage Score data
	};

	int initValue[6][20] ={
		{200, 300, 400, 500, 600, 700, 800, 900, 1000, 1100, 1200, 1300, 1400, 1500, 1600, 1704}, //CPU Freq
		{0, 7, 13, 20, 26, 33, 40, 46, 53, 60, 68, 74, 80, 87, 93, 100}, //CPU Freq Score
		{1, 2, 3, 4, 5, 6, 7, 8, 9, 10}, //CPU Thread Number
		{0, 20, 30, 50, 70, 81, 91, 94, 97, 100}, //CPU Thread Score
		{0, 2, 4, 6, 8, 10, 12, 14, 16, 18, 20}, //Memory Usage
		{0, 10, 20, 30, 40, 50, 60, 70, 80, 90, 100} //Memory Score
		};
	int initTune[3] = { 100, -30, 1}; //0- Plus Threshold, 1-Minus Threshold 2-Continue Threshold

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


			for (j=0; j<20;j++){

				if ( j == 19)
					fprintf(fp, "%d\n", initValue[i][j]);
				else
					fprintf(fp, "%d ", initValue[i][j]);
				
				}

			printf("%s file is generated. \n", file_name[i]);
			
			fclose(fp);
		
			}
		
		}


	if ( 0 == access("/data/cpulog/tune.txt", F_OK)){
		printf( "%s file already exists.\n", "tune.txt");
		}
	
	else{
		//If there is no data text file, the file will be generated with initial value.
		fp = fopen( file_name[i], "wt");

		fp = fopen( "/data/cpulog/tune.txt", "wt");
		if(fp == NULL){
				printf("fopen w error\n");
				return 0;
				}

		for (i=0; i<3 ;i++){
			//if ( i == 2)
				fprintf(fp, "%d\n", initTune[i]);
			//else
			//	fprintf(fp, "%d", initTune[i]);
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
	fp = fopen("/data/cpulog/freq.txt", "rt");
	if(fp == NULL){
		printf("fopen open error\n");
		return 0;
		}
	
	while(fscanf(fp, "%d", &data) != EOF){
		astCpuScore[i].cpuFreq= data;
		i++;
		}
	
		i = 0;	
	fclose(fp);
	
	//usagescore.txt file has CPU Usage Score data.
	fp = fopen("/data/cpulog/freqscore.txt", "rt");
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
		astThreadScore[i].threadNum= data;
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

	//tune.txt file has Tune value data.
	fp = fopen("/data/cpulog/tune.txt", "rt");
	if(fp == NULL){
		printf("fopen open error\n");
		return 0;
		}
	
	fscanf(fp, "%d", &plusThreshold);
	fscanf(fp, "%d", &minusThreshold);
	fscanf(fp, "%d", &continueThreshold);
					
	fclose(fp);
	

	return 0;


}

SCORE_RESULT_T calcResourceScore(RESOURCE_USAGE_T *stUsage)
{
	int scoreResult = 0;
	int scoreSum = 0;
	float avgScoreResult = 0;
	int cpuFreqScore = 0;
	int cpuScore = 0;
	int threadScore = 0;
	int memoryScore =0;
	int i = 0, cpuSign = 1, threadSign = 1, memorySign = 1;
	float diff_memoryUsage;
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

	diff_memoryUsage = stUsage->memoryUsage - preUsage.memoryUsage;
	stDiff = calcDiff(&preUsage, stUsage);

	preUsage = *stUsage;

	//printf("copy Freq = %d preCPU=%d, preThread=%d, preMem=%4.2f\n", stUsage->cpuFreq, preUsage.cpuUsage, preUsage.threadUsage, preUsage.memoryUsage);

	//if(stDiff.cpuUsage < 0)	cpuSign = -1;
	//if(stDiff.threadUsage < 0)	threadSign = -1;
	//if(stDiff.memoryUsage < 0)	memorySign = -1;

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
/*
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
	*/

		i = 0;
	while (i < FREQ_SCORE_POINT_NUM)
	{
		if( stUsage->cpuFreq == astCpuScore[i].cpuFreq){
 			cpuFreqScore = astCpuScore[i].score;
			cpuScore = cpuFreqScore - stUsage->cpuUsage;
			break;
		}
		else{
			i++;
		}
	}

	i = 0;
	while (i < THREAD_SCORE_POINT_NUM)
	{
		if( stUsage->threadUsage == astThreadScore[i].threadNum){
 			threadScore = cpuFreqScore - astThreadScore[i].score;
			break;
		}
		else{
			i++;
		}
	}
	
	i = 0;
	while (i < MEM_SCORE_POINT_NUM)
	{
		if( diff_memoryUsage*10 <= astMemoryScore[i].diffUsage){
 			memoryScore = astMemoryScore[i].score /10;
			break;
		}
		else{
			i++;
		}
	}
	
	
	scoreResult = cpuScore*cpuSign + threadScore*threadSign + memoryScore*memorySign;
	//memmove(pScore+1, pScore, sizeof(int) * (AVG_SCORE-1));
	//*pScore = scoreResult;

	/*
	for (i = 0; i < AVG_SCORE; i++)
	{
		scoreSum += *(pScore+i);
		//printf("Score[%d]=%d", i, *(pScore+i));

		//if(i == AVG_SCORE-1)
			//printf("	sum=[%d]\n", scoreSum);
	}

	avgScoreResult = (float)scoreSum/AVG_SCORE;	
	*/
	
	//printf("Score CPU=%d, Thread=%d, Mem=%d, sum=%d, avg=%.2f\n\n", 
		//cpuScore*cpuSign, threadScore*threadSign, memoryScore*memorySign, scoreResult, avgScoreResult);

		printf("Score Freq = %d CPU=%d, Thread=%d, Mem=%d, sum=%d, \n\n", 
		cpuFreqScore, cpuScore, threadScore, memoryScore, scoreResult);

/*
	for (i = 0; i < AVG_SCORE; i++)
	{
		printf("pointer Score[%d]=%d\n ", i, *(pScore+i));
	}
*/
	stResult.score = scoreResult;
	stResult.avgScore = avgScoreResult;

	if (stResult.score > plusThreshold){

			minusContinueCount = 0;
			if ( plusContinueCount > continueThreshold)
				stResult.finalDecision = 1;
			else 
				plusContinueCount++;
			
			}
	else if (stResult.score < minusThreshold){

			plusContinueCount = 0;
			if ( minusContinueCount > continueThreshold)
				stResult.finalDecision = -1;
			else 
				minusContinueCount++;
			
			}
	else	{
			plusContinueCount = 0;
			minusContinueCount = 0;
			stResult.finalDecision = 0;
		}

	return stResult;

};

RESOURCE_USAGE_T calcDiff(RESOURCE_USAGE_T * stPre, RESOURCE_USAGE_T * stCur)
{
	RESOURCE_USAGE_T stDiff;
	stDiff.cpuUsage = stCur->cpuUsage - stPre->cpuUsage;
	stDiff.threadUsage = stCur->threadUsage - stPre->threadUsage;
	stDiff.memoryUsage = stCur->memoryUsage - stPre->memoryUsage;

	printf("curCPU=%d, curThread=%d, curMem=%4.2f\n", stCur->cpuUsage, stCur->threadUsage, stCur->memoryUsage);
	//printf("preCPU=%d, preThread=%d, preMem=%4.2f\n", stPre->cpuUsage, stPre->threadUsage, stPre->memoryUsage);
	//printf("diffCPU=%d, diffThread=%d, diffMem=%4.2f\n", stDiff.cpuUsage, stDiff.threadUsage, stDiff.memoryUsage);

	
	return stDiff;
};

