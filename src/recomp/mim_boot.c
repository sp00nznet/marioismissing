/*
 * Mario is Missing! — Boot chain recompilation
 *
 * Reset vector -> hardware init -> main loop
 * These are the first functions that run when the game starts.
 *
 * Game info:
 *   - LoROM mapping
 *   - 512 KB ROM
 *   - Educational game — mostly question/answer data + sprite rendering
 *   - Luigi walks through cities collecting artifacts
 *
 * TODO: Disassemble reset vector and NMI handler from ROM
 *       and recompile the actual boot chain here.
 */

#include "mim/functions.h"
#include "mim/cpu_ops.h"
#include <snesrecomp/snesrecomp.h>
#include <snesrecomp/func_table.h>
#include <stdio.h>

/*
 * Register all recompiled functions in the function table.
 * As we recompile more functions from the ROM, they get added here.
 */
void mim_register_all(void) {
    /* Boot chain — addresses TBD from disassembly */
    /* func_table_register(0xXXXXXX, mim_reset); */
    /* func_table_register(0xXXXXXX, mim_hw_init); */
    /* func_table_register(0xXXXXXX, mim_nmi); */

    printf("mim: registered %d recompiled functions\n", 0);
}

/*
 * Reset vector — placeholder
 *
 * The actual reset vector address needs to be read from the ROM header.
 * For LoROM, the reset vector is at $00:FFFC-FFFD in the ROM.
 *
 * Typical SNES reset sequence:
 *   SEI            ; disable interrupts
 *   CLC / XCE      ; switch to native mode
 *   REP/SEP        ; set register sizes
 *   set stack       ; initialize stack pointer
 *   init hardware   ; PPU, DMA, etc.
 */
void mim_reset(void) {
    printf("mim: reset vector (stub — needs disassembly)\n");

    /* Standard SNES boot preamble */
    OP_SEI();
    OP_CLC();
    op_xce();           /* native mode */
    op_sep(0x30);       /* M=1, X=1 (8-bit A and index) */

    /* Set stack to $1FFF (common for LoROM games) */
    op_lda_imm8(0x1F);
    op_xba();
    op_lda_imm8(0xFF);
    OP_TCS();

    /* Call hardware init */
    mim_hw_init();
}

/*
 * Hardware init — placeholder
 *
 * Initializes PPU registers, clears VRAM/OAM/CGRAM,
 * sets up DMA, loads initial graphics data.
 */
void mim_hw_init(void) {
    printf("mim: hardware init (stub — needs disassembly)\n");

    OP_SET_DB(0x00);

    /* Force blank */
    op_lda_imm8(0x8F);
    op_sta_abs8(0x2100);

    /* Disable NMI/IRQ */
    op_stz_abs8(0x4200);
    /* Disable DMA/HDMA */
    op_stz_abs8(0x420B);
    op_stz_abs8(0x420C);
}

/*
 * NMI handler — placeholder
 */
void mim_nmi(void) {
    /* Stub — will be recompiled from ROM */
}

/*
 * Main loop — placeholder (one iteration per frame)
 */
void mim_main_loop(void) {
    /* Stub — will be recompiled from ROM */
}
