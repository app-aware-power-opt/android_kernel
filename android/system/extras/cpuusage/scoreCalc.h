typedef struct
{
	float cpuUsage;
	int threadUsage;
	float memoryUsage;
}RESOURCE_USAGE_T;

typedef struct {
	int score;
	float avgScore;
}SCORE_RESULT_T;

SCORE_RESULT_T calcResourceScore(RESOURCE_USAGE_T *stUsage);

int loadUsageScoreValue(void);

		
