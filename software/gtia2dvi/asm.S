#include "hardware/regs/addressmap.h"
#include "hardware/regs/sio.h"

.global dump_data

.thumb

.cpu cortex-m0plus

.macro decl_func_x name
.section .scratch_x.\name, "ax"
.global \name
.type \name,%function
.thumb_func
\name:
.endm

#define decl_func decl_func_x

#define BLACK (0)
#define RED (0b11111 << 11)
#define GREEN (0b111111 << 5)
#define BLUE (0b11111)
#define YELLOW (RED + GREEN)
#define WHITE (RED + GREEN + BLUE)
#define MAGENTA (RED + BLUE)
#define CYAN (GREEN + BLUE)
#define GRAY1 ((7 << 11) + (7 << 5) + 7)
#define GRAY2 ((14 << 11) + (14 << 5) + 14)


.equ        max_y, 240

#define     GTIA_LUMA_SM         1
#define     GTIA_COLOR_SM        0
#define     GTIA_CSYNC_SM        2

#define     PIO_FSTAT            0x004
#define     PIO_FSTAT_RXEMPTY    8


#define     COL             r1
#define     X               r2
#define     Y               r3
#define     COLS            r4
#define     FRAME_WIDTH     r5          
#define     PIO1BASE        r6
#define     FRAMEBUF        r7

// r0: FRAMEBUF (word-aligned)
decl_func draw_csyncs
	push {r4-r7, lr}

    mov   FRAMEBUF, r0
    ldr   COL, colors     
    ldr   PIO1BASE, pio1base
    ldr   FRAME_WIDTH, frame_width
    ldr   COLS, colors

    mov  Y,#0
loopy:
    mov  X,#0
loopx:
wait_for_sm_data:    
    ldr    r0, [PIO1BASE, #PIO_FSTAT]
    lsr    r0, #(PIO_FSTAT_RXEMPTY + GTIA_CSYNC_SM+1)
    bcs    wait_for_sm_data

    mov    COL, COLS
    ldr    r0, [PIO1BASE,#0x020 + GTIA_CSYNC_SM*4]
    cmp     r0, #1
    bne     col2
    lsr     COL,#16                     @ swap color
col2:

    mov  r0, Y
    mul  r0, FRAME_WIDTH
    add  r0, X
    lsl  r0, #1                         @ r0 = (y*width + x )*2

    strh   COL, [FRAMEBUF,r0]           @ put pixel

    add   X, #1                         @ x++
    cmp   X, FRAME_WIDTH
    bne   loopx

    add   Y,#1                          @ y++
    cmp   Y, #max_y
    bne   loopy

    LDR     r0, sleep_time              @ sleep_ms()
    BL      sleep_ms

	pop {r4-r7, pc}


.align 4
colors:    
    .short  RED,GREEN
sleep_time: 
    .word 1000    
pio1base:
    .word	0x50300000
frame_width:
    .word  400




#define     X               r2
#define     Y               r3
#define     COLS            r4
#define     COL             r5
#define     PIO1BASE        r6
#define     INTERP0BASE    r7

#define     FRAME_WIDTH    r8          
#define     START_X        r9
#define     START_Y        r10

// r0: FRAMEBUF (word-aligned)
decl_func draw_luma_asm   
	push {r4-r7, lr}

    @ setup interpolator 0
	ldr INTERP0BASE, f1_interp0base
    str r0, [INTERP0BASE, #SIO_INTERP0_BASE2_OFFSET - SIO_INTERP0_ACCUM0_OFFSET]         @ base2 = framebuf

    mov r1, #0
    str r1, [INTERP0BASE,#SIO_INTERP0_ACCUM0_OFFSET - SIO_INTERP0_ACCUM0_OFFSET ]        @ accum0 = 0
    mov r1, #2
    str r1, [INTERP0BASE,#SIO_INTERP0_BASE0_OFFSET- SIO_INTERP0_ACCUM0_OFFSET]        @ base0 = 2
    ldr r1, f1_interp0_ctrl0
    str r1, [INTERP0BASE,#SIO_INTERP0_CTRL_LANE0_OFFSET - SIO_INTERP0_ACCUM0_OFFSET]        @ ctrl_lane0

    ldr   PIO1BASE, f1_pio1base
    ldr   r0, f1_frame_width
    mov   FRAME_WIDTH, r0
    ldr   r0, f1_delta_x
    mov   START_X, r0
    ldr   r0, f1_delta_y
    mov   START_Y, r0
    adr   COLS, luma_colors
    mov   X, START_X

next_frame_loop:
    mov   Y, START_Y
    mov r0, #0
    str r0, [INTERP0BASE,#SIO_INTERP0_ACCUM0_OFFSET - SIO_INTERP0_ACCUM0_OFFSET ]        @ accum0 = 0

next_pixel_loop:
    ldr    r0, [PIO1BASE, #PIO_FSTAT]
    lsr    r0, #(PIO_FSTAT_RXEMPTY + GTIA_COLOR_SM+1)
    bcs    x_luma      @ go to csync handle if no color data

    ldr    COL, [PIO1BASE, #0x020 + GTIA_COLOR_SM*4]

x_luma:
    @read luma data if exists

    lsr    r0, #(GTIA_LUMA_SM - GTIA_COLOR_SM)
    bcs    next_pixel_loop      @ wait for next pixel if no luma data
    ldr    COL, [PIO1BASE, #0x020 + GTIA_LUMA_SM*4]


    nop
    nop
    nop
    nop
    nop

    nop
    nop
    nop
    nop
    nop

    nop
    nop
    nop
    nop
    nop

    nop

 
plot_and_add_x:
    @ skip to x++ if X,Y doesnt placed in screen
    cmp     X, FRAME_WIDTH
    bhi     x_csync
    cmp     Y, #max_y-1
    bhi     x_csync

    @  read current pixel position from interpolator 0
    ldr  r0,  [INTERP0BASE,#SIO_INTERP0_POP_FULL_OFFSET - SIO_INTERP0_ACCUM0_OFFSET ]

    @ eval RGB565 for luma
    lsl    COL, #1
    ldrh   COL, [COLS, COL]
    strh   COL, [r0]           @ put pixel

    add     X, #1                @ increase X
    b       next_pixel_loop

x_csync:
    add     X,#1
    ldr     r0, [PIO1BASE, #PIO_FSTAT]
    lsr     r0, #(PIO_FSTAT_RXEMPTY + GTIA_CSYNC_SM+1)
    bcs     next_pixel_loop

    ldr     r0, [PIO1BASE,#0x020 + GTIA_CSYNC_SM*4]
    cmp     r0, #1
    bne     next_frame_loop

    mov     X, START_X          @ reset X position
    add     Y, #1               @ increase Y
    add     X, #4               @ why needed ???
  
    b       next_pixel_loop

	pop {r4-r7, pc}


#define RGB565(R, G, B) ((((R) >> 3) & 0x1f) << 11) | ((((G) >> 2) & 0x3f) << 5) | (((B) >> 3) & 0x1f)
#define LUM(X) RGB565(X, X, X)

.align 2
 
f1_pio1base:
    .word	PIO1_BASE
f1_interp0base:
    .word SIO_BASE + SIO_INTERP0_ACCUM0_OFFSET
f1_interp0_ctrl0:
    .word 0x7c00
f1_frame_width:
    .word  400  -1
f1_delta_x:
    .word -26
f1_delta_y:
    .word -43

luma_colors:
    .short	    LUM(16 * 0), LUM(16 * 1), LUM(16 * 2), LUM(16 * 3)
    .short      LUM(16 * 4), LUM(16 * 5), LUM(16 * 6), LUM(16 * 7)
    .short      LUM(16 * 8), LUM(16 * 9), LUM(16 * 10), LUM(16 * 11)
	.short      LUM(16 * 12), LUM(16 * 13), LUM(16 * 14+14), LUM(16 * 15+15)

.short 1,2,3,4

f1_frame_size_b:
#    .word 400*240*2
   .word 400*2*2

lumbuf1:
    .ascii "test"
    .fill   1024,1


@ #define DMA_BASE 0x50000000
@ #define DMA_CH6_BASE 0x180
@ #define DMA_CH11_BASE 0x2c0

@ #define DMA_READ_ADDR       0x0   
@ #define DMA_WRITE_ADDR      0x4   
@ #define DMA_TRANS_COUNT     0x8   
@ #define DMA_CTRL_TRIG       0xc   

@ #define DMA_CTRL_TRIG_EN                1
@ #define DMA_CTRL_TRIG_HIGH_PRIORITY     1 << 1
@ #define DMA_CTRL_TRIG_DATA_SIZE         1 << 2
@ #define DMA_CTRL_TRIG_INCR_READ         1 << 4
@ #define DMA_CTRL_TRIG_INCR_WRITE        1 << 5
@ #define DMA_CTRL_TRIG_IRQ_QUIET        1 << 21

@ #define DREQ_PIO0_RX2 6
@ #define DREQ_PIO1_RX2 14

@ run_dma:
@     ldr r0, f1_dmabase 
@     mov r1, PIO1BASE
@     add r1, #0x020 + GTIA_COLOR_SM*4
@     str r1, [r0, #DMA_READ_ADDR]

@ @    adr r1, lumbuf1
@     str FRAMEBUF, [r0, #DMA_WRITE_ADDR]

@     mov r1,#1
@     str r1, [r0, #DMA_TRANS_COUNT]

@     ldr r1, dma_cfg
@    str r1, [r0, #DMA_CTRL_TRIG]

@     b x_luma

@ f1_dmabase:
@     .word   DMA_BASE + DMA_CH11_BASE

@ dma_cfg:
@     .word DMA_CTRL_TRIG_EN + DMA_CTRL_TRIG_DATA_SIZE + DMA_CTRL_TRIG_INCR_WRITE + (DREQ_PIO1_RX2 << 15) +DMA_CTRL_TRIG_IRQ_QUIET

