typedef struct
{
	int cpuUsage;
	int threadUsage;
	float memoryUsage;
	int cpuFreq;
}RESOURCE_USAGE_T;

typedef struct {
	int score;
	float avgScore;
	int finalDecision;
	int targetFreq;
}SCORE_RESULT_T;

SCORE_RESULT_T calcResourceScore(RESOURCE_USAGE_T *stUsage);

int loadUsageScoreValue(void);

		
