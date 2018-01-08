/*****************************************************************************
  FileName:        ustaw_zegar.c
  Processor:       PIC24HJ128GP502
  Compiler:        XC16 ver 1.30
 *****************************************************************************/
    /*Ustawiamy zegar wewnetrzny na ok 40 MHz (dokladnie na 39.61375 MHz*/
    #include "xc.h" /* wykrywa rodzaj procka i includuje odpowiedni plik 
    naglówkowy "p24HJ128GP502.h"*/
    #include <stdio.h>
    #include <stdlib.h>
    #include <stdint.h> /*definicje typów uint8_t itp*/
    #include "ustaw_zegar.h" /*z uwagi na FCY musi byc przed #include <libpic30.h>*/

    /*definicja funkcji*/
    void ustaw_zegar(void) {
    /*
    * to co mozemy ustawic za pomoca '#pragma' jest dostepne w pliku 
    * xc16/docs/config_index.html
    */
    #pragma config JTAGEN = OFF
    // Watchdog timer enable/disable by user software
    #pragma config FWDTEN = OFF 

    //********************Start Ustawien Zegara************************
    /* 
    * Fcy - zegar instrukcji , Fosc - zegar rdzenia (jest zawsze dwa razy wiekszy 
    * od zegara instrukcji)) Ustawiamy zegar instrukcji na 40 Mhz z wewnetrznego 
    * oscylatora Fin=7.37 MHz w/g wzoru Fcy=Fosc/2 gdzie Fosc=Fin x (M/(N1+N2))
    * gdzie M=43, N2=2, N1=2 ustawiane w rejestrach PLLFBD/PLLPOST/PLLPRE
    */
    //Select Internal FRC (Fast RC Oscillator)
    #pragma config FNOSC = FRC // FOSCSEL-->FNOSC=0b000 (Fast RC Oscillator (FRC))
    //Enable Clock Switching and Configure
    #pragma config FCKSM = CSECMD //FOSC-->FCKSM=0b01 - wlacz zegar
    #pragma config OSCIOFNC = OFF //FOSC-->OSCIOFNC=1 - Fcy b?dzie na pinie OSCO

    /*Config PLL prescaler, PLL postscaler, PLL divisor*/
    PLLFBD = 41 ; //M=43 (0 bit to 2 st?d 41 = 43 patrz w rejestrze), tutaj 3.685 x 43 = 158.455MHz
    CLKDIVbits.PLLPRE=0 ;  //N1=2, tutaj 7.37 MHz / 2 = 3.685 MHz
    CLKDIVbits.PLLPOST=0 ; //N2=2, tutaj 158.455 MHz / 2 = 79.2275 MHz (Fosc)
    /*   
    * UWAGA przerwania musza byc wylaczone podczas wywolywania ponizszych 
    * dwóch funkcji __builtin_write_...brak definicji w pliku naglówkowym
    * to wewnetrzne funkcje kompilatora patrz help M-LAB IDE
    * i datasheet str 140(11.6.3.1 Control Register Lock)
    */
    /*Initiate Clock Switch to Internal FRC with PLL (OSCCON-->NOSC = 0b001)*/
      __builtin_write_OSCCONH(0x01); //tutaj argumentem jest wartosc z NOSC
    /*Start clock switching*/
      __builtin_write_OSCCONL(0x01);

    /*Wait for Clock switch to occur*/
      while(OSCCONbits.COSC !=0b001);

      /*Wait for PLL to lock*/
      while(OSCCONbits.LOCK !=1) {};

    }