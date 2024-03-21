/* TIO.H
 * Terminal i/o include file
 * Copyright (c) 2006 Ladybridge Systems, All Rights Reserved
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3, or (at your option)
 * any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 * 
 * START-HISTORY:
 * 31 Dec 23 SD launch - prior history suppressed
 * END-HISTORY
 *
 * START-DESCRIPTION:
 *
 * END-DESCRIPTION
 *
 * START-CODE
 */

/* Print unit control */
#define DISPLAY               -1
#define LOW_PRINT_UNIT        -1
#define HIGH_PRINT_UNIT      255
#define TEMPLATE_PRINT_UNIT   -3
#define TIO_DEFAULT_TMGN       0
#define TIO_DEFAULT_BMGN       0
#define TIO_DEFAULT_LMGN       0

/* Terminal size restrictions */

/* The following are also defined in BP TERM */
#define MIN_WIDTH        20
#define DEFAULT_WIDTH    80
#define MAX_WIDTH     32767
#define MIN_DEPTH        10
#define DEFAULT_DEPTH    24
#define MAX_DEPTH     32767

#define BACKSPACE_KEY 8
#define TAB_KEY       9
#define ESC_KEY      27

/* ======================================================================
   Print unit control                                                     */

/* !!PU!!  Places that need changing for new items are marked this way */
struct PRINT_UNIT {
                   struct PRINT_UNIT * next;
                   int16_t unit;
                   int16_t level;   /* Level at which opened */
                   int16_t mode;
#define PRINT_TO_DISPLAY  0
#define PRINT_TO_PRINTER  1
#define PRINT_TO_FILE     3
#define PRINT_TO_STDERR   4
#define PRINT_TO_AUX_PORT 5
#define PRINT_AND_HOLD    6
                   u_int32_t flags;
#define PU_ACTIVE        0x00000001  /* Print unit is active */
#define PU_POSTSCRIPT    0x00000002  /* Emulated PostScript printer */
#define PU_NFMT          0x00000004  /* NFMT SETPTR option */
#define PU_NEXT          0x00000008  /* NEXT SETPTR option */
#define PU_RAW           0x00000010  /* RAW SETPTR option (as opposed to GDI) */
#define PU_KEEP_OPEN     0x00000020  /* Do not honour close requests */
#define PU_LAND          0x00000040  /* LANDSCAPE SETPTR option */
#define PU_IS_RAW        0x00000080  /* Active as raw mode */
#define PU_PART_CLOSED   0x00000100  /* "Closed" while using KEEP_OPEN */
#define PU_SUPPRESS_HF   0x00000200  /* Suppress header/footer during banner page */
#define PU_HDR_NEXT      0x00000400  /* Print header with next output */
#define PU_PAGINATE      0x00000800  /* Pagination active */
#define PU_OUTPUT        0x00001000  /* Some output has occurred */
#define PU_PCL           0x00002000  /* PCL printer */
#define PU_DUPLEX_LONG   0x00004000  /* Duplex, long edge binding */
#define PU_DUPLEX_SHORT  0x00008000  /* Duplex, short edge binding */
#define PU_SPOOL_HOLD    0x00010000  /* Spooling hold file in mode 6 (Windows) */
#define PU_USER_SETABLE  0x0000E07E  /* Mask for values that can be set via PSET opcode */
#define PU_TEMPLATE_MASK 0x0000E07E  /* Mask for values to copy from template */

/* SETPTR settings */
                   int16_t width;          /* Page width */
                   int16_t lines_per_page; /* Total lines per page */
                   int16_t top_margin;     /* Top margin lines */
                   int16_t bottom_margin;  /* Bottom margin lines */
                   int16_t left_margin;    /* LEFT.MARGIN */
                   int16_t cpi;            /* CPI: Chars per inch * 100 */
                   int16_t lpi;            /* LPI: Lines per inch */
                   int16_t paper_size;     /* PAPER.SIZE */
#define PU_SZ_A4        1
#define PU_SZ_LETTER    2
#define PU_SZ_LEGAL     3
#define PS_SZ_LEDGER    4
#define PS_SZ_A3        5
#define PS_SZ_MONARCH   6
#define PS_SZ_COM_10    7
#define PS_SZ_DL        8
#define PS_SZ_C5        9
#define PS_SZ_B5       10
                   int16_t copies;         /* COPIES */
                   int16_t weight;         /* WEIGHT */
                   char symbol_set[7+1];     /* SYMBOL.SET */
                   char newline[2+1];        /* CR/LF/CRLF */
                   char * printer_name;
                   char * file_name;
                   char * banner;
                   char * heading;
                   char * footing;
                   char * spooler;
                   char * form;
                   char * options;
                   char * prefix;           /* Prefix file sent at start of job */
                   char * overlay;          /* Catalogued subroutine for page overlay */
                   char * style;            /* Report style name */

/* Items below this point reflect the dynamic state of the print job */

                   int16_t heading_lines;       /* Derived from heading... */
                   int16_t footing_lines;       /* ...and footing text */
                   int16_t data_lines_per_page; /* Excluding margins, header
                                                     and footer */
                   int16_t page_no; 
                   int32_t line;          /* Current line number from zero
                                              within data lines.
                                              Declared long for NFMT case.  */
                   int16_t col;          /* Current column (from zero) */
                   char * buff;            /* Data buffer and... */
                   int16_t bytes;        /* ...used byte count */
                  struct {
                          char * pathname;
                          OSFILE fu;
                          int32_t jobno;   /* Only for mode 1 */
                         } file;
                  };
typedef struct PRINT_UNIT PRINT_UNIT;


Public struct TIO
 {
   bool hush;
   PRINT_UNIT dsp;         /* DISPLAY and head of chain */
   PRINT_UNIT lptr_0;      /* Print unit 0 - real printer */
   OSFILE como_file;
   bool suppress_como;     /* Temporary suppression (@() func) */
   char prompt_char;       /* Prompt character */
   char term_type[32+1];
   int16_t terminfo_hpa;
   char * terminfo_colour_map;
   char break_char;        /* Break key character */
 } tio;

Public int autologout init(0);      /* Autologout interval */

Public char onewline[10+1] init("\r\n");
Public char inewline init(13);

Public bool is_SDTerm init(FALSE);    /* SDTerm? */

/* Output buffer (socket connections only) */

#define OUTBUF_SIZE 10240
Public char * outbuf  init(NULL);
Public int outbuf_bytes init(0);  /* Bytes in buffer */

/* Telnet binary mode negotiation state */
Public bool telnet_binary_mode_in init(FALSE);   /* Client -> server */
Public bool telnet_binary_mode_out init(FALSE);  /* Server -> client */
Public bool telnet_negotiation init(TRUE);       /* Watch for TN_IAC */


Public int16_t lock_beep_timer;

/* EXECUTE CAPTURING data */

Public bool capturing init(FALSE);             /* Active? */
Public STRING_CHUNK * capture_head init(NULL); /* Head of chain */
Public STRING_CHUNK * capture_tail;            /* Tail of chain */

Public bool case_inversion init(FALSE); 
Public bool trap_break_char; /* Treat break_char as a break request? */

/* OP_TIO.C */
void emit_header_footer(PRINT_UNIT * pu, bool is_heading);
PRINT_UNIT * tio_set_printer(int16_t unit, int16_t mode,
                             int16_t lines_per_page, int16_t width,
                             int16_t top_margin, int16_t bottom_margin,
                             int16_t left_margin);
void copy_print_unit(PRINT_UNIT * old_pu, PRINT_UNIT * new_pu);
void free_print_unit(PRINT_UNIT * pu);
void stack_display_pu(void);
void unstack_display_pu(void);

/* PIO.C */
void end_printer(PRINT_UNIT * pu);
bool validate_printer(char * printer_name);

/* TO_FILE.C */
void end_file(PRINT_UNIT * pu);

/* OP_TIO.C */
void set_term(bool trap_break);
void set_socket(int sock);

/* LINUXPRT.C */
void spool_print_job(PRINT_UNIT * pu);

/* END-CODE */


