/*==============================================================================*/
/*                                                                              */
/* Project     = Sample program for flash self-programming library              */
/* Module      = r_fsl_sample_c.c                                               */
/* Version     = V1.02(Target FSL Version for CC-RL is V2.21)                   */
/* Target CPU  = RL78/G13 ( R5F100LEA / QB-R5F100LE-TB )                        */
/* Tool-Chain  = CS+ for CC-RL (V3.01)                                          */
/*                                                                              */
/* Date        = 10, 31, 2018                                                   */
/*                                                                              */
/*==============================================================================*/
/*                                COPYRIGHT                                     */
/*==============================================================================*/
/* DISCLAIMER                                                                   */
/* This software is supplied by Renesas Electronics Corporation and is only     */
/* intended for use with Renesas products. No other uses are authorized. This   */
/* software is owned by Renesas Electronics Corporation and is protected under  */
/* all applicable laws, including copyright laws.                               */
/* THIS SOFTWARE IS PROVIDED "AS IS" AND RENESAS MAKES NO WARRANTIES REGARDING  */
/* THIS SOFTWARE, WHETHER EXPRESS, IMPLIED OR STATUTORY, INCLUDING BUT NOT      */
/* LIMITED TO WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE   */
/* AND NON-INFRINGEMENT. ALL SUCH WARRANTIES ARE EXPRESSLY DISCLAIMED.          */
/* TO THE MAXIMUM EXTENT PERMITTED NOT PROHIBITED BY LAW, NEITHER RENESAS       */
/* ELECTRONICS CORPORATION NOR ANY OF ITS AFFILIATED COMPANIES SHALL BE LIABLE  */
/* FOR ANY DIRECT, INDIRECT, SPECIAL, INCIDENTAL OR CONSEQUENTIAL DAMAGES FOR   */
/* ANY REASON RELATED TO THIS SOFTWARE, EVEN IF RENESAS OR ITS AFFILIATES HAVE  */
/* BEEN ADVISED OF THE POSSIBILITY OF SUCH DAMAGES.                             */
/* Renesas reserves the right, without notice, to make changes to this software */
/* and to discontinue the availability of this software. By using this software,*/
/* you agree to the additional terms and conditions found by accessing the      */
/* following link:                                                              */
/* http://www.renesas.com/disclaimer                                            */
/*                                                                              */
/* Copyright (C) 2010-2018 Renesas Electronics Corporation. All rights reserved.*/
/*==============================================================================*/
/*------------------------------------------------------------------------------*/
/* Expanded functions                                                           */
/*------------------------------------------------------------------------------*/
#include "iodefine.h"

/*------------------------------------------------------------------------------*/
/* interrupt function                                                           */
/*------------------------------------------------------------------------------*/
#pragma interrupt samIntWdt (vect=INTWDTI)  /* WDT interrupt                    */


/*------------------------------------------------------------------------------*/
/* section and switch                                                           */
/*------------------------------------------------------------------------------*/
/***********************************************/
/* Symbol for program switch of sample program */
/***********************************************/
#if 0
/* Can be set when QB-R5F100LE-TB is used alone */
#define __QB_R5F100LE_TB__


#else
/* Other boards */
#define __NON_TARGET__

#endif


/*------------------------------------------------------------------------------*/
/* Include files                                                                */
/*------------------------------------------------------------------------------*/
/* Standard library (runtime library is used) */
#include <string.h>             /* For memcpy */

/* Flash self-programming library */
#include "fsl.h"                /* Library header file */
#include "fsl_types.h"          /* Library header file */



/*------------------------------------------------------------------------------*/
/* structure                                                                    */
/*------------------------------------------------------------------------------*/
/* Writing setting parameter structure for sample program */
typedef struct {
    fsl_u08 *pubDataBuffer;             /* Pointer to the start of data buffer    */
    fsl_u16  dubStartBlock;             /* Writing start block number             */
    __far const fsl_u08 *pubWriteData;  /* Pointer to the beginning of write data */
    fsl_u32  duwWriteDataSize;          /* Write data size                        */
    fsl_u08  dubEndOption;              /* End option                             */
} SAM_FSL_PARM_T;


/*------------------------------------------------------------------------------*/
/* Constant definitions                                                         */
/*------------------------------------------------------------------------------*/
/*****************************************/
/* Symbol definitions for sample program */
/*****************************************/
/* Error code */
#define SAM_ERR_PARM        0x7F        /* Parameter error             */
#define SAM_ERR_OPT         0x80        /* Option execution error flag */

/* Basic data */
#define SAM_BLOCK_SIZE      0x400l      /* Standard block size         */
#define SAM_WORD_SIZE       4           /* Standard word size          */
#define SAM_DUMYY_DATA      0xFF        /* Dummy write data            */
#define SAM_DATA_SIZE       0x490       /* Write data size             */
#define SAM_BUFFER_SIZE     256         /* Write buffer size           */

/* Option code for sample program */
#define SAM_OPT_NON         0x00        /* No options                                           */
#define SAM_OPT_RST         0x01        /* Reset execution                                      */
#define SAM_OPT_CBOOT       0x02        /* Boot flag change                                     */
#define SAM_OPT_ACT_SWAP    0x03        /* Boot flag change + swapping (executable only in RAM) */
#define SAM_OPT_CBOOT_RST   0x04        /* Boot flag change + reset                             */
#define SAM_OPT_CBOOT_SUP   0x05        /* Boot flag change + swapping + return to start        */
#define SAM_OPT_SUP         0x06        /* Swapping + return to start (no change in boot flag)  */

/* Control macro for sample program */
#define SAM_WDT_RESET()     WDTE = 0xAC /* Watchdog timer reset        */

/**************************************************/
/* QB-R5F100LE-TB: Symbol definitions for writing */
/**************************************************/
#ifdef __QB_R5F100LE_TB__
#define SAM_START_BLOCK 0x04            /* Writing start block number (0x4:1000H)                   */
#define SAM_SET_OPTION  SAM_OPT_CBOOT   /* After writing, the option to change the boot flag is set */

/* QB-R5F100LE-TB, LED control macros */
#define SAM_LED1_INIT() P6_bit.no2 = 1, PM6_bit.no2 = 0
                                        /* Initializes on-board LED1 (turns off) */
#define SAM_LED1_ON()   P6_bit.no2 = 0  /* Turns on on-board LED1                */
#define SAM_LED1_OFF()  P6_bit.no2 = 1  /* Turns off on-board LED1               */

#define SAM_LED2_INIT() P6_bit.no3 = 1, PM6_bit.no3 = 0
                                        /* Initializes on-board LED2 (turns off) */
#define SAM_LED2_ON()   P6_bit.no3 = 0  /* Turns on on-board LED2                */
#define SAM_LED2_OFF()  P6_bit.no3 = 1  /* Turns off on-board LED2               */

/*************************************************************/
/* Symbol definitions for making settings for normal writing */
/*************************************************************/
#else 
#define SAM_START_BLOCK 0x0D            /* Writing start block number (0xD:3400H) */
#define SAM_SET_OPTION  SAM_OPT_NON     /* No option setting                      */

/* No processes for LED (invalid code) */
#define SAM_LED1_INIT()
#define SAM_LED1_ON()
#define SAM_LED1_OFF()
#define SAM_LED2_INIT()
#define SAM_LED2_ON()
#define SAM_LED2_OFF()

#endif

/*------------------------------------------------------------------------------*/
/* external reference parameter                                                 */
/*------------------------------------------------------------------------------*/
/* Data buffer body */
fsl_u08 fsl_data_buffer[ SAM_BUFFER_SIZE ];

/* Fixed-value table for writing */
extern const __far fsl_u08 samDubWriteData[ SAM_DATA_SIZE ];

/*********************************************************/
/* QB-R5F100LE-TB                                        */
/*********************************************************/
#ifdef __QB_R5F100LE_TB__
/* Sets the operating clock to internal 32 MHz clock */
const __far fsl_descriptor_t fsl_descriptor_pstr = { 0x00, 0x20, 0x01 };
#else
/* Sets the operating clock to internal 32 MHz clock */
const __far fsl_descriptor_t fsl_descriptor_pstr = { 0x00, 0x20, 0x01 };
#endif

/*------------------------------------------------------------------------------*/
/* prototype declarations                                                       */
/*------------------------------------------------------------------------------*/
/* Sample functions for target board */
void    samTargetInit( void );        /* Target board initialization processing */
void    samTargetEnd ( void );        /* Target board end processing            */

/* Sample functions for writing program */
fsl_u08 samFslBlockWrite( SAM_FSL_PARM_T * );   /* Writing program body */
fsl_u08 samFslStart     ( void );               /* Flash self-programming library start processing */
void    samFslEnd       ( void );               /* Flash self-programming library end processing   */


/*##############################################################################*/
/*------------------------------------------------------------------------------*/
/* start-up : hdwinit functions                                                 */
/*------------------------------------------------------------------------------*/
/*==============================================================================*/
/* Function name : hdwinit                                                      */
/* IN            : void                                                         */
/* OUT           : void                                                         */
/* Description   : Hardware initialization function for the start-up routine    */
/*                 Note 1: The start-up routine should be executed before the   */
/*                 main function.                                               */
/*                 Note 2: Should be called in the DI(interrupt disabled) state.*/
/*==============================================================================*/
void hdwinit( void )
{
    /***********************************/
    /* Setting of low-voltage detector */
    /***********************************/
    LVIM = 0x00;    /* Initializes the voltage detection register setting       */
    LVIS = 0x00;    /* Initializes the voltage detection level register setting */
    

#if 0
/* Makes the setting to switch to external 20 MHz oscillator for QB-R5F100LE-TB */
    
    
    
    
    /***********************************************************************************************/
    /* Initialization of clock control registers                                                   */
    /* The operating clock is changed from on-chip oscillator to X1 oscillator (20 MHz is assumed) */
    /* Note: Sets the operating clock frequency to 0x14(20 MHz).                                   */
    /***********************************************************************************************/
    /* Subsystem clock supply mode control register           */
    OSMC = 0b00000000;    /* Enables supply of subsystem clock to peripheral functions                    */
    
    /* Clock operation mode control register                  */
    CMC  = 0b01010001;    /* Sets the oscillation modes for X1 and XT1 pins and sets the X1 frequency to  */
                          /* 10 MHz < fx <= 20 MHz                                                        */
    
    /* Oscillation stabilization time select register         */
    OSTS = 0b00000011;    /* Sets the X1 oscillation stabilization time (102.4 us or more at fx = 20 MHz) */
    
    /* Clock operation status control register                */
    CSC  = 0b00000000;    /* Starts X1 and XT1 oscillators                                                */
    
    /* Oscillation stabilization time counter status register */
    while( ( 0xF0 & OSTC ) != 0xF0 );   /* Waits for the X1 oscillation stabilization time to pass        */
    
    /* System clock control register       */
    CKC  = 0b00010000;    /* Changes the main system clock from on-chip oscillator to X1 oscillator       */
    
    while( MCS  == 0 );   /* Confirms the main system clock (fMAIN) status                                */
                          /* Note: Oscillator switching is confirmed (MCS: bit 5 of CKC register)         */
    
    /* Clock operation status control register: Controls operation of the high-speed on-chip oscillator   */
    /* HIOSTOP = 1; */          /* CSC = 0b00000001: Stops the high-speed on-chip oscillator              */
    /* Since the high-speed on-chip oscillator is required to execute RL78 self-programming,              */
    /* the high-speed on-chip oscillator is not stopped.                                                  */
    
    
    
    
#endif
}


/*##############################################################################*/
/*------------------------------------------------------------------------------*/
/* main functions                                                               */
/*------------------------------------------------------------------------------*/
/*==============================================================================*/
/* Function name : main                                                         */
/* IN            : void                                                         */
/* OUT           : void                                                         */
/* Description   : Main function                                                */
/*==============================================================================*/
void main( void )
{
    fsl_u08         dubRetCode;                /* Return code                   */
    SAM_FSL_PARM_T  dtyFslData;                /* Parameter for writing program */
    
    /* Target board initialization processing */
    samTargetInit();
    
    /* Enables interrupts */
    __EI();
    
    /* Parameter setting */
    dtyFslData.pubDataBuffer    = fsl_data_buffer;  /* Start address of data buffer  */
    dtyFslData.dubStartBlock    = SAM_START_BLOCK;  /* Writing start block number    */
    dtyFslData.pubWriteData     = samDubWriteData;  /* Start address of write data   */
    dtyFslData.duwWriteDataSize = SAM_DATA_SIZE;    /* Write data size               */
    dtyFslData.dubEndOption     = SAM_SET_OPTION;   /* End option of writing program */
    
    /* Calls the sample writing program */
    dubRetCode = samFslBlockWrite( &dtyFslData );
    
    /* Normal termination */ 
    if( dubRetCode == FSL_OK )
    { }
    /* Abnormal termination */
    else {
        /* Abnormal determination process */
        /* Error in parameter check or rewrite process */
        if( ( dubRetCode & SAM_ERR_OPT ) != SAM_ERR_OPT )
        {
            /* Error check */
            switch( dubRetCode )
            {
                /* Errors caused by flash self-programming library             */
                
                case FSL_ERR_PARAMETER:         /* Parameter error             */
                case FSL_ERR_PROTECTION:        /* Protect error               */
                case FSL_ERR_ERASE:             /* Erase error                 */
                case FSL_ERR_IVERIFY:           /* Internal verification error */
                case FSL_ERR_WRITE:             /* Writing error               */
                    break;
                
                /* Sample program error */
                case SAM_ERR_PARM:              /* Parameter error             */
                    break;
                
                /* Others */
                default:
                    break;
            }
        }
        /* Error caused by option execution */
        else {
            dubRetCode &= ~SAM_ERR_OPT;         /* Option flag is false        */
            
            /* Error check */
            switch( dubRetCode )
            {
                /* Errors caused by flash self-programming library             */
                case FSL_ERR_PARAMETER:         /* Parameter error             */
                case FSL_ERR_PROTECTION:        /* Protect error               */
                case FSL_ERR_ERASE:             /* Erase error                 */
                case FSL_ERR_IVERIFY:           /* Internal verification error */
                case FSL_ERR_WRITE:             /* Writing error               */
                    break;
                
                /* Others */
                default:
                    break;
            }
        }
    }
    
    /* Target board end processing */
    samTargetEnd();
    
#ifdef __QB_R5F100LE_TB__
/* WDT reset execution process for QB-R5F100LE-TB */
    /* Disables interrupts */
    __DI();
    
    /* Resets the watchdog timer once */
    SAM_WDT_RESET();
#endif
    
    /* End loop (this is reset by watchdog timer) */
    while( 1 )
    {
        __nop();
        __nop();
    }
}


/*##############################################################################*/
/*------------------------------------------------------------------------------*/
/* sub functions                                                                */
/*------------------------------------------------------------------------------*/
/*==============================================================================*/
/* Function name : samFslBlockWrite                                             */
/* IN            : SAM_FSL_PARM_T / Setting parameter group                     */
/* OUT           : fsl_u08        / Return code                                 */
/* Description   : Sample program for flash self-programming                    */
/*==============================================================================*/
fsl_u08 samFslBlockWrite( SAM_FSL_PARM_T *ptyFslData  )
{
    fsl_u08     dubRetCode;         /* Return code                              */
    fsl_u32     duwWriteBlockNum;   /* Number of remaining blocks to be written */
    fsl_u16     duhWriteBufSize;    /* Size of data buffer for writing          */
    
    /*******************************************************/
    /* Setting parameter check + Initialization processing */
    /*******************************************************/
    if( ( ptyFslData->duwWriteDataSize == 0 ) ||
        ( ptyFslData->pubDataBuffer    == 0 ) )
    {
        dubRetCode = SAM_ERR_PARM;  /* Parameter error */
    }
    else {
        /* When the data buffer is not in word units (4 bytes), the remainder is     */
        /* rounded down to adjust the maximum write data length to be in word units. */
        duhWriteBufSize = SAM_BUFFER_SIZE - SAM_BUFFER_SIZE % SAM_WORD_SIZE;
        
        /* Flash self-programming library start processing */
        dubRetCode = samFslStart();
    }
    
    /*******************/
    /* Writing process */
    /*******************/
    /* Loops until all data has been written or an error has occurred */
    for( duwWriteBlockNum = ( ( ptyFslData->duwWriteDataSize - 1 ) / SAM_BLOCK_SIZE ) + 1 ;
         ( duwWriteBlockNum != 0 ) && ( dubRetCode == FSL_OK )                            ;
         duwWriteBlockNum-- ,ptyFslData->dubStartBlock++                                  )
    {
        /* Confirms the state of the target block        */
        dubRetCode = FSL_BlankCheck( ptyFslData->dubStartBlock );
        
        /* If the target block is not in the blank state */
        if( dubRetCode == FSL_ERR_BLANKCHECK )
        {
            dubRetCode = FSL_Erase( ptyFslData->dubStartBlock );
        }
        
        /* Writing process (in block units) */
        if( dubRetCode == FSL_OK )
        {
            fsl_u32     duwRemDataSize;     /* Size of data not yet written */
            fsl_u32     duwWriteAddress;    /* Writing destination address  */
            fsl_u32     duwOneWriteSize;    /* Write data length            */
            fsl_write_t dtyWriteBuff;       /* Write data storage settings  */
            
            /* Calculates the writing start address */
            duwWriteAddress = ptyFslData->dubStartBlock * SAM_BLOCK_SIZE;
            
            /* If this is not the last write block (data remains to be written to the next block) */
            if( duwWriteBlockNum > 1 )
            {
                /* Sets the write data size for the block size */
                duwRemDataSize = SAM_BLOCK_SIZE;
            }
            else {
                /* When this is the last block, sets the remaining data for the write data size */
                duwRemDataSize = ptyFslData->duwWriteDataSize;
            }
            
            /* Loops until all remaining data of the target block has been written or an error has occurred */
            for( ;  ( duwRemDataSize != 0 ) && ( dubRetCode == FSL_OK ) ;
                    duwWriteAddress         += duhWriteBufSize          )
            {
                /* If the remaining write data is the same size as the data buffer */
                if( duwRemDataSize >= duhWriteBufSize )
                {   
                    /* Sets the buffer size as the unit for writing */
                    duwOneWriteSize = duhWriteBufSize;
                }
                else {
                    /* Sets the remaining write data size as the unit for writing */
                    duwOneWriteSize = ptyFslData->duwWriteDataSize;
                }
                
                /* Copies the write data to the data buffer */
                _COM_memcpy_ff( ptyFslData->pubDataBuffer     ,
                        ptyFslData->pubWriteData      ,
                        (unsigned int)duwOneWriteSize );
                
                /* Re-calculates the remaining write data */
                duwRemDataSize               -= duwOneWriteSize;
                ptyFslData->duwWriteDataSize -= duwOneWriteSize;
                ptyFslData->pubWriteData     += duwOneWriteSize;
                
                /* When the write data is not in word units (4 bytes), the size is rounded up */
                /* and dummy data is set for the remaining data                               */
                for( ; ( duwOneWriteSize % SAM_WORD_SIZE ) != 0 ; duwOneWriteSize++ )
                {
                    ptyFslData->pubDataBuffer[ duwOneWriteSize ] = SAM_DUMYY_DATA;
                }
                
                /* Sets the parameters */
                dtyWriteBuff.fsl_data_buffer_p_u08       = ptyFslData->pubDataBuffer;
                dtyWriteBuff.fsl_destination_address_u32 = duwWriteAddress;
                dtyWriteBuff.fsl_word_count_u08          = (fsl_u08)( duwOneWriteSize / SAM_WORD_SIZE );
                
                /* Executes data writing to the specified address */
                dubRetCode = FSL_Write( &dtyWriteBuff );
            }
        }
        /* If writing to the target block has completed normally */
        if( dubRetCode == FSL_OK )
        {
            /* Executes internal verification of the target block */
            dubRetCode = FSL_IVerify( ptyFslData->dubStartBlock );
        }
    }
    
    /* If the error code is other than SAM_ERR_PARM (library is executed) */
    if( dubRetCode != SAM_ERR_PARM )
    {
        /* If the writing process is completed normally, options are executed */
        if( dubRetCode == FSL_OK )
        {
            /*************************************************************************/
            /* Usage notes on boot swapping                                          */
            /* When executing boot swapping, the necessary settings for operation of */
            /* the target microcontroller (e.g. option byte) must be written to also */
            /* the area (1000H to 1FFFH) before swapping. If the settings are not    */
            /* made, the target microcontroller will not operate correctly.          */
            /*************************************************************************/
            switch( ptyFslData->dubEndOption )
            {
                /* Reset processing */
                case SAM_OPT_RST:
                    /* Forced reset processing                                                   */
                    /* Note: When debugging with the OCD or IECUBE, a reset cannot be generated  */
                    /*       with the FSL_ForceReset function.                                   */
                    /*       In such a case, generate a reset from the debugger manually,        */
                    /*       or use another method to generate a reset.                          */
                    FSL_ForceReset();
                    break;
                    
                /* Boot flag change process */
                case SAM_OPT_CBOOT:
                    /* Boot flag change process (boot clusters are changed after a reset)            */
                    /* Note: Since the address of a program located in boot cluster 0 (BCL0) or      */
                    /*       boot cluster 1 (BCL1) is changed, after a reset, execution of a program */
                    /*       located at an address lower than 2000H in the debugger can no longer be */
                    /*       correctly confirmed unless it is performed from the same program.       */
                    dubRetCode = FSL_InvertBootFlag();
                    
                    break;
                    
                /* Boot flag change process and boot swapping process */
                case SAM_OPT_ACT_SWAP:
#if 0               /* This facility (function) is removed from the sample program because it is    */
                    /* a facility (function) that can support only RAM location setting.            */

                    /* Boot flag change process and boot swapping process                           */
                    /* Note: When the rewriting program or library is in a boot cluster, if the     */
                    /*       FSL_SwapActiveBootCluster function is executed, the location address   */
                    /*       changes immediately after execution. Thus, correct operation is not    */
                    /*       possible. To achieve correct operation, locate this re-writing program */
                    /*       or library at an area other than boot clusters.                        */
                    /*       Since the address of a program located in BCL0 or BCL1 is changed,     */
                    /*       execution of a program located at an address lower than 2000H in the   */
                    /*       debugger can no longer be correctly confirmed unless it is performed   */
                    /*       from the same program.                                                 */
                    dubRetCode = FSL_SwapActiveBootCluster();
#endif
                    break;
                    
                /* Boot flag change process and reset processing */
                case SAM_OPT_CBOOT_RST:
                    /* Boot flag change process (boot clusters are changed after a reset)            */
                    /* Note: Since the address of a program located in BCL0 or BCL1 is changed,      */
                    /*       after a reset, execution of a program located at an address lower than  */
                    /*       2000H in the debugger can no longer be correctly confirmed unless       */
                    /*       it is performed from the same program.                                  */
                    dubRetCode = FSL_InvertBootFlag();
                    
                    if( dubRetCode == FSL_OK )
                    {
                        /* Forced reset processing                                                   */
                        /* Note: When debugging with the OCD or IECUBE, a reset cannot be generated  */
                        /*       with the FSL_ForceReset function.                                   */
                        /*       In such a case, generate a reset from the debugger manually, or use */
                        /*       another method to generate a reset.                                 */
                        FSL_ForceReset();
                    }
                    break;
                    
                /* Boot flag change process, boot swapping process, and re-execution from the program start address */
                case SAM_OPT_CBOOT_SUP:
                    /* Boot flag change process (boot clusters are changed after a reset)            */
                    /* Note: Since the address of a program located in BCL0 or BCL1 is changed,      */
                    /*       after a reset, execution of a program located at an address lower than  */
                    /*       2000H in the debugger can no longer be correctly confirmed unless       */
                    /*       it is performed from the same program.                                  */
                    dubRetCode = FSL_InvertBootFlag();
                    
                    if( dubRetCode == FSL_OK )
                    {
                        /* After boot swapping is executed, the program is re-executed from a new boot area */
                        /* Note: Since the address of a program located in BCL0 or BCL1 is changed,         */
                        /*       execution of a program located at an address lower than 2000H in           */
                        /*       the debugger can no longer be correctly confirmed unless it is performed   */
                        /*       from the same program.                                                     */
                        /*       As swapping is performed in a state where no reset is executed and the     */
                        /*       process is re-executed from a new startup routine, operation differs from  */
                        /*       normal reset operation.                                                    */
                        /*       Find out the status of RAM and interrupts so that execution can be carried */
                        /*       out with no problems occurring on the user program side.                   */
                        dubRetCode = FSL_SwapBootCluster();
                    }
                    break;
                    
                /* Boot swapping process and re-execution from the program start address (no change in boot flag) */
                case SAM_OPT_SUP:
                        /* After boot swapping is executed, the program is re-executed from a new boot area */
                        /* Note: Since the address of a program located in BCL0 or BCL1 is changed,         */
                        /*       execution of a program located at an address lower than 2000H in           */
                        /*       the debugger can no longer be correctly confirmed unless it is performed   */
                        /*       from the same program. However, after a reset, program execution can be    */
                        /*       re-confirmed in the debugger because the cluster locations are returned to */
                        /*       the previous state.                                                        */
                        /*       As swapping is performed in a state where no reset is executed and the     */
                        /*       process is re-executed from a new startup routine, operation differs from  */
                        /*       normal reset operation.                                                    */
                        /*       Find out the status of RAM and interrupts so that execution can be carried */
                        /*       out with no problems occurring on the user program side.                   */
                    dubRetCode = FSL_SwapBootCluster();
                    
                    break;
                    
                /* Option is not executed */
                default:
                    break;
            }
            
            /* Adds an option flag to an error occurring during execution of an option to */
            /* differentiate it with an error occurring at writing.                       */
            if( dubRetCode != FSL_OK )
            {
                dubRetCode |= SAM_ERR_OPT;
            }
        }
        
        /* Flash self-programming library end processing */
        samFslEnd();
    }
    
    return dubRetCode;
}


/*==============================================================================*/
/* Function name : samTargetInit                                                */
/* IN            : void                                                         */
/* OUT           : void                                                         */
/* Description   : Target board initialization processing                       */
/*==============================================================================*/
void samTargetInit( void )
{
    /******************/
    /* Watchdog timer */
    /******************/
    SAM_WDT_RESET();        /* Watchdog timer reset */
    
    /*********************************/
    /* LED initialization processing */
    /*********************************/
    SAM_LED1_INIT();        /* Initializes LED1 (turns off) */
    SAM_LED2_INIT();        /* Initializes LED2 (turns off) */
    SAM_LED1_ON();          /* Turns on LED1                */
    SAM_LED2_ON();          /* Turns on LED2                */
}

/*==============================================================================*/
/* Function name : samTargetEnd                                                 */
/* IN            : void                                                         */
/* OUT           : void                                                         */
/* Description   : Target board end processing                                  */
/*==============================================================================*/
void samTargetEnd( void )
{
    SAM_LED1_OFF();         /* Turns off LED1               */
    SAM_LED2_OFF();         /* Turns off LED2               */
}

/*==============================================================================*/
/* Function name : samFslStart                                                  */
/* IN            : void                                                         */
/* OUT           : fsl_u08  /  Return code                                      */
/* Description   : Start of the flash self-programming library                  */
/*==============================================================================*/
fsl_u08 samFslStart( void )
{
    fsl_u08 dubRetCode;
    
    /* Flash self-programming library initialization */
    dubRetCode = FSL_Init( &fsl_descriptor_pstr );
    
    /* When initialization has completed normally */
    if( dubRetCode == FSL_OK )
    {
        /* Flash self-programming library start processing */
        FSL_Open();
        
        FSL_PrepareFunctions();
        
        FSL_PrepareExtFunctions();
    }
    
    
    
    return dubRetCode;
}


/*==============================================================================*/
/* Function name : samFslEnd                                                    */
/* IN            : void                                                         */
/* OUT           : void                                                         */
/* Description   : End of the flash self-programming library                    */
/*==============================================================================*/
void samFslEnd( void )
{
    /* Flash self-programming library end processing */
    FSL_Close();
}


/*##############################################################################*/
/*------------------------------------------------------------------------------*/
/* interrupt functions                                                          */
/*------------------------------------------------------------------------------*/
/*==============================================================================*/
/* Function name : samIntWdt                                                    */
/* IN            : void                                                         */
/* OUT           : void                                                         */
/* Description   : WDT reset function                                           */
/*==============================================================================*/
__near void samIntWdt( void )
{
    SAM_WDT_RESET();
}


/********************************************************************************/
/********************************************************************************/
/*                                                                              */
/*  Program of LED lighting and swapping for QB-R5F100LE-TB                     */
/*                                                                              */
/*  Note: Two LEDs are lighted up alternately. When SW1 is pressed, the LED     */
/*        flashing method is changed and boot swapping is performed within      */
/*        a few seconds                                                         */
/*                                                                              */
/*    - Usage method (when used as an executable program instead of dummy data) */
/*      When QB-R5F100LE-TB is used, change [#if 0] on the 53th line of this    */
/*      code to [#if 1] and enable symbols of [__QB_R5F100LE_TB__].             */
/*      After that, register this code and the [sample_c.dr] file in a new      */
/*      project, enable the on-chip debugging settings, and create a load       */
/*      module.                                                                 */
/*      After the debugger is started, set a breakpoint near the beginning of   */
/*      the main program and execute the sample program to the end. A reset is  */
/*      generated at the watchdog timer, boot swapping is executed,             */
/*      and the program of LED lighting and swapping is executed. (LEDs start   */
/*      flashing alternately.)                                                  */
/*                                                                              */
/*==============================================================================*/
/*     At this time (period between changing the boot flag and returning it to  */
/*     its former state), do not execute a reset by the debugger.               */
/*     Since the program has already been changed, it may not operate correctly.*/
/*==============================================================================*/
/*                                                                              */
/*      If SW1 is pressed in this state, the LEDs are changed from flashing     */
/*      alternately to flashing simultaneously. Then, after a few seconds,      */
/*      a reset occurs again, boot swapping is executed, execution returns to   */
/*      the original sample program, and a break occurs (execution stops) at    */
/*      the set breakpoint.                                                     */
/*                                                                              */
/*      Note: This executable program is a sample program created for easily    */
/*            confirming boot swapping. It may not operate correctly in some    */
/*            cases depending on the project settings or operation status or    */
/*            connection state of QB-R5F100LE-TB.                               */
/*                                                                              */
/********************************************************************************/
const __far fsl_u08 samDubWriteData[ SAM_DATA_SIZE ] = 
{
/* 0000 */  0xD0, 0x00, 0xFF, 0xFF, 0x00, 0x14, 0x01, 0x00, 0x84, 0x01, 0xC7, 0xC5, 0xC1, 0x16, 0x34, 0x80,
/* 0010 */  0x08, 0xF6, 0xB9, 0xA5, 0xA5, 0x35, 0x44, 0xC0, 0x08, 0x35, 0xDF, 0xF6, 0x8E, 0xFD, 0xC1, 0x62,
/* 0020 */  0x9E, 0xFD, 0x31, 0x82, 0xA1, 0x3D, 0xF1, 0x11, 0x4E, 0x02, 0x61, 0xE8, 0x71, 0x00, 0x91, 0x08,
/* 0030 */  0x9F, 0x03, 0x08, 0x11, 0x4E, 0x00, 0x61, 0xE8, 0xE1, 0x9F, 0x01, 0x08, 0x30, 0x80, 0x08, 0x14,
/* 0040 */  0xBF, 0x04, 0x08, 0x11, 0x8C, 0x01, 0x99, 0x41, 0x0E, 0x11, 0x4F, 0xE8, 0xFF, 0xDC, 0x14, 0x11,
/* 0050 */  0xF9, 0xE9, 0xFF, 0x61, 0x42, 0xDC, 0x0C, 0x52, 0x00, 0xFC, 0xF8, 0xFF, 0x0E, 0x71, 0x00, 0x90,
/* 0060 */  0x08, 0xEF, 0x02, 0x52, 0x05, 0xC0, 0x9E, 0xFD, 0xC0, 0xC4, 0xC6, 0xD7, 0x71, 0x10, 0x90, 0x08,
/* 0070 */  0xD7, 0xFF, 0x71, 0x18, 0x90, 0x08, 0xD7, 0xFF, 0xC7, 0xC5, 0xC3, 0xC1, 0x71, 0x20, 0x90, 0x08,
/* 0080 */  0x8E, 0xFD, 0xC1, 0x41, 0x00, 0x36, 0xBA, 0x02, 0x32, 0xB7, 0x03, 0x34, 0x80, 0x0A, 0x11, 0xAB,
/* 0090 */  0xB9, 0xA7, 0xA7, 0xA5, 0xA5, 0x17, 0x43, 0xDC, 0xF5, 0xC0, 0x9E, 0xFD, 0xC0, 0xC2, 0xC4, 0xC6,
/* 00A0 */  0xD7, 0xFF, 0xC7, 0xC5, 0xC3, 0xC1, 0x71, 0x30, 0x90, 0x08, 0x8E, 0xFD, 0xC1, 0x41, 0x00, 0x36,
/* 00B0 */  0xB8, 0x03, 0x32, 0x33, 0x04, 0x34, 0x80, 0x0B, 0xEE, 0xD3, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
/* 00C0 */  0xFF, 0xFF, 0xEF, 0x82, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xD8, 0x00,
/* 00D0 */  0xEC, 0xFD, 0xFF, 0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0x61, 0xCF, 0x51, 0x00, 0x71, 0x8C, 0x71, 0x09,
/* 00E0 */  0xFE, 0xCB, 0xF8, 0x20, 0xFE, 0xFC, 0x72, 0x04, 0x00, 0xF6, 0xBF, 0x96, 0xFB, 0xBF, 0x80, 0xFB,
/* 00F0 */  0xBF, 0x84, 0xFB, 0xE6, 0xBF, 0x82, 0xFB, 0x30, 0x9A, 0xFB, 0xBF, 0x98, 0xFB, 0x53, 0xC0, 0xF6,
/* 0100 */  0x93, 0x93, 0x58, 0x20, 0xFE, 0xDF, 0xF9, 0x41, 0x00, 0x36, 0x02, 0x00, 0x34, 0xBA, 0xFC, 0xEF,
/* 0110 */  0x05, 0x11, 0x8B, 0x99, 0xA7, 0xA5, 0x17, 0x44, 0x02, 0x00, 0xDF, 0xF5, 0x36, 0x02, 0x00, 0x34,
/* 0120 */  0xBA, 0xFC, 0xEF, 0x0A, 0x41, 0x00, 0x11, 0x8B, 0x41, 0x0F, 0x11, 0x99, 0xA7, 0xA5, 0x17, 0x44,
/* 0130 */  0x02, 0x00, 0xDF, 0xF0, 0x36, 0xBA, 0xFB, 0x30, 0xBA, 0xFC, 0xEF, 0x04, 0xCC, 0x00, 0x00, 0xA7,
/* 0140 */  0x47, 0xDF, 0xF9, 0x41, 0x0F, 0x36, 0xBA, 0xFC, 0x30, 0xBA, 0xFC, 0xEF, 0x05, 0x11, 0xCC, 0x00,
/* 0150 */  0x00, 0xA7, 0x47, 0xDF, 0xF8, 0x41, 0x00, 0x36, 0x02, 0x00, 0x34, 0xBA, 0xFC, 0xEF, 0x05, 0x11,
/* 0160 */  0x8B, 0x99, 0xA7, 0xA5, 0x17, 0x44, 0x02, 0x00, 0xDF, 0xF5, 0x36, 0xBA, 0xFC, 0x30, 0xBA, 0xFC,
/* 0170 */  0xEF, 0x04, 0xCC, 0x00, 0x00, 0xA7, 0x47, 0xDF, 0xF9, 0xFC, 0x31, 0x02, 0x00, 0xF6, 0xFC, 0x73,
/* 0180 */  0x04, 0x00, 0xEF, 0xFE, 0x71, 0x7B, 0xFA, 0xC1, 0xC3, 0xC5, 0xC7, 0x52, 0x0C, 0x92, 0x92, 0x69,
/* 0190 */  0xD4, 0xFE, 0xC1, 0xDF, 0xF8, 0x8E, 0xFD, 0x70, 0x8E, 0xFC, 0xC1, 0x20, 0x06, 0xFB, 0xF8, 0xFF,
/* 01A0 */  0x30, 0x04, 0x00, 0x52, 0x00, 0xF3, 0xFC, 0x0A, 0x00, 0x00, 0x62, 0x9C, 0x01, 0xD1, 0xDF, 0x0C,
/* 01B0 */  0xFC, 0x6C, 0x00, 0x00, 0xFC, 0x78, 0x00, 0x00, 0xFC, 0xA2, 0x00, 0x00, 0x8C, 0x01, 0xD1, 0xDF,
/* 01C0 */  0x07, 0xFC, 0x34, 0x04, 0x00, 0x62, 0x9C, 0x01, 0x8C, 0x01, 0xD1, 0xDF, 0x43, 0xCE, 0xAB, 0xAC,
/* 01D0 */  0xCC, 0x00, 0x01, 0x8B, 0xD1, 0xDF, 0x0B, 0xCC, 0x00, 0x01, 0x71, 0x23, 0x06, 0x71, 0x33, 0x06,
/* 01E0 */  0xEF, 0x09, 0xCC, 0x00, 0x00, 0x71, 0x22, 0x06, 0x71, 0x32, 0x06, 0xF6, 0xBC, 0x02, 0xBC, 0x04,
/* 01F0 */  0xF6, 0x61, 0x49, 0x04, 0xDF, 0x06, 0x30, 0x0F, 0x27, 0x61, 0x49, 0x02, 0xDC, 0xD5, 0xFC, 0xAF,
/* 0200 */  0x02, 0x00, 0x61, 0x79, 0x02, 0xF6, 0x61, 0x49, 0x02, 0xDF, 0xE5, 0x61, 0x79, 0x04, 0xEF, 0xE0,
/* 0210 */  0xFC, 0x72, 0x00, 0x00, 0x10, 0x06, 0xC0, 0x9E, 0xFC, 0x60, 0x9E, 0xFD, 0x34, 0xD4, 0xFE, 0x52,
/* 0220 */  0x06, 0xC0, 0xB9, 0xA5, 0xA5, 0x92, 0xDF, 0xF9, 0xC6, 0xC4, 0xC2, 0xC0, 0x71, 0x7A, 0xFA, 0x61,
/* 0230 */  0xFC, 0xC7, 0x20, 0x06, 0xFB, 0xF8, 0xFF, 0xF6, 0xBB, 0xFC, 0x81, 0x02, 0x00, 0xF6, 0x61, 0x49,
/* 0240 */  0x00, 0xDF, 0x0A, 0xE6, 0xBB, 0x71, 0x23, 0x06, 0x71, 0x32, 0x06, 0xEF, 0x08, 0xF6, 0xBB, 0x71,
/* 0250 */  0x22, 0x06, 0x71, 0x33, 0x06, 0xF6, 0xBC, 0x02, 0xBC, 0x04, 0xF6, 0x61, 0x49, 0x04, 0xDF, 0x06,
/* 0260 */  0x30, 0x1F, 0x4E, 0x61, 0x49, 0x02, 0xDC, 0xD5, 0xFC, 0xAF, 0x02, 0x00, 0xCE, 0xAB, 0xAC, 0x61,
/* 0270 */  0x79, 0x02, 0xF6, 0x61, 0x49, 0x02, 0xDF, 0xE2, 0x61, 0x79, 0x04, 0xEF, 0xDD, 0x10, 0x06, 0xC6,
/* 0280 */  0xD7, 0xF5, 0xA9, 0xFF, 0xF5, 0xAA, 0xFF, 0x71, 0x22, 0x06, 0x71, 0x32, 0x06, 0x71, 0x22, 0x06,
/* 0290 */  0x71, 0x2B, 0x26, 0x71, 0x32, 0x06, 0x71, 0x3B, 0x26, 0x71, 0x0B, 0x38, 0x71, 0x0A, 0x39, 0x71,
/* 02A0 */  0x2B, 0xE8, 0x71, 0x2A, 0xEC, 0x71, 0x2B, 0xE0, 0x71, 0x2B, 0xE4, 0x71, 0x7A, 0xFA, 0xD7, 0x00,
/* 02B0 */  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xD7, 0xEF, 0x21, 0xEF, 0x23, 0xEF, 0x25,
/* 02C0 */  0xEF, 0x27, 0xEF, 0x2A, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xC7, 0xC5, 0xC1, 0x8E, 0xFC, 0x75,
/* 02D0 */  0x8E, 0xFD, 0x74, 0xCE, 0xFC, 0x0F, 0x13, 0xF1, 0x04, 0x80, 0x0A, 0x61, 0xCB, 0x52, 0x03, 0xEF,
/* 02E0 */  0x2D, 0x52, 0x06, 0xEF, 0x29, 0x52, 0x08, 0xEF, 0x25, 0xA8, 0x00, 0x16, 0xEF, 0x50, 0xFC, 0xC4,
/* 02F0 */  0x08, 0x0F, 0x62, 0x4C, 0xFF, 0xDD, 0x3D, 0x71, 0x48, 0x90, 0x08, 0x71, 0x78, 0x90, 0x08, 0x8F,
/* 0300 */  0x90, 0x08, 0x31, 0x55, 0x2F, 0x51, 0x0F, 0x9E, 0xFC, 0xAF, 0x92, 0x08, 0x61, 0xCB, 0xA8, 0x00,
/* 0310 */  0x16, 0x41, 0x0E, 0x11, 0xAF, 0xE6, 0xFF, 0x47, 0xDC, 0x18, 0x17, 0x31, 0x2D, 0x9F, 0x02, 0x08,
/* 0320 */  0x60, 0x9F, 0x01, 0x08, 0xF5, 0x00, 0x08, 0xF5, 0x03, 0x08, 0x30, 0x00, 0x04, 0xBF, 0x06, 0x08,
/* 0330 */  0xEF, 0x60, 0x52, 0x05, 0x65, 0x9E, 0xFC, 0x64, 0x9E, 0xFD, 0xC0, 0xC4, 0xC6, 0xD7, 0x8C, 0x00,
/* 0340 */  0x70, 0x8C, 0x01, 0xBF, 0x04, 0x08, 0x8C, 0x02, 0x5C, 0x03, 0xDF, 0xE6, 0x8C, 0x05, 0xD1, 0xDF,
/* 0350 */  0xE1, 0x8C, 0x06, 0xD1, 0xDD, 0xDC, 0x4C, 0x41, 0xDE, 0xD8, 0x70, 0xF1, 0x31, 0x2D, 0xBF, 0x06,
/* 0360 */  0x08, 0xB1, 0xB1, 0xC5, 0x14, 0x8C, 0x02, 0x70, 0x8C, 0x03, 0xBF, 0x00, 0x08, 0x05, 0x35, 0x8C,
/* 0370 */  0x04, 0x9F, 0x02, 0x08, 0x1C, 0x00, 0x35, 0x16, 0x15, 0x70, 0xC4, 0xDC, 0xB5, 0x41, 0x0E, 0x66,
/* 0380 */  0x11, 0x3F, 0xDF, 0xFF, 0x67, 0x11, 0x3F, 0xE0, 0xFF, 0x60, 0x11, 0x3F, 0xE1, 0xFF, 0xDE, 0xA2,
/* 0390 */  0x52, 0x04, 0x13, 0xFC, 0xF8, 0xFF, 0x0E, 0x62, 0x4C, 0xFF, 0xDF, 0x98, 0x8F, 0x91, 0x08, 0x31,
/* 03A0 */  0x03, 0x0F, 0x71, 0x40, 0x90, 0x08, 0x60, 0x4C, 0x03, 0x61, 0xF8, 0x71, 0x70, 0x90, 0x08, 0xEF,
/* 03B0 */  0x83, 0xFC, 0xC4, 0x08, 0x0F, 0xEF, 0xE0, 0x00, 0xEF, 0x18, 0xEF, 0x3F, 0x00, 0x00, 0x00, 0x00,
/* 03C0 */  0xC5, 0xC1, 0x8E, 0xFC, 0x75, 0x8E, 0xFD, 0x74, 0xCE, 0xFC, 0x0F, 0x13, 0xF1, 0x04, 0x80, 0x0B,
/* 03D0 */  0x61, 0xCB, 0x30, 0x80, 0x08, 0xBF, 0x04, 0x08, 0x52, 0x10, 0xFC, 0xF8, 0xFF, 0x0E, 0xD2, 0xDF,
/* 03E0 */  0x14, 0x67, 0x5F, 0x80, 0x08, 0x9F, 0x80, 0x08, 0x67, 0x5C, 0x01, 0x61, 0xE8, 0x71, 0x00, 0x80,
/* 03F0 */  0x08, 0x52, 0x11, 0xEF, 0x0A, 0x71, 0x58, 0x90, 0x08, 0xEF, 0x2E, 0x52, 0x13, 0xEF, 0x00, 0xFC,
/* 0400 */  0xF8, 0xFF, 0x0E, 0x62, 0x4C, 0xFF, 0xDF, 0x0C, 0x8F, 0x91, 0x08, 0x31, 0x03, 0x15, 0x71, 0x40,
/* 0410 */  0x90, 0x08, 0xEF, 0x15, 0x8F, 0x90, 0x08, 0x31, 0x55, 0x0F, 0x51, 0x0F, 0x9E, 0xFC, 0xAF, 0x92,
/* 0420 */  0x08, 0x61, 0xCB, 0xFC, 0xC4, 0x08, 0x0F, 0xEF, 0xDA, 0x65, 0x9E, 0xFC, 0x64, 0x9E, 0xFD, 0xC0,
/* 0430 */  0xC4, 0xC6, 0xD7, 0x00, 0xC7, 0x57, 0xFE, 0x52, 0x00, 0xEE, 0x00, 0x00, 0xFE, 0x0B, 0x00, 0x61,
/* 0440 */  0xF8, 0xEC, 0x88, 0x0B, 0x0F, 0x52, 0x1F, 0xC6, 0xD7, 0xFF, 0xC1, 0x51, 0xFF, 0x50, 0x0F, 0xEF,
/* 0450 */  0x1A, 0xC1, 0x51, 0x47, 0x50, 0x07, 0xEF, 0x13, 0xC1, 0x51, 0x47, 0x50, 0x07, 0xEF, 0x0C, 0xC1,
/* 0460 */  0x51, 0x47, 0x50, 0x47, 0xEF, 0x05, 0xC1, 0x51, 0xF7, 0x50, 0x07, 0x5F, 0x90, 0x08, 0x61, 0x48,
/* 0470 */  0xC0, 0xD7, 0xD7, 0xFB, 0x80, 0xFB, 0xF6, 0x47, 0xDD, 0x13, 0xB7, 0xC7, 0x17, 0x31, 0x2D, 0x04,
/* 0480 */  0x00, 0xFB, 0x16, 0x8C, 0x02, 0x9E, 0xFC, 0xAB, 0xC6, 0x61, 0xCA, 0xEF, 0xE9, 0xEF, 0xFE, 0xFF,
};

