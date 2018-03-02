/*
*********************************************************************************************************
*
*                                              EXAMPLE #2
*********************************************************************************************************
*/

#include    <STDIO.H>
#include    <STRING.H>
#include    <STDLIB.H>
#include    <CONIO.H>
#include    <DOS.H>

#include    "80186S.H"
#include    "RTKERNEL.H"

/*
*********************************************************************************************************
*                                              CONSTANTS
*********************************************************************************************************
*/

#define          TASK_STK_SIZE    1024                /* Size of each task's stacks (# of bytes)       */
#define          KEY_Q_SIZE         10                /* Size of keyboard queue                        */

/*
*********************************************************************************************************
*                                              VARIABLES
*********************************************************************************************************
*/

BYTE            TaskStatStk [ TASK_STK_SIZE ];          /* Statistics task stack                         */
BYTE            TaskClkStk [ TASK_STK_SIZE ];           /* Clock      task stack                         */
BYTE            TaskKeyStk [ TASK_STK_SIZE ];           /* Keyboard   task stack                         */
BYTE            Task1Stk [ TASK_STK_SIZE ];             /* Task #1    task stack                         */
BYTE            Task2Stk [ TASK_STK_SIZE ];             /* Task #2    task stack                         */
BYTE            Task3Stk [ TASK_STK_SIZE ];             /* Task #3    task stack                         */

OS_EVENT        *DispSemPtr;                          /* Pointer to display  semaphore                 */
OS_EVENT        *KeyQPtr;                             /* Pointer to keyboard queue                     */
OS_EVENT        *KeyMboxPtr;                          /* Pointer to keyboard mailbox                   */

void            *KeyQ[KEY_Q_SIZE];                    /* Keyboard queue                                */

void interrupt (*OldTickISR)(void);
UWORD            OldBP;
UWORD            OldSP;

/*
*********************************************************************************************************
*                                         FUNCTION PROTOTYPES
*********************************************************************************************************
*/

void far         TaskStat(void *data);                /* Function prototypes of tasks                  */
void far         TaskKey(void *data);
void far         TaskClk(void *data);
void far         Task1(void *data);
void far         Task2(void *data);
void far         Task3(void *data);

void             DispChar(UBYTE x, UBYTE y, char  c);
void             DispStr(UBYTE x,  UBYTE y, char *s);

void far         NewTickISR(void);
/*$PAGE*/
/*
*********************************************************************************************************
*                                                  MAIN
*********************************************************************************************************
*/

void main(void)
{
	WORD tsk_id;

	clrscr();
	OldBP      = _BP;
	OldSP      = _SP;
	OldTickISR = getvect ( 0x08 );                         // Get MS-DOS's tick vector                 
	setvect ( 0x81, OldTickISR);                           // Store MS-DOS's tick to chain           
	setvect ( rtKERNEL, (void interrupt (*)(void))OSCtxSw );   // uC/OS's context switch vector         
	os_init();
	DispSemPtr = os_sem_create ( 1 );                      // Display  semaphore                       
	KeyQPtr    = os_q_create(&KeyQ[0], KEY_Q_SIZE);          // Keyboard queue                           
	KeyMboxPtr = os_mbox_create((void *)0);                  // Keyboard mailbox                         
	os_task_create(TaskStat, (void *)0, (void *)&TaskStatStk[TASK_STK_SIZE], 62, &tsk_id );
	os_start();                                             // Start multitasking                       
}

/*$PAGE*/
/*
*********************************************************************************************************
*                                              STATISTICS TASK
*********************************************************************************************************
*/

void far TaskStat(void *data)
{
    char   s[80];
    double max;
    double usage;
    LONG  idle;
    WORD  ctxsw;
	WORD	tsk_id;

    data = data;                                           /* Prevent compiler warning                 */
    DispStr(0, 0, "uC/OS, The Real-Time Kernel");
    DISABLE();                                   /* Install uC/OS's clock tick ISR           */
    setvect ( 0x08, (void interrupt (*)(void))NewTickISR );
    ENABLE();
    DispStr(0, 22, "Determining  CPU's capacity ...");     /* Determine maximum count for os_idle_ctr    */
    os_time_dly(1);                                          /* Synchronize to clock tick                */
    os_idle_ctr = 0L;                                        /* Determine MAX. idle counter value        */
    os_time_dly(18);
    max       = (double)os_idle_ctr;
    os_task_create(TaskKey, (void *)0, (void *)&TaskKeyStk[TASK_STK_SIZE], 10, &tsk_id);
    os_task_create(Task1,   (void *)0, (void *)&Task1Stk[TASK_STK_SIZE],   11, &tsk_id);
    os_task_create(Task2,   (void *)0, (void *)&Task2Stk[TASK_STK_SIZE],   12, &tsk_id);
    os_task_create(Task3,   (void *)0, (void *)&Task3Stk[TASK_STK_SIZE],   13, &tsk_id);
    os_task_create(TaskClk, (void *)0, (void *)&TaskClkStk[TASK_STK_SIZE], 14, &tsk_id);
    while (1)
	{
        DISABLE();
        ctxsw      = os_ctx_sw_ctr;
        idle       = os_idle_ctr;
        os_ctx_sw_ctr = 0;                                 /* Reset statistics counters                */
        os_idle_ctr  = 0L;
        ENABLE();
        usage = 100.0 - (100.0 * (double)idle / max);      /* Compute and display statistics           */
        sprintf(s, "Task Switches: %d     CPU Usage: %5.2f %%", ctxsw, usage);
        DispStr(0, 22, s);
        sprintf(s, "Idle Ctr: %7.0f / %7.0f  ", (double)idle, max);
        DispStr(0, 23, s);
        os_time_dly(18);                                     /* Wait one second                          */
    }
}
/*$PAGE*/
/*
*********************************************************************************************************
*                                              KEYBOARD TASK
*********************************************************************************************************
*/

void far TaskKey ( void *data )
{
    WORD ctr;
    char  s[30];

    data = data;                                           /* Prevent compiler warning                 */
    ctr  = 0;
    DispStr(0, 5, "Keyboard Task:");
    DispStr(0, 6, "   Press 1 to send a message to task #1");
    DispStr(0, 7, "   Press 2 to send a message to task #2");
    DispStr(0, 8, "   Press X to quit");
    while (1)
	{
        os_time_dly(1);
        sprintf(s, "%05d", ctr);
        DispStr(15, 5, s);
        ctr++;
        if (kbhit())
		{
            switch (getch()) {
                case '1':
					OSMboxPost ( KeyMboxPtr, (void *)1);
                    break;

                case '2':
					os_q_post ( KeyQPtr, (void *)1);
                    break;

                case 'x':
                case 'X':
					DISABLE();
                    setvect(0x08, OldTickISR);
                    ENABLE();
                    clrscr();
                    _BP = OldBP;
                    _SP = OldSP;
                    exit(0);
                    break;
            }
        }
    }
}
/*$PAGE*/
/*
*********************************************************************************************************
*                                               TASK #1
*********************************************************************************************************
*/

void far Task1(void *data)
{
    BYTE	err;
    WORD	toctr;
    WORD	msgctr;
    char	s[30];

    data   = data;
    msgctr = 0;
    toctr  = 0;
    while ( 1 )
	{
        sprintf (s, "Task #1 Timeout Counter: %05d", toctr);
        DispStr (0, 10, s);
        sprintf (s, "        Message Counter: %05d", msgctr);
        DispStr (0, 11, s);
        OSMboxPend (KeyMboxPtr, 36, &err);
        switch (err)
		{
            case OS_NO_ERR:
                 msgctr++;
                 break;

            case OS_TIMEOUT:
                 toctr++;
                 break;
        }
    }
}
/*$PAGE*/
/*
*********************************************************************************************************
*                                               TASK #2
*********************************************************************************************************
*/

void far Task2(void *data)
{
    UBYTE  err;
    UWORD  toctr;
    UWORD  msgctr;
    char   s[30];

    data   = data;
    msgctr = 0;
    toctr  = 0;
    while (1)
	{
        sprintf(s, "Task #2 Timeout Counter: %05d", toctr);
        DispStr(0, 15, s);
        sprintf(s, "        Message Counter: %05d", msgctr);
        DispStr(0, 16, s);
        os_q_pend(KeyQPtr, 72, &err);
        switch (err) {
            case OS_NO_ERR:
                 msgctr++;
                 break;

            case OS_TIMEOUT:
                 toctr++;
                 break;
        }
    }
}
/*$PAGE*/
/*
*********************************************************************************************************
*                                               TASK #3
*********************************************************************************************************
*/

void far Task3(void *data)
{
    BYTE  x, y, z;

    data = data;
    DispStr(50, 0, "---------- Task #3 ----------");
    while (1)
	{
        os_time_dly(1);
        x = random(29);                          /* Find X position where task number will appear      */
        y = random(20);                          /* Find Y position where task number will appear      */
        z = random(4);
        switch (z)
		{
            case 0: DispChar(x + 50, y + 1, '*');
                    break;

            case 1: DispChar(x + 50, y + 1, 'X');
                    break;

            case 2: DispChar(x + 50, y + 1, '#');
                    break;

            case 3: DispChar(x + 50, y + 1, ' ');
                    break;
        }
    }
}
/*$PAGE*/
/*
*********************************************************************************************************
*                                               CLOCK TASK
*********************************************************************************************************
*/

void far TaskClk(void *data)
{
    struct time now;
    struct date today;
    char        s[40];

    data = data;
    while (1)
	{
        os_time_dly(18);
        gettime(&now);
        getdate(&today);
        sprintf(s, "%02d-%02d-%02d  %02d:%02d:%02d",
                   today.da_mon,
                   today.da_day,
                   today.da_year,
                   now.ti_hour,
                   now.ti_min,
                   now.ti_sec);
        DispStr(58, 23, s);
    }
}
/*$PAGE*/
/*
*********************************************************************************************************
*                                           DISPLAY CHARACTER FUNCTION
*********************************************************************************************************
*/

void DispChar( BYTE x, BYTE y, char c )
{
    BYTE err;

    os_sem_wait ( DispSemPtr, 0, &err );
    gotoxy(x + 1, y + 1);
    putchar ( c );
    os_sem_signal ( DispSemPtr );
}


/*
*********************************************************************************************************
*                                             DISPLAY STRING FUNCTION
*********************************************************************************************************
*/

void DispStr( BYTE x, BYTE y, char *s )
{
    BYTE err;

    os_sem_wait(DispSemPtr, 0, &err);
    gotoxy(x + 1, y + 1);
    puts(s);
    os_sem_signal(DispSemPtr);
}
