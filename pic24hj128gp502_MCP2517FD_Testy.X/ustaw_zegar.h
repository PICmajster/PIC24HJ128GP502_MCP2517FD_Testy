/*****************************************************************************
  FileName:        ustaw_zegar.h
  Processor:       PIC24HJ128GP502
  Compiler:        XC16 ver 1.30
******************************************************************************/

#ifndef USTAW_ZEGAR_H
#define	USTAW_ZEGAR_H

#define FCY 40000000 /* podajemy wartosc ustawionego zegara (40 MHz), wazne 
aby przed includowaniem <libpic30.h>, potrzebne to jest do wyliczania delay-i*/
/*deklaracja funkcji*/
void ustaw_zegar(void) ;

#endif	/* USTAW_ZEGAR_H */

