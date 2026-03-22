/*
 * Mario is Missing! — Recompiled function declarations
 *
 * Addresses are LoROM bank:offset mapped to 24-bit SNES addresses.
 */
#ifndef MIM_FUNCTIONS_H
#define MIM_FUNCTIONS_H

/* Register all recompiled functions in the function table */
void mim_register_all(void);

/* === Boot chain (Bank $00/$80) === */
void mim_008000(void);     /* Reset vector */
void mim_008050(void);     /* PPU/hardware init */
void mim_008349(void);     /* RNG/timer seed init */
void mim_00842C(void);     /* OAM sprite table init */
void mim_008453(void);     /* DMA table loader A */
void mim_008471(void);     /* DMA table loader B */
void mim_008506(void);     /* DMA table engine (processes transfer table) */

/* === NMI handler === */
void mim_00819D(void);     /* NMI entry point */
void mim_0085C0(void);     /* NMI DMA/VRAM/OAM transfer handler */
void mim_0081E1(void);     /* NMI indirect vector dispatch JML [$08] */
void mim_0081E5(void);     /* Wait for VBlank end */

/* === SPC700 / Audio === */
void mim_009203(void);     /* SPC700 command write (A=cmd, Y=data) */

/* === Main loop === */
void mim_009A5E(void);     /* Main loop entry / game state dispatcher */
void mim_0090EA(void);     /* Initial asset load (ROM banks $86-$87) */
void mim_00913A(void);     /* Asset reload (audio data) */

/* Aliases for main.c readability */
#define mim_reset       mim_008000
#define mim_nmi         mim_00819D
#define mim_main_loop   mim_009A5E

#endif /* MIM_FUNCTIONS_H */
