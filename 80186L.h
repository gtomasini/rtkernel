#ifndef	i80186l_h
#define	i80186l_h
/*
*********************************************************************************************************
*                                             rtKERNEL
*                        Microcomputer Real-Time Multitasking KERNEL          
*
*                                       80186/80188 Specific code
*                                           LARGE MEMORY MODEL
*
* File : 80186L.H
*********************************************************************************************************
*/

/*
*********************************************************************************************************
*                                              CONSTANTS
*********************************************************************************************************
*/

#define  FALSE    0
#define  TRUE     1

/*
*********************************************************************************************************
*                                                MACROS
*********************************************************************************************************
*/
#define  DISABLE()            asm  cli
#define  ENABLE()             asm  sti

#define  OS_TASK_SW()         asm  INT   rtKERNEL

/*
*********************************************************************************************************
*                                              DATA TYPES
*********************************************************************************************************
*/

typedef unsigned char  BOOLEAN;
typedef unsigned char  BYTE;                    // Unsigned  8 bit quantity                           
typedef signed   char  SBYTE;                   // Signed    8 bit quantity                           
typedef unsigned int   WORD;                    // Unsigned 16 bit quantity                           
typedef signed   int   SWORD;                   // Signed   16 bit quantity                           
typedef unsigned long  LONG;                    // Unsigned 32 bit quantity                           
typedef signed   long  SDWORD;                  // Signed   32 bit quantity                           

#endif
