/*
 *	Z80 - Assembler
 *	Copyright (C) 1987-2022 by Udo Munk
 *	Copyright (C) 2022 by Thomas Eberhardt
 *
 *	History:
 *	17-SEP-1987 Development under Digital Research CP/M 2.2
 *	28-JUN-1988 Switched to Unix System V.3
 *	22-OCT-2006 changed to ANSI C for modern POSIX OS's
 *	03-FEB-2007 more ANSI C conformance and reduced compiler warnings
 *	18-MAR-2007 use default output file extension dependent on format
 *	04-OCT-2008 fixed comment bug, ';' string argument now working
 *	22-FEB-2014 fixed is...() compiler warnings
 *	13-JAN-2016 fixed buffer overflow, new expression parser from Didier
 *	02-OCT-2017 bug fixes in expression parser from Didier
 *	28-OCT-2017 added variable symbol lenght and other improvements
 *	15-MAY-2018 mark unreferenced symbols in listing
 *	30-JUL-2021 fix verbose option
 *	28-JAN-2022 added syntax check for OUT (n),A
 *	24-SEP-2022 added undocumented Z80 instructions and 8080 mode (TE)
 */

/*
 *	processing of all real Z80/8080 opcodes
 */

#include <stdio.h>
#include <string.h>
#include "z80a.h"
#include "z80aglb.h"

char *get_second(char *);
int ldreg(int, int), ldixhl(int, int), ldiyhl(int, int);
int ldbcde(int), ldhl(void), ldixy(int);
int ldsp(void), ldihl(void), ldiixy(int), ldinn(void);
int addhl(void), addix(void), addiy(void), sbadchl(int);
int aluop(int, char *);

extern int eval(char *);
extern int calc_val(char *);
extern int chk_byte(int);
extern int chk_sbyte(int);
extern void asmerr(int);
extern int get_reg(char *);
extern void put_label(void);

#define UNUSED(x)	(void)(x)

/*
 *	process 1byte opcodes without arguments
 */
int op_1b(int b1, int dummy)
{
	UNUSED(dummy);

	if (pass == 1)
		if (*label)
			put_label();
	ops[0] = b1;
	return(1);
}

/*
 *	process 2byte opcodes without arguments
 */
int op_2b(int b1, int b2)
{
	if (pass == 1)
		if (*label)
			put_label();
	ops[0] = b1;
	ops[1] = b2;
	return(2);
}

/*
 *	IM
 */
int op_im(int dummy1, int dummy2)
{
	UNUSED(dummy1);
	UNUSED(dummy2);

	if (pass == 1) {		/* PASS 1 */
		if (*label)
			put_label();
	} else {			/* PASS 2 */
		ops[0] = 0xed;
		switch(eval(operand)) {
		case 0:			/* IM 0 */
			ops[1] = 0x46;
			break;
		case 1:			/* IM 1 */
			ops[1] = 0x56;
			break;
		case 2:			/* IM 2 */
			ops[1] = 0x5e;
			break;
		default:
			ops[1] = 0;
			asmerr(E_ILLOPE);
			break;
		}
	}
	return(2);
}

/*
 *	PUSH and POP
 */
int op_pupo(int base_op, int dummy)
{
	register int len;

	UNUSED(dummy);

	if (pass == 1)
		if (*label)
			put_label();
	switch (get_reg(operand)) {
	case REGAF:			/* PUSH/POP AF */
		len = 1;
		ops[0] = base_op + 0x30;
		break;
	case REGBC:			/* PUSH/POP BC */
		len = 1;
		ops[0] = base_op;
		break;
	case REGDE:			/* PUSH/POP DE */
		len = 1;
		ops[0] = base_op + 0x10;
		break;
	case REGHL:			/* PUSH/POP HL */
		len = 1;
		ops[0] = base_op + 0x20;
		break;
	case REGIX:			/* PUSH/POP IX */
		len = 2;
		ops[0] = 0xdd;
		ops[1] = base_op + 0x20;
		break;
	case REGIY:			/* PUSH/POP IY */
		len = 2;
		ops[0] = 0xfd;
		ops[1] = base_op + 0x20;
		break;
	case NOOPERA:			/* missing operand */
		len = 1;
		ops[0] = 0;
		asmerr(E_MISOPE);
		break;
	default:			/* invalid operand */
		len = 1;
		ops[0] = 0;
		asmerr(E_ILLOPE);
	}
	return(len);
}

/*
 *	EX
 */
int op_ex(int dummy1, int dummy2)
{
	register int len;

	UNUSED(dummy1);
	UNUSED(dummy2);

	if (pass == 1)
		if (*label)
			put_label();
	if (strncmp(operand, "DE,HL", 5) == 0) {
		len = 1;
		ops[0] = 0xeb;
	} else if (strncmp(operand, "AF,AF'", 7) == 0) {
		len = 1;
		ops[0] = 0x08;
	} else if (strncmp(operand, "(SP),HL", 7) == 0) {
		len = 1;
		ops[0] = 0xe3;
	} else if (strncmp(operand, "(SP),IX", 7) == 0) {
		len = 2;
		ops[0] = 0xdd;
		ops[1] = 0xe3;
	} else if (strncmp(operand, "(SP),IY", 7) == 0) {
		len = 2;
		ops[0] = 0xfd;
		ops[1] = 0xe3;
	} else {
		len = 1;
		ops[0] = 0;
		asmerr(E_ILLOPE);
	}
	return(len);
}

/*
 *	CALL
 */
int op_call(int dummy1, int dummy2)
{
	register char *p1, *p2;
	register int i;

	UNUSED(dummy1);
	UNUSED(dummy2);

	if (pass == 1) {		/* PASS 1 */
		if (*label)
			put_label();
	} else {			/* PASS 2 */
		p1 = operand;
		p2 = tmp;
		while (*p1 != ',' && *p1 != '\0')
			*p2++ = *p1++;
		*p2 = '\0';
		switch (get_reg(tmp)) {
		case REGC:		/* CALL C,nn */
			i = eval(strchr(operand, ',') + 1);
			ops[0] = 0xdc;
			ops[1] = i & 0xff;
			ops[2] = i >> 8;
			break;
		case FLGNC:		/* CALL NC,nn */
			i = eval(strchr(operand, ',') + 1);
			ops[0] = 0xd4;
			ops[1] = i & 0xff;
			ops[2] = i >> 8;
			break;
		case FLGZ:		/* CALL Z,nn */
			i = eval(strchr(operand, ',') + 1);
			ops[0] = 0xcc;
			ops[1] = i & 0xff;
			ops[2] = i >> 8;
			break;
		case FLGNZ:		/* CALL NZ,nn */
			i = eval(strchr(operand, ',') + 1);
			ops[0] = 0xc4;
			ops[1] = i & 0xff;
			ops[2] = i >> 8;
			break;
		case FLGPE:		/* CALL PE,nn */
			i = eval(strchr(operand, ',') + 1);
			ops[0] = 0xec;
			ops[1] = i & 0xff;
			ops[2] = i >> 8;
			break;
		case FLGPO:		/* CALL PO,nn */
			i = eval(strchr(operand, ',') + 1);
			ops[0] = 0xe4;
			ops[1] = i & 0xff;
			ops[2] = i >> 8;
			break;
		case FLGM:		/* CALL M,nn */
			i = eval(strchr(operand, ',') + 1);
			ops[0] = 0xfc;
			ops[1] = i & 0xff;
			ops[2] = i >> 8;
			break;
		case FLGP:		/* CALL P,nn */
			i = eval(strchr(operand, ',') + 1);
			ops[0] = 0xf4;
			ops[1] = i & 0xff;
			ops[2] = i >> 8;
			break;
		case NOREG:		/* CALL nn */
			i = eval(operand);
			ops[0] = 0xcd;
			ops[1] = i & 0xff;
			ops[2] = i >> 8;
			break;
		case NOOPERA:		/* missing operand */
			ops[0] = 0;
			ops[1] = 0;
			ops[2] = 0;
			asmerr(E_MISOPE);
			break;
		default:		/* invalid operand */
			ops[0] = 0;
			ops[1] = 0;
			ops[2] = 0;
			asmerr(E_ILLOPE);
		}
	}
	return(3);
}

/*
 *	RST
 */
int op_rst(int dummy1, int dummy2)
{
	register int op;

	UNUSED(dummy1);
	UNUSED(dummy2);

	if (pass == 1) {		/* PASS 1 */
		if (*label)
			put_label();
	} else {			/* PASS 2 */
		op = eval(operand);
		if (op < 0 || (op / 8 > 7) || (op % 8 != 0)) {
			ops[0] = 0;
			asmerr(E_VALOUT);
		} else
			ops[0] = 0xc7 + op;
	}
	return(1);
}

/*
 *	RET
 */
int op_ret(int dummy1, int dummy2)
{
	UNUSED(dummy1);
	UNUSED(dummy2);

	if (pass == 1) {		/* PASS 1 */
		if (*label)
			put_label();
	} else {			/* PASS 2 */
		switch (get_reg(operand)) {
		case NOOPERA:		/* RET */
			ops[0] = 0xc9;
			break;
		case REGC:		/* RET C */
			ops[0] = 0xd8;
			break;
		case FLGNC:		/* RET NC */
			ops[0] = 0xd0;
			break;
		case FLGZ:		/* RET Z */
			ops[0] = 0xc8;
			break;
		case FLGNZ:		/* RET NZ */
			ops[0] = 0xc0;
			break;
		case FLGPE:		/* RET PE */
			ops[0] = 0xe8;
			break;
		case FLGPO:		/* RET PO */
			ops[0] = 0xe0;
			break;
		case FLGM:		/* RET M */
			ops[0] = 0xf8;
			break;
		case FLGP:		/* RET P */
			ops[0] = 0xf0;
			break;
		default:		/* invalid operand */
			ops[0] = 0;
			asmerr(E_ILLOPE);
		}
	}
	return(1);
}

/*
 *	JP
 */
int op_jp(int dummy1, int dummy2)
{
	register char *p1, *p2;
	register int i, len;

	UNUSED(dummy1);
	UNUSED(dummy2);

	if (pass == 1)
		if (*label)
			put_label();
	p1 = operand;
	p2 = tmp;
	while (*p1 != ',' && *p1 != '\0')
		*p2++ = *p1++;
	*p2 = '\0';
	switch (get_reg(tmp)) {
	case REGC:			/* JP C,nn */
		len = 3;
		if (pass == 2) {
			i = eval(strchr(operand, ',') + 1);
			ops[0] = 0xda;
			ops[1] = i & 0xff;
			ops[2] = i >> 8;
		}
		break;
	case FLGNC:			/* JP NC,nn */
		len = 3;
		if (pass == 2) {
			i = eval(strchr(operand, ',') + 1);
			ops[0] = 0xd2;
			ops[1] = i & 0xff;
			ops[2] = i >> 8;
		}
		break;
	case FLGZ:			/* JP Z,nn */
		len = 3;
		if (pass == 2) {
			i = eval(strchr(operand, ',') + 1);
			ops[0] = 0xca;
			ops[1] = i & 0xff;
			ops[2] = i >> 8;
		}
		break;
	case FLGNZ:			/* JP NZ,nn */
		len = 3;
		if (pass == 2) {
			i = eval(strchr(operand, ',') + 1);
			ops[0] = 0xc2;
			ops[1] = i & 0xff;
			ops[2] = i >> 8;
		}
		break;
	case FLGPE:			/* JP PE,nn */
		len = 3;
		if (pass == 2) {
			i = eval(strchr(operand, ',') + 1);
			ops[0] = 0xea;
			ops[1] = i & 0xff;
			ops[2] = i >> 8;
		}
		break;
	case FLGPO:			/* JP PO,nn */
		len = 3;
		if (pass == 2) {
			i = eval(strchr(operand, ',') + 1);
			ops[0] = 0xe2;
			ops[1] = i & 0xff;
			ops[2] = i >> 8;
		}
		break;
	case FLGM:			/* JP M,nn */
		len = 3;
		if (pass == 2) {
			i = eval(strchr(operand, ',') + 1);
			ops[0] = 0xfa;
			ops[1] = i & 0xff;
			ops[2] = i >> 8;
		}
		break;
	case FLGP:			/* JP P,nn */
		len = 3;
		if (pass == 2) {
			i = eval(strchr(operand, ',') + 1);
			ops[0] = 0xf2;
			ops[1] = i & 0xff;
			ops[2] = i >> 8;
		}
		break;
	case REGIHL:			/* JP (HL) */
		len = 1;
		ops[0] = 0xe9;
		break;
	case REGIIX:			/* JP (IX) */
		len = 2;
		ops[0] = 0xdd;
		ops[1] = 0xe9;
		break;
	case REGIIY:			/* JP (IY) */
		len = 2;
		ops[0] = 0xfd;
		ops[1] = 0xe9;
		break;
	case NOREG:			/* JP nn */
		len = 3;
		if (pass == 2) {
			i = eval(operand);
			ops[0] = 0xc3;
			ops[1] = i & 0xff;
			ops[2] = i >> 8;
		}
		break;
	case NOOPERA:			/* missing operand */
		len = 1;
		ops[0] = 0;
		asmerr(E_MISOPE);
		break;
	default:			/* invalid operand */
		len = 1;
		ops[0] = 0;
		asmerr(E_ILLOPE);
	}
	return(len);
}

/*
 *	JR
 */
int op_jr(int dummy1, int dummy2)
{
	register char *p1, *p2;

	UNUSED(dummy1);
	UNUSED(dummy2);

	if (pass == 1) {		/* PASS 1 */
		if (*label)
			put_label();
	} else {			/* PASS 2 */
		p1 = operand;
		p2 = tmp;
		while (*p1 != ',' && *p1 != '\0')
			*p2++ = *p1++;
		*p2 = '\0';
		switch (get_reg(tmp)) {
		case REGC:		/* JR C,n */
			ops[0] = 0x38;
			ops[1] = chk_sbyte(eval(strchr(operand, ',') + 1) - pc - 2);
			break;
		case FLGNC:		/* JR NC,n */
			ops[0] = 0x30;
			ops[1] = chk_sbyte(eval(strchr(operand, ',') + 1) - pc - 2);
			break;
		case FLGZ:		/* JR Z,n */
			ops[0] = 0x28;
			ops[1] = chk_sbyte(eval(strchr(operand, ',') + 1) - pc - 2);
			break;
		case FLGNZ:		/* JR NZ,n */
			ops[0] = 0x20;
			ops[1] = chk_sbyte(eval(strchr(operand, ',') + 1) - pc - 2);
			break;
		case NOREG:		/* JR n */
			ops[0] = 0x18;
			ops[1] = chk_sbyte(eval(operand) - pc - 2);
			break;
		case NOOPERA:		/* missing operand */
			ops[0] = 0;
			ops[1] = 0;
			asmerr(E_MISOPE);
			break;
		default:		/* invalid operand */
			ops[0] = 0;
			ops[1] = 0;
			asmerr(E_ILLOPE);
		}
	}
	return(2);
}

/*
 *	DJNZ
 */
int op_djnz(int dummy1, int dummy2)
{
	UNUSED(dummy1);
	UNUSED(dummy2);

	if (pass == 1) {		/* PASS 1 */
		if (*label)
			put_label();
	} else {			/* PASS 2 */
		ops[0] = 0x10;
		ops[1] = chk_sbyte(eval(operand) - pc - 2);
	}
	return(2);
}

/*
 *	LD
 */
int op_ld(int dummy1, int dummy2)
{
	register int len, op;
	register char *p1, *p2;

	UNUSED(dummy1);
	UNUSED(dummy2);

	if (pass == 1)
		if (*label)
			put_label();
	p1 = operand;
	p2 = tmp;
	while (*p1 != ',' && *p1 != '\0')
		*p2++ = *p1++;
	*p2 = '\0';
	switch (op = get_reg(tmp)) {
	case REGA:			/* LD A,? */
	case REGB:			/* LD B,? */
	case REGC:			/* LD C,? */
	case REGD:			/* LD D,? */
	case REGE:			/* LD E,? */
	case REGH:			/* LD H,? */
	case REGL:			/* LD L,? */
		len = ldreg(0x40 + (op << 3), 0x06 + (op << 3));
		break;
	case REGIXH:			/* LD IXH,? (undocumented) */
		len = ldixhl(0x60, 0x26);
		break;
	case REGIXL:			/* LD IXL,? (undocumented) */
		len = ldixhl(0x68, 0x2e);
		break;
	case REGIYH:			/* LD IYH,? (undocumented) */
		len = ldiyhl(0x60, 0x26);
		break;
	case REGIYL:			/* LD IYL,? (undocumented) */
		len = ldiyhl(0x68, 0x2e);
		break;
	case REGI:			/* LD I,A */
		if (get_reg(get_second(operand)) == REGA) {
			len = 2;
			ops[0] = 0xed;
			ops[1] = 0x47;
			break;
		}
		len = 1;
		ops[0] = 0;
		asmerr(E_ILLOPE);
		break;
	case REGR:			/* LD R,A */
		if (get_reg(get_second(operand)) == REGA) {
			len = 2;
			ops[0] = 0xed;
			ops[1] = 0x4f;
			break;
		}
		len = 1;
		ops[0] = 0;
		asmerr(E_ILLOPE);
		break;
	case REGBC:			/* LD BC,? */
		len = ldbcde(0x01);
		break;
	case REGDE:			/* LD DE,? */
		len = ldbcde(0x11);
		break;
	case REGHL:			/* LD HL,? */
		len = ldhl();
		break;
	case REGIX:			/* LD IX,? */
		len = ldixy(0xdd);
		break;
	case REGIY:			/* LD IY,? */
		len = ldixy(0xfd);
		break;
	case REGSP:			/* LD SP,? */
		len = ldsp();
		break;
	case REGIHL:			/* LD (HL),? */
		len = ldihl();
		break;
	case REGIBC:			/* LD (BC),A */
		if (get_reg(get_second(operand)) == REGA) {
			len = 1;
			ops[0] = 0x02;
			break;
		}
		len = 1;
		ops[0] = 0;
		asmerr(E_ILLOPE);
		break;
	case REGIDE:			/* LD (DE),A */
		if (get_reg(get_second(operand)) == REGA) {
			len = 1;
			ops[0] = 0x12;
			break;
		}
		len = 1;
		ops[0] = 0;
		asmerr(E_ILLOPE);
		break;
	case NOOPERA:			/* missing operand */
		len = 1;
		ops[0] = 0;
		asmerr(E_MISOPE);
		break;
	default:
		if (strncmp(operand, "(IX+", 4) == 0)
			len = ldiixy(0xdd);	/* LD (IX+d),? */
		else if (strncmp(operand, "(IY+", 4) == 0)
			len = ldiixy(0xfd);	/* LD (IY+d),? */
		else if (*operand == '(')
			len = ldinn();		/* LD (nn),? */
		else {			/* invalid operand */
			len = 1;
			ops[0] = 0;
			asmerr(E_ILLOPE);
		}
	}
	return(len);
}

/*
 *	LD [A,B,C,D,E,H,L],?
 */
int ldreg(int base_op, int base_opn)
{
	register int op;
	register int i, len;
	register char *p;

	p = get_second(operand);
	switch (op = get_reg(p)) {
	case REGA:			/* LD reg,A */
	case REGB:			/* LD reg,B */
	case REGC:			/* LD reg,C */
	case REGD:			/* LD reg,D */
	case REGE:			/* LD reg,E */
	case REGH:			/* LD reg,H */
	case REGL:			/* LD reg,L */
	case REGIHL:			/* LD reg,(HL) */
		len = 1;
		ops[0] = base_op + op;
		break;
	case REGIXH:			/* LD reg,IXH (undocumented) */
	case REGIXL:			/* LD reg,IXL (undocumented) */
	case REGIYH:			/* LD reg,IYH (undocumented) */
	case REGIYL:			/* LD reg,IYL (undocumented) */
		if ((base_op & 0xf0) != 0x60) {
			/* only for A,B,C,D,E */
			switch (op) {
			case REGIXH:	/* LD [ABCDE],IXH (undocumented) */
				len = 2;
				ops[0] = 0xdd;
				ops[1] = base_op + 0x04;
				break;
			case REGIXL:	/* LD [ABCDE],IXL (undocumented) */
				len = 2;
				ops[0] = 0xdd;
				ops[1] = base_op + 0x05;
				break;
			case REGIYH:	/* LD [ABCDE],IYH (undocumented) */
				len = 2;
				ops[0] = 0xfd;
				ops[1] = base_op + 0x04;
				break;
			case REGIYL:	/* LD [ABCDE],IYL (undocumented) */
				len = 2;
				ops[0] = 0xfd;
				ops[1] = base_op + 0x05;
				break;
			}
		}
		else {
			/* not for H, L */
			len = 1;
			ops[0] = 0;
			asmerr(E_ILLOPE);
		}
		break;
	case REGI:			/* LD reg,I */
	case REGR:			/* LD reg,R */
	case REGIBC:			/* LD reg,(BC) */
	case REGIDE:			/* LD reg,(DE) */
		if (base_op == 0x78) {
			/* only for A */
			switch (op) {
			case REGI:	/* LD A,I */
				len = 2;
				ops[0] = 0xed;
				ops[1] = 0x57;
				break;
			case REGR:	/* LD A,R */
				len = 2;
				ops[0] = 0xed;
				ops[1] = 0x5f;
				break;
			case REGIBC:	/* LD A,(BC) */
				len = 1;
				ops[0] = 0x0a;
				break;
			case REGIDE:	/* LD A,(DE) */
				len = 1;
				ops[0] = 0x1a;
				break;
			}
		}
		else {
			/* not A */
			len = 1;
			ops[0] = 0;
			asmerr(E_ILLOPE);
		}
		break;
	case NOREG:			/* operand isn't register */
		if (strncmp(p, "(IX+", 4) == 0) {
			len = 3;	/* LD reg,(IX+d) */
			if (pass == 2) {
				ops[0] = 0xdd;
				ops[1] = base_op + 0x06;
				ops[2] = chk_sbyte(calc_val(strchr(p, '+') + 1));
			}
		} else if (strncmp(p, "(IY+", 4) == 0) {
			len = 3;	/* LD reg,(IY+d) */
			if (pass == 2) {
				ops[0] = 0xfd;
				ops[1] = base_op + 0x06;
				ops[2] = chk_sbyte(calc_val(strchr(p, '+') + 1));
			}
		} else if (base_op == 0x78 && *p == '(' && *(p + strlen(p) - 1) == ')') {
			/* only for A */
			len = 3;	/* LD A,(nn) */
			if (pass == 2) {
				i = calc_val(p + 1);
				ops[0] = 0x3a;
				ops[1] = i & 255;
				ops[2] = i >> 8;
			}
		} else {		/* LD reg,n */
			len = 2;
			if (pass == 2) {
				ops[0] = base_opn;
				ops[1] = chk_byte(eval(p));
			}
		}
		break;
	case NOOPERA:			/* missing operand */
		ops[0] = 0;
		len = 1;
		asmerr(E_MISOPE);
		break;
	default:			/* invalid operand */
		ops[0] = 0;
		len = 1;
		asmerr(E_ILLOPE);
	}
	return(len);
}

/*
 *	LD IX[HL],? (undocumented)
 */
int ldixhl(int base_op, int base_opn)
{
	register int op;
	register int len;
	register char *p;

	p = get_second(operand);
	switch (op = get_reg(p)) {
	case REGA:			/* LD IX[HL],A (undocumented) */
	case REGB:			/* LD IX[HL],B (undocumented) */
	case REGC:			/* LD IX[HL],C (undocumented) */
	case REGD:			/* LD IX[HL],D (undocumented) */
	case REGE:			/* LD IX[HL],E (undocumented) */
		len = 2;
		ops[0] = 0xdd;
		ops[1] = base_op + op;
		break;
	case REGIXH:			/* LD IX[HL],IXH (undocumented) */
		len = 2;
		ops[0] = 0xdd;
		ops[1] = base_op + 0x04;
		break;
	case REGIXL:			/* LD IX[HL],IXL (undocumented) */
		len = 2;
		ops[0] = 0xdd;
		ops[1] = base_op + 0x05;
		break;
	case NOREG:			/* LD IX[HL],n (undocumented) */
		len = 3;
		if (pass == 2) {
			ops[0] = 0xdd;
			ops[1] = base_opn;
			ops[2] = chk_byte(eval(p));
		}
		break;
	case NOOPERA:			/* missing operand */
		len = 1;
		ops[0] = 0;
		asmerr(E_MISOPE);
		break;
	default:			/* invalid operand */
		len = 1;
		ops[0] = 0;
		asmerr(E_ILLOPE);
	}
	return(len);
}

/*
 *	LD IY[HL],? (undocumented)
 */
int ldiyhl(int base_op, int base_opn)
{
	register int op;
	register int len;
	register char *p;

	p = get_second(operand);
	switch (op = get_reg(p)) {
	case REGA:			/* LD IY[HL],A (undocumented) */
	case REGB:			/* LD IY[HL],B (undocumented) */
	case REGC:			/* LD IY[HL],C (undocumented) */
	case REGD:			/* LD IY[HL],D (undocumented) */
	case REGE:			/* LD IY[HL],E (undocumented) */
		len = 2;
		ops[0] = 0xfd;
		ops[1] = base_op + op;
		break;
	case REGIYH:			/* LD IY[HL],IYH (undocumented) */
		len = 2;
		ops[0] = 0xfd;
		ops[1] = base_op + 0x04;
		break;
	case REGIYL:			/* LD IY[HL],IYL (undocumented) */
		len = 2;
		ops[0] = 0xfd;
		ops[1] = base_op + 0x05;
		break;
	case NOREG:			/* LD IY[HL],n (undocumented) */
		len = 3;
		if (pass == 2) {
			ops[0] = 0xfd;
			ops[1] = base_opn;
			ops[2] = chk_byte(eval(p));
		}
		break;
	case NOOPERA:			/* missing operand */
		len = 1;
		ops[0] = 0;
		asmerr(E_MISOPE);
		break;
	default:			/* invalid operand */
		len = 1;
		ops[0] = 0;
		asmerr(E_ILLOPE);
	}
	return(len);
}

/*
 *	LD {BC,DE},?
 */
int ldbcde(int base_op)
{
	register int i, len;
	register char *p;

	p = get_second(operand);
	switch (get_reg(p)) {
	case NOREG:			/* operand isn't register */
		if (*p == '(' && *(p + strlen(p) - 1) == ')') {
			len = 4;	/* LD {BC,DE},(nn) */
			if (pass == 2) {
				i = calc_val(p + 1);
				ops[0] = 0xed;
				ops[1] = base_op + 0x4a;
				ops[2] = i & 0xff;
				ops[3] = i >> 8;
			}
		} else {
			len = 3;	/* LD {BC,DE},nn */
			if (pass == 2) {
				i = eval(p);
				ops[0] = base_op;
				ops[1] = i & 0xff;
				ops[2] = i >> 8;
			}
		}
		break;
	case NOOPERA:			/* missing operand */
		len = 1;
		ops[0] = 0;
		asmerr(E_MISOPE);
		break;
	default:			/* invalid operand */
		len = 1;
		ops[0] = 0;
		asmerr(E_ILLOPE);
	}
	return(len);
}

/*
 *	LD HL,?
 */
int ldhl(void)
{
	register int i, len;
	register char *p;

	p = get_second(operand);
	switch (get_reg(p)) {
	case NOREG:			/* operand isn't register */
		if (*p == '(' && *(p + strlen(p) - 1) == ')') {
			len = 3;	/* LD HL,(nn) */
			if (pass == 2) {
				i = calc_val(p + 1);
				ops[0] = 0x2a;
				ops[1] = i & 0xff;
				ops[2] = i >> 8;
			}
		} else {
			len = 3;	/* LD HL,nn */
			if (pass == 2) {
				i = eval(p);
				ops[0] = 0x21;
				ops[1] = i & 0xff;
				ops[2] = i >> 8;
			}
		}
		break;
	case NOOPERA:			/* missing operand */
		len = 1;
		ops[0] = 0;
		asmerr(E_MISOPE);
		break;
	default:			/* invalid operand */
		len = 1;
		ops[0] = 0;
		asmerr(E_ILLOPE);
	}
	return(len);
}

/*
 *	LD I[XY],?
 */
int ldixy(int prefix)
{
	register int i, len;
	register char *p;

	p = get_second(operand);
	switch (get_reg(p)) {
	case NOREG:			/* operand isn't register */
		if (*p == '(' && *(p + strlen(p) - 1) == ')') {
			len = 4;	/* LD I[XY],(nn) */
			if (pass == 2) {
				i = calc_val(p + 1);
				ops[0] = prefix;
				ops[1] = 0x2a;
				ops[2] = i & 0xff;
				ops[3] = i >> 8;
			}
		} else {
			len = 4;	/* LD I[XY],nn */
			if (pass == 2) {
				i = eval(p);
				ops[0] = prefix;
				ops[1] = 0x21;
				ops[2] = i & 0xff;
				ops[3] = i >> 8;
			}
		}
		break;
	case NOOPERA:			/* missing operand */
		len = 1;
		ops[0] = 0;
		asmerr(E_MISOPE);
		break;
	default:			/* invalid operand */
		len = 1;
		ops[0] = 0;
		asmerr(E_ILLOPE);
	}
	return(len);
}

/*
 *	LD SP,?
 */
int ldsp(void)
{
	register int i, len;
	register char *p;

	p = get_second(operand);
	switch (get_reg(p)) {
	case REGHL:			/* LD SP,HL */
		len = 1;
		ops[0] = 0xf9;
		break;
	case REGIX:			/* LD SP,IX */
		len = 2;
		ops[0] = 0xdd;
		ops[1] = 0xf9;
		break;
	case REGIY:			/* LD SP,IY */
		len = 2;
		ops[0] = 0xfd;
		ops[1] = 0xf9;
		break;
	case NOREG:			/* operand isn't register */
		if (*p == '(' && *(p + strlen(p) - 1) == ')') {
			len = 4;	/* LD SP,(nn) */
			if (pass == 2) {
				i = calc_val(p + 1);
				ops[0] = 0xed;
				ops[1] = 0x7b;
				ops[2] = i & 0xff;
				ops[3] = i >> 8;
			}
		} else {
			len = 3;	/* LD SP,nn */
			if (pass == 2) {
				i = eval(p);
				ops[0] = 0x31;
				ops[1] = i & 0xff;
				ops[2] = i >> 8;
			}
		}
		break;
	case NOOPERA:			/* missing operand */
		len = 1;
		ops[0] = 0;
		asmerr(E_MISOPE);
		break;
	default:			/* invalid operand */
		len = 1;
		ops[0] = 0;
		asmerr(E_ILLOPE);
	}
	return(len);
}

/*
 *	LD (HL),?
 */
int ldihl(void)
{
	register int op;
	register int len;
	register char *p;

	p = get_second(operand);
	switch (op = get_reg(p)) {
	case REGA:			/* LD (HL),A */
	case REGB:			/* LD (HL),B */
	case REGC:			/* LD (HL),C */
	case REGD:			/* LD (HL),D */
	case REGE:			/* LD (HL),E */
	case REGH:			/* LD (HL),H */
	case REGL:			/* LD (HL),L */
		len = 1;
		ops[0] = 0x70 + op;
		break;
	case NOREG:			/* LD (HL),n */
		len = 2;
		if (pass == 2) {
			ops[0] = 0x36;
			ops[1] = chk_byte(eval(p));
		}
		break;
	case NOOPERA:			/* missing operand */
		len = 1;
		ops[0] = 0;
		asmerr(E_MISOPE);
		break;
	default:			/* invalid operand */
		len = 1;
		ops[0] = 0;
		asmerr(E_ILLOPE);
	}
	return(len);
}

/*
 *	LD (I[XY]+d),?
 */
int ldiixy(int prefix)
{
	register int op;
	register int len;
	register char *p;

	p = get_second(operand);
	switch (op = get_reg(p)) {
	case REGA:			/* LD (I[XY]+d),A */
	case REGB:			/* LD (I[XY]+d),B */
	case REGC:			/* LD (I[XY]+d),C */
	case REGD:			/* LD (I[XY]+d),D */
	case REGE:			/* LD (I[XY]+d),E */
	case REGH:			/* LD (I[XY]+d),H */
	case REGL:			/* LD (I[XY]+d),L */
		len = 3;
		if (pass == 2) {
			ops[0] = prefix;
			ops[1] = 0x70 + op;
			ops[2] = chk_sbyte(calc_val(strchr(operand, '+') + 1));
		}
		break;
	case NOREG:			/* LD (I[XY]+d),n */
		len = 4;
		if (pass == 2) {
			ops[0] = prefix;
			ops[1] = 0x36;
			ops[2] = chk_sbyte(calc_val(strchr(operand, '+') + 1));
			ops[3] = chk_byte(eval(p));
		}
		break;
	case NOOPERA:			/* missing operand */
		len = 1;
		ops[0] = 0;
		asmerr(E_MISOPE);
		break;
	default:			/* invalid operand */
		len = 1;
		ops[0] = 0;
		asmerr(E_ILLOPE);
	}
	return(len);
}

/*
 *	LD (nn),?
 */
int ldinn(void)
{
	register int i, len;
	register char *p;

	p = get_second(operand);
	switch (get_reg(p)) {
	case REGA:			/* LD (nn),A */
		len = 3;
		if (pass == 2) {
			i = calc_val(operand + 1);
			ops[0] = 0x32;
			ops[1] = i & 0xff;
			ops[2] = i >> 8;
		}
		break;
	case REGBC:			/* LD (nn),BC */
		len = 4;
		if (pass == 2) {
			i = calc_val(operand + 1);
			ops[0] = 0xed;
			ops[1] = 0x43;
			ops[2] = i & 0xff;
			ops[3] = i >> 8;
		}
		break;
	case REGDE:			/* LD (nn),DE */
		len = 4;
		if (pass == 2) {
			i = calc_val(operand + 1);
			ops[0] = 0xed;
			ops[1] = 0x53;
			ops[2] = i & 0xff;
			ops[3] = i >> 8;
		}
		break;
	case REGHL:			/* LD (nn),HL */
		len = 3;
		if (pass == 2) {
			i = calc_val(operand + 1);
			ops[0] = 0x22;
			ops[1] = i & 0xff;
			ops[2] = i >> 8;
		}
		break;
	case REGSP:			/* LD (nn),SP */
		len = 4;
		if (pass == 2) {
			i = calc_val(operand + 1);
			ops[0] = 0xed;
			ops[1] = 0x73;
			ops[2] = i & 0xff;
			ops[3] = i >> 8;
		}
		break;
	case REGIX:			/* LD (nn),IX */
		len = 4;
		if (pass == 2) {
			i = calc_val(operand + 1);
			ops[0] = 0xdd;
			ops[1] = 0x22;
			ops[2] = i & 0xff;
			ops[3] = i >> 8;
		}
		break;
	case REGIY:			/* LD (nn),IY */
		len = 4;
		if (pass == 2) {
			i = calc_val(operand + 1);
			ops[0] = 0xfd;
			ops[1] = 0x22;
			ops[2] = i & 0xff;
			ops[3] = i >> 8;
		}
		break;
	case NOOPERA:			/* missing operand */
		len = 1;
		ops[0] = 0;
		asmerr(E_MISOPE);
		break;
	default:			/* invalid operand */
		len = 1;
		ops[0] = 0;
		asmerr(E_ILLOPE);
	}
	return(len);
}

/*
 *	ADD ?,?
 */
int op_add(int dummy1, int dummy2)
{
	register int len;
	register char *p1, *p2;

	UNUSED(dummy1);
	UNUSED(dummy2);

	if (pass == 1)
		if (*label)
			put_label();
	p1 = operand;
	p2 = tmp;
	while (*p1 != ',' && *p1 != '\0')
		*p2++ = *p1++;
	*p2 = '\0';
	switch (get_reg(tmp)) {
	case REGA:			/* ADD A,? */
		len = aluop(0x80, get_second(operand));
		break;
	case REGHL:			/* ADD HL,? */
		len = addhl();
		break;
	case REGIX:			/* ADD IX,? */
		len = addix();
		break;
	case REGIY:			/* ADD IY,? */
		len = addiy();
		break;
	case NOOPERA:			/* missing operand */
		len = 1;
		ops[0] = 0;
		asmerr(E_MISOPE);
		break;
	default:			/* invalid operand */
		len = 1;
		ops[0] = 0;
		asmerr(E_ILLOPE);
	}
	return(len);
}

/*
 *	ADD HL,?
 */
int addhl(void)
{
	switch (get_reg(get_second(operand))) {
	case REGBC:			/* ADD HL,BC */
		ops[0] = 0x09;
		break;
	case REGDE:			/* ADD HL,DE */
		ops[0] = 0x19;
		break;
	case REGHL:			/* ADD HL,HL */
		ops[0] = 0x29;
		break;
	case REGSP:			/* ADD HL,SP */
		ops[0] = 0x39;
		break;
	case NOOPERA:			/* missing operand */
		ops[0] = 0;
		asmerr(E_MISOPE);
		break;
	default:			/* invalid operand */
		ops[0] = 0;
		asmerr(E_ILLOPE);
	}
	return(1);
}

/*
 *	ADD IX,?
 */
int addix(void)
{
	switch (get_reg(get_second(operand))) {
	case REGBC:			/* ADD IX,BC */
		ops[0] = 0xdd;
		ops[1] = 0x09;
		break;
	case REGDE:			/* ADD IX,DE */
		ops[0] = 0xdd;
		ops[1] = 0x19;
		break;
	case REGIX:			/* ADD IX,IX */
		ops[0] = 0xdd;
		ops[1] = 0x29;
		break;
	case REGSP:			/* ADD IX,SP */
		ops[0] = 0xdd;
		ops[1] = 0x39;
		break;
	case NOOPERA:			/* missing operand */
		ops[0] = 0;
		ops[1] = 0;
		asmerr(E_MISOPE);
		break;
	default:			/* invalid operand */
		ops[0] = 0;
		ops[1] = 0;
		asmerr(E_ILLOPE);
	}
	return(2);
}

/*
 *	ADD IY,?
 */
int addiy(void)
{
	switch (get_reg(get_second(operand))) {
	case REGBC:			/* ADD IY,BC */
		ops[0] = 0xfd;
		ops[1] = 0x09;
		break;
	case REGDE:			/* ADD IY,DE */
		ops[0] = 0xfd;
		ops[1] = 0x19;
		break;
	case REGIY:			/* ADD IY,IY */
		ops[0] = 0xfd;
		ops[1] = 0x29;
		break;
	case REGSP:			/* ADD IY,SP */
		ops[0] = 0xfd;
		ops[1] = 0x39;
		break;
	case NOOPERA:			/* missing operand */
		ops[0] = 0;
		ops[1] = 0;
		asmerr(E_MISOPE);
		break;
	default:			/* invalid operand */
		ops[0] = 0;
		ops[1] = 0;
		asmerr(E_ILLOPE);
	}
	return(2);
}

/*
 *	ADC ?,?
 */
int op_adc(int dummy1, int dummy2)
{
	register int len;
	register char *p1, *p2;

	UNUSED(dummy1);
	UNUSED(dummy2);

	if (pass == 1)
		if (*label)
			put_label();
	p1 = operand;
	p2 = tmp;
	while (*p1 != ',' && *p1 != '\0')
		*p2++ = *p1++;
	*p2 = '\0';
	switch (get_reg(tmp)) {
	case REGA:			/* ADC A,? */
		len = aluop(0x88, get_second(operand));
		break;
	case REGHL:			/* ADC HL,? */
		len = sbadchl(0x4a);
		break;
	case NOOPERA:			/* missing operand */
		len = 1;
		ops[0] = 0;
		asmerr(E_MISOPE);
		break;
	default:			/* invalid operand */
		len = 1;
		ops[0] = 0;
		asmerr(E_ILLOPE);
	}
	return(len);
}

/*
 *	SUB
 */
int op_sub(int dummy1, int dummy2)
{
	UNUSED(dummy1);
	UNUSED(dummy2);

	if (pass == 1)
		if (*label)
			put_label();
	return(aluop(0x90, operand));
}

/*
 *	SBC ?,?
 */
int op_sbc(int dummy1, int dummy2)
{
	register int len;
	register char *p1, *p2;

	UNUSED(dummy1);
	UNUSED(dummy2);

	if (pass == 1)
		if (*label)
			put_label();
	p1 = operand;
	p2 = tmp;
	while (*p1 != ',' && *p1 != '\0')
		*p2++ = *p1++;
	*p2 = '\0';
	switch (get_reg(tmp)) {
	case REGA:			/* SBC A,? */
		len = aluop(0x98, get_second(operand));
		break;
	case REGHL:			/* SBC HL,? */
		len = sbadchl(0x42);
		break;
	case NOOPERA:			/* missing operand */
		len = 1;
		ops[0] = 0;
		asmerr(E_MISOPE);
		break;
	default:			/* invalid operand */
		len = 1;
		ops[0] = 0;
		asmerr(E_ILLOPE);
	}
	return(len);
}

/*
 *	SBC HL,? and ADC HL,?
 */
int sbadchl(int base_op)
{
	switch (get_reg(get_second(operand))) {
	case REGBC:			/* SBC/ADC HL,BC */
		ops[0] = 0xed;
		ops[1] = base_op;
		break;
	case REGDE:			/* SBC/ADC HL,DE */
		ops[0] = 0xed;
		ops[1] = base_op + 0x10;
		break;
	case REGHL:			/* SBC/ADC HL,HL */
		ops[0] = 0xed;
		ops[1] = base_op + 0x20;
		break;
	case REGSP:			/* SBC/ADC HL,SP */
		ops[0] = 0xed;
		ops[1] = base_op + 0x30;
		break;
	case NOOPERA:			/* missing operand */
		ops[0] = 0;
		ops[1] = 0;
		asmerr(E_MISOPE);
		break;
	default:			/* invalid operand */
		ops[0] = 0;
		ops[1] = 0;
		asmerr(E_ILLOPE);
	}
	return(2);
}

/*
 *	DEC and INC
 */
int op_decinc(int base_op, int base_op16)
{
	register int len, op;

	if (pass == 1)
		if (*label)
			put_label();
	switch (op = get_reg(operand)) {
	case REGA:			/* INC/DEC A */
	case REGB:			/* INC/DEC B */
	case REGC:			/* INC/DEC C */
	case REGD:			/* INC/DEC D */
	case REGE:			/* INC/DEC E */
	case REGH:			/* INC/DEC H */
	case REGL:			/* INC/DEC L */
	case REGIHL:			/* INC/DEC (HL) */
		len = 1;
		ops[0] = base_op + (op << 3);
		break;
	case REGBC:			/* INC/DEC BC */
		len = 1;
		ops[0] = base_op16;
		break;
	case REGDE:			/* INC/DEC DE */
		len = 1;
		ops[0] = base_op16 + 0x10;
		break;
	case REGHL:			/* INC/DEC HL */
		len = 1;
		ops[0] = base_op16 + 0x20;
		break;
	case REGSP:			/* INC/DEC SP */
		len = 1;
		ops[0] = base_op16 + 0x30;
		break;
	case REGIX:			/* INC/DEC IX */
		len = 2;
		ops[0] = 0xdd;
		ops[1] = base_op16 + 0x20;
		break;
	case REGIY:			/* INC/DEC IY */
		len = 2;
		ops[0] = 0xfd;
		ops[1] = base_op16 + 0x20;
		break;
	case REGIXH:			/* INC/DEC IXH (undocumented) */
		len = 2;
		ops[0] = 0xdd;
		ops[1] = base_op + 0x20;
		break;
	case REGIXL:			/* INC/DEC IXL (undocumented) */
		len = 2;
		ops[0] = 0xdd;
		ops[1] = base_op + 0x28;
		break;
	case REGIYH:			/* INC/DEC IYH (undocumented) */
		len = 2;
		ops[0] = 0xfd;
		ops[1] = base_op + 0x20;
		break;
	case REGIYL:			/* INC/DEC IYL (undocumented) */
		len = 2;
		ops[0] = 0xfd;
		ops[1] = base_op + 0x28;
		break;
	case NOREG:			/* operand isn't register */
		if (strncmp(operand, "(IX+", 4) == 0) {
			len = 3;	/* INC/DEC (IX+d) */
			if (pass == 2) {
				ops[0] = 0xdd;
				ops[1] = base_op + 0x30;
				ops[2] = chk_sbyte(calc_val(strchr(operand, '+') + 1));
			}
		} else if (strncmp(operand, "(IY+", 4) == 0) {
			len = 3;	/* INC/DEC (IY+d) */
			if (pass == 2) {
				ops[0] = 0xfd;
				ops[1] = base_op + 0x30;
				ops[2] = chk_sbyte(calc_val(strchr(operand, '+') + 1));
			}
		} else {
			len = 1;
			ops[0] = 0;
			asmerr(E_ILLOPE);
		}
		break;
	case NOOPERA:			/* missing operand */
		len = 1;
		ops[0] = 0;
		asmerr(E_MISOPE);
		break;
	default:			/* invalid operand */
		len = 1;
		ops[0] = 0;
		asmerr(E_ILLOPE);
	}
	return(len);
}

/*
 *	OR
 */
int op_or(int dummy1, int dummy2)
{
	UNUSED(dummy1);
	UNUSED(dummy2);

	if (pass == 1)
		if (*label)
			put_label();
	return(aluop(0xb0, operand));
}

/*
 *	XOR
 */
int op_xor(int dummy1, int dummy2)
{
	UNUSED(dummy1);
	UNUSED(dummy2);

	if (pass == 1)
		if (*label)
			put_label();
	return(aluop(0xa8, operand));
}

/*
 *	AND
 */
int op_and(int dummy1, int dummy2)
{
	UNUSED(dummy1);
	UNUSED(dummy2);

	if (pass == 1)
		if (*label)
			put_label();
	return(aluop(0xa0, operand));
}

/*
 *	CP
 */
int op_cp(int dummy1, int dummy2)
{
	UNUSED(dummy1);
	UNUSED(dummy2);

	if (pass == 1)
		if (*label)
			put_label();
	return(aluop(0xb8, operand));
}

/*
 *	ADD A, ADC A, SUB, SBC A, AND, XOR, OR, CP
 */
int aluop(int base_op, char *p)
{
	register int len, op;

	switch (op = get_reg(p)) {
	case REGA:			/* ALUOP {A,}A */
	case REGB:			/* ALUOP {A,}B */
	case REGC:			/* ALUOP {A,}C */
	case REGD:			/* ALUOP {A,}D */
	case REGE:			/* ALUOP {A,}E */
	case REGH:			/* ALUOP {A,}H */
	case REGL:			/* ALUOP {A,}L */
	case REGIHL:			/* ALUOP {A,}(HL) */
		len = 1;
		ops[0] = base_op + op;
		break;
	case REGIXH:			/* ALUOP {A,}IXH (undocumented) */
		len = 2;
		ops[0] = 0xdd;
		ops[1] = base_op + 0x04;
		break;
	case REGIXL:			/* ALUOP {A,}IXL (undocumented) */
		len = 2;
		ops[0] = 0xdd;
		ops[1] = base_op + 0x05;
		break;
	case REGIYH:			/* ALUOP {A,}IYH (undocumented) */
		len = 2;
		ops[0] = 0xfd;
		ops[1] = base_op + 0x04;
		break;
	case REGIYL:			/* ALUOP {A,}IYL (undocumented) */
		len = 2;
		ops[0] = 0xfd;
		ops[1] = base_op + 0x05;
		break;
	case NOREG:			/* operand isn't register */
		if (strncmp(p, "(IX+", 4) == 0) {
			len = 3;	/* ALUOP {A,}(IX+d) */
			if (pass == 2) {
				ops[0] = 0xdd;
				ops[1] = base_op + 0x06;
				ops[2] = chk_sbyte(calc_val(strchr(p, '+') + 1));
			}
		} else if (strncmp(p, "(IY+", 4) == 0) {
			len = 3;	/* ALUOP {A,}(IY+d) */
			if (pass == 2) {
				ops[0] = 0xfd;
				ops[1] = base_op + 0x06;
				ops[2] = chk_sbyte(calc_val(strchr(p, '+') + 1));
			}
		} else {
			len = 2;	/* ALUOP {A,}n */
			if (pass == 2) {
				ops[0] = base_op + 0x46;
				ops[1] = chk_byte(eval(p));
			}
		}
		break;
	case NOOPERA:			/* missing operand */
		len = 1;
		ops[0] = 0;
		asmerr(E_MISOPE);
		break;
	default:			/* invalid operand */
		len = 1;
		ops[0] = 0;
		asmerr(E_ILLOPE);
	}
	return(len);
}

/*
 *	RLC, RRC, RL, RR, SLA, SRA, SLL, SRL
 */
int op_rotshf(int base_op, int dummy)
{
	register char *p;
	register int len, op;

	UNUSED(dummy);

	if (pass == 1)
		if (*label)
			put_label();
	switch (op = get_reg(operand)) {
	case REGA:			/* ROTSHF A */
	case REGB:			/* ROTSHF B */
	case REGC:			/* ROTSHF C */
	case REGD:			/* ROTSHF D */
	case REGE:			/* ROTSHF E */
	case REGH:			/* ROTSHF H */
	case REGL:			/* ROTSHF L */
	case REGIHL:			/* ROTSHF (HL) */
		len = 2;
		ops[0] = 0xcb;
		ops[1] = base_op + op;
		break;
	case NOREG:			/* operand isn't register */
		if (strncmp(operand, "(IX+", 4) == 0) {
			len = 4;
			if (pass == 1)
				break;
			if (undoc_flag && (p = strrchr(operand, ')'))
				       && *(p + 1) == ',') {
				switch (op = get_reg(p + 2)) {
				case REGA:	/* ROTSHF (IX+d),A (undocumented) */
				case REGB:	/* ROTSHF (IX+d),B (undocumented) */
				case REGC:	/* ROTSHF (IX+d),C (undocumented) */
				case REGD:	/* ROTSHF (IX+d),D (undocumented) */
				case REGE:	/* ROTSHF (IX+d),E (undocumented) */
				case REGH:	/* ROTSHF (IX+d),H (undocumented) */
				case REGL:	/* ROTSHF (IX+d),L (undocumented) */
					ops[0] = 0xdd;
					ops[1] = 0xcb;
					ops[2] = chk_sbyte(calc_val(strchr(operand, '+') + 1));
					ops[3] = base_op + op;
					break;
				default:	/* invalid operand */
					ops[0] = 0;
					ops[1] = 0;
					ops[2] = 0;
					ops[3] = 0;
					asmerr(E_ILLOPE);
				}
			} else {		/* ROTSHF (IX+d) */
				ops[0] = 0xdd;
				ops[1] = 0xcb;
				ops[2] = chk_sbyte(calc_val(strchr(operand, '+') + 1));
				ops[3] = base_op + 0x06;
			}
		} else if (strncmp(operand, "(IY+", 4) == 0) {
			len = 4;
			if (pass == 1)
				break;
			if (undoc_flag && (p = strrchr(operand, ')'))
				       && *(p + 1) == ',') {
				switch (op = get_reg(p + 2)) {
				case REGA:	/* ROTSHF (IY+d),A (undocumented) */
				case REGB:	/* ROTSHF (IY+d),B (undocumented) */
				case REGC:	/* ROTSHF (IY+d),C (undocumented) */
				case REGD:	/* ROTSHF (IY+d),D (undocumented) */
				case REGE:	/* ROTSHF (IY+d),E (undocumented) */
				case REGH:	/* ROTSHF (IY+d),H (undocumented) */
				case REGL:	/* ROTSHF (IY+d),L (undocumented) */
					ops[0] = 0xfd;
					ops[1] = 0xcb;
					ops[2] = chk_sbyte(calc_val(strchr(operand, '+') + 1));
					ops[3] = base_op + op;
					break;
				default:	/* invalid operand */
					ops[0] = 0;
					ops[1] = 0;
					ops[2] = 0;
					ops[3] = 0;
					asmerr(E_ILLOPE);
				}
			} else {		/* ROTSHF (IY+d) */
				ops[0] = 0xfd;
				ops[1] = 0xcb;
				ops[2] = chk_sbyte(calc_val(strchr(operand, '+') + 1));
				ops[3] = base_op + 0x06;
			}
		} else {
			len = 1;
			ops[0] = 0;
			asmerr(E_ILLOPE);
		}
		break;
	case NOOPERA:			/* missing operand */
		len = 1;
		ops[0] = 0;
		asmerr(E_MISOPE);
		break;
	default:			/* invalid operand */
		len = 1;
		ops[0] = 0;
		asmerr(E_ILLOPE);
	}
	return(len);
}

/*
 *	OUT
 */
int op_out(int dummy1, int dummy2)
{
	register int op;
	register char *p;

	UNUSED(dummy1);
	UNUSED(dummy2);

	if (pass == 1) {		/* PASS 1 */
		if (*label)
			put_label();
	} else {			/* PASS 2 */
		if (strncmp(operand, "(C),", 4) == 0) {
			p = get_second(operand);
			switch(op = get_reg(p)) {
			case REGA:	/* OUT (C),A */
			case REGB:	/* OUT (C),B */
			case REGC:	/* OUT (C),C */
			case REGD:	/* OUT (C),D */
			case REGE:	/* OUT (C),E */
			case REGH:	/* OUT (C),H */
			case REGL:	/* OUT (C),L */
				ops[0] = 0xed;
				ops[1] = 0x41 + (op << 3);
				break;
			case NOOPERA:	/* missing operand */
				ops[0] = 0;
				ops[1] = 0;
				asmerr(E_MISOPE);
				break;
			default:
				if (undoc_flag && *p == '0') {
					ops[0] = 0xed;	/* OUT (C),0 (undocumented) */
					ops[1] = 0x71;
				} else {	/* invalid operand */
					ops[0] = 0;
					ops[1] = 0;
					asmerr(E_ILLOPE);
				}
			}
		} else {
			/* check syntax for OUT (n),A */
			p = strchr(operand, ')');
			if (strncmp(p, "),A", 3)) {
				ops[0] = 0;
				ops[1] = 0;
				asmerr(E_ILLOPE);
			}
			ops[0] = 0xd3;	/* OUT (n),A */
			ops[1] = chk_byte(calc_val(operand + 1));
		}
	}
	return(2);
}

/*
 *	IN
 */
int op_in(int dummy1, int dummy2)
{
	register char *p1, *p2;
	register int op;

	UNUSED(dummy1);
	UNUSED(dummy2);

	if (pass == 1) {		/* PASS 1 */
		if (*label)
			put_label();
	} else {			/* PASS 2 */
		p1 = operand;
		p2 = tmp;
		while (*p1 != ',' && *p1 != '\0')
			*p2++ = *p1++;
		*p2 = '\0';
		switch (op = get_reg(tmp)) {
		case REGA:
			if (strncmp(operand, "A,(C)", 5) == 0) {
				ops[0] = 0xed;	/* IN A,(C) */
				ops[1] = 0x78;
			} else {
				ops[0] = 0xdb;	/* IN A,(n) */
				ops[1] = chk_byte(calc_val(get_second(operand) + 1));
			}
			break;
		case REGB:			/* IN B,(C) */
		case REGC:			/* IN C,(C) */
		case REGD:			/* IN D,(C) */
		case REGE:			/* IN E,(C) */
		case REGH:			/* IN H,(C) */
		case REGL:			/* IN L,(C) */
			ops[0] = 0xed;
			ops[1] = 0x40 + (op << 3);
			break;
		default:
			if (undoc_flag && strncmp(operand, "F,(C)", 5) == 0) {
				ops[0] = 0xed;	/* IN F,(C) (undocumented) */
				ops[1] = 0x70;
			} else {		/* invalid operand */
				ops[0] = 0;
				ops[1] = 0;
				asmerr(E_ILLOPE);
			}
		}
	}
	return(2);
}

/*
 *	BIT, RES, SET
 */
int op_trsbit(int base_op, int dummy)
{
	register char *p1, *p2;
	register int len;
	register int i;
	register int op;

	UNUSED(dummy);

	len = 2;
	i = 0;
	if (pass == 1)
		if (*label)
			put_label();
	ops[0] = 0xcb;
	p1 = operand;
	p2 = tmp;
	while (*p1 != ',' && *p1 != '\0')
		*p2++ = *p1++;
	*p2 = '\0';
	if (pass == 2) {
		i = eval(tmp);
		if (i < 0 || i > 7)
			asmerr(E_VALOUT);
	}
	switch (op = get_reg(++p1)) {
	case REGA:			/* TRSBIT n,A */
	case REGB:			/* TRSBIT n,B */
	case REGC:			/* TRSBIT n,C */
	case REGD:			/* TRSBIT n,D */
	case REGE:			/* TRSBIT n,E */
	case REGH:			/* TRSBIT n,H */
	case REGL:			/* TRSBIT n,L */
	case REGIHL:			/* TRSBIT n,(HL) */
		ops[1] = base_op + i * 8 + op;
		break;
	case NOREG:			/* operand isn't register */
		if (strncmp(p1, "(IX+", 4) == 0) {
			len = 4;
			if (pass == 1)
				break;
			if (undoc_flag && base_op != 0x40 && (p2 = strrchr(p1, ')'))
							  && *(p2 + 1) == ',') {
				/* only for SET/RES */
				switch (op = get_reg(p2 + 2)) {
				case REGA:	/* SETRES n,(IX+d),A (undocumented) */
				case REGB:	/* SETRES n,(IX+d),B (undocumented) */
				case REGC:	/* SETRES n,(IX+d),C (undocumented) */
				case REGD:	/* SETRES n,(IX+d),D (undocumented) */
				case REGE:	/* SETRES n,(IX+d),E (undocumented) */
				case REGH:	/* SETRES n,(IX+d),H (undocumented) */
				case REGL:	/* SETRES n,(IX+d),L (undocumented) */
					ops[0] = 0xdd;
					ops[1] = 0xcb;
					ops[2] = chk_sbyte(calc_val(strchr(operand, '+') + 1));
					ops[3] = base_op + i * 8 + op;
					break;
				default:	/* invalid operand */
					ops[0] = 0;
					ops[1] = 0;
					ops[2] = 0;
					ops[3] = 0;
					asmerr(E_ILLOPE);
				}
			} else {		/* TRSBIT n,(IX+d) */
				ops[0] = 0xdd;
				ops[1] = 0xcb;
				ops[2] = chk_sbyte(calc_val(strchr(operand, '+') + 1));
				ops[3] = base_op + 0x06 + i * 8;
			}
		} else if (strncmp(p1, "(IY+", 4) == 0) {
			len = 4;
			if (pass == 1)
				break;
			if (undoc_flag && base_op != 0x40 && (p2 = strrchr(p1, ')'))
							  && *(p2 + 1) == ',') {
				/* only for SET/RES */
				switch (op = get_reg(p2 + 2)) {
				case REGA:	/* SETRES n,(IY+d),A (undocumented) */
				case REGB:	/* SETRES n,(IY+d),B (undocumented) */
				case REGC:	/* SETRES n,(IY+d),C (undocumented) */
				case REGD:	/* SETRES n,(IY+d),D (undocumented) */
				case REGE:	/* SETRES n,(IY+d),E (undocumented) */
				case REGH:	/* SETRES n,(IY+d),H (undocumented) */
				case REGL:	/* SETRES n,(IY+d),L (undocumented) */
					ops[0] = 0xfd;
					ops[1] = 0xcb;
					ops[2] = chk_sbyte(calc_val(strchr(operand, '+') + 1));
					ops[3] = base_op + i * 8 + op;
					break;
				default:	/* invalid operand */
					ops[0] = 0;
					ops[1] = 0;
					ops[2] = 0;
					ops[3] = 0;
					asmerr(E_ILLOPE);
				}
			} else {		/* TRSBIT n,(IY+d) */
				ops[0] = 0xfd;
				ops[1] = 0xcb;
				ops[2] = chk_sbyte(calc_val(strchr(operand, '+') + 1));
				ops[3] = base_op + 0x06 + i * 8;
			}
		} else {
			ops[0] = 0;
			ops[1] = 0;
			asmerr(E_ILLOPE);
		}
		break;
	case NOOPERA:			/* missing operand */
		ops[0] = 0;
		ops[1] = 0;
		asmerr(E_MISOPE);
		break;
	default:			/* invalid operand */
		ops[0] = 0;
		ops[1] = 0;
		asmerr(E_ILLOPE);
	}
	return(len);
}

/*
 *	8080 MOV
 */
int op8080_mov(int dummy1, int dummy2)
{
	register char *p1, *p2;
	register int op1, op2;

	UNUSED(dummy1);
	UNUSED(dummy2);

	if (pass == 1)
		if (*label)
			put_label();
	p1 = operand;
	p2 = tmp;
	while (*p1 != ',' && *p1 != '\0')
		*p2++ = *p1++;
	*p2 = '\0';
	switch (op1 = get_reg(tmp)) {
	case REGA:			/* MOV A,reg */
	case REGB:			/* MOV B,reg */
	case REGC:			/* MOV C,reg */
	case REGD:			/* MOV D,reg */
	case REGE:			/* MOV E,reg */
	case REGH:			/* MOV H,reg */
	case REGL:			/* MOV L,reg */
	case REGM:			/* MOV M,reg */
		p1 = get_second(operand);
		switch (op2 = get_reg(p1)) {
		case REGA:		/* MOV reg,A */
		case REGB:		/* MOV reg,B */
		case REGC:		/* MOV reg,C */
		case REGD:		/* MOV reg,D */
		case REGE:		/* MOV reg,E */
		case REGH:		/* MOV reg,H */
		case REGL:		/* MOV reg,L */
		case REGM:		/* MOV reg,M */
			if (op1 == REGM && op2 == REGM) {
				ops[0] = 0;
				asmerr(E_ILLOPE);
			}
			else
				ops[0] = 0x40 + (op1 << 3) + op2;
			break;
		case NOOPERA:		/* missing operand */
			ops[0] = 0;
			asmerr(E_MISOPE);
			break;
		default:		/* invalid operand */
			ops[0] = 0;
			asmerr(E_ILLOPE);
		}
		break;
	case NOOPERA:			/* missing operand */
		ops[0] = 0;
		asmerr(E_MISOPE);
		break;
	default:			/* invalid operand */
		ops[0] = 0;
		asmerr(E_ILLOPE);
	}
	return(1);
}

/*
 *	8080 ADC, ADD, ANA, CMP, ORA, SBB, SUB, XRA
 */
int op8080_alu(int base_op, int dummy)
{
	register int op;

	UNUSED(dummy);

	if (pass == 1)
		if (*label)
			put_label();
	switch (op = get_reg(operand)) {
	case REGA:			/* ALUOP A */
	case REGB:			/* ALUOP B */
	case REGC:			/* ALUOP C */
	case REGD:			/* ALUOP D */
	case REGE:			/* ALUOP E */
	case REGH:			/* ALUOP H */
	case REGL:			/* ALUOP L */
	case REGM:			/* ALUOP M */
		ops[0] = base_op + op;
		break;
	case NOOPERA:			/* missing operand */
		ops[0] = 0;
		asmerr(E_MISOPE);
		break;
	default:			/* invalid operand */
		ops[0] = 0;
		asmerr(E_ILLOPE);
	}
	return(1);
}

/*
 *	8080 DCR and INR
 */
int op8080_decinc(int base_op, int dummy)
{
	register int op;

	UNUSED(dummy);

	if (pass == 1)
		if (*label)
			put_label();
	switch (op = get_reg(operand)) {
	case REGA:			/* DEC/INC A */
	case REGB:			/* DEC/INC B */
	case REGC:			/* DEC/INC C */
	case REGD:			/* DEC/INC D */
	case REGE:			/* DEC/INC E */
	case REGH:			/* DEC/INC H */
	case REGL:			/* DEC/INC L */
	case REGM:			/* DEC/INC M */
		ops[0] = base_op + (op << 3);
		break;
	case NOOPERA:			/* missing operand */
		ops[0] = 0;
		asmerr(E_MISOPE);
		break;
	default:			/* invalid operand */
		ops[0] = 0;
		asmerr(E_ILLOPE);
	}
	return(1);
}

/*
 *	8080 INX, DAD, DCX
 */
int op8080_reg16(int base_op, int dummy)
{
	register int op;

	UNUSED(dummy);

	if (pass == 1)
		if (*label)
			put_label();
	switch (op = get_reg(operand)) {
	case REGB:			/* INX/DAD/DCX B */
	case REGD:			/* INX/DAD/DCX D */
	case REGH:			/* INX/DAD/DCX H */
		ops[0] = base_op + (op << 3);
		break;
	case REGSP:			/* INX/DAD/DCX SP */
		ops[0] = base_op + 0x30;
		break;
	case NOOPERA:			/* missing operand */
		ops[0] = 0;
		asmerr(E_MISOPE);
		break;
	default:			/* invalid operand */
		ops[0] = 0;
		asmerr(E_ILLOPE);
	}
	return(1);
}

/*
 *	8080 STAX and LDAX
 */
int op8080_regbd(int base_op, int dummy)
{
	register int op;

	UNUSED(dummy);

	if (pass == 1)
		if (*label)
			put_label();
	switch (op = get_reg(operand)) {
	case REGB:			/* STAX/LDAX B */
	case REGD:			/* STAX/LDAX D */
		ops[0] = base_op + (op << 3);
		break;
	case NOOPERA:			/* missing operand */
		ops[0] = 0;
		asmerr(E_MISOPE);
		break;
	default:			/* invalid operand */
		ops[0] = 0;
		asmerr(E_ILLOPE);
	}
	return(1);
}

/*
 *	8080 ACI, ADI, ANI, CPI, ORI, SBI, SUI, XRI, OUT, IN
 */
int op8080_imm(int base_op, int dummy)
{
	register int len;

	UNUSED(dummy);

	if (pass == 1)
		if (*label)
			put_label();
	switch (get_reg(operand)) {
	case NOREG:			/* IMMOP n */
		len = 2;
		if (pass == 2) {
			ops[0] = base_op;
			ops[1] = chk_byte(eval(operand));
		}
		break;
	case NOOPERA:			/* missing operand */
		len = 1;
		ops[0] = 0;
		asmerr(E_MISOPE);
		break;
	default:			/* invalid operand */
		len = 1;
		ops[0] = 0;
		asmerr(E_ILLOPE);
	}
	return(len);
}

/*
 *	8080 RST
 */
int op8080_rst(int dummy1, int dummy2)
{
	register int op;

	UNUSED(dummy1);
	UNUSED(dummy2);

	if (pass == 1) {		/* PASS 1 */
		if (*label)
			put_label();
	} else {			/* PASS 2 */
		op = eval(operand);
		if (op < 0 || op > 7) {
			ops[0] = 0;
			asmerr(E_VALOUT);
		} else
			ops[0] = 0xc7 + (op << 3);
	}
	return(1);
}
/*
 *	8080 PUSH and POP
 */
int op8080_pupo(int base_op, int dummy)
{
	register int op;

	UNUSED(dummy);

	if (pass == 1)
		if (*label)
			put_label();
	switch (op = get_reg(operand)) {
	case REGB:			/* PUSH/POP B */
	case REGD:			/* PUSH/POP D */
	case REGH:			/* PUSH/POP H */
		ops[0] = base_op + (op << 3);
		break;
	case REGPSW:			/* PUSH/POP PSW */
		ops[0] = base_op + 0x30;
		break;
	case NOOPERA:			/* missing operand */
		ops[0] = 0;
		asmerr(E_MISOPE);
		break;
	default:			/* invalid operand */
		ops[0] = 0;
		asmerr(E_ILLOPE);
	}
	return(1);
}

/*
 *	8080 SHLD, LHLD, STA, LDA
 *	     JMP, JNZ, JZ, JNC, JC, JPO, JPE, JP, JM
 *	     CALL, CNZ, CZ, CNC, CC, CPO, CPE, CP, CM
 */
int op8080_addr(int base_op, int dummy)
{
	register int i, len;

	UNUSED(dummy);

	if (pass == 1)
		if (*label)
			put_label();
	switch (get_reg(operand)) {
	case NOREG:			/* OP nn */
		len = 3;
		if (pass == 2) {
			i = eval(operand);
			ops[0] = base_op;
			ops[1] = i & 0xff;
			ops[2] = i >> 8;
		}
		break;
	case NOOPERA:			/* missing operand */
		len = 1;
		ops[0] = 0;
		asmerr(E_MISOPE);
		break;
	default:			/* invalid operand */
		len = 1;
		ops[0] = 0;
		asmerr(E_ILLOPE);
	}
	return(len);
}

/*
 *	8080 MVI
 */
int op8080_mvi(int dummy1, int dummy2)
{
	register int len;
	register char *p1, *p2;
	register int op;

	UNUSED(dummy1);
	UNUSED(dummy2);

	if (pass == 1)
		if (*label)
			put_label();
	p1 = operand;
	p2 = tmp;
	while (*p1 != ',' && *p1 != '\0')
		*p2++ = *p1++;
	*p2 = '\0';
	switch (op = get_reg(tmp)) {
	case REGA:			/* MVI A,n */
	case REGB:			/* MVI B,n */
	case REGC:			/* MVI C,n */
	case REGD:			/* MVI D,n */
	case REGE:			/* MVI E,n */
	case REGH:			/* MVI H,n */
	case REGL:			/* MVI L,n */
	case REGM:			/* MVI M,n */
		p1 = get_second(operand);
		switch (get_reg(p1)) {
		case NOREG:		/* MVI reg,n */
			len = 2;
			if (pass == 2) {
				ops[0] = 0x06 + (op << 3);
				ops[1] = chk_byte(eval(p1));
			}
			break;
		case NOOPERA:		/* missing operand */
			len = 1;
			ops[0] = 0;
			asmerr(E_MISOPE);
			break;
		default:		/* invalid operand */
			len = 1;
			ops[0] = 0;
			asmerr(E_ILLOPE);
		}
		break;
	case NOOPERA:			/* missing operand */
		len = 1;
		ops[0] = 0;
		asmerr(E_MISOPE);
		break;
	default:			/* invalid operand */
		len = 1;
		ops[0] = 0;
		asmerr(E_ILLOPE);
	}
	return(len);
}

/*
 *	8080 LXI
 */
int op8080_lxi(int dummy1, int dummy2)
{
	register int i, len;
	register char *p1, *p2;
	register int op;

	UNUSED(dummy1);
	UNUSED(dummy2);

	if (pass == 1)
		if (*label)
			put_label();
	p1 = operand;
	p2 = tmp;
	while (*p1 != ',' && *p1 != '\0')
		*p2++ = *p1++;
	*p2 = '\0';
	switch (op = get_reg(tmp)) {
	case REGB:			/* LXI B,nn */
	case REGD:			/* LXI D,nn */
	case REGH:			/* LXI H,nn */
	case REGSP:			/* LXI SP,nn */
		p1 = get_second(operand);
		switch (get_reg(p1)) {
		case NOREG:		/* LXI reg,nn */
			len = 3;
			if (pass == 2) {
				i = eval(p1);
				if (op == REGSP)
					ops[0] = 0x31;
				else
					ops[0] = 0x01 + (op << 3);
				ops[1] = i & 0xff;
				ops[2] = i >> 8;
			}
			break;
		case NOOPERA:		/* missing operand */
			len = 1;
			ops[0] = 0;
			asmerr(E_MISOPE);
			break;
		default:		/* invalid operand */
			len = 1;
			ops[0] = 0;
			asmerr(E_ILLOPE);
		}
		break;
	case NOOPERA:			/* missing operand */
		len = 1;
		ops[0] = 0;
		asmerr(E_MISOPE);
		break;
	default:			/* invalid operand */
		len = 1;
		ops[0] = 0;
		asmerr(E_ILLOPE);
	}
	return(len);
}

/*
 *	returns a pointer to the second operand for
 *	opcodes:	opcode destination,source
 *	if source is missing returns NULL
 */
char *get_second(char *s)
{
	register char *p;

	if ((p = strchr(s, ',')) != NULL)
		return(p + 1);
	else
		return(NULL);
}

/*
 *	computes value of the following expressions:
 *	LD A,(IX+7)   LD A,(1234)
 *		 --         -----
 */
int calc_val(char *s)
{
	register char *p;
	register int i;

	if ((p = strrchr(s, ')')) == NULL) {
		asmerr(E_MISPAR);
		return(0);
	}
	i = p - s;
	strncpy(tmp, s, i);
	*(tmp + i) = '\0';
	return(eval(tmp));
}
