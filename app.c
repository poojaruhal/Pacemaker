/*
The project "Implantable Heart Device for Heartrate Correction - PACEMAKER".
The program simulates the working of a pacemaker.

Submitted by:
Pallavi Panchal	2015H112186P
Pooja Ruhal		2015H112171P
Vishakha Gupta	2015H112166P
*/


#include <includes.h>
#include <stdlib.h>

// Shared array
int Sensor_Data[6] = { 2.2, 2.2, 0, 0, 0, 0 };
OS_MUTEX  sensor_mutex;									// Used to get exclusive access to shared variable.
char *mutex = "sensor mutex";
//int heartbeatHigh = 100;
//int hearbeatLow = 50;
int atriumRateHigh = 400;
int atriumRateLow = 60;
int ventricleRateHigh = 100;
int ventricleRateLow = 60;
int atriumRate = 230;	// initial atriumRate
int ventricleRate = 60; // initial ventricleRate
int bodyActivity = 0;   // initial bodyActivity

static  OS_TCB   AppTaskStartTCB;
static  CPU_STK  AppTaskStartStk[APP_TASK_START_STK_SIZE];

/*
Tasks required to simulate a Pacemaker
*/

// T1: contraction of atrium
static OS_TCB AtriumBeatTCB;
static OS_TCB AtriumBeatStk[APP_TASK_START_STK_SIZE];

// T2: contraction of ventricule
static OS_TCB VentricleBeatTCB;
static OS_TCB VentricleBeatStk[APP_TASK_START_STK_SIZE];

// T3: body activity generator
static OS_TCB BodyActivityTCB;
static OS_TCB BodyActivityStk[APP_TASK_START_STK_SIZE];

// T4: sense heartbeat
static OS_TCB SenseHeartbeatTCB;
static OS_TCB SenseHeartbeatStk[APP_TASK_START_STK_SIZE];

// T5: sense body activity
static OS_TCB SenseBodyActivityTCB;
static OS_TCB SenseBodyActivityStk[APP_TASK_START_STK_SIZE];

// T6: send electric pulses
static OS_TCB SendElectricPulseTCB;
static OS_TCB SendElectricPulseStk[APP_TASK_START_STK_SIZE];

static void AppTaskStart(void  *p_arg);
static void AtriumBeat(void  *p_err);
static void VentricleBeat(void  *p_err);
static void BodyActivity(void  *p_err);
static void SenseHeartbeat(void  *p_err);
static void SenseBodyActivity(void  *p_err);
static void SendElectricPulse(void  *p_err);

int  main(void)
{
	OS_ERR  err;
	OSInit(&err);                                               // Init uC/OS-III.                                      
	OSTaskCreate((OS_TCB     *)&AppTaskStartTCB,                // This task creates all other pacemaker tasks
		(CPU_CHAR   *)"App Task Start",
		(OS_TASK_PTR)AppTaskStart,
		(void       *)0,
		(OS_PRIO)2,
		(CPU_STK    *)&AppTaskStartStk[0],
		(CPU_STK_SIZE)APP_TASK_START_STK_SIZE / 10u,
		(CPU_STK_SIZE)APP_TASK_START_STK_SIZE,
		(OS_MSG_QTY)0u,
		(OS_TICK)0u,
		(void       *)0,
		(OS_OPT)(OS_OPT_TASK_STK_CHK | OS_OPT_TASK_STK_CLR),
		(OS_ERR     *)&err);
	OSMutexCreate(&sensor_mutex, &mutex, &err);                   // Mutex for sensor is created
	OSStart(&err);                                                // Start multitasking (i.e. give control to uC/OS-III)
}

static  void  AppTaskStart(void *p_arg)							  //initializes all tasks
{
	OS_ERR  err;
	(void)p_arg;
	BSP_Init();                                                   // Initialize BSP functions 
	CPU_Init();                                                   // Initialize uC/CPU services

#if OS_CFG_STAT_TASK_EN > 0u
	OSStatTaskCPUUsageInit(&err);                                 
#endif

	APP_TRACE_DBG(("uCOS-III is Running...\n\r"));

	// Task 1
	OSTaskCreate((OS_TCB     *)&AtriumBeatTCB,					  // Create task to generate atrium contraction                            
		(CPU_CHAR   *)"Artium Starts Contraction",
		(OS_TASK_PTR)AtriumBeat,
		(void       *)0,
		(OS_PRIO)3,                                               // Priority is 3. Priotity 1 and 0 are reserved for internal tasks of the OS
		(CPU_STK    *)&AtriumBeatStk[0],
		(CPU_STK_SIZE)APP_TASK_START_STK_SIZE / 10u,
		(CPU_STK_SIZE)APP_TASK_START_STK_SIZE,
		(OS_MSG_QTY)0u,
		(OS_TICK)0u,
		(void       *)0,
		(OS_OPT)(OS_OPT_TASK_STK_CHK | OS_OPT_TASK_STK_CLR),
		(OS_ERR     *)&err);

	// Task 2
	OSTaskCreate((OS_TCB     *)&VentricleBeatTCB,                 // Creates task to generate ventricle contraction                           
		(CPU_CHAR   *)"Ventricle Starts Contraction",
		(OS_TASK_PTR)VentricleBeat,
		(void       *)0,
		(OS_PRIO)3,
		(CPU_STK    *)&VentricleBeatStk[0],
		(CPU_STK_SIZE)APP_TASK_START_STK_SIZE / 10u,
		(CPU_STK_SIZE)APP_TASK_START_STK_SIZE,
		(OS_MSG_QTY)0u,
		(OS_TICK)0u,
		(void       *)0,
		(OS_OPT)(OS_OPT_TASK_STK_CHK | OS_OPT_TASK_STK_CLR),
		(OS_ERR     *)&err);

	// Task 3
	OSTaskCreate((OS_TCB     *)&BodyActivityTCB,                 // Creates task to generate body activity                           
		(CPU_CHAR   *)"Body activity generation",
		(OS_TASK_PTR)BodyActivity,
		(void       *)0,
		(OS_PRIO)3,
		(CPU_STK    *)&BodyActivityStk[0],
		(CPU_STK_SIZE)APP_TASK_START_STK_SIZE / 10u,
		(CPU_STK_SIZE)APP_TASK_START_STK_SIZE,
		(OS_MSG_QTY)0u,
		(OS_TICK)0u,
		(void       *)0,
		(OS_OPT)(OS_OPT_TASK_STK_CHK | OS_OPT_TASK_STK_CLR),
		(OS_ERR     *)&err);

	// Task 4
	OSTaskCreate((OS_TCB     *)&SenseHeartbeatTCB,                // Create task to sense heartbeat                       
		(CPU_CHAR   *)"Device senses heartbeat",
		(OS_TASK_PTR)SenseHeartbeat,
		(void       *)0,
		(OS_PRIO)3,
		(CPU_STK    *)&SenseHeartbeatStk[0],
		(CPU_STK_SIZE)APP_TASK_START_STK_SIZE / 10u,
		(CPU_STK_SIZE)APP_TASK_START_STK_SIZE,
		(OS_MSG_QTY)0u,
		(OS_TICK)0u,
		(void       *)0,
		(OS_OPT)(OS_OPT_TASK_STK_CHK | OS_OPT_TASK_STK_CLR),
		(OS_ERR     *)&err);

	// Task 5
	OSTaskCreate((OS_TCB     *)&SenseBodyActivityTCB,				// Create task to sense body activity
		(CPU_CHAR   *)"Device sense body activity",
		(OS_TASK_PTR)SenseBodyActivity,
		(void       *)0,
		(OS_PRIO)3,
		(CPU_STK    *)&SenseBodyActivityStk[0],
		(CPU_STK_SIZE)APP_TASK_START_STK_SIZE / 10u,
		(CPU_STK_SIZE)APP_TASK_START_STK_SIZE,
		(OS_MSG_QTY)0u,
		(OS_TICK)0u,
		(void       *)0,
		(OS_OPT)(OS_OPT_TASK_STK_CHK | OS_OPT_TASK_STK_CLR),
		(OS_ERR     *)&err);
	
	// Task 6
	OSTaskCreate((OS_TCB     *)&SendElectricPulseTCB,				// Create task to send electric inpulse
		(CPU_CHAR   *)"Pacemaker sends electric pulses",
		(OS_TASK_PTR)SendElectricPulse,
		(void       *)0,
		(OS_PRIO)2,
		(CPU_STK    *)&SendElectricPulseStk[0],
		(CPU_STK_SIZE)APP_TASK_START_STK_SIZE / 10u,
		(CPU_STK_SIZE)APP_TASK_START_STK_SIZE,
		(OS_MSG_QTY)0u,
		(OS_TICK)0u,
		(void       *)0,
		(OS_OPT)(OS_OPT_TASK_STK_CHK | OS_OPT_TASK_STK_CLR),
		(OS_ERR     *)&err);
}

static  int generate_radom_val(int min, int max)					// Function to generate random value
{
	int r;
	int range = 1 + max - min;
	int bin = RAND_MAX / range;
	int lim = bin * range;
	do{
		r = rand();
	} while (r >= lim);

	return min + (r / bin);
}

static void AtriumBeat(void *p_err)
{
	OS_ERR err;
	(void)p_err;													// p_err is the pointer to the error code returned by this function
	BSP_Init();														// Initialize BSP
	CPU_Init();														// Initialize CPU
	CPU_TS  ts;														// timestamp at which message was recieved
	

#if OS_CFG_STAT_TASK_EN > 0u
	OSStatTaskCPUUsageInit(&err);									// Compute CPU capacity with no task running
#endif

	APP_TRACE_DBG(("AtriumBeat task initiated.\n\r"));
	while (DEF_ON) {
		OSTimeDlyHMSM(0, 0, 2, 0, OS_OPT_TIME_DLY, &err);			// Infinite loop with a period of 2 seconds

		OSMutexPend(&sensor_mutex,									// Lock acquired, if available else blocked
			0,
			OS_OPT_PEND_BLOCKING,
			&ts,
			&err);

		printf("Lock Acquired. Current atrium contraction.\n");
		OSTimeDlyHMSM(0, 0, 2, 0, OS_OPT_TIME_DLY, &err);
		Sensor_Data[0] = generate_radom_val(20,700);
		printf("Atrium beat is %d\n", Sensor_Data[0]);

		OSMutexPost(&sensor_mutex,                                  // Lock released, on task completion
			OS_OPT_POST_NONE,
			&err);
		printf("Lock Released.\n\n");
		printf("****************************************\n");
	}
}

static void VentricleBeat(void *p_err)
{
	OS_ERR err;
	(void)p_err;													// p_err is the pointer to the error code returned by this function
	BSP_Init();														// Initialize BSP
	CPU_Init();														// Initialize CPU
	CPU_TS  ts;										
	

#if OS_CFG_STAT_TASK_EN > 0u
	OSStatTaskCPUUsageInit(&err);									// Compute CPU capacity with no task running
#endif

	APP_TRACE_DBG(("VentricleBeat task initiated.\n\r"));
	while (DEF_ON) {												// Infinite loop with a period of 2 seconds
		OSTimeDlyHMSM(0, 0, 2, 0, OS_OPT_TIME_DLY, &err);           // Time delay

		OSMutexPend(&sensor_mutex,                                  // Lock acquired, if available else blocked
			0,
			OS_OPT_PEND_BLOCKING,
			&ts,
			&err);

		printf("Lock Acquired. Current ventricle contraction.\n");
		OSTimeDlyHMSM(0, 0, 2, 0, OS_OPT_TIME_DLY, &err);			// Time delay
		Sensor_Data[1] = generate_radom_val(20, 150);
		printf("ventricle beat is %d\n", Sensor_Data[1]);

		OSMutexPost(&sensor_mutex,                                  // Lock released, on task completion
			OS_OPT_POST_NONE,
			&err);
		printf("Lock Released.\n\n");
		printf("****************************************\n");
	}
}

static void BodyActivity(void *p_err)
{
	OS_ERR err;
	(void)p_err;													// p_err is the pointer to the error code returned by this function
	BSP_Init();														// Initialize BSP
	CPU_Init();														// Initialize CPU
	CPU_TS  ts;
	

#if OS_CFG_STAT_TASK_EN > 0u
	OSStatTaskCPUUsageInit(&err);									// Compute CPU capacity with no task running
#endif

	APP_TRACE_DBG(("BodyActivity task initiated.\n\r"));
	while (DEF_ON) {												// Infinite loop with a period of 2 seconds
		OSTimeDlyHMSM(0, 0, 2, 0, OS_OPT_TIME_DLY, &err);           // Time Delay

		OSMutexPend(&sensor_mutex,                                  // Lock acquired, if available else blocked
			0,
			OS_OPT_PEND_BLOCKING,
			&ts,
			&err);

		printf("Lock Acquired. Current Body Activity.\n");
		OSTimeDlyHMSM(0, 0, 2, 0, OS_OPT_TIME_DLY, &err);			// Time Delay
		Sensor_Data[2] = generate_radom_val(0, 10);
		printf("Body Activity is %d\n", Sensor_Data[2]);

		OSMutexPost(&sensor_mutex,                                  // Lock released, on task completion
			OS_OPT_POST_NONE,
			&err);
		printf("Lock released.\n\n");
		printf("****************************************\n");
	}
}

static void SenseHeartbeat(void *p_err)
{
	OS_ERR err;
	(void)p_err;													// p_err is the pointer to the error code returned by this function
	BSP_Init();														// Initialize BSP
	CPU_Init();														// Initialize CPU
	int i = 0;
	CPU_TS ts;
	int temp = 1;

#if OS_CFG_STAT_TASK_EN > 0u
	OSStatTaskCPUUsageInit(&err);									// Compute CPU capacity with no task running
#endif

	APP_TRACE_DBG(("SenseHeartbeat task initiated.\n\r"));
	while (DEF_ON) {												// Infinite loop with a period of 2 seconds
		OSTimeDlyHMSM(0, 0, 2, 0, OS_OPT_TIME_DLY, &err);			// Time delay

		OSMutexPend(&sensor_mutex,									// Lock acquired, if available else blocked
			0,
			OS_OPT_PEND_BLOCKING,
			&ts,
			&err);

		printf("Lock Acquired. Sensing heartbeat.\n");
		OSTimeDlyHMSM(0, 0, 2, 0, OS_OPT_TIME_DLY, &err);			// Time delay
		atriumRate = Sensor_Data[0];
		ventricleRate = Sensor_Data[1];

		if (atriumRate >= atriumRateLow && atriumRate <= atriumRateHigh){				// Atrium rate is in range [60, 400]
			printf("Atrium rate is under control: %d. \n", atriumRate);
		}
		OSTimeDlyHMSM(0, 0, 2, 0, OS_OPT_TIME_DLY, &err);								// Time delay
		if (ventricleRate >= ventricleRateLow && ventricleRate <= ventricleRateHigh) {  // Ventricle rate is in range [60, 100]
			printf("Ventricle rate is under control: %d. \n", ventricleRate);
		}
		OSTimeDlyHMSM(0, 0, 2, 0, OS_OPT_TIME_DLY, &err);								// Time delay
		if (atriumRate < atriumRateLow) {
			printf("Atrium rate is low.\n");
			OSTaskQPost(&SendElectricPulseTCB,											// Calling emergency task, by posting message to queue
				(void *)&temp,
				sizeof(temp),
				OS_OPT_POST_FIFO,
				&err);
		}
		else if (ventricleRate < ventricleRateLow) {
			printf("Ventricle rate is low.\n");
			OSTaskQPost(&SendElectricPulseTCB,											// Calling emergency task, by posting message to queue
				(void *)&temp,
				sizeof(temp),
				OS_OPT_POST_FIFO,
				&err);
		}
		OSTimeDlyHMSM(0, 0, 2, 0, OS_OPT_TIME_DLY, &err);								// Time delay
		if(atriumRate > atriumRateHigh){
			printf("Atrium rate is high. \n checking body activity.\n");
			printf("Sensing Body Activity.\n");
			OSTimeDlyHMSM(0, 0, 1, 0, OS_OPT_TIME_DLY, &err);							// Time delay
			Sensor_Data[3] = 1;
			if (Sensor_Data[5] == 1) {
				printf("Abnormal body activity! \nCalling electric pulse generator.\n");
				OSTaskQPost(&SendElectricPulseTCB,										// Calling emergency task, by posting message to queue
					(void *)&temp,
					sizeof(temp),
					OS_OPT_POST_FIFO,
					&err);
			}
			else {
				printf("Condition normal\n");
			}
		}
		else if (ventricleRate > ventricleRateHigh) {
			printf("Ventricle rate is high \n checking body activity\n");
			printf("Sensing Body Activity\n");
			OSTimeDlyHMSM(0, 0, 1, 0, OS_OPT_TIME_DLY, &err);							// Time delay
			Sensor_Data[3] = 1;
			if (Sensor_Data[5] == 1) {
				printf("Abnormal body activity! \nCalling electric pulse generator\n");
				OSTaskQPost(&SendElectricPulseTCB,										// Calling emergency task, by posting message to queue
					(void *)&temp,
					sizeof(temp),
					OS_OPT_POST_FIFO,
					&err);
			}
			else {
				printf("Condition normal\n");
			}
		}
		
		OSMutexPost(&sensor_mutex,														// Lock released, on task completion
			OS_OPT_POST_NONE,
			&err);
		printf("Lock released\n\n");
		printf("****************************************\n");
	}
}

static void SenseBodyActivity(void *p_err)
{
	OS_ERR err;
	(void)p_err;																			// p_err is the pointer to the error code returned by this function
	BSP_Init();																				// Initialize BSP
	CPU_Init();																				// Initialize CPU
	int i = 0;
	CPU_TS ts;
	int temp = 1;
	
#if OS_CFG_STAT_TASK_EN > 0u
	OSStatTaskCPUUsageInit(&err);															// Compute CPU capacity with no task running
#endif

	APP_TRACE_DBG(("SenseBodyActivity task initiated\n\r"));
	while (DEF_ON) {																		// Infinite loop with a period of 2 seconds
		OSTimeDlyHMSM(0, 0, 2, 0, OS_OPT_TIME_DLY, &err);									// Time delay
		
		OSMutexPend(&sensor_mutex,															// Lock acquired, if available else blocked
			0,
			OS_OPT_PEND_BLOCKING,
			&ts,
			&err);
		
		printf("Lock acquired. Sensing body activity\n");
		OSTimeDlyHMSM(0, 0, 2, 0, OS_OPT_TIME_DLY, &err);									// Time delay
		bodyActivity = Sensor_Data[2];
		if (bodyActivity >= 0 && bodyActivity <= 3) {
			printf("Person is at rest.\n");
			Sensor_Data[5] = 1;
			OSTimeDlyHMSM(0, 0, 2, 0, OS_OPT_TIME_DLY, &err);								// Time delay
			if (Sensor_Data[3] == 1 || Sensor_Data[4] == 1) {
				printf("Abnormal body activity! \nCalling electric pulse generator\n");
				OSTaskQPost(&SendElectricPulseTCB,											// Calling emergency task, by posting message to queue
					(void *)&temp,
					sizeof(temp),
					OS_OPT_POST_FIFO,
					&err);
			}
			else {
				printf("Condition normal\n");
			}
		}
		else if (bodyActivity > 3 && bodyActivity <= 5) {
			printf("Person is Walking.\n Condition normal \n");
		}
		else{
			printf("Person is Running.\n Condition normal \n");
		}

		OSMutexPost(&sensor_mutex,														// Lock released, on task completion
			OS_OPT_POST_NONE,
			&err);

		printf("Lock released\n\n");
		printf("****************************************\n");
	}
}

static void SendElectricPulse(void *p_err)
{
	OS_ERR err;
	void *p_msg;
	OS_MSG_SIZE msg_size;
	(void)p_err;																			// p_err is the pointer to the error code returned by this function
	BSP_Init();																				// Initialize BSP
	CPU_Init();																				// Initialize CPU
	CPU_TS ts;
	int temp = 1;

#if OS_CFG_STAT_TASK_EN > 0u
	OSStatTaskCPUUsageInit(&err);															// Compute CPU capacity with no task running
#endif

	APP_TRACE_DBG(("SendElectricPulse task initiated\n\r"));
	while (DEF_ON) {
		p_msg = OSTaskQPend(0,	OS_OPT_PEND_BLOCKING, 	&msg_size, 	&ts, 	&err);
		if (*(int*)p_msg == 1) {
			OSTimeDlyHMSM(0, 0, 2, 0, OS_OPT_TIME_DLY, &err);								// Time delay
			printf("Sending electric pulse\n");
			if (atriumRate > atriumRateHigh) {
				int atriumRateTemp1 = ((atriumRate - atriumRateLow) / (atriumRateHigh - atriumRateLow))*(atriumRateHigh - atriumRateLow) + atriumRateLow;
				printf("Atrium rate reduced to: %d\n", atriumRateTemp1);
			}
			else if (atriumRate < atriumRateLow) {
				int atriumRateTemp2 = ((atriumRate - atriumRateLow) / (atriumRateHigh - atriumRateLow))*(atriumRateHigh - atriumRateLow) + atriumRateLow;
				printf("Atrium rate increased to: %d\n", atriumRateTemp2);
			}
			if (ventricleRate > ventricleRateHigh) {
				int ventricleRateTemp1 = ((ventricleRate - ventricleRateLow) / (ventricleRateHigh - ventricleRateLow))*(ventricleRateHigh - ventricleRateLow) + ventricleRateLow;
				printf("Ventricle rate reduced to: %d\n", ventricleRateTemp1);
			}
			else if (ventricleRate < ventricleRateLow) {
				int ventricleRateTemp2 = ((ventricleRate - ventricleRateLow) / (ventricleRateHigh - ventricleRateLow))*(ventricleRateHigh - ventricleRateLow) + ventricleRateLow;
				printf("Ventricle rate increased to: %d\n", ventricleRateTemp2);
			}
			printf("Heartrate normalized\n\n");
			printf("****************************************\n");
		}
	}
}