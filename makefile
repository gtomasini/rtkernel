#
# rtKERNEL 1.00
#
# MAKEFILE
#
# FILE: rtk.mak
#
# 30-8-94
#
# Guillermo Pablo Tomasini
#
# Se crea una bliblioteca llamada rtkernel.lib apta para ser linkeada con aplicación de usuario
# modelo: large
.AUTODEPEND
#
*Translator Definitions*
CC = bcc +rtk.CFG
TASM = TASM
TLIB = tlib
TLINK = tlink
LIBPATH = c:\BC\LIB;d:\bc\lib
INCLUDEPATH = c:\BC\INCLUDE;d:\bc\include
#
*Explicit Rules*
rtkernel.lib:: rtk.cfg rtkernel.obj
tlib rtkernel /c -+rtkernel.obj
rtkernel.lib:: rtk.cfg i186l_a.obj
tlib rtkernel /c -+i186l_a.obj
rtkernel.lib:: rtk.cfg i186l_c.obj
tlib rtkernel /c -+i186l_c.obj
#
*Individual File Dependencies*
rtkernel.obj: rtk.cfg rtkernel.c
$(CC) -c rtkernel.c
i186l_c.obj: rtk.cfg i186l_c.c
$(CC) -c i186l_c.c
i186l_a.obj: rtk.cfg i186l_a.asm
$(TASM) /MX /ZI /O I186L_A.ASM,I186L_A.OBJ
#
#*Compiler Configuration File*
rtk.cfg: rtk.mak
copy &&|
-ml
127-1
-v
-G
-O
-Og
