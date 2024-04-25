/*
 * Z80SIM  -  a Z80-CPU simulator
 *
 * Copyright (C) 2014-2022 Udo Munk
 * Copyright (C) 2021-2022 David McNaughton
 *
 * Configuration for a Cromemco Z-1 system
 *
 * History:
 * 15-DEC-14 first version
 * 20-DEC-14 added 4FDC emulation and machine boots CP/M 2.2
 * 28-DEC-14 second version with 16FDC, CP/M 2.2 boots
 * 01-JAN-15 fixed 16FDC, machine now also boots CDOS 2.58 from 8" and 5.25"
 * 01-JAN-15 fixed frontpanel switch settings, added boot flag to fp switch
 * 12-JAN-15 fdc and tu-art improvements, implemented banked memory
 * 10-MAR-15 TU-ART lpt's implemented for CP/M, CDOS and Cromix
 * 26-MAR-15 TU-ART tty's implemented for CDOS and Cromix
 * 11-AUG-16 implemented memwrt as function to support ROM
 * 01-DEC-16 implemented memrdr to separate memory from CPU
 * 06-DEC-16 implemented status display and stepping for all machine cycles
 * 12-JAN-17 improved configuration and front panel LED timing
 * 10-APR-18 trap CPU on unsupported bus data during interrupt
 * 22-APR-18 implemented TCP socket polling
 * 29-AUG-21 new memory configuration sections
 * 02-SEP-21 implement banked ROM
 * 14-JUL-22 added generic AT modem and HAL
 */

/*
 *	The following defines may be activated, commented or modified
 *	by user for her/his own purpose.
 */
#define DEF_CPU Z80	/* default CPU (Z80 or I8080) */
#define CPU_SPEED 4	/* default CPU speed */
#define Z80_UNDOC	/* compile undocumented Z80 instructions */
/*#define WANT_FASTB*/	/* much faster but not accurate Z80 block instr. */
#define CORE_LOG	/* use LOG() logging in core simulator */

/*#define WANT_ICE*/	/* attach ICE to headless machine */
/*#define WANT_TIM*/	/* don't count t-states */
/*#define HISIZE  1000*//* no history */
/*#define SBSIZE  10*/	/* no breakpoints */

#define HAS_DAZZLER	/* has simulated I/O for Cromemco Dazzler */
#define HAS_DISKS	/* uses disk images */
#define HAS_CONFIG	/* has configuration files somewhere */
#define HAS_BANKED_ROM	/* has banked RDOS ROM */

/*#define HAS_DISKMANAGER*/	/* uses file based disk map for disks[] */
/*#define HAS_NETSERVER*/	/* uses civet webserver to present a web based frontend */
#define HAS_MODEM		/* has simulated 'AT' style modem over TCP/IP (telnet) */
#define HAS_HAL			/* implements a hardware abstraction layer (HAL) for TU-ART devices */

#define CROMEMCOSIM
#define MACHINE "cromemco"
#define DOCUMENT_ROOT "../webfrontend/www/" MACHINE

#define NUMNSOC 2	/* number of TCP/IP sockets, 2 per TU-ART */
#define TCPASYNC	/* use async I/O if possible */
#define SERVERPORT 4010	/* first TCP/IP server port used */
#define NUMUSOC 0	/* number of UNIX sockets */

/*
 * SIGIO on BSD sockets not working with Cygwin
 */
#ifdef __CYGWIN__
#undef TCPASYNC
#endif

extern void do_sleep_ms(int);
#define SLEEP_MS(t)	do_sleep_ms(t)

/*
 *	The following defines may be modified and activated by
 *	user, to print her/his copyright for a simulated system,
 *	which contains the Z80/8080 CPU emulations as a part.
 */

#define USR_COM	"Cromemco Z-1 Simulation"
#define USR_REL	"1.19"
#define USR_CPR	"\nCopyright (C) 2014-2022 by Udo Munk & " \
		"2021-2022 by David McNaughton"

#include "simcore.h"
