/*
 * Z80SIM  -  a Z80-CPU simulator
 *
 * Copyright (C) 1987-2024 Udo Munk
 * Copyright (C) 2021 David McNaughton
 * Copyright (C) 2022-2024 Thomas Eberhardt
 */

/*
 *	Declaration of variables in simglb.c
 */

typedef unsigned long long Tstates_t;	/* uint64 for counting T-states */
typedef enum { BUS_DMA_NONE, BUS_DMA_BYTE,
	       BUS_DMA_BURST, BUS_DMA_CONTINUOUS } BusDMA_t;

extern void start_bus_request(BusDMA_t, Tstates_t (*)(BYTE));
extern void end_bus_request(void);

extern void (*ice_before_go)(void);
extern void (*ice_after_go)(void);
extern void (*ice_cust_cmd)(char *, WORD *);
extern void (*ice_cust_help)(void);

extern int	cpu;

extern BYTE	A, B, C, D, E, H, L;
extern int	F;
#ifndef EXCLUDE_Z80
extern WORD	IX, IY;
extern BYTE	A_, B_, C_, D_, E_, H_, L_, I, R, R_;
extern int	F_;
#endif
extern WORD	PC, SP;
extern BYTE	IFF;
extern Tstates_t T;
extern unsigned long long cpu_start, cpu_stop;

#ifdef BUS_8080
extern BYTE	cpu_bus;
extern int	m1_step;
#endif

extern BYTE	io_port, io_data;
extern int	busy_loop_cnt[];

extern BYTE	cpu_state;
extern int	cpu_error;
#ifndef EXCLUDE_Z80
extern int	int_nmi, int_mode;
#endif
extern int	int_int, int_data, int_protection;
extern BYTE	bus_request;
extern BusDMA_t bus_mode;
extern Tstates_t (*dma_bus_master)(BYTE);
extern int	tmax, cpu_needed;

#ifdef HISIZE
extern struct	history his[];
extern int	h_next, h_flag;
#endif

#ifdef SBSIZE
extern struct	softbreak soft[];
extern int	sb_next;
#endif

extern long	t_states;
extern int	t_flag;
extern WORD	t_start, t_end;

#ifdef FRONTPANEL
extern unsigned long long fp_clock;
extern float	fp_fps;
extern WORD 	fp_led_address;
extern BYTE 	fp_led_data;
extern WORD 	address_switch;
extern BYTE 	fp_led_output;
#endif

extern int	s_flag, l_flag, m_flag, x_flag, i_flag, f_flag,
		u_flag, r_flag, c_flag, M_flag, R_flag;

extern char	xfn[];
extern char	*diskdir, diskd[];
extern char	confdir[], conffn[];
extern char	rompath[];

extern char	parity[];
