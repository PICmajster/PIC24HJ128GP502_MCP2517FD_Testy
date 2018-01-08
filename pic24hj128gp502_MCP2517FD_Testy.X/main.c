/*****************************************************************************
  FileName:        main.c
  Processor:       PIC24HJ128GP502
  Compiler:        XC16 ver 1.30
  Created on:      8 stycznia 2018, 13:02
 ******************************************************************************/

#include "xc.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h> /*definicje typu uint8_t itp*/
#include <string.h>
#include "ustaw_zegar.h" /*tutaj m.in ustawione FCY*/
#include <libpic30.h>
#include <p24HJ128GP502.h> /*dostep do delay-i,musi byc po zaincludowaniu ustaw_zegar.h*/
#include "drv_canfdspi_register.h"

#define MAX_DATA_BYTES 64
#define SPI_DEFAULT_BUFFER_LENGTH 96
/*instrukcje do dzialania z kontrolerem CAN*/
#define cINSTRUCTION_RESET	0x00
#define cINSTRUCTION_WRITE  0x02
#define cINSTRUCTION_READ	0x03
/*wybrany rejestr do testu zapisu i odczytu*/
#define cREGADDR_CiFLTCON0   0x1D0 /*adres rejestru kontrolera CAN*/
/*miganie dioda LED - */
#define LED1_TOG PORTA ^= (1<<_PORTA_RA1_POSITION) /*zmienia stan bitu na przeciwny*/

void MCP2517FD_TEST_REGISTER_ACCESS(void) ;
void DRV_CANFDSPI_WriteByteArray(uint16_t address,uint8_t *txd, uint16_t nBytes);
void DRV_CANFDSPI_ReadByteArray(uint16_t address, uint8_t *rxd, uint16_t nBytes);
void DRV_SPI_TransferData(uint16_t spiTransferSize);
void DRV_CANFDSPI_Reset(void);
void SPI_CS_DESELECT(void);
void SPI_CS_SELECT(void);

void config_DMA0_SPI1(uint16_t Size);
void config_DMA1_SPI1(uint16_t Size);
void config_SPI_MASTER(void);
// SPI Transmit buffer DMA
uint8_t BuforTX[SPI_DEFAULT_BUFFER_LENGTH] __attribute__((space(dma)));
// SPI Receive buffer DMA
uint8_t BuforRX[SPI_DEFAULT_BUFFER_LENGTH] __attribute__((space(dma)));
/*Bufory pomocnicze*/
uint8_t rxd[MAX_DATA_BYTES]; /*bufor pomocniczy do ktorego przepisujemy dane z DMA*/
uint8_t txd[MAX_DATA_BYTES]; /*bufor pomocniczy z ktorego przepisujemy dane do DMA*/

REG_CiFLTCON_BYTE foo ; /*tworzymy nowa Unia, do formowania zawartosci (1 bajtu) rejestru CiFLTCON*/
 
int main(void) {
    ustaw_zegar(); /*odpalamy zegar wewnetrzny na ok 40MHz*/
    __delay_ms(50); /*stabilizacja napiec*/
    /*
     * wylaczamy ADC , wszystkie piny chcemy miec cyfrowe
     * pojedynczo piny analogowe wylaczamy w rejestrze AD1PCFGL 
     * Po resecie procka piny oznaczone ANx sa w trybie analogowych wejsc.
     */
    PMD1bits.AD1MD = 1; /*wylaczamy ADC*/
    /* 
     * ustawiamy wszystkie piny analogowe (oznacznone ANx) jako cyfrowe
     * do zmiany mamy piny AN0-AN5 i AN9-AN12 co daje hex na 16 bitach = 0x1E3F
     */
    AD1PCFGL = 0x1E3F;
    
    TRISAbits.TRISA1 = 0 ; /*RA1 jako wyjscie tu mamy LED*/
    TRISBbits.TRISB7 = 0 ; /*RB7 jako wyjscie tu mamy CS*/
    /*remaping pinow na potrzeby SPI
    SDO --> pin 11
    SDI --> pin 14
    SCK --> pin 15
    */
    RPOR2bits.RP4R = 7;     /*inaczej _RP4R = 7*/
    RPINR20bits.SDI1R = 5;  /*inaczej _SDI1R = 5*/
    RPOR3bits.RP6R = 8;     /*inaczej _RP6R = 8*/
   
    /*SPI init with DMA*/
    config_SPI_MASTER();
    DRV_CANFDSPI_Reset();    
    /*tworzymy pojednyczy bajt rejestru CiFLTCON*/   
            foo.bF.BufferPointer = 0b01100 ;
            foo.bF.Enable = 1 ;
            /*foo.byte = 0b10001100  / dec --> 140;*/
               
      while (1) {

        /*Petla glowna programu*/
        MCP2517FD_TEST_REGISTER_ACCESS(); /*odpal test transferu danych*/ 
        __delay_ms(1000) ;
    }

}


void DRV_CANFDSPI_WriteByteArray(uint16_t address, uint8_t *txd, uint16_t nBytes)
{
    uint16_t i;
    uint16_t spiTransferSize = nBytes + 2;
    
    // Compose command
    BuforTX[0] = (uint8_t) ((cINSTRUCTION_WRITE << 4) + ((address >> 8) & 0xF));
    BuforTX[1] = (uint8_t) (address & 0xFF);

    // Add data
    for (i = 2; i < spiTransferSize; i++) {
        BuforTX[i] = txd[i - 2];
    }
/*tu odpalamy kanal DMA z danymi*/
    DRV_SPI_TransferData(spiTransferSize);

}

void DRV_CANFDSPI_ReadByteArray(uint16_t address, uint8_t *rxd, uint16_t nBytes)
{
    uint16_t i;
    uint16_t spiTransferSize = nBytes + 2;
    
    // Compose command
    BuforTX[0] = (uint8_t) ((cINSTRUCTION_READ << 4) + ((address >> 8) & 0xF));
    BuforTX[1] = (uint8_t) (address & 0xFF);

    // Clear data
    for (i = 2; i < spiTransferSize; i++) {
        BuforTX[i] = 0;
    }

    DRV_SPI_TransferData(spiTransferSize);

    // Update data 
    for (i = 0; i < nBytes; i++) {
        rxd[i] = BuforRX[i + 2];
    }

    }

void MCP2517FD_TEST_REGISTER_ACCESS(void) {
/*dane wpisujemy i odczytujemy z rejestru CiFLTCON0*/  
    /*rejestr sklada sie z 4 bajtów = 32bity*/
            uint8_t length , i;
            
            // wypelniamy bufory danymi*/
            for (length = 0; length < 4; length++) {
                    txd[length] = foo.byte ; /*powielamy 4 razy bajt bo rejestr ma 32bity */
                    rxd[length] = 0xff; /*wypelniamy wartoscia*/
                }
                
            /*Write data to registers CiFLTCON0*/
             DRV_CANFDSPI_WriteByteArray(cREGADDR_CiFLTCON0, txd, length);
 __delay_ms(500) ; /*opoznimy zapis i odczyt rejestru aby lepiej bylo widac na analizatorze stanow logicznych*/
            /*Read data back from registers CiFLTCON0*/
             DRV_CANFDSPI_ReadByteArray(cREGADDR_CiFLTCON0, rxd, length);
 
             PORTAbits.RA1 = 0; /*zgas LED*/
             /*sprawdzamy poprawnosc danych zapisanych do kontrolera CAN i odebranych*/
             _Bool good = false ;
             for(i=0 ; i < length; i++ ) {
                 good = txd[i] == rxd[i] ;
                 if(!good) PORTAbits.RA1 = 1; /*zapal LED jesli brak zgodnosci danych nadanych i odebranych*/
             } 
            }
            


void DRV_SPI_TransferData(uint16_t spiTransferSize)
{
    
    /*Assert CS*/
    SPI_CS_SELECT(); /*ustaw 0 na pinie CS*/

        /*Send Data from DMA*/
        config_DMA1_SPI1(spiTransferSize);          /*Enable DMA1 Channel*/	
        config_DMA0_SPI1(spiTransferSize);          /*Enable DMA0 Channel*/
        DMA0REQbits.FORCE = 1;                      /*Manual Transfer Start*/
        while(DMA0REQbits.FORCE==1) {};    
        /*Uwaga czekamy az kanal odbiorczy DMA zostanie zamkniety*/
        while(DMA1CONbits.CHEN != 0) {} /*czekaj az kanal odbiorczy DMA1 po ostatniej transmisji  zostanie zamkniety/zwolniony*/
        /*od tego momentu dane odebrane z kontrolera CAN powinny byc dostepne w BuforRX[]*/        
    /*De-assert CS jest w przerwaniu odbiorczym DMA1*/
    
}

void DRV_CANFDSPI_Reset(void)
{
    uint16_t spiTransferSize = 2;
    
    /*Compose command*/
    BuforTX[0] = (uint8_t) (cINSTRUCTION_RESET << 4);
    BuforTX[1] = 0;

    DRV_SPI_TransferData(spiTransferSize);

    
}

void config_DMA0_SPI1(uint16_t Size){
/*=============================================================================
 Konfiguracja DMA kanal 0 do transmisji SPI w trybie One_Shot (bez powtorzenia)
===============================================================================
 DMA0 configuration
 Direction: Read from DMA RAM and write to SPI buffer
 AMODE: Register Indirect with Post-Increment mode
 MODE: OneShot, Ping-Pong Disabled*/
 
/* Rejestr DMA0CON
 * bit15    -0 chen/chanel --> disable
 * bit14    -1 size --> Byte    
 * bit13    -1 dir --> Read from DMA RAM address write to peripheral address
 * bit12    -0 half --> Initiate block transfer complete interrupt when all of the data has been moved
 * bit11    -0 nullw --> Normal operation
 * bit10-6  -Unimplemented raed as 0
 * bit5-4   -00 amode  --> Register Indirect with Post_Incerement mode
 * bit3-2   -Unimplemented read as 0
 * bit1-0   -01 mode --> OneShot, Ping-Pong modes disabled*/
DMA0CON = 0x6001 ;/*0x6001 - wartosc wynika z ustawienia bitow jak wyzej*/
DMA0CNT = Size - 1;/*ustal ile znaków do przeslania max 1024 bajty*/
/*IRQ Select Register,wskazujemy SPI1*/
DMA0REQ = 0x000A ; /*SPI1*/
/*Peripheral Adress Register*/
DMA0PAD =  (volatile unsigned int)&SPI1BUF ; /*rzutowanie typu i pobranie adresu rejestru SPI1BUF*/
/*wewnetrzna konstrukcja/funkcja kompilatora*/
DMA0STA = __builtin_dmaoffset(BuforTX) ;/*taka jest konstrukcja wskazania na bufor z danymi*/

IFS0bits.DMA0IF = 0 ; /*clear DMA Interrupt Flag */
IEC0bits.DMA0IE = 1 ; /*enable DMA Interrupt */

/*Wazne :kanal DMA moze byc otwarty dopiero po wpisaniu danych do rejestru DMASTA i DMAxCNT*/
DMA0CONbits.CHEN  = 1; /*Canal DMA0 enable*/

  /*po zakonczonym transferze automatycznie kanal DMA zostaje wylaczony, zmienia
   *sie wartosc rejestru DMAxCON na 16-tym bicie z "1" na "0". Aby ponownie odpalic transfer
   *nalezy wlaczyc ten bit DMAxCON.bits.CHEN=1 i odpalic transfer DMA0REQbits.FORCE = 1
   */
}

void config_DMA1_SPI1(uint16_t Size)
{
/*=============================================================================
 Konfiguracja DMA kanal 1 do odbioru SPI w trybie One_Shot (bez powtorzenia)
===============================================================================
 DMA1 configuration
 Direction: Read from SPI buffer and write to DMA RAM
 AMODE: Register Indirect with Post-Increment mode
 MODE: OneShot, Ping-Pong Disabled*/
    
 /* Rejestr DMA1CON
 * bit15    -0 chen/chanel --> disable
 * bit14    -1 size --> Byte    
 * bit13    -0 dir --> Read from Peripheral address, write to DMA RAM address 
 * bit12    -0 half --> Initiate block transfer complete interrupt when all of the data has been moved
 * bit11    -0 nullw --> Normal operation
 * bit10-6  -Unimplemented raed as 0
 * bit5-4   -00 amode  --> Register Indirect with Post_Incerement mode
 * bit3-2   -Unimplemented read as 0
 * bit1-0   -01 mode --> OneShot, Ping-Pong modes disabled*/
    DMA1CON = 0x4001 ;          /*0x4001 - wartosc wynika z ustawienia bitow jak wyzej*/							
	DMA1CNT = Size - 1 ;/*ustal ile znaków do odebrania max 1024 bajty*/					
	DMA1REQ = 0x000A; /*podpinamy peryferium SPI do DMA*/					
	DMA1PAD = (volatile unsigned int) &SPI1BUF; /*rzutowanie typu i pobranie adresu rejestru SPI1BUF*/
	DMA1STA= __builtin_dmaoffset(BuforRX); /*taka jest konstrukcja wskazania na bufor z danymi*/
	IFS0bits.DMA1IF  = 0;			/*Clear DMA interrupt*/
	IEC0bits.DMA1IE  = 1;			/*Enable DMA interrupt*/
	DMA1CONbits.CHEN = 1;			/*Enable DMA Channel*/		
	
}

/*konfiguracja SPI dla Mastera*/
void config_SPI_MASTER(void) {
     
IFS0bits.SPI1IF = 0;                    /*Clear the Interrupt Flag*/
IEC0bits.SPI1IE = 0;                    /*Disable the Interrupt*/
/*Set clock SPI on SCK, 40 MHz / (4*8) = 1,250 MHz*/
SPI1CON1bits.PPRE = 0b10;             /*Set Primary Prescaler 4:1*/
SPI1CON1bits.SPRE = 0b000;            /*Set Secondary Prescaler 8:1*/
SPI1CON1bits.CKE = 1 ;                  /*UWAGA ten tryb jest potrzebny do MCP2517FD*/
SPI1CON1bits.MODE16 = 0;                /*Communication is word-wide (8 bits)*/
SPI1CON1bits.MSTEN = 1;                 /*Master Mode Enabled*/
SPI1STATbits.SPIEN = 1;                 /*Enable SPI Module*/
IFS0bits.SPI1IF = 0;                    /*Clear the Interrupt Flag*/
IEC0bits.SPI1IE = 1;                    /*Enable the Interrupt SPI*/
}


/*=============================================================================
Interrupt Service Routines.
=============================================================================*/

void __attribute__((interrupt, no_auto_psv)) _DMA0Interrupt(void)
{
        
    IFS0bits.DMA0IF = 0;                /*Clear the DMA0 Interrupt Flag*/

}

void __attribute__((interrupt, no_auto_psv)) _DMA1Interrupt(void)
{
      /*De-assert CS*/
      SPI_CS_DESELECT();        /*ustaw 1 na pinie CS*/
      
      IFS0bits.DMA1IF = 0;      /*Clear the DMA1 Interrupt Flag*/

} 

void __attribute__((interrupt, no_auto_psv)) _SPI1Interrupt(void)
{
      //LED1_TOG ;
      IFS0bits.SPI1IF = 0;		/*Clear the DMA0 Interrupt Flag*/

}

void SPI_CS_SELECT(void){
    
    /*tu zasterowac pinem CS --> 0*/
    PORTBbits.RB7 = 0 ; //wyjscie RB7 stan niski
}

void SPI_CS_DESELECT(void){
    
    /*tu zasterowac pinem CS --> 1*/
    PORTBbits.RB7 = 1 ; //wyjscie RB7 stan wysoki
}