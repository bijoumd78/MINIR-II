//	---------------------------------------------------------
//	  Copyright (C) Siemens AG 1998  All Rights Reserved.
//	---------------------------------------------------------
//
//	 Project: NUMARIS/4
//	    File: \n4_servers3\pkg\MrServers\MrVista\include\unpack.h
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

#if ! defined PACKED_MEMBER
#error "pack/unpack should not be nested"
#endif

#if defined(_MSC_VER) || defined(_INTEL_COMPILER)

#undef PACKED_MEMBER
#pragma pack(pop, pack_header_included )
// #pragma warning( pop )
// #pragma message( "Packing=default Warning on again" )

#elif defined(__GNUC__)

#undef PACKED_MEMBER

#else

#error "Unknown compiler. Insert appropriate pack-pragma here" 

#endif
