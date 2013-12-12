//for aplication aware score calc
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <math.h>
#include "scoreCalc.h"
#include "cpufreq.h"

#define MEM_SCORE_POINT_NUM 20
#define FREQ_SCORE_POINT_NUM 20
#define THREAD_SCORE_POINT_NUM 20
#define AVG_SCORE 3

int calcTargetFreq(int score);

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
int pseONOFF = 0;
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
		if ( 0 == access("/data/cpulog/pseON.txt", F_OK)){
		printf( "%s file already exists.\n", "tune.txt");
		}
	
	else{
	
		//If there is no data text file, the file will be generated with initial value.
		fp = fopen( file_name[i], "wt");

		fp = fopen( "/data/cpulog/pseON.txt", "wt");
		if(fp == NULL){
				printf("fopen w error\n");
				return 0;
				}

				fprintf(fp, "%d\n", 1);
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
	//pseON.txt file has Tune value data.
	// pseON value 1 is PSE & FCB is ON
	// pseON value 0 is PSE & FCB is OFF
	fp = fopen("/data/cpulog/pseON.txt", "rt");
	if(fp == NULL){
		printf("fopen open error\n");
		return 0;
		}
	
	fscanf(fp, "%d", &pseONOFF);
					
	fclose(fp);

	

	return 0;


}


#define CPUUSAGE_UP_THRESHOLD 20
#define CPUUSAGE_MIN_UP_THRESHOLD 50
#define CPUUSAGE_MIN_DOWN_THRESHOLD 30
#define CPUUSAGE_DOWN_THRESHOLD -20
#define CPUUSAGE_DOWN_THRESHOLD_LIMIT 30

int getCPUFreqIndexFromTbl(int freq)
{
	int i = 0;
	while (i < FREQ_SCORE_POINT_NUM)
	{
		if(freq == astCpuScore[i].cpuFreq){
			break;
		}
		else{
			i++;
		}
	}

	return i;
}

int getCPUFreqFromTbl(int index)
{
	return astCpuScore[index].cpuFreq;
}

unsigned int checkCPUUsageCond(int cpuUsage, int cpuFreq, RESOURCE_USAGE_T stDiff)
{
	unsigned int freq = 0;
	int freqindex = -1, nextfreqindex = -1;
	float freqadj = 0;
	static int maxfreqindex = -1, minfreqindex = -1;

	if(maxfreqindex == -1)
		maxfreqindex = getCPUFreqIndexFromTbl(CPUFREQ_MAX/1000);
	if(minfreqindex == -1)
		minfreqindex = getCPUFreqIndexFromTbl(CPUFREQ_MIN/1000);

	if(stDiff.cpuUsage >= CPUUSAGE_UP_THRESHOLD)
	{
		// if the usage is over up threshold, scale up the freq
		freqindex = getCPUFreqIndexFromTbl(cpuFreq);
		freqadj = (maxfreqindex - freqindex +1)*((float)cpuUsage/100) + (freqindex - 1);
		if((int)freqadj < freqindex)
		{
			freqadj = freqindex;
		}
		else if((int)freqadj == freqindex)
		{
			if((int)freqadj < maxfreqindex)
			{
				freqadj = freqadj + 1;
			}
		}
				
		nextfreqindex = (int)freqadj;
		
		freq = getCPUFreqFromTbl(nextfreqindex)*1000;
		printf("CPU Usage : %d(%d), need to scale up from %d to %d\n", cpuUsage, stDiff.cpuUsage, cpuFreq, freq/1000);
	}
	else if((stDiff.cpuUsage <= CPUUSAGE_DOWN_THRESHOLD) && (cpuUsage <= CPUUSAGE_DOWN_THRESHOLD_LIMIT))
	{
		// if the usage is under down threshold, scale down the freq
		freqindex = getCPUFreqIndexFromTbl(cpuFreq);
		freqadj = (freqindex - minfreqindex +1)*((float)cpuUsage/100);
		//freqadj = (maxfreqindex - minfreqindex +1)*((float)cpuUsage/100);
		if(freqadj <= minfreqindex)
			freqadj = minfreqindex;
		nextfreqindex = (int)freqadj;
		
		freq = getCPUFreqFromTbl(nextfreqindex)*1000;
		printf("CPU Usage : %d(%d), need to scale down from %d to %d\n", cpuUsage, stDiff.cpuUsage, cpuFreq, freq/1000);
	}
	else if(cpuUsage < CPUUSAGE_MIN_DOWN_THRESHOLD)
	{
		// if the usage is below minimum down threshold, step down
		if(cpuFreq == (CPUFREQ_MIN/1000))
		{
			freq = 0;
			printf("CPU Usage : %d, need to step down but freq is already min %d\n", cpuUsage, cpuFreq);
		}
		else
		{
			freq = get_prev_freq((unsigned int)cpuFreq*1000);
			printf("CPU Usage : %d, need to step down from %d to %d\n", cpuUsage, cpuFreq, freq/1000);
		}
	}
	else if(cpuUsage > CPUUSAGE_MIN_UP_THRESHOLD)
	{
		// if the usage is over minimum up threshold, step up
		if(cpuFreq == (CPUFREQ_MAX/1000))
		{
			freq = 0;
			printf("CPU Usage : %d, need to step up but freq is already max %d\n", cpuUsage, cpuFreq);
		}
		else
		{
			freq = get_next_freq((unsigned int)cpuFreq*1000);
			printf("CPU Usage : %d, need to step up from %d to %d\n", cpuUsage, cpuFreq, freq/1000);
		}
	}
	else
	{
		freq = 0;
	}

	return freq;
}


#define THREAD_UP_THRESHOLD 2
#define THREAD_DOWN_THRESHOLD -2

int checkThreadCond(RESOURCE_USAGE_T stDiff)
{
	int threadcond = 0;
	
	if(stDiff.threadUsage >= THREAD_UP_THRESHOLD)
	{
		threadcond = 1;
	}
	else if(stDiff.threadUsage <= THREAD_DOWN_THRESHOLD)
	{
		threadcond = -1;
	}
	else
	{
		threadcond = 0;
	}

	return threadcond;
}

#define MEM_UP_THRESHOLD 0.2
#define MEM_DOWN_THRESHOLD -0.2

int checkMemCond(RESOURCE_USAGE_T stDiff)
{
	int memcond = 0;
	
	if(stDiff.memoryUsage >= MEM_UP_THRESHOLD)
	{
		memcond = 1;
	}
	else if(stDiff.memoryUsage <= MEM_DOWN_THRESHOLD)
	{
		memcond = -1;
	}
	else
	{
		memcond = 0;
	}

	return memcond;
}

/*
 * result
 *  1 : step up from the current frequency
 * -1 : step down from the current frequency
 *  2 : scale up from the given frequency
 * -2 : scale down from the given frequency
 *  3 : set to the given frequency
 *  0 : no need to change
 */

#define ADJ_NO_NEED 0
#define ADJ_STEP_UP 1
#define ADJ_STEP_DOWN -1
#define ADJ_SCALE_UP 2
#define ADJ_SCALE_DOWN -2
#define ADJ_FREQ_ONLY 3

int adjustCond(int cpuFreq, unsigned int *freq, int threadcond, int memcond)
{
	int condpoint = 0;
	unsigned int adjfreq = 0;
	int result = ADJ_NO_NEED;

	condpoint = threadcond + memcond;

	if((*freq) == 0)
	{
		if(condpoint > 0)
		{
			if(cpuFreq*1000 == CPUFREQ_MAX)
			{
				result = ADJ_NO_NEED; // no need to change
				printf("freq %d is max, no need to step up\n", cpuFreq);
			}
			else
			{
				adjfreq = (unsigned int)cpuFreq*1000;
				while(condpoint > 0)
				{
					adjfreq = get_next_freq(adjfreq);
					condpoint--;
				}
				*freq = adjfreq;
				printf("step up from %d to %d\n", cpuFreq, (*freq)/1000);
				result = ADJ_STEP_UP; // step up
			}
			
		}
		else if(condpoint < 0)
		{
			if(cpuFreq*1000  == CPUFREQ_MIN)
			{
				result = ADJ_NO_NEED; // no need to change
				printf("freq %d is min, no need to step down\n", cpuFreq);
			}
			else
			{
				adjfreq = (unsigned int)cpuFreq*1000;
				while(condpoint < 0)
				{
					adjfreq = get_prev_freq(adjfreq);
					condpoint++;
				}
				*freq = adjfreq;
				printf("step down from %d to %d\n", cpuFreq, (*freq)/1000);
				result = ADJ_STEP_DOWN; // step down
			}
		}
		else
		{
			result = ADJ_NO_NEED; // no need to change
		}
	}
	else
	{
		if(condpoint > 0)
		{
			if(*freq == CPUFREQ_MAX)
			{
				result = ADJ_NO_NEED; // no need to change
				printf("freq %d is max, no need to scale up\n", *freq);
			}
			else
			{
				adjfreq = *freq;
				//printf("scale up adjfreq start %d\n", adjfreq);
				while(condpoint > 0)
				{
					adjfreq = get_next_freq(adjfreq);
					condpoint--;
					//printf("scale up adjfreq %d\n", adjfreq);
				}
				//printf("scale up from %d to %d\n", (*freq)/1000, adjfreq/1000);
				*freq = adjfreq;
				result = ADJ_SCALE_UP; // scale up
			}
		}
		else if(condpoint < 0)
		{
			if(*freq  == CPUFREQ_MIN)
			{
				result = ADJ_NO_NEED; // no need to change
				printf("freq %d is min, no need to scale down\n", *freq);
			}
			else
			{
				adjfreq = *freq;
				//printf("scale down adjfreq start %d\n", adjfreq);
				while(condpoint < 0)
				{
					adjfreq = get_prev_freq(adjfreq);
					condpoint++;
					//printf("scale down adjfreq %d\n", adjfreq);
				}
				//printf("scale down from %d to %d\n", (*freq)/1000, adjfreq/1000);
				*freq = adjfreq;
				result = ADJ_SCALE_DOWN; // scale down
			}
		}
		else
		{
			result = ADJ_FREQ_ONLY; // no need to step up or down, use the given frequency
		}
	}

	return result;
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
	SCORE_RESULT_T stResult = {0, };

	static int * pScore = NULL;
	static int fInitlBuffer = 1;
	static int cpufreq_changed = 0;

	unsigned int freq = 0;
	int threadcond = 0, memcond = 0, adjcond = 0;

	
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


	freq = checkCPUUsageCond(stUsage->cpuUsage, stUsage->cpuFreq, stDiff);
	if(freq != 0)
		printf("freq need to be changed from %d to %d\n", stUsage->cpuFreq, freq/1000);

	threadcond = checkThreadCond(stDiff);
	memcond = checkMemCond(stDiff);

	// if frequency is needed to be changed, do overweight only in case the direction is same
	if(freq != 0)
	{
		if((stDiff.cpuUsage < 0) && (threadcond + memcond < 0))
		{
			adjcond = adjustCond(stUsage->cpuFreq, &freq, threadcond, memcond);
		}
		else if((stDiff.cpuUsage > 0) && (threadcond + memcond > 0))
		{
			adjcond = adjustCond(stUsage->cpuFreq, &freq, threadcond, memcond);
		}
		else
		{
			adjcond = ADJ_FREQ_ONLY;
		}
	}
	else
	{
		adjcond = adjustCond(stUsage->cpuFreq, &freq, threadcond, memcond);
	}

	if((pseONOFF == 1) && (adjcond != 0))
	{
		printf("[DBG ADDJUST] Cur : %d, Next : %d, CPU usage : %d(%d), T : %d, M : %d, A: %d\n", stUsage->cpuFreq, freq/1000, stUsage->cpuUsage, stDiff.cpuUsage, threadcond, memcond, adjcond);
		set_cpufreq_to_value(freq);
	}
	stResult.finalDecision = adjcond;

	/*	
	if (stResult.score > plusThreshold){

			minusContinueCount = 0;
			if ( plusContinueCount >= continueThreshold)
			{
				stResult.finalDecision = 1;
				//set_cpufreq_to_min();
				if (pseONOFF == 1) //pseONOFF value can be read from pseON.txt default 1
					//set_cpufreq_to_prev_step();
					stResult.targetFreq = calcTargetFreq(stResult.score);
			}
			else 
				plusContinueCount++;
			}
	
	else if (stResult.score < minusThreshold){

			plusContinueCount = 0;
			
			if ( minusContinueCount > continueThreshold)
			{
				stResult.finalDecision = -1;
				//set_cpufreq_to_max();
				if (pseONOFF == 1)
					//set_cpufreq_to_next_step();
					stResult.targetFreq = calcTargetFreq(stResult.score);
			}
			else 
				minusContinueCount++;
			
			}
	
	else{
			if( (plusContinueCount != 0) || (minusContinueCount !=0)){

				if (plusContinueCount > 0){
						plusContinueCount = 0;
						stResult.finalDecision = 2;
					}
				else if (minusContinueCount > 0){
						minusContinueCount = 0;
						stResult.finalDecision = -2;
					}
				else
						stResult.finalDecision = 0;


				if (pseONOFF == 1){
					printf("Come back to ONDEMAND \n");
					//set_scaling_governor(G_ONDEMAND);
					}
									
				}
			
			else
				stResult.finalDecision = 0;
		}
		*/

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


int calcTargetFreq(int score)
{
	int curFreq = 0;
	int variationFreq = 0;
	int targetFreq = 0;

	curFreq = read_scaling_cur_freq();
	variationFreq = ((float)score/210) * curFreq;
	targetFreq = curFreq -variationFreq;
	
	set_cpufreq_to_value(targetFreq);

	printf("===> [curFreq=%d], [variationFreq=%d], [targetFreq=%d]\n", curFreq, variationFreq, targetFreq);
	printf("=====================================\n");

	return targetFreq;

}

