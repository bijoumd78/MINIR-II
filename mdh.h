/*---------------------------------------------------------------------------*/
/*  Copyright (C) Siemens AG 1998  All Rights Reserved.  Confidential        */
/*---------------------------------------------------------------------------*/
/*
 * Project: NUMARIS/4
 *    File: \n4\pkg\MrServers\MrMeasSrv\SeqIF\MDH\mdh.h
 * Version:
 *  Author: CC_MEAS SCHOSTZF
 *    Date: n.a.
 *
 *    Lang: C
 *
 * Descrip: measurement data header
 *
 *                      ATTENTION
 *                      ---------
 *
 *  If you change the measurement data header, you have to take care that
 *  long variables start at an address which is aligned for longs. If you add
 *  a short variable, then add two shorts from the short array or use the
 *  second one from the previous change (called "dummy", if only one was added and
 *  no dummy exists).
 *  Then you have to extend the swap-method from MdhProxy.
 *  This is necessary, because a 32 bit swaped is executed from MPCU to image
 *  calculator.
 *  Additional, you have to change the dump method from libIDUMP/IDUMPRXUInstr.cpp.
 *
 * Functns: n.a.
 *
 *---------------------------------------------------------------------------*/

/*--------------------------------------------------------------------------*/
/* Include control                                                          */
/*--------------------------------------------------------------------------*/
#ifndef MDH_H
#define MDH_H


/*--------------------------------------------------------------------------*/
/* Include MR basic type definitions                                        */
/*--------------------------------------------------------------------------*/
#include "MrBasicTypes.h"

/*--------------------------------------------------------------------------*/
/*  Definition of header parameters                                         */
/*--------------------------------------------------------------------------*/
#define MDH_NUMBEROFEVALINFOMASK   2
#define MDH_NUMBEROFICEPROGRAMPARA 4

/*--------------------------------------------------------------------------*/
/*  Definition of free header parameters (short)                            */
/*--------------------------------------------------------------------------*/
#define MDH_FREEHDRPARA  (4)

/*--------------------------------------------------------------------------*/
/* Definition of time stamp tick interval/frequency                         */
/* (used for ulTimeStamp and ulPMUTimeStamp                                 */
/*--------------------------------------------------------------------------*/
#define RXU_TIMER_INTERVAL  (2500000)     /* data header timer interval [ns]*/
#define RXU_TIMER_FREQUENCY (400)         /* data header timer frequency[Hz]*/

/*--------------------------------------------------------------------------*/
/*  Definition of bit masks for ulFlagsAndDMALength field                   */
/*--------------------------------------------------------------------------*/
#define MDH_DMA_LENGTH_MASK   (0x01FFFFFFL)
#define MDH_PACK_BIT_MASK     (0x02000000L)
#define MDH_ENABLE_FLAGS_MASK (0xFC000000L)

/*--------------------------------------------------------------------------*/
/* Definition of loop counter structure                                     */
/* Note: any changes of this structure affect the corresponding swapping    */
/*       method of the measurement data header proxy class (MdhProxy)       */
/*--------------------------------------------------------------------------*/
#include "pack.h"
typedef struct
{
  PACKED_MEMBER( uint16_t,  ushLine         ); /* line index                   */
  PACKED_MEMBER( uint16_t,  ushAcquisition  ); /* acquisition index            */
  PACKED_MEMBER( uint16_t,  ushSlice        ); /* slice index                  */
  PACKED_MEMBER( uint16_t,  ushPartition    ); /* partition index              */
  PACKED_MEMBER( uint16_t,  ushEcho         ); /* echo index                   */
  PACKED_MEMBER( uint16_t,  ushPhase        ); /* phase index                  */
  PACKED_MEMBER( uint16_t,  ushRepetition   ); /* measurement repeat index     */
  PACKED_MEMBER( uint16_t,  ushSet          ); /* set index                    */
  PACKED_MEMBER( uint16_t,  ushSeg          ); /* segment index  (for TSE)     */
  PACKED_MEMBER( uint16_t,  ushIda          ); /* IceDimension a index         */
  PACKED_MEMBER( uint16_t,  ushIdb          ); /* IceDimension b index         */
  PACKED_MEMBER( uint16_t,  ushIdc          ); /* IceDimension c index         */
  PACKED_MEMBER( uint16_t,  ushIdd          ); /* IceDimension d index         */
  PACKED_MEMBER( uint16_t,  ushIde          ); /* IceDimension e index         */
} sLoopCounter;                                /* sizeof : 28 byte             */

/*--------------------------------------------------------------------------*/
/*  Definition of slice vectors                                             */
/*--------------------------------------------------------------------------*/

typedef struct
{
  PACKED_MEMBER( float,  flSag          );
  PACKED_MEMBER( float,  flCor          );
  PACKED_MEMBER( float,  flTra          );
} sVector;

typedef struct
{
  sVector sSlicePosVec; /* slice position vector        */
  PACKED_MEMBER( float,           aflQuaternion[4] ); /* rotation matrix as quaternion*/
} sSliceData;                                         /* sizeof : 28 byte             */

/*--------------------------------------------------------------------------*/
/*  Definition of cut-off data                                              */
/*--------------------------------------------------------------------------*/
typedef struct
{
  PACKED_MEMBER( uint16_t,  ushPre          );    /* write ushPre zeros at line start */
  PACKED_MEMBER( uint16_t,  ushPost         );    /* write ushPost zeros at line end  */
} sCutOffData;


/*--------------------------------------------------------------------------*/
/*  Definition of measurement data header                                   */
/*--------------------------------------------------------------------------*/
typedef struct
{
  PACKED_MEMBER( uint32_t,     ulFlagsAndDMALength           );    // bit  0..24: DMA length [bytes]
                                                                   // bit     25: pack bit
                                                                   // bit 26..31: pci_rx enable flags                   4 byte
  PACKED_MEMBER( int32_t,      lMeasUID                      );    // measurement user ID                               4
  PACKED_MEMBER( uint32_t,     ulScanCounter                 );    // scan counter [1...]                               4
  PACKED_MEMBER( uint32_t,     ulTimeStamp                   );    // time stamp [2.5 ms ticks since 00:00]             4
  PACKED_MEMBER( uint32_t,     ulPMUTimeStamp                );    // PMU time stamp [2.5 ms ticks since last trigger]  4
  PACKED_MEMBER( uint32_t,     aulEvalInfoMask[MDH_NUMBEROFEVALINFOMASK] ); // evaluation info mask field           8
  PACKED_MEMBER( uint16_t,     ushSamplesInScan              );    // # of samples acquired in scan                     2
  PACKED_MEMBER( uint16_t,     ushUsedChannels               );    // # of channels used in scan                        2   =32
  sLoopCounter sLC;																					 			 // loop counters                                    28   =60
  sCutOffData sCutOff;    																				 // cut-off values                                    4
  PACKED_MEMBER( uint16_t,     ushKSpaceCentreColumn         );    // centre of echo                                    2
  PACKED_MEMBER( uint16_t,     ushCoilSelect                 );    // Bit 0..3: CoilSelect                              2
  PACKED_MEMBER( float,        fReadOutOffcentre             );    // ReadOut offcenter value                           4
  PACKED_MEMBER( uint32_t,     ulTimeSinceLastRF             );    // Sequence time stamp since last RF pulse           4
  PACKED_MEMBER( uint16_t,     ushKSpaceCentreLineNo         );    // number of K-space centre line                     2
  PACKED_MEMBER( uint16_t,     ushKSpaceCentrePartitionNo    );    // number of K-space centre partition                2
  PACKED_MEMBER( uint16_t,     aushIceProgramPara[MDH_NUMBEROFICEPROGRAMPARA] ); // free parameter for IceProgram   8   =88
  PACKED_MEMBER( uint16_t,     aushFreePara[MDH_FREEHDRPARA] );    // free parameter                          4 * 2 =   8
  sSliceData sSD;																									 // Slice Data                                       28   =124
  PACKED_MEMBER( uint16_t,	   ushChannelId                  );    // channel Id must be the last parameter             2
  PACKED_MEMBER( uint16_t,	   ushPTABPosNeg                 );    // negative, absolute PTAB position in [0.1 mm]      2
                                                                   // (automatically set by PCI_TX firmware)
} sMDH;                                                            // total length: 32 * 32 Bit (128 Byte)            128

#include "unpack.h"

#endif   /* MDH_H */

/*---------------------------------------------------------------------------*/
/*  Copyright (C) Siemens AG 1998  All Rights Reserved.  Confidential        */
/*---------------------------------------------------------------------------*/
