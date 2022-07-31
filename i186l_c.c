#include	"80186l.h"
#include	"rtkernel.h"
#include	<dos.h>
#include	<alloc.h>

/*
旼컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴커
�                                  CREATE A TASK                               �
읕컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴켸
*/

RETCODE os_task_create ( void (far *task)(void *pd), void *pdata, WORD stksize, PRIO p, TASK_ID *tsk_id )
{
	WORD		*stk;
	RETCODE		err;
	void far 	*pmem;

	if ( (pmem=farmalloc(stksize))==NULL)// alloc memory for stack task
		return OS_NO_MEMORY;

	stk		= (WORD*)((BYTE *)pmem+stksize);
	*--stk	= (WORD)FP_SEG(pdata);		// simulate call to function with argument
	*--stk	= (WORD)FP_OFF(pdata);
	*--stk	= (WORD)FP_SEG(os_task_end);// at end of task execute os_task_end()
	*--stk	= (WORD)FP_OFF(os_task_end);
	*--stk	= (WORD)0x0200;				// PSW = Interrupts enabled
	*--stk	= (WORD)FP_SEG(task);		// put pointer to task   on top of stack
	*--stk	= (WORD)FP_OFF(task);
	*--stk	= (WORD)0x0000;				// AX = 0
	*--stk	= (WORD)0x0000;				// CX = 0
	*--stk	= (WORD)0x0000;				// DX = 0
	*--stk	= (WORD)0x0000;				// BX = 0
	*--stk	= (WORD)0x0000;				// SP = 0
	*--stk	= (WORD)0x0000;				// BP = 0
	*--stk	= (WORD)0x0000;				// SI = 0
	*--stk	= (WORD)0x0000;				// DI = 0
	*--stk	= (WORD)0x0000;				// ES = 0
	*--stk	= _DS;						// Save current value of DS
	err		= os_tcb_init ( p, (void far *)stk, pmem, tsk_id );	// Get and initialize a TCB
	if ( err == OS_NO_ERR )
		if ( os_running )
			os_sched ();	// Find highest prio. task if multitasking has started
	return ( err );
}

