#ifndef	util_h
#define util_h

#include "rtkernel.h"

void	disp_char (BYTE x, BYTE y, char  c);
void	disp_str (BYTE x,  BYTE y, char *s);
void 	scroll_down( BYTE lines, BYTE x1, BYTE y1, BYTE x2, BYTE y2, BYTE color );
void	scroll_up ( BYTE lines, BYTE x1, BYTE y1, BYTE x2, BYTE y2, BYTE color );
RETCODE	init_screen ( char *fname );
void	init_uart (long baud_rate, BYTE num_data_bits, BYTE num_stop_bits,\
					BYTE parity, unsigned base_address, BYTE irq_vect );
void	restore_int ( void );

#define	COM1_BASE_ADR		0x3f8
#define COM2_BASE_ADR		0x2f8

#define IRQ4_VECT			0x0c
#define	IRQ3_VECT			0x0b

typedef enum { PAR=0x00, IMPAR=0x01, NONE=0x02 };

// offsets de los registros de UART respecto de direccion base

typedef enum { RBR=0x00, THR=0x00, IER=0x01, MSB=0x01, LSB=0x00,
					 IIR=0x02, LCR=0x03, MCR=0x04, LSR=0x05, MSR=0x06  };

#endif


