//	---------------------------------------------------------
//	  Copyright (C) Siemens AG 1998  All Rights Reserved.
//	---------------------------------------------------------
//
//	 Project: NUMARIS/4
//	    File: \n4_servers3\pkg\MrServers\MrVista\include\pack.h
//	 Version:
//	  Author: CC_IMAMANAG
//	    Date: n.a.
//
//	    Lang: C++
//
//	 Descrip:
//	
//
//	 Classes:
//
//	---------------------------------------------------------

#if defined PACKED_MEMBER
#error "pack/unpack should not be nested"
#endif

#if defined(_MSC_VER) || defined(_INTEL_COMPILER)
// #pragma message( "Packing=1 Warning off" )
// #pragma warning( push )
#pragma warning( disable:4103)

#pragma pack(push, pack_header_included)
#pragma pack(1)

#define PACKED_MEMBER( __type__, __name__ ) __type__ __name__ 

#elif defined(__GNUC__)

#define PACKED_MEMBER( __type__, __name__ ) __type__ __name__ __attribute__ ((packed))

#else

#error "Unknown compiler. Insert appropriate pack-pragma here" 

#endif
