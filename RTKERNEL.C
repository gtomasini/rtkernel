/*
--------------------------------------------------------------------------------
                                   rtKERNEL								   
                        PC Real-Time Multitasking Operating System			   
                                    KERNEL									   
																			   
																			   
 File :	RTKERNEL.C               										   
																			   
 Author:	Guillermo Pablo Tomasini 										   
																			   
 Date:		20-8-94															   
																			   
--------------------------------------------------------------------------------
*/


#include	<alloc.h>	//because NULL
#include	<dos.h>		//because getvect(), setvect(), FP_OFF & FP_SEG
#include	<limits.h>	//becuase INT_MAX
#include	<mem.h>		//because _fmemcpy()

#include    "80186l.h"
#include    "rtkernel.h"

#ifdef   TURBOC
#pragma  inline
#endif

/*
--------------------------------------------------------------------------------
                                     CONSTANTS         					   
--------------------------------------------------------------------------------
*/

#define	WCONTROL			(0x00<<6) | (0x03<<4) | (0x03<<1) | 0x00	//word control for 8253
#define	BASE_ADRESS_TIMER	0x40										//base address of 8253



/*
--------------------------------------------------------------------------------
                              EXTERN VARIABLES                                
--------------------------------------------------------------------------------
*/

extern void far	new_tick_isr ( void );
extern void far	os_ctx_sw ( void );


/*
--------------------------------------------------------------------------------
                              GLOBAL VARIABLES                                
--------------------------------------------------------------------------------
*/
OS_TCB		*os_tcb_cur;			// pointer to current TCB   
OS_TCB		*os_tcb_run;			// ptr to higher priority task
WORD		os_ctx_sw_ctr;			// context switch counter                                       
LONG		os_idle_ctr;			// counter used in os_task_idle()                                 
WORD		os_running;				// flag indicating that rtKERNEL has been started and is running
WORD		divisor		= 0xffff;	// by default, 18 interrupts by second
LONG		count_ticks;			// used by i186l_a for count ticks

/*
--------------------------------------------------------------------------------
                              LOCAL VARIABLES                                 
--------------------------------------------------------------------------------
*/

static WORD		os_tcb_current_prio;		// priority of running task
static BYTE		os_lock_nesting;            // multitasking lock nesting level
static BYTE		os_int_nesting;             // interrupt nesting level
static OS_TCB	*os_tcb_free_list;			// pointer to list of free TCBs
static OS_ECB	*os_ecb_free_list;			// pointer to list of free EVENT control blocks
static OS_QCB	*os_qcb_free_list;          	// pointer to list of free QUEUE control blocks
static TIME		os_time;					// current value of system time (in ticks)
static OS_TCB	os_tcb_tbl [ OS_MAX_TASKS ];				// table of TCBs
static OS_ECB	os_ecb_tbl [ OS_MAX_EVENTS ];				// table of EVENT control blocks
static OS_QCB	os_qcb_tbl [ OS_MAX_QS ];			   		// table of QUEUE control blocks
static TASK_ID	idle_task_id;								// idle task id
static OS_TCB	*os_tcb_task_tbl [ OS_MAX_TASKS ];			// ptr table to created tasks
															// order by task_id
static OS_TCB	*os_tcb_dlylist;							// ptr to delay task list
static OS_TCB	*os_tcb_curprio [ OS_LO_PRIO ];				// ptr array to current task
															// in each priority level
static WORD		tick_size	= 55;							// by default 55.5 msec

static WORD		old_bp;
static WORD		old_sp;
static WORD		old_ss;

/*
--------------------------------------------------------------------------------
                          LOCAL FUNCTION PROTOTYPES                           
--------------------------------------------------------------------------------
*/

static void 	os_task_idle		( void *data	);
static void		os_tcb_ins_rdylist	( OS_TCB *ptcb	);
static void		os_tcb_del_rdylist	( OS_TCB *ptcb	);
static void		os_tcb_ins_evnlist	( OS_TCB *ptcb	); 
static void		os_tcb_purge_evnlist( OS_ECB *pecb	);
static void		os_tcb_del_evnlist	( OS_ECB *pecb	);
static void 	os_tcb_ins_dlylist	( OS_TCB *ptcb	);
static void 	os_tcb_del_dlylist	( OS_TCB *ptcb	);
static void 	os_tcb_quit_evnlist	( OS_TCB *ptcb	);
static void		os_dest_alltasks	( void );

static void		interrupt (*old_tick_isr)(void);			// old tick int. handler


/*
--------------------------------------------------------------------------------
                            rtKERNEL INITIALIZATION                           
--------------------------------------------------------------------------------
*/

void os_init ( void )
{
	int	i;

	DISABLE();
	old_tick_isr = getvect ( 0x08 );							// get MS-DOS's tick vector                 
	setvect ( 0x81, old_tick_isr );								// store MS-DOS's tick to chain           
	setvect ( rtKERNEL, (void interrupt (*)(void))os_ctx_sw );	// rtKERNEL context switch vector        
    setvect ( 0x08, (void interrupt (*)(void))new_tick_isr );	// new int. handler
	ENABLE();

	os_time         = 0L;
	os_tcb_cur      = NULL;
	os_tcb_run		= NULL;
	os_int_nesting  = 0;
	os_lock_nesting = 0;
	os_running      = FALSE;
	os_idle_ctr     = 0L;
	os_ctx_sw_ctr   = 0;

	for ( i = 0; i < OS_MAX_TASKS; i++ )	// init. list of free TCBs
	{
		if ( i < OS_MAX_TASKS-1 )
    		os_tcb_tbl [ i ].next_free	= &os_tcb_tbl [ i + 1 ];
		else
			os_tcb_tbl [ i ].next_free	= NULL;

		os_tcb_tbl [ i ].task_id 	= i;
		os_tcb_tbl [ i ].task_stat	= OS_TASK_NULL;
		os_tcb_task_tbl [ i ]	 	= NULL;
	}

	os_tcb_free_list				= os_tcb_tbl;

	for ( i = 0; i < (OS_MAX_EVENTS - 1); i++ )	// init. list of free ECB
	{
	    os_ecb_tbl [ i ].os_event_ptr		= &os_ecb_tbl [ i + 1 ];
//		os_ecb_tbl [ i ].os_tsk_cnt			= 0;
		os_ecb_tbl [ i ].os_tcb_blk_task	= NULL;
	}

	os_ecb_tbl [ OS_MAX_EVENTS - 1 ].os_event_ptr	= NULL;
	os_ecb_free_list								= os_ecb_tbl;

	for ( i = 0; i < (OS_MAX_QS - 1); i++ )	// init. list of free QCB
	    os_qcb_tbl [ i ].os_qcb_ptr = &os_qcb_tbl [ i + 1 ];

	os_qcb_tbl [ OS_MAX_QS - 1 ].os_qcb_ptr	= NULL;
	os_qcb_free_list						= os_qcb_tbl;

	for ( i = 0; i < OS_LO_PRIO; i++ )
		os_tcb_curprio [ i ]	= NULL;
	
	os_task_create ( os_task_idle, NULL, 512, OS_LO_PRIO-1, &idle_task_id );
}


/*
--------------------------------------------------------------------------------
                            rtKERNEL END                                      
--------------------------------------------------------------------------------
*/

void os_end ( void )
{
//WARNING: no allowed automatic var here 

	os_running	= FALSE;
	DISABLE();
	outportb ( BASE_ADRESS_TIMER + 3, WCONTROL );	// 8253 mode
	outportb ( BASE_ADRESS_TIMER + 0, 0xff );		// 1ro. low 
	outportb ( BASE_ADRESS_TIMER + 0, 0xff );		// 2do. high
	os_dest_alltasks ( );					//destroy all tasks
	_BP = old_bp;							//restore old bp
	_SP = old_sp;							//restore old sp
	_SS	= old_ss;							//restore old ss
	setvect ( 0x08, old_tick_isr );			//restore old int. handler
	farfree ( os_tcb_cur->allocmem_ptr );	//free memory of cur. task
	ENABLE();
}									//return to next inst. after os_start();

/*
--------------------------------------------------------------------------------
                             DESTROY ALL TASK'S                               
--------------------------------------------------------------------------------
*/

void os_dest_alltasks ( void )
{
	int i;
	OS_TCB *ptcb;

	for ( i=0; i<OS_MAX_TASKS; i++)
		if ( (ptcb = os_tcb_task_tbl [ i ]) != NULL && ptcb != os_tcb_cur )
			os_task_del ( ptcb->task_id );
}

/*
--------------------------------------------------------------------------------
                                    IDLE TASK                                 
--------------------------------------------------------------------------------
*/

static void far os_task_idle ( void *data )
{
	data = data;

	while (1)
	{
    	DISABLE();
	    os_idle_ctr++;
    	ENABLE();
    }
}

/*
--------------------------------------------------------------------------------
                                START MULTITASKING                            
--------------------------------------------------------------------------------
*/

void os_start ( void )
{
//WARNING: no allowed automatic var. here 

	old_bp      	= _BP;		// salvage bp
	old_sp      	= _SP;		// freeze return address
	old_ss			= _SS;		// freeze return address
	os_running		= 1;
	os_start_run();				// no return 
}


/*
--------------------------------------------------------------------------------
                                   rtKERNEL SCHEDULER                         
--------------------------------------------------------------------------------
*/

void os_sched ( void )
{
	DISABLE();

	if ( !( os_lock_nesting | os_int_nesting ) )// task scheduling must be enabled and not ISR level
		if ( os_tcb_run != os_tcb_cur )
		{
	       	os_ctx_sw_ctr++;	// increment context switch counter                   
			OS_TASK_SW();		// context switch through interrupt
		}						
	ENABLE();
}

/*
--------------------------------------------------------------------------------
                                 PREVENT SCHEDULING                           
--------------------------------------------------------------------------------
*/

void os_sched_lock ( void )
{
	DISABLE();
	os_lock_nesting++;				// increment lock nesting level  
	ENABLE();
}

/*
--------------------------------------------------------------------------------
                                  ENABLE SCHEDULING                           
--------------------------------------------------------------------------------
*/

void os_sched_unlock ( void )
{
	DISABLE();
	if ( os_lock_nesting )
	{
	    os_lock_nesting--;                 // decrement lock nesting level                  
	    if ( !( os_lock_nesting | os_int_nesting) )
		// See if scheduling re-enabled and not an ISR   
	        os_sched();                   // See if a higher priority task is ready        
		else
    	    ENABLE();
    } 
	else
    	ENABLE();
}


/*
--------------------------------------------------------------------------------
                                  INITIALIZE TCB                              
--------------------------------------------------------------------------------
*/

RETCODE os_tcb_init ( PRIO prio, void far *stk, void far *pmem, TASK_ID *tsk_id )
{
	OS_TCB *ptcb;

	*tsk_id	= NULL_ID;

	if ( prio > OS_LO_PRIO - 1 )
		return OS_PRIO_ERR;

	DISABLE();
	ptcb = os_tcb_free_list;         // get a free TCB from the free TCB list    

	if ( ptcb != NULL )
	{
    	os_tcb_free_list        	= ptcb->next_free;	// Update pointer to free TCB list
	    ENABLE();
    	ptcb->stack_ptr    			= stk;				// Load Stack pointer in TCB
		ptcb->allocmem_ptr			= pmem;
	    ptcb->prio       			= prio;				// Load task priority into TCB
	    ptcb->dly_time        		= 0L;
    	ptcb->pecb		  			= NULL;				// Task is not pending on an event
	    DISABLE();
		*tsk_id						= ptcb->task_id;
		os_tcb_task_tbl [ *tsk_id ]	= ptcb;

		os_tcb_ins_rdylist ( ptcb );
	    ENABLE();

	    return ( OS_NO_ERR );
   	}
	else
	{
	    ENABLE ();
	    return ( OS_NO_MORE_TCB );
    }
}


/*
--------------------------------------------------------------------------------
                                     ENTER ISR                                
--------------------------------------------------------------------------------
*/

void os_int_enter ( void )
{
	DISABLE();
	os_int_nesting++;                    // increment ISR nesting level                        
	ENABLE();
}


/*
--------------------------------------------------------------------------------
                                     EXIT ISR                                 
--------------------------------------------------------------------------------
*/

WORD os_int_exit ( void )
{
	DISABLE();

	if ( !( --os_int_nesting | os_lock_nesting ) )	// reschedule only if all ISRs completed & not locked 
	{
		register int i;

		for ( i=0 ; i < OS_LO_PRIO; i++ )
		{
			if ( os_tcb_curprio [ i ] != NULL )
			{
				if ( i == os_tcb_current_prio )
				{
					if ( os_tcb_cur->next_rdy != os_tcb_run )
					{				
						os_tcb_run->task_stat	= OS_TASK_RDY;
					  	os_tcb_run				= os_tcb_run->next_rdy;
						os_tcb_run->task_stat	= OS_TASK_RUN;
					  	os_ctx_sw_ctr++;
						return -1;		// perform interrupt level context switch
			    	}
				}
				else
				{
					os_tcb_run->task_stat	= OS_TASK_RDY;
					os_tcb_run				= os_tcb_curprio [ i ];
					os_tcb_current_prio		= i;		
				  	os_ctx_sw_ctr++;
					return -1;	// perform interrupt level context switch
				}
			break;
			}
		}
	}
	ENABLE();
	return 0;	// no perform interrupt level context switch
}


/*
--------------------------------------------------------------------------------
                                    DELETE A TASK					   
--------------------------------------------------------------------------------
*/

RETCODE os_task_del ( TASK_ID tsk_id )
{
	register OS_TCB		*ptcb;

	if ( tsk_id > OS_MAX_TASKS )
		return OS_TSK_NO_EXIST;

	if ( os_running && tsk_id == idle_task_id )
	    return ( OS_TASK_DEL_IDLE );	// Not allowed to delete idle task

	ptcb	= os_tcb_task_tbl [ tsk_id ];
	if ( ptcb == NULL )					// Task to delete must exist
		return OS_TSK_NO_EXIST;

	if ( ptcb->task_stat == OS_TASK_NULL )
		return OS_TSK_NO_EXIST;

	DISABLE ( );

	os_tcb_task_tbl	[ tsk_id ]	= NULL;

	switch ( ptcb->task_stat )
	{
		case OS_TASK_RUN:
		case OS_TASK_RDY:
			os_tcb_del_rdylist ( ptcb );
			break;

		case OS_TASK_DLY:
			os_tcb_del_dlylist ( ptcb );
			break;

		case OS_TASK_SEM:
		case OS_TASK_MBOX:
		case OS_TASK_Q:
			os_tcb_quit_evnlist ( ptcb );
			os_tcb_del_dlylist ( ptcb );
			break;
	}
	ptcb->next_free		= os_tcb_free_list;		//insert in tcb free list
	os_tcb_free_list	= ptcb;
	ptcb->task_stat		= OS_TASK_NULL;
	farfree ( ptcb->allocmem_ptr );				// free stack memory
	if ( ptcb == os_tcb_cur )	// task to delete is the current active task?
		os_sched();				// never returns
	ENABLE();
	return OS_NO_ERR;
}

/*
--------------------------------------------------------------------------------
                                   END OF TASK                                      
--------------------------------------------------------------------------------
*/

void os_task_end ( void )
{
	DISABLE ( );
	os_tcb_task_tbl	[ os_tcb_cur->task_id ]	= NULL;
	os_tcb_del_rdylist ( os_tcb_cur );
	os_tcb_cur->next_free	= os_tcb_free_list;		//insert in tcb free list
	os_tcb_free_list		= os_tcb_cur;
	os_tcb_cur->task_stat	= OS_TASK_NULL;
	farfree ( os_tcb_cur->allocmem_ptr );
	os_sched();				// never returns
}

/*
--------------------------------------------------------------------------------
                             CHANGE PRIORITY OF A TASK                        
--------------------------------------------------------------------------------
*/

RETCODE os_task_change_prio ( TASK_ID tsk_id, PRIO newprio )
{
	register OS_TCB		*ptcb = os_tcb_task_tbl [ tsk_id ];

	if ( tsk_id > OS_MAX_TASKS )
	    return ( OS_TASK_DEL_ERR );

	if ( ptcb == NULL )				// Task to delete must exist
		return OS_TSK_NO_EXIST;

	if ( ptcb->task_stat == OS_TASK_NULL )
		return OS_TSK_NO_EXIST;

	if ( newprio > OS_LO_PRIO - 1 || ptcb->prio == newprio )
		return OS_PRIO_ERR;

	DISABLE();

	switch ( ptcb->task_stat )
	{
		case OS_TASK_RUN:
			os_tcb_del_rdylist ( ptcb );
			ptcb->prio	= newprio;
			os_tcb_ins_rdylist ( ptcb );
			os_sched ( );
			break;

		case OS_TASK_RDY:
			os_tcb_del_rdylist ( ptcb );
			ptcb->prio	= newprio;
			os_tcb_ins_rdylist ( ptcb );
			ENABLE ( );
			break;

		case OS_TASK_DLY:
		case OS_TASK_SEM:
		case OS_TASK_MBOX:
		case OS_TASK_Q:
			ptcb->prio	= newprio;
			ENABLE ( );
			break;
	}
	return OS_NO_ERR;
}

/*
--------------------------------------------------------------------------------
                       DELAY TASK 'n' TICKS   (n = 1 WORD)                     
--------------------------------------------------------------------------------
*/

void os_tick_dly ( LONG ticks ) 
{
	if ( ticks )
	{
	    DISABLE();

	    os_tcb_cur->dly_time = ticks;			// Load ticks in TCB

		// delay current task        
		os_tcb_del_rdylist ( os_tcb_cur );
		os_tcb_ins_dlylist ( os_tcb_cur );
		os_tcb_cur->task_stat	=	OS_TASK_DLY;
	    os_sched();
    }
}

/*
--------------------------------------------------------------------------------
                             DELAY TASK 'n' msecs                                    
--------------------------------------------------------------------------------
*/
// if msecs<tick_size => no delay

void os_msec_dly ( LONG msecs ) 
{
	os_tick_dly ( (LONG) msecs/tick_size );
}

/*
--------------------------------------------------------------------------------
                               PROCESS SYSTEM TICK                            
--------------------------------------------------------------------------------
*/

void os_time_tick ( void )
{
	DISABLE ( );
    if ( os_tcb_dlylist != NULL )
	{                       // Delayed or waiting for event with TO     
		register OS_TCB		*aux_ptcb	= os_tcb_dlylist;

		--os_tcb_dlylist->dly_time;	// decrement nbr of ticks to end of delay
		while ( !os_tcb_dlylist->dly_time )
		{
			os_tcb_dlylist			= os_tcb_dlylist->next_dly;
			if ( aux_ptcb->pecb != NULL )
				os_tcb_quit_evnlist ( aux_ptcb );
			os_tcb_ins_rdylist ( aux_ptcb );
			aux_ptcb->timeout_flag	= 1;
			aux_ptcb->task_stat		= OS_TASK_RDY;
			if ( os_tcb_dlylist == NULL )
				break;
			aux_ptcb				= os_tcb_dlylist;
		}
	}
	os_time++;
	ENABLE();
}

/*
--------------------------------------------------------------------------------
                                 SET SYSTEM CLOCK							   
--------------------------------------------------------------------------------
*/

void os_time_set ( LONG ticks )
{
	DISABLE();
	os_time = ticks;
	ENABLE();
}

TASK_ID os_get_cur_taskid ( void )
{
	return os_tcb_cur->task_id;
}

/*
--------------------------------------------------------------------------------
                              GET CURRENT SYSTEM TIME						   
--------------------------------------------------------------------------------
*/

LONG os_time_get ( void )
{
	TIME ticks;

	DISABLE();
	ticks = os_time;
	ENABLE();
	return ( ticks );
}

/*
--------------------------------------------------------------------------------
                              SET TICK SIZE                 				   
--------------------------------------------------------------------------------
*/

RETCODE os_set_ticksize ( WORD ticksize )	//ticksize in msecs.
{
	if ( ticksize > 54 || ticksize < 12 )
		return OS_BAD_TICKSIZE;

	DISABLE ( );

	tick_size	= ticksize;
	divisor		= (1193180L * ticksize/1000);

	// TICK SIZE SET     
	// see "PROGRAMMERS PROBLEM SOLVER PC XT/AT" pags. 45-49
	// channel #0, write low-then-high byte, mode=3, binary data
	
	outportb ( BASE_ADRESS_TIMER + 3, WCONTROL );				// 8253 mode
	outportb ( BASE_ADRESS_TIMER + 0, divisor       & 0xff );	// 1ro. low
	outportb ( BASE_ADRESS_TIMER + 0, (divisor >> 8) & 0xff );	// 2do. high
	ENABLE ( );
	return OS_NO_ERR;
}

/*
--------------------------------------------------------------------------------
                               GET TICK SIZE                 				   
--------------------------------------------------------------------------------
*/

WORD os_get_ticksize ( void )
{
	return tick_size;
}

/*
--------------------------------------------------------------------------------
                              INITIALIZE SEMAPHORE							   
--------------------------------------------------------------------------------
*/

OS_ECB *os_sem_create ( SWORD cnt )
{
    register OS_ECB *pecb;

	if ( cnt < 0 )	// semaphore cannot start negative
		return NULL;

    DISABLE();
    pecb = os_ecb_free_list;		// get next free event control block        
    if ( os_ecb_free_list != NULL )	// See if pool of free ECB pool was empty   
        os_ecb_free_list = (OS_ECB *)os_ecb_free_list->os_event_ptr;

    ENABLE();
    if ( pecb != NULL )
	{                         // Get an event control block               
		pecb->os_event_cnt	= cnt;	// Set semaphore value
		return ( pecb );
    }
	else
        return ( NULL );	// Ran out of event control blocks
}

/*
--------------------------------------------------------------------------------
                               PEND ON SEMAPHORE							   
--------------------------------------------------------------------------------
*/

RETCODE os_sem_wait ( OS_ECB *pecb, TIME timeout )
{
	if ( pecb	== NULL)
		return OS_NULL_PECB;

    DISABLE();
    if ( --pecb->os_event_cnt < 0 )
	// must wait until event occurs  
	{                               
        os_tcb_cur->pecb		= pecb;			// store pointer to event control block in TCB       
		os_tcb_ins_evnlist ( os_tcb_cur );		// insert in event list
		os_tcb_del_rdylist ( os_tcb_cur );		// delete of rdy list
        os_tcb_cur->task_stat  	= OS_TASK_SEM;  // resource not available, pend on semaphore 
        os_tcb_cur->dly_time 	= timeout;      // store pend timeout in TCB
		os_tcb_cur->timeout_flag= 0;			
		if ( timeout )							// timeout?...
			os_tcb_ins_dlylist ( os_tcb_cur );	// insert in delay list
        os_sched ( );           // find next highest priority task ready to run       
		DISABLE ( );
		os_tcb_cur->pecb		= NULL;
		ENABLE ( );
		if ( os_tcb_cur->timeout_flag )
			return OS_TIMEOUT;
		else
		{
			DISABLE ( );
			os_tcb_del_dlylist ( os_tcb_cur );
			ENABLE ( );
			return OS_NO_ERR;
		}
    }
	else
	{              // semaphore > 0, resource available       
        ENABLE();
        return	OS_NO_ERR;
    }
}

/*
--------------------------------------------------------------------------------
                                 DELETE A QUEUE                 				   
--------------------------------------------------------------------------------
*/

RETCODE os_q_dest ( OS_ECB *pecb )
{
	OS_QCB	*os_q;

	if ( pecb == NULL )
		return OS_NULL_PECB;
	
	if ( ( os_q = (OS_QCB*)pecb->os_event_ptr ) == NULL )
		return OS_Q_NULL;

	os_q->os_qcb_ptr			= os_qcb_free_list;	//insert in free list
	os_qcb_free_list			= os_q;
	farfree(os_q->qstart);
	os_event_dest ( pecb );
	return OS_NO_ERR;
}

/*
--------------------------------------------------------------------------------
                               DELETE AN EVENT                				   
--------------------------------------------------------------------------------
*/

RETCODE	os_event_dest ( OS_ECB *pecb )
{
	if ( pecb == NULL )
		return OS_NULL_PECB;

	DISABLE ( );
	os_tcb_purge_evnlist ( pecb );
	ENABLE ( );
	pecb->os_event_ptr	= os_ecb_free_list;
	os_ecb_free_list	= pecb;
	return OS_NO_ERR;
}

/*
--------------------------------------------------------------------------------
                                  POST TO A SEMAPHORE						   
--------------------------------------------------------------------------------
*/

RETCODE os_sem_signal ( OS_ECB *pecb )
{
	if ( pecb	== NULL)
		return OS_NULL_PECB;

    if ( pecb->os_event_cnt < INT_MAX )// make sure semaphore will not overflow
	{            
	    DISABLE();
		pecb->os_event_cnt++;
        if ( pecb->os_event_cnt <= 0 )
		{                   
			os_tcb_del_evnlist ( pecb );	//signal the waiting task
			os_sched ( );	// find highest priority task ready to run            
			return OS_NO_ERR;
        }
		else
            ENABLE();
        return OS_NO_ERR;
    }
	else
        return OS_SEM_OVF;
}

/*
--------------------------------------------------------------------------------
                               INITIALIZE MESSAGE MAILBOX					   
--------------------------------------------------------------------------------
*/

OS_ECB *os_mbox_create ( void *msg )
{
    OS_ECB *pecb;

    DISABLE();
    pecb = os_ecb_free_list;            // Get next free event control block                  
    if ( os_ecb_free_list != NULL )		// See if pool of free ECB pool was empty        
        os_ecb_free_list = (OS_ECB *)os_ecb_free_list->os_event_ptr;
    ENABLE();
    if ( pecb != NULL )
        pecb->os_event_ptr    = msg;             // Deposit message in event control block             

    return ( pecb );               // Return pointer to event control block              
}

/*
--------------------------------------------------------------------------------
                             PEND ON MAILBOX FOR A MESSAGE					   
--------------------------------------------------------------------------------
*/

void	*os_mbox_receive ( OS_ECB *pecb, TIME timeout, RETCODE *err )
{
    void  *msg;

	if ( pecb	== NULL)
	{
		*err	=	OS_NULL_PECB;
		return NULL;
	}

    DISABLE();
    if ( (msg = pecb->os_event_ptr) != NULL )
	{    // See if there is already a message            
        pecb->os_event_ptr = NULL;               // Clear the mailbox
        ENABLE();
        *err = OS_NO_ERR;
    }
	else
	{
        os_tcb_cur->pecb		= pecb;		// Store pointer to event control block in TCB       
		os_tcb_ins_evnlist ( os_tcb_cur );
		os_tcb_del_rdylist ( os_tcb_cur );
        os_tcb_cur->task_stat 		= OS_TASK_MBOX;	// Message not available, task will pend       
        os_tcb_cur->dly_time		= timeout;		// Load timeout in TCB                         
		os_tcb_cur->timeout_flag	= 0;			
		if ( timeout )
			os_tcb_ins_dlylist ( os_tcb_cur );

        os_sched();					// Find next highest priority task ready to run  
		DISABLE();
		msg                     = pecb->os_event_ptr;	// Message received                     
		pecb->os_event_ptr    = NULL;					// Clear the mailbox                    
		ENABLE();
		if ( os_tcb_cur->timeout_flag )
			*err	= OS_TIMEOUT;
		else
		{
			DISABLE ( );
			os_tcb_del_dlylist ( os_tcb_cur );
			ENABLE ( );
			*err	= OS_NO_ERR;
		}
    }
    return msg;					// Return the message received (or NULL)        
}

/*
--------------------------------------------------------------------------------
                             POST MESSAGE TO A MAILBOX						   
--------------------------------------------------------------------------------
*/

RETCODE os_mbox_send ( OS_ECB *pecb, void *msg )
{
	if ( pecb	== NULL)
		return	OS_NULL_PECB;

    DISABLE();
    if ( pecb->os_event_ptr != NULL )
	{       // Make sure mailbox doesn't already contain a msg  
        ENABLE();
        return ( OS_MBOX_FULL );
    }
	else
	{
        pecb->os_event_ptr = msg;                // Place message in mailbox                           
		os_tcb_purge_evnlist ( pecb );
		os_sched ( );	// Find highest priority task ready to run            
        return OS_NO_ERR;
    }
}

/*
--------------------------------------------------------------------------------
                             INITIALIZE MESSAGE QUEUE						   
--------------------------------------------------------------------------------
*/

OS_ECB *os_q_create ( WORD qsize, BYTE elem_size )
{
// the queue size is qsize-1
    OS_ECB		*pecb;
    OS_QCB		*pq;
	void far	*start;

	if ( (start=farmalloc( (qsize+1)*elem_size ))==NULL)
		return NULL;

    DISABLE();
    pecb = os_ecb_free_list;		// Get next free event control block
    if ( os_ecb_free_list != NULL )	// See if pool of free ECB pool was empty             
        os_ecb_free_list = (OS_ECB *)os_ecb_free_list->os_event_ptr;
    ENABLE();
    if ( pecb != NULL )
	{               // See if we have an event control block              
        DISABLE();                    // Get a free queue control block                     
        pq = os_qcb_free_list;
        if ( os_qcb_free_list != NULL ) 
            os_qcb_free_list = os_qcb_free_list->os_qcb_ptr;
    	ENABLE();
	    if ( pq != NULL )	// See if we were able to get a queue control block   
		{                    
			pecb->os_event_ptr	= pq;
            pq->qstart			= start;       // Yes, initialize the queue
            pq->tail_offset	= pq->head_offset = pq->entries = 0;
			pq->qsize			= qsize+1;
            pq->elem_size		= elem_size;
        }
		else
		{                                 // No,  since we couldn't get a queue control block   
            DISABLE();                    // Return event control block on error      
            pecb->os_event_ptr = (void *)os_ecb_free_list;
            os_ecb_free_list   = pecb;
            ENABLE();
            pecb = NULL;
        }
    }
    return ( pecb );
}

/*
--------------------------------------------------------------------------------
                            PEND ON A QUEUE FOR A MESSAGE					   
--------------------------------------------------------------------------------
*/

void *os_q_read ( OS_ECB *pecb, TIME timeout, RETCODE *err )
{
    void  *msg;
    OS_QCB  *pq = pecb->os_event_ptr;		// Point at queue control block       

    DISABLE();
    if ( pq->entries )			// see if any messages in the queue                   
	{                   
        pq->entries--;                 // update the number of entries in the queue          
        msg =  (BYTE*) (pq->qstart) + pq->head_offset*pq->elem_size;	// yes, extract oldest message from the queue 
		pq->head_offset = ++pq->head_offset % pq->qsize;
        ENABLE();
        *err = OS_NO_ERR;
    }
	else
	{
        os_tcb_cur->pecb		= pecb;       // store pointer to event control block in TCB        
		os_tcb_ins_evnlist ( os_tcb_cur );
		os_tcb_del_rdylist ( os_tcb_cur );
        os_tcb_cur->task_stat 	= OS_TASK_Q;    // task will have to pend for a message to be posted  
        os_tcb_cur->dly_time	= timeout;      // load timeout into TCB                              
		os_tcb_cur->timeout_flag= 0;			
		if ( timeout )						  	// timeout?...
			os_tcb_ins_dlylist ( os_tcb_cur );	// insert in delay list
        os_sched();                               // Find next highest priority task ready to run       
        DISABLE();
        pq->entries--;							// update the number of entries in the queue          
        msg =  (BYTE*) (pq->qstart) + pq->head_offset*pq->elem_size;// message received, extract oldest message from the queue 
		pq->head_offset = ++pq->head_offset % pq->qsize;						    
        ENABLE();
		if ( os_tcb_cur->timeout_flag )			
			*err	= OS_TIMEOUT;
		else
		{
			DISABLE ( );
			os_tcb_del_dlylist ( os_tcb_cur );
			ENABLE ( );
	        *err	= OS_NO_ERR;
		}
    }                                            // Return message received (or NULL)                  
    return (msg);
}

/*
--------------------------------------------------------------------------------
                               POST MESSAGE TO A QUEUE						   
--------------------------------------------------------------------------------
*/

BYTE os_q_write ( OS_ECB *pecb, void *msg )
{
    OS_QCB  *pq = pecb->os_event_ptr;		// point to queue control block                      

    DISABLE();
    if ( pq->entries < pq->qsize )	// make sure that queue is not full
	{
		// insert message into queue
		_fmemcpy ( (BYTE *)pq->qstart + pq->elem_size*pq->tail_offset, msg, pq->elem_size );
		pq->tail_offset	= ++pq->tail_offset % pq->qsize;
		pq->entries++;						// update the number of entries in the queue          
		os_tcb_del_evnlist ( pecb );
		os_sched();							// find highest priority task ready to run            
		return OS_NO_ERR;
	}
	else
	{                                 
        ENABLE ( );
        return ( OS_Q_FULL );
    }
}

/*
--------------------------------------------------------------------------------
                         INSERT A TASK IN READY LIST                            
--------------------------------------------------------------------------------
*/

void os_tcb_ins_rdylist ( OS_TCB *ptcb )
{
	WORD	prio	=	ptcb->prio;

   	ptcb->task_stat		= OS_TASK_RDY;        

	if ( os_tcb_run == NULL )
	{
		os_tcb_run = ptcb->next_rdy	= ptcb->prev_rdy = ptcb;
		os_tcb_current_prio		=	prio;
		os_tcb_curprio [ prio ]	= os_tcb_run;
	}
	else
		if ( os_tcb_curprio [ prio ] == NULL )
			os_tcb_curprio [ prio ] = ptcb->next_rdy = ptcb->prev_rdy = ptcb;
		else
		{
			os_tcb_curprio [ prio ]->prev_rdy->next_rdy	= ptcb;
			ptcb->prev_rdy				 				= os_tcb_curprio [ prio ]->prev_rdy;
			os_tcb_curprio [ prio ]->prev_rdy			= ptcb;	
			ptcb->next_rdy								= os_tcb_curprio [ prio ];
		}
}

/*
--------------------------------------------------------------------------------
                           DELETE A TASK FROM READY LIST 					   
--------------------------------------------------------------------------------
*/

void os_tcb_del_rdylist ( OS_TCB *ptcb )
{
	WORD	prio	=	ptcb->prio;

	if ( ptcb == os_tcb_run )
	{
		if ( os_tcb_cur->next_rdy != os_tcb_run )
		{	//change os_tcb_run ( round robin )
			os_tcb_run					= os_tcb_run->next_rdy;
			os_tcb_run->task_stat		= OS_TASK_RUN;
			os_tcb_curprio [ prio ]		= os_tcb_run;
			ptcb->prev_rdy->next_rdy	= ptcb->next_rdy;
			ptcb->next_rdy->prev_rdy	= ptcb->prev_rdy;
		}
		else	//was the only task in this priority
		{
			int	i;

			os_tcb_curprio [ prio ]	= NULL;
			
			//search next highest priority task ready tu run
			for ( i=0 ; i < OS_LO_PRIO; i++ )
				if ( os_tcb_curprio [ i ] != NULL )
				{
					os_tcb_run				= os_tcb_curprio [ i ];
					os_tcb_current_prio		= i;	
					os_tcb_run->task_stat	= OS_TASK_RUN;
					break;
				}
		}
	}
	else
	{
		if ( ptcb->next_rdy == ptcb )//was the only task in this priority?
			os_tcb_curprio [ prio ]	= NULL;
		else
		{		
			ptcb->prev_rdy->next_rdy	= ptcb->next_rdy;
			ptcb->next_rdy->prev_rdy	= ptcb->prev_rdy;
		}
	}
}

/*
--------------------------------------------------------------------------------
                           INSERT A TASK IN AN EVENT LIST					   
--------------------------------------------------------------------------------
*/

void os_tcb_ins_evnlist ( OS_TCB *ptcb )
{
	OS_ECB	*pecb	=	ptcb->pecb;

	if ( pecb->os_tcb_blk_task == NULL ||
					ptcb->prio < pecb->os_tcb_blk_task->prio )
	{
		ptcb->next_blk			= pecb->os_tcb_blk_task;
		pecb->os_tcb_blk_task	= ptcb;
	}
	else
	{
		OS_TCB	*p;

		for ( p = pecb->os_tcb_blk_task; p != NULL; p=p->next_blk )
			if ( ptcb->prio < p->prio || p->next_blk == NULL )
			{
				ptcb->next_blk		= p->next_blk;
				p->next_blk			= ptcb;
				break;
			}			
	}
}

/*
--------------------------------------------------------------------------------
                           PURGE AN EVENT LIST           					   
--------------------------------------------------------------------------------
*/

void os_tcb_purge_evnlist ( OS_ECB *pecb )
{
	OS_TCB	*ptcb	= pecb->os_tcb_blk_task;

	for (; ptcb != NULL; ptcb=ptcb->next_blk )
	{
		ptcb->pecb	= NULL;
		os_tcb_ins_rdylist ( ptcb );
	}

	pecb->os_tcb_blk_task	= NULL;
}

/*
--------------------------------------------------------------------------------
                           DELETE A TASK FROM AN EVENT LIST    			   
--------------------------------------------------------------------------------
*/

void os_tcb_del_evnlist ( OS_ECB *pecb )
{
	if ( pecb->os_tcb_blk_task != NULL )
	{
		os_tcb_ins_rdylist ( pecb->os_tcb_blk_task );
		pecb->os_tcb_blk_task		= pecb->os_tcb_blk_task->next_blk;
	}
}

/*
--------------------------------------------------------------------------------
                           INSERT A TASK IN DELAY LIST   					   
--------------------------------------------------------------------------------
*/

void os_tcb_ins_dlylist ( OS_TCB *ptcb )
{
	if ( os_tcb_dlylist == NULL )
	{
		ptcb->next_dly			= NULL;
		os_tcb_dlylist			= ptcb;
	}
	else if ( os_tcb_dlylist->dly_time > ptcb->dly_time )
	{
		ptcb->next_dly				= os_tcb_dlylist;
		os_tcb_dlylist->dly_time	-= ptcb->dly_time;
		os_tcb_dlylist				= ptcb;
	}
	else
	{
		OS_TCB	*p		= os_tcb_dlylist;
		TIME	time	= 0;

		for ( ; p != NULL; p=p->next_dly )
		{
			time	+= p->dly_time;
			if ( p->next_dly == NULL )
			{
				ptcb->dly_time	-= time;
				p->next_dly		= ptcb;
				ptcb->next_dly	= NULL;
				break;
			}
			else if ( ( time + p->next_dly->dly_time ) > ptcb->dly_time )
			{
				ptcb->dly_time			-= time;
				p->next_dly->dly_time	-= ptcb->dly_time;
				ptcb->next_dly			= p->next_dly;
				p->next_dly				= ptcb;
				break;
			}
		}
	 }
}

/*
--------------------------------------------------------------------------------
                           DELETE A TASK FROM DELAY LIST 					   
--------------------------------------------------------------------------------
*/

void os_tcb_del_dlylist ( OS_TCB *ptcb )
{
	OS_TCB	*aux_ptcb	= os_tcb_dlylist;

	if ( aux_ptcb == ptcb )
	{
		 os_tcb_dlylist					= aux_ptcb->next_dly;
		aux_ptcb->next_dly->dly_time	+= aux_ptcb->dly_time;
	}
	else
	 	for ( ; aux_ptcb != NULL; aux_ptcb=aux_ptcb->next_dly )
			if ( aux_ptcb->next_dly == ptcb )
			{
				aux_ptcb->next_dly			= ptcb->next_dly;
				if ( ptcb->next_dly != NULL )
					ptcb->next_dly->dly_time	+= ptcb->dly_time;
				ptcb->next_dly				= NULL;
				break;
			}
}

/*
--------------------------------------------------------------------------------
                           DELETE A TASK FROM EVENT LIST 					   
--------------------------------------------------------------------------------
*/

void os_tcb_quit_evnlist ( OS_TCB *ptcb )
{
	OS_ECB	*pecb		= ptcb->pecb;
	OS_TCB	*auxptcb	= pecb->os_tcb_blk_task;

	if ( auxptcb == ptcb )
	{
		pecb->os_tcb_blk_task	=	ptcb->next_blk;
		ptcb->next_blk			=	NULL;
		ptcb->pecb				=	NULL;
		return;
	}
	
	for (;  auxptcb != NULL; auxptcb = auxptcb->next_blk )
		if ( auxptcb->next_blk == ptcb )
		{
			auxptcb->next_blk		= ptcb->next_blk;
			ptcb->next_blk			= NULL;
			ptcb->pecb				= NULL;
			break;
		}
}

//end of file
