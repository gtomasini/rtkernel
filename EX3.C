#include    <stdio.h>
#include    <string.h>
#include    <stdlib.h>
#include    <conio.h>
#include    <dos.h>

#include    "80186l.h"
#include    "rtkernel.h"
#include	"util.h"
/*
*********************************************************************************************************
                                      CONSTANTS
*********************************************************************************************************
*/

#define			TASK_STK_SIZE	1024                // Size of each task's stacks (# of bytes)      
#define			RX_Q_SIZE		255

#define			F1			0x3b
#define			F2			0x3c
#define			F3			0x3d
#define			F4			0x3e

#define			F10			0x44

#define			PAGE_UP		0x49
#define			PAGE_DOWN	0x51

/*
*********************************************************************************************************
                                        VARIABLES
*********************************************************************************************************
*/


OS_ECB		*rx_qp1;				// Pointer to rx queue                     
OS_ECB		*rx_qp2;				// Pointer to rx queue                     
OS_ECB		*tx_qp;					// pointer to tx queue			

OS_ECB        *key_mbox_ptr1;		// Pointer to keyboard mailbox                   
OS_ECB        *key_mbox_ptr2;		// Pointer to keyboard mailbox                   

OS_ECB        *dos_sem;				// Pointer to DOS  semaphore                 

char *message[]={	"press F1 to send message to task A   ",
					"press F2 to send message to task B   ",
					"press P_UP to increment system tick  ",
					"press P_DOWN to decrement system tick",
					"press F10 to exit                    ",
					"press any other key to send by tx1   "
				};


/*
*********************************************************************************************************
                                  FUNCTION PROTOTYPES
*********************************************************************************************************
*/

void far	key_task ( void *data );
void far	A_task ( void *data );
void far	B_task ( void *data );
void far	ran_task ( void *data );
void far	rx1_task ( void *data );
void far	rx2_task ( void *data );
void far	stat_task ( void *data );
void far	tx_task ( void *data );	

/*
*********************************************************************************************************
                                           MAIN
*********************************************************************************************************
*/

int main ( void )
{
	TASK_ID	task_id;

	init_screen ( "screen.txt" );
	init_uart ( 2400, 8, 1, NONE, COM1_BASE_ADR, IRQ4_VECT );
	init_uart ( 2400, 8, 1, NONE, COM2_BASE_ADR, IRQ3_VECT );
	os_init();
	key_mbox_ptr1	= os_mbox_create( NULL );			// Keyboard mailbox                         
	key_mbox_ptr2	= os_mbox_create( NULL );			// Keyboard mailbox                         
	dos_sem			= os_sem_create ( 1 );	   			// DOS  semaphore                       
	rx_qp1			= os_q_create ( RX_Q_SIZE, 1 );
	rx_qp2			= os_q_create ( RX_Q_SIZE, 1 );
	tx_qp			= os_q_create ( RX_Q_SIZE, 1 );
	os_task_create ( stat_task, NULL, 1024, 0, &task_id );
	os_start();
	os_q_dest ( rx_qp1 );
	os_q_dest ( rx_qp2 );
	os_q_dest ( tx_qp );
	clrscr();
	_setcursortype ( _NORMALCURSOR );
	return 0;
}

/*
*********************************************************************************************************
                                       KEY TASK 
*********************************************************************************************************
*/

void far key_task ( void *data )
{
    WORD	ctr;
    char	s[30];
	TASK_ID	task_id;
	int		loop=0;
	BYTE	cnt_dly=0;

    data	= data;	// Prevent compiler warning 
	task_id	= os_get_cur_taskid();

    ctr		= 0;

	sprintf ( s, "%d", task_id );
    disp_str ( 18, 2, s );

    while ( 1 )
	{
		int	key, key2;

        sprintf ( s, "%05d", ctr );
		disp_str ( 21, 2, s );
        ctr++;

		if ( kbhit() )
		{
			key = getch();

			if ( !key )
				switch ( getch() )	//extended code
				{
	                case F1:		//F1
						os_mbox_send ( key_mbox_ptr1, (void *)1);
	                    break;

	                case F2:		//F2
						os_mbox_send ( key_mbox_ptr2, (void *)2);
	                    break;

					case PAGE_UP:		//PAGE UP
						os_set_ticksize ( os_get_ticksize()+1 );
						break;

					case PAGE_DOWN:		//PAGE DOWN
						os_set_ticksize ( os_get_ticksize()-1 );
						break;

	                case F10:
						os_end();
				}
			else
				os_q_write ( tx_qp, &key );
        }
		if ( ++cnt_dly==30 )
		{
			cnt_dly	= 0;
			if ( ++loop == 6 )
				loop=0;
			disp_str ( 1, 3, message [ loop ] );
		}
        os_tick_dly ( 1 );
	}
}

/*
*********************************************************************************************************
                                     A TASK    
*********************************************************************************************************
*/

void far A_task(void *data)
{
    RETCODE	err;
    WORD	toctr;
    WORD	msgctr;
    char	s[30];
	TASK_ID	task_id;

    data	= data;
	task_id	= os_get_cur_taskid();
	sprintf ( s, "%d", task_id );
	disp_str ( 18, 6, s );
    msgctr = 0;
    toctr  = 0;
    while ( 1 )
	{
        sprintf ( s, "%05d", toctr );
		disp_str ( 34, 6, s );
        sprintf ( s, "%05d", msgctr );
        disp_str ( 34, 7, s );
		os_mbox_receive ( key_mbox_ptr1, 20, &err);
        switch ( err )
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


/*
*********************************************************************************************************
                                       B TASK  
*********************************************************************************************************
*/

void far B_task(void *data)
{
    RETCODE	err;
    WORD	toctr;
    WORD	msgctr;
    char	s[30];
	TASK_ID	task_id;

    data	= data;
	task_id	= os_get_cur_taskid();
	sprintf ( s, "%d", task_id );
	disp_str ( 18, 8, s );
    msgctr = 0;
    toctr  = 0;
    while ( 1 )
	{
        sprintf ( s, "%05d", toctr );
		disp_str ( 34, 8, s );
        sprintf ( s, "%05d", msgctr );
        disp_str ( 34, 9, s );
		os_mbox_receive ( key_mbox_ptr2, 20, &err);
        switch ( err )
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

/*
*********************************************************************************************************
                                   RANDOM TASK 
*********************************************************************************************************
*/

void far ran_task ( void *data )
{
    BYTE  	x, y, z;
	char 	s[30];
	TASK_ID	task_id;

    data 	= data;
	task_id	= os_get_cur_taskid();

	sprintf ( s, "task id %d:", task_id );
	disp_str ( 65, 2, s);

    while (1)
	{
		os_tick_dly (1);
        x = random ( 37 );		// Find X position where task number will appear      
        y = random ( 6 );		// Find Y position where task number will appear      
        z = random ( 4 );
		switch ( z )
		{
			case 0: 
				disp_char( x + 41, y + 4, '*' );
				break;

			case 1:
				disp_char( x + 41, y + 4, 'X' );
				break;

			case 2:
				disp_char( x + 41, y + 4, '#' );
				break;

			case 3:
				disp_char( x + 41, y + 4, ' ' );
				break;
		}
    }
}


/*
*********************************************************************************************************
                                      RX1 TASK 
*********************************************************************************************************
*/

#define	X_LW1	2
#define X_RW1	37
#define Y_TW1	20
#define Y_BW1	23

void far rx1_task ( void *data )
{
	BYTE status;
	BYTE x=X_LW1,y=Y_TW1;
	TASK_ID	task_id= os_get_cur_taskid();
	char	s[30];

	data = data;

	sprintf ( s, "%d:", task_id );
	disp_str ( 24, 18, s );

	while ( 1 )
	{
		RETCODE err;
		char	*rx_data;

		rx_data	= os_q_read ( rx_qp1, 0, &err );

		disp_char ( x , y, *rx_data );

		if ( ++x > X_RW1 )
		{
			x	= X_LW1;
			if ( ++y > Y_BW1 )
			{	
				y	= Y_BW1;
				scroll_up ( 1, Y_TW1, X_LW1, Y_BW1, X_RW1, 0 );
			}
		}
	}
}

/*
*********************************************************************************************************
                                        RX2 TASK
*********************************************************************************************************
*/

#define	X_LW2	42
#define X_RW2	78
#define Y_TW2	20
#define Y_BW2	23

void far rx2_task ( void *data )
{
	BYTE status;
	BYTE x=X_LW2,y=Y_TW2;
	TASK_ID	task_id= os_get_cur_taskid();
	char	s[30];

	data = data;

	sprintf ( s, "%d:", task_id );
	disp_str ( 65, 18, s );

	while ( 1 )
	{
		RETCODE err;
		char	*rx_data;

		rx_data	= os_q_read ( rx_qp2, 0, &err );

		disp_char ( x , y, *rx_data );

		if ( ++x > X_RW2 )
		{
			x	= X_LW2;
			if ( ++y > Y_BW2 )
			{	
				y	= Y_BW2;
				scroll_up ( 1, Y_TW2, X_LW2, Y_BW2, X_RW2, 0 );
			}
		}
	}
}

/*
*********************************************************************************************************
                                      TX TASK  
*********************************************************************************************************
*/

#define	X_LW_TX	42
#define X_RW_TX	78
#define Y_TW_TX	14
#define Y_BW_TX	15

void far tx_task ( void *data )
{
	BYTE status;
	BYTE x=X_LW_TX,y=Y_TW_TX;
	TASK_ID	task_id= os_get_cur_taskid();
	char	s[30];

	data = data;

	sprintf ( s, "%d:", task_id );
	disp_str ( 65, 12, s );

	while ( 1 )
	{
		RETCODE err;
		char	*tx_data;

		tx_data	= os_q_read ( tx_qp, 0, &err );
		outportb ( COM1_BASE_ADR+THR, *tx_data );

		disp_char ( x , y, *tx_data );

		if ( ++x > X_RW_TX )
		{
			x	= X_LW_TX;
			if ( ++y > Y_BW_TX )
			{	
				y	= Y_BW_TX;
				scroll_up ( 1, Y_TW_TX, X_LW_TX, Y_BW_TX, X_RW_TX, 0 );
			}
		}
	}
}

/*
*********************************************************************************************************
                                    STAT TASK  
*********************************************************************************************************
*/

void far stat_task ( void *data )
{
	double	max;
	char	s[30];
	TASK_ID	task_id;

	data	= data;

	os_tick_dly ( 10 );
    max       = (double)os_idle_ctr;
	os_task_create( key_task, NULL, 1024, 1, &task_id );
	os_task_create( A_task, NULL, 1024, 1, &task_id );
	os_task_create( B_task, NULL, 1024, 1, &task_id );
	os_task_create( ran_task, NULL, 1024, 1, &task_id );
	os_task_create( rx1_task, NULL, 1024, 1, &task_id );
	os_task_create( rx2_task, NULL, 1024, 1, &task_id );
	os_task_create( tx_task, NULL, 1024, 1, &task_id );

	task_id= os_get_cur_taskid();

	sprintf ( s, "%d:", task_id );
	disp_str ( 28, 12, s );

    while (1)
	{
	    LONG  idle;
    	WORD  ctxsw;
	    char   s[80];
	    double usage;
		struct dostime_t t;

        DISABLE();
        ctxsw			= os_ctx_sw_ctr;
        idle			= os_idle_ctr;
        os_ctx_sw_ctr	= 0;					// reset statistics counters               
        os_idle_ctr		= 0L;
        ENABLE();
        usage = 100.0 - (100.0 * (double)idle / max);	// Compute and display statistics        
        sprintf( s, "%d", ctxsw );
        disp_str( 17, 13, s);
		sprintf( s, "%5.2f %%", usage );
        disp_str( 32, 13, s);
        sprintf( s, "%7.0f / %7.0f ", (double)idle, max );
		disp_str( 12, 14, s );
		_dos_gettime(&t);
		sprintf( s, "%2d:%02d:%02d.%02d\n", t.hour, t.minute, t.second, t.hsecond);
		disp_str( 8, 15, s );
		sprintf( s, "%d", os_get_ticksize() );
		disp_str( 35, 15, s);
        os_tick_dly ( 10 );		// Wait 10 system ticks
    }
}


	








