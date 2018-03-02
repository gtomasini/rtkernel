#include    <conio.h>
#include	<stdio.h>
#include	<dos.h>

#include    "80186l.h"
#include	"rtkernel.h"
#include	"util.h"


/*
*********************************************************************************************************
                                      CONSTANTS
*********************************************************************************************************
*/

#define	VIDEO				0x10

#define	RESET_DLAB(base_address)	outportb(base_address+LCR, inportb(base_address+LCR) & ~0x80)
#define SET_DLAB(base_addres)		outportb(base_address+LCR, inportb(base_address+LCR) & ~0x80)

#define	BASE_ADDRESS_8259	0x20
#define	END_OF_INTERRUPT	0x20
#define	CONTROL_WORD1		0x01
#define	CONTROL_WORD2		0x00

#define LINE_BUFFER_LEN		100

/*
*********************************************************************************************************
                                  FUNCTION PROTOTYPES
*********************************************************************************************************
*/

void goto_xy ( BYTE x, BYTE y, BYTE page );
void write_char ( BYTE character, WORD nbr, BYTE page, BYTE color );

static void interrupt my_uart_handler(void);
static void interrupt (*old_irq_isr)(void);

/*
*********************************************************************************************************
                                        VARIABLES
*********************************************************************************************************
*/

extern OS_ECB		*dos_sem;			// Pointer to display  semaphore                 
extern OS_ECB		*rx_qp1;					
extern OS_ECB		*rx_qp2;					

/*
*********************************************************************************************************
                               DISPLAY CHARACTER FUNCTION
*********************************************************************************************************
*/

void disp_char ( BYTE x, BYTE y, char c )
{
	os_sem_wait ( dos_sem, 0 );
	goto_xy ( x , y, 0 );
	putchar ( c );
	os_sem_signal ( dos_sem );
}

/*
*********************************************************************************************************
                                    DISPLAY STRING FUNCTION
*********************************************************************************************************
*/

void disp_str( BYTE x, BYTE y, char *s )
{
	os_sem_wait ( dos_sem, 0 );
    goto_xy ( x , y, 0 );
    puts ( s );
	os_sem_signal ( dos_sem );
}

/*
*********************************************************************************************************
                                 INIT UART FUNCTION
*********************************************************************************************************
*/

void init_uart ( long baud_rate, BYTE num_data_bits, BYTE num_stop_bits,
					BYTE parity, unsigned base_address, BYTE irq_vect )
{
	unsigned int divisor	=	115200/baud_rate;
	BYTE IIR_var;

	RESET_DLAB ( base_address );	//DLAB=0

	if ((inportb(base_address + IER) & 0xf0) != 0x00)
		fprintf(stderr, "No esta presente COM en I/O port %04x\n", base_address);

	// deshabilita las interrupciones

	RESET_DLAB(base_address);

	outportb(base_address+IER, 0x00);
	// limpia las interrupciones
	inportb(base_address+IIR);
	inportb(base_address+RBR);
	inportb(base_address+LSR);
	inportb(base_address+MSR);

	// BAUD RATE -------------------------------------------------------

	SET_DLAB(base_address);  		// DLAB = 1
	outportb(base_address + LSB, (BYTE) divisor);
	outportb(base_address + MSB, (BYTE)((divisor & 0xff00)>>8));

	// LINE CONTROL (8, par, etc... ) -------------------------------------

	// cantidad de BITS
	// los ultimos dos bits en cero

	RESET_DLAB(base_address);
	outportb(base_address+LCR, inportb(base_address+LCR)&~0x03);

	switch(num_data_bits)
	{
		case 8:
			outportb(base_address+LCR, inportb(base_address+LCR)|0x03);
			break;

		case 7:
			outportb(base_address+LCR, inportb(base_address+LCR)|0x02);
			break;

		case 6:
			outportb(base_address+LCR, inportb(base_address+LCR)|0x01);
			break;

		case 5:
			outportb(base_address+LCR, inportb(base_address+LCR)|0x00);
			break;

		default:
			fprintf(stderr, "ERROR EN num_data_bits, adopto 8\n");
			outportb(base_address+LCR, inportb(base_address+LCR)|0x03);
		}

	// STOP BITS
	switch(num_stop_bits)
	{
		case 1:
			outportb(base_address+LCR, inportb(base_address+LCR)&~0x04);
			break;

		case 2:
			outportb(base_address+LCR, inportb(base_address+LCR)|0x04);
			break;

		default:
			fprintf(stderr, "ERROR EN num_stop_bits, adopto 1\n");
			outportb(base_address+LCR, inportb(base_address+LCR)&~0x04);
			break;
	}

	switch(parity)
	{
		case NONE:
			outportb(base_address+LCR, inportb(base_address+LCR)&~0x08);
			break;

		case PAR:
			outportb(base_address+LCR, inportb(base_address+LCR)|0x18);
			break;

		case IMPAR:
			outportb(base_address+LCR, inportb(base_address+LCR)&~0x10);
			outportb(base_address+LCR, inportb(base_address+LCR)|0x08);
			break;

		default:
			fprintf(stderr, "ERROR EN parity, adopto paridad PAR\n");
			outportb(base_address+LCR, inportb(base_address+LCR)|0x18);
		}

	// SET BREAK DISABLED y STICK PARITY DISABLED
	outportb(base_address+LCR, inportb(base_address+LCR)&~0x60);

	// FIN DE LINE CONTROL

	// LIMPIA INTERRUPCIONES 8250
	while ((IIR_var=inportb(base_address+IIR)) != 0x01)
	{
		#ifdef DEBUG
		printf("IIR=%02x\n", IIR_var);
		#else
		(void) IIR_var;
		#endif
		inportb(base_address + LSR);
		inportb(base_address + RBR);
		inportb(base_address + MSR);
	}

	#ifdef DEBUG
	printf("IIR=%02x\n", IIR_var);
	#endif

	//INTERRUPT ENABLE REGISTER
	//enable tx & rx interrupt 

	RESET_DLAB(base_address);
	outportb(base_address+IER, 0x01);

	//Loop Back en off
	outportb(base_address+MCR, inportb(base_address+MCR) & ~0x10);

	outportb(base_address+MCR, inportb(base_address+MCR)|0x0c);

	//enable 8253

	disable();
	
	switch ( irq_vect )
	{
		case IRQ4_VECT:
			outportb ( BASE_ADDRESS_8259+CONTROL_WORD1,
						inportb ( BASE_ADDRESS_8259+CONTROL_WORD1 ) &  ~0x10);
			break;

		case IRQ3_VECT:
			outportb ( BASE_ADDRESS_8259+CONTROL_WORD1,
						inportb ( BASE_ADDRESS_8259+CONTROL_WORD1 ) &  ~0x08);
			break;

		default:
			fprintf(stderr, "ERROR IN 8253 interrupt enbable.\n");
	}

	old_irq_isr = getvect ( irq_vect );
	setvect ( irq_vect, my_uart_handler );
}

/*
*********************************************************************************************************
                           MY_UART_HANDLER FUNCTION
*********************************************************************************************************
*/

void interrupt my_uart_handler ( void )
{
	BYTE	rx_data;
	BYTE	status;
	
//	os_int_enter();
	status	= inportb ( COM1_BASE_ADR+LSR );

	if ( (status & 0x01) && !(status & 0x0c) )
	{
		rx_data	= inportb ( COM1_BASE_ADR+RBR );		//lee RBR
		os_q_write ( rx_qp1, &rx_data );		

		//se recibi¢ algo no hay parity error ni framming error
		//lo pongo en la cola de recepci¢n y contesto
	}

	status=inportb ( COM2_BASE_ADR+LSR );

	if ( (status & 0x01) && !(status & 0x0c) )
	{
		rx_data	= inportb ( COM2_BASE_ADR+RBR );		//lee RBR

		os_q_write ( rx_qp2, &rx_data );		
		//se recibi¢ algo no hay parity error ni framming error
		//lo pongo en la cola de recepci¢n y contesto
	}
//	os_int_exit();
	outportb ( CONTROL_WORD2+BASE_ADDRESS_8259, END_OF_INTERRUPT );
	enable();
}


/*
*********************************************************************************************************
                                    GOTO_XY FUNCTION
*********************************************************************************************************
*/

void goto_xy ( BYTE x, BYTE y, BYTE page )
{
	union REGS regs;

	regs.h.ah = 2;		// set cursor position 
	regs.h.dh = y;
	regs.h.dl = x;
	regs.h.bh = page;  /* video page 0 */
	int86 ( VIDEO, &regs, &regs );
}


/*
*********************************************************************************************************
                               SCROLL_UP FUNCTION
*********************************************************************************************************
*/

void scroll_up ( BYTE lines, BYTE x1, BYTE y1, BYTE x2, BYTE y2, BYTE color )
{
	union REGS regs;

	regs.h.ah = 6;  /* set cursor position */
	regs.h.al = lines;
	regs.h.ch = x1;
	regs.h.cl = y1;
	regs.h.dh = x2;
	regs.h.dl = y2;
	regs.h.bh = color;  /* video page 0 */
    os_sem_wait ( dos_sem, 0 );
	int86(VIDEO, &regs, &regs);
	os_sem_signal ( dos_sem );
}


/*
*********************************************************************************************************
                               SCROLL_DOWN FUNCTION
*********************************************************************************************************
*/

void scroll_down ( BYTE lines, BYTE x1, BYTE y1, BYTE x2, BYTE y2, BYTE color )
{
	union REGS regs;

	regs.h.ah = 7;  /* set cursor position */
	regs.h.al = lines;
	regs.h.ch = x1;
	regs.h.cl = y1;
	regs.h.dh = y2;
	regs.h.dl = x2;
	regs.h.bh = color;  /* video page 0 */
    os_sem_wait ( dos_sem, 0 );
	int86(VIDEO, &regs, &regs);
    os_sem_signal ( dos_sem );
}

/*
*********************************************************************************************************
                               WRITE_CHAR FUNCTION
*********************************************************************************************************
*/

void write_char ( BYTE character, WORD nbr, BYTE page, BYTE color )
{
	union REGS regs;

	regs.h.ah = 0x0a;	// write char
	regs.h.al = character;
	regs.x.cx = nbr;
	regs.h.bl = color;
	regs.h.bh = page;	// video page 0 
    os_sem_wait ( dos_sem, 0 );
	int86 ( VIDEO, &regs, &regs );
    os_sem_signal ( dos_sem );
}


/*
*********************************************************************************************************
                                 INIT_SCREEN FUNCTION
*********************************************************************************************************
*/

RETCODE init_screen ( char *fname )
{
	FILE	*fp;
	char	buffer [ LINE_BUFFER_LEN ];
	int		line=0;

	if ( ( fp=fopen( fname, "r" ) ) == NULL )
		return -1;

	clrscr();
	_setcursortype(_NOCURSOR);	//avoid the cursor

    while ( fgets ( buffer, LINE_BUFFER_LEN, fp ) )
	{	
		buffer[80]	= '\0';
		if ( line++ )
			printf ( "%s", buffer );
	}
	scroll_down ( 1, 0, 0, 79, 24, 0 );
	fseek ( fp, 0, SEEK_SET );
    fgets ( buffer, LINE_BUFFER_LEN, fp );
	buffer[80]	= '\0';
	gotoxy ( 1, 1 );
	puts ( buffer );
	fclose ( fp );
	return 0;
}
	


