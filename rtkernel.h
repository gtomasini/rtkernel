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
#ifndef	rtkernel_h
#define	rtkernel_h

#include	"80186l.h"

/*
--------------------------------------------------------------------------------
                               rtKERNEL CONFIGURATION         				    
--------------------------------------------------------------------------------
*/

#define rtKERNEL				0x80  // Interrupt vector assigned to rtKERNEL                           

#define OS_MAX_TASKS			64	// Maximum number of tasks in your application                 
#define OS_MAX_EVENTS			20	// Maximum number of event control blocks in your application  
#define OS_MAX_QS				5	// Maximum number of queue control blocks in your application  

#define OS_IDLE_TASK_STK_SIZE	1024    // Idle task stack size (BYTEs)                                

#define	OS_LO_PRIO					4         // IDLE task priority                                           

/*
--------------------------------------------------------------------------------
                               rtKERNEL ERRROR CODES          				    
--------------------------------------------------------------------------------
*/

#define OS_NO_ERR            0         // ERROR CODES                                                  
#define OS_TIMEOUT          10
#define	OS_NO_MEMORY		11
#define OS_MBOX_FULL        20
#define OS_Q_FULL           30
#define OS_Q_NULL			31
#define OS_PRIO_ERR         41
#define OS_SEM_ERR          50
#define OS_SEM_OVF          51
#define OS_TASK_DEL_ERR     60
#define OS_TASK_DEL_IDLE    61
#define OS_NO_MORE_TCB      70
#define	OS_TSK_NO_EXIST		71
#define OS_INTERNAL_ERROR	80
#define OS_NULL_PECB		81		
#define OS_BAD_TICKSIZE		90

#define	NULL_ID				-1
#define NULL_PRIO			-1


/*$PAGE*/
/*
--------------------------------------------------------------------------------
                               EVENT CONTROL BLOCK (ECB)      				    
--------------------------------------------------------------------------------
*/

typedef WORD	TASK_ID;
typedef SBYTE	PRIO;
typedef	LONG	TIME;

typedef WORD	RETCODE;

typedef struct os_event
{
	SWORD			os_event_cnt;   	// count of used when event is a semaphore                      
    void			*os_event_ptr;		// pointer to message or queue structure
//	WORD			os_tsk_cnt;			// blocked task's number in this event
	struct os_tcb	*os_tcb_blk_task;	// first task blocked in this event   
}					OS_ECB;

/*
--------------------------------------------------------------------------------
                               rtKERNEL TASK CONTROL BLOCK (TCB)      		    
--------------------------------------------------------------------------------
*/

// TASK STATUS                                                  

typedef enum task_state
{
	OS_TASK_RUN,		// running
	OS_TASK_RDY,		// ready to run        
	OS_TASK_SEM,		// pending on semaphore
	OS_TASK_MBOX,		// pending on mailbox  
	OS_TASK_Q,			// pending on queue
	OS_TASK_DLY,		// task delayed
	OS_TASK_NULL		// task no exist
};

typedef struct os_tcb 
{
    void far		*stack_ptr;		// pointer to current top of stack                             
	void far		*allocmem_ptr;	// pointer to alloc memory
    enum task_state	task_stat;		// task status                                                 
    PRIO			prio;			// task priority (0 == highest, 3 == lowest)
    TASK_ID			task_id;		// task id
    TIME			dly_time;		// nbr. ticks to delay task or, timeout waiting for event       
    OS_ECB			*pecb;			// pointer to event control block (ECB)
	WORD			timeout_flag:1;	// 1 if task was timeouted
	struct os_tcb	*next_free;     // ptr. to next TCB in the TCB free list                     
	struct os_tcb	*next_rdy;		// ptr. to next task ready in the same prio. level
	struct os_tcb	*prev_rdy;		// ptr. to prev. task ready in the same prio. level
	struct os_tcb	*next_blk;		// ptr. to next blocked task if exist
	struct os_tcb	*next_dly;		// ptr. to next delayed task if exist
}									OS_TCB;

/*
--------------------------------------------------------------------------------
                               rtKERNEL QUEUE CONTROL BLACK (QCB)               
--------------------------------------------------------------------------------
*/

typedef struct os_q
{
    struct os_q	*os_qcb_ptr;		// link to next queue control block in list of free blocks  
    void		*qstart;			// pointer to start of queue data                        
    BYTE		tail_offset;        // offset to where the next element will be inserted 
    BYTE		head_offset;   		// offset to where the next element will be extracted
    BYTE		elem_size;			// element size
	WORD		qsize;				// size of queue (maximum number of entries)              
    WORD		entries;			// current number of entries in the queue                       
}				OS_QCB;

/*$PAGE*/
/*
--------------------------------------------------------------------------------
                               rtKERNEL GLOBAL VARIABLES      				    
--------------------------------------------------------------------------------
*/

									// SYSTEM VARIABLES                            
extern WORD     os_ctx_sw_ctr;		// Counter of number of context switches       
extern LONG		os_idle_ctr;		// Idle counter                                
extern WORD		os_running;			// Flag indicating that kernel is running      
extern OS_TCB   *os_tcb_cur;		// Pointer to currently running TCB            
extern OS_TCB   *os_tcb_high_rdy;	// Pointer to highest priority TCB ready to run
extern OS_TCB   *prio_tbl[];		// Table of pointers to all created TCBs       
extern WORD		divisor;

/*
--------------------------------------------------------------------------------
                               rtKERNEL EXPORTABLE SERVICES   				    
--------------------------------------------------------------------------------
*/

void		os_init				( void );
void		os_start			( void );
void		os_end				( void );
void		os_start_run		( void );
void		os_sched			( void );
void    	os_sched_lock		( void );
void    	os_sched_unlock		( void );

RETCODE   	os_task_create		( void (far *task)(void *pd), void *pdata, WORD stksize, PRIO prio, TASK_ID *tsk_id );
RETCODE   	os_task_del			( TASK_ID tsk_id );
RETCODE    	os_task_change_prio ( TASK_ID tsk_id, PRIO newp);
void		os_task_end			( void );

void		os_tick_dly			( LONG ticks );
void		os_msec_dly			( LONG msecs ); 
void		os_time_tick		( void );
void		os_time_set			( LONG ticks );
LONG		os_time_get			( void );

RETCODE		os_set_ticksize		( WORD ticksize );
WORD		os_get_ticksize		( void );

OS_ECB		*os_sem_create		( SWORD value );
RETCODE		os_sem_signal		( OS_ECB *pevent );
RETCODE		os_sem_wait			( OS_ECB *pevent, TIME timeout );

OS_ECB		*os_mbox_create		( void *msg );
RETCODE		os_mbox_send		( OS_ECB *pevent, void *msg );
void		*os_mbox_receive	( OS_ECB *pevent, TIME timeout, RETCODE *err );

OS_ECB		*os_q_create		( WORD qsize, BYTE esize );
BYTE		os_q_write			( OS_ECB *pevent, void *msg );
void		*os_q_read			( OS_ECB *pevent, TIME timeout, RETCODE *err );

RETCODE		os_event_dest		( OS_ECB *pecb	);

TASK_ID		os_get_cur_taskid	( void );

#define		os_sem_dest( pecb )		os_event_dest( pecb )
#define		os_mbox_dest( pecb )	os_event_dest( pecb )
RETCODE 	os_q_dest ( OS_ECB *pecb );

/*
--------------------------------------------------------------------------------
                               rtKERNEL EXPORTABLE FUNCTIONS  				    
--------------------------------------------------------------------------------
*/

void    os_int_enter		( void );
WORD    os_int_exit			( void );
RETCODE	os_tcb_init			( PRIO priority, void far *stk, void far *mem, TASK_ID *tsk_id );

#endif




