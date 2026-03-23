/*
 * Mario is Missing! — Boot chain recompilation
 *
 * Reset vector ($00:8000) -> hardware init -> WRAM clear -> main loop
 *
 * Game info:
 *   - LoROM mapping, 1 MB ROM, no SRAM
 *   - No special coprocessors (pure 65816)
 *   - Developer: Radical Entertainment / The Software Toolworks
 */

#include "mim/functions.h"
#include "mim/cpu_ops.h"
#include <snesrecomp/snesrecomp.h>
#include <snesrecomp/func_table.h>
#include <stdio.h>
#include <string.h>

/* Forward declarations for subroutines called from boot */
static void mim_00818E(void);  /* VRAM clear */
static void mim_00817B(void);  /* CGRAM clear */
static void mim_00915A(uint16_t src_addr, uint8_t src_bank);  /* SPC700 IPL upload */

/* Forward declarations for graphics/decompression functions */
static void mim_008BF5(void);  /* WRAM compressed data load */
static void mim_008BB7(void);  /* Decompress to WRAM $7F:8000 */
static void mim_008781(void);  /* Decompress + DMA to VRAM */
static void mim_0087DF(void);  /* Decompress + DMA to VRAM (alt) */
static void mim_00D18B(void);  /* Fill tilemap buffer via DMA */
static void mim_00D11F(void);  /* Tilemap writer */
static void mim_008F27(void);  /* Sprite/tile DMA setup */
static void mim_008E9D(void);  /* CGRAM/palette DMA loader */
static void mim_00828C(void);  /* Screen fade-in */
static void mim_00D3D2(void);  /* Shared game graphics loader */

/* Forward declarations for stub functions (defined after main loop) */
static void mim_00D24B(void);  /* Title screen */
static void mim_00D4A3(void);  /* Game state machine */
static void mim_018320(void);  /* State setup */
static void mim_028000(void);  /* Bank 2 game logic */

/*
 * Register all recompiled functions in the function table.
 */
void mim_register_all(void) {
    func_table_register(0x008000, mim_008000);
    func_table_register(0x808000, mim_008000);  /* mirror */
    func_table_register(0x008050, mim_008050);
    func_table_register(0x808050, mim_008050);
    func_table_register(0x008349, mim_008349);
    func_table_register(0x808349, mim_008349);
    func_table_register(0x00842C, mim_00842C);
    func_table_register(0x80842C, mim_00842C);
    func_table_register(0x008453, mim_008453);
    func_table_register(0x808453, mim_008453);
    func_table_register(0x008471, mim_008471);
    func_table_register(0x808471, mim_008471);
    func_table_register(0x00819D, mim_00819D);
    func_table_register(0x80819D, mim_00819D);
    func_table_register(0x0085C0, mim_0085C0);
    func_table_register(0x8085C0, mim_0085C0);
    func_table_register(0x0081E1, mim_0081E1);
    func_table_register(0x8081E1, mim_0081E1);
    func_table_register(0x009203, mim_009203);
    func_table_register(0x809203, mim_009203);
    func_table_register(0x0090EA, mim_0090EA);
    func_table_register(0x8090EA, mim_0090EA);
    func_table_register(0x00913A, mim_00913A);
    func_table_register(0x80913A, mim_00913A);
    func_table_register(0x00911A, mim_00911A);
    func_table_register(0x80911A, mim_00911A);
    func_table_register(0x008249, mim_008249);
    func_table_register(0x808249, mim_008249);
    func_table_register(0x0082A7, mim_0082A7);
    func_table_register(0x8082A7, mim_0082A7);
    func_table_register(0x0090AF, mim_0090AF);
    func_table_register(0x8090AF, mim_0090AF);
    func_table_register(0x008506, mim_008506);
    func_table_register(0x808506, mim_008506);
    func_table_register(0x009A5E, mim_009A5E);
    func_table_register(0x809A5E, mim_009A5E);
    func_table_register(0x00D24B, (snes_func_t)mim_00D24B);
    func_table_register(0x80D24B, (snes_func_t)mim_00D24B);
    func_table_register(0x00D4A3, (snes_func_t)mim_00D4A3);
    func_table_register(0x80D4A3, (snes_func_t)mim_00D4A3);
    func_table_register(0x018320, (snes_func_t)mim_018320);
    func_table_register(0x818320, (snes_func_t)mim_018320);
    func_table_register(0x028000, (snes_func_t)mim_028000);
    func_table_register(0x828000, (snes_func_t)mim_028000);
    func_table_register(0x008BF5, (snes_func_t)mim_008BF5);
    func_table_register(0x808BF5, (snes_func_t)mim_008BF5);
    func_table_register(0x008BB7, (snes_func_t)mim_008BB7);
    func_table_register(0x808BB7, (snes_func_t)mim_008BB7);
    func_table_register(0x008781, (snes_func_t)mim_008781);
    func_table_register(0x808781, (snes_func_t)mim_008781);
    func_table_register(0x00D18B, (snes_func_t)mim_00D18B);
    func_table_register(0x80D18B, (snes_func_t)mim_00D18B);
    func_table_register(0x00D11F, (snes_func_t)mim_00D11F);
    func_table_register(0x80D11F, (snes_func_t)mim_00D11F);
    func_table_register(0x008F27, (snes_func_t)mim_008F27);
    func_table_register(0x808F27, (snes_func_t)mim_008F27);
    func_table_register(0x0087DF, (snes_func_t)mim_0087DF);
    func_table_register(0x8087DF, (snes_func_t)mim_0087DF);
    func_table_register(0x008E9D, (snes_func_t)mim_008E9D);
    func_table_register(0x808E9D, (snes_func_t)mim_008E9D);
    func_table_register(0x00828C, (snes_func_t)mim_00828C);
    func_table_register(0x80828C, (snes_func_t)mim_00828C);
    func_table_register(0x00D3D2, (snes_func_t)mim_00D3D2);
    func_table_register(0x80D3D2, (snes_func_t)mim_00D3D2);

    printf("mim: registered 36 recompiled functions (72 with mirrors)\n");
}

/*
 * $00:8000 — Reset vector
 *
 *   SEI
 *   CLC / XCE        ; switch to native mode
 *   SEP #$20         ; 8-bit A
 *   REP #$10         ; 16-bit X/Y
 *   STZ $4200        ; disable NMI
 *   STZ $420C/$420B  ; disable DMA/HDMA
 *   STZ $2140-$2143  ; clear APU ports
 *   LDA #$8F / STA $2100  ; force blank
 *   LDX #$01FF / TXS ; stack = $01FF
 *   REP #$20         ; 16-bit A
 *   LDX #$0000       ; clear all WRAM
 *   loop: STA $7E0000,X / STA $7F0000,X / INX INX / BNE loop
 *   JSL $80:8050     ; PPU init
 *   JSL $80:8453     ; DMA table loader A
 *   JSL $80:8471     ; DMA table loader B
 *   JSL $80:842C     ; OAM init
 *   JSL $80:8349     ; RNG seed
 *   JMP $9A5E        ; main loop
 */
void mim_008000(void) {
    printf("mim: RESET $00:8000\n");

    OP_SEI();
    OP_CLC();
    op_xce();               /* native mode */
    op_sep(0x20);           /* M=1 (8-bit A) */
    op_rep(0x10);           /* X=0 (16-bit index) */

    /* Disable interrupts and DMA */
    op_stz_abs8(0x4200);    /* NMITIMEN = 0 */
    op_stz_abs8(0x420C);    /* HDMAEN = 0 */
    op_stz_abs8(0x420B);    /* MDMAEN = 0 */

    /* Clear APU communication ports */
    op_stz_abs8(0x2140);
    op_stz_abs8(0x2141);
    op_stz_abs8(0x2142);
    op_stz_abs8(0x2143);

    /* Force blank + max brightness */
    op_lda_imm8(0x8F);
    op_sta_abs8(0x2100);    /* INIDISP = $8F */

    /* Set stack pointer to $01FF */
    op_ldx_imm16(0x01FF);
    g_cpu.S = g_cpu.X;      /* TXS */

    /* Clear all WRAM ($7E0000-$7EFFFF and $7F0000-$7FFFFF) */
    printf("mim: clearing WRAM...\n");
    op_rep(0x20);           /* 16-bit A */
    {
        uint8_t *wram = bus_get_wram();
        if (wram) {
            memset(wram, 0, 0x20000);  /* 128 KB */
        }
    }

    /* JSL $80:8050 — PPU/hardware init */
    printf("mim: calling PPU init ($80:8050)\n");
    mim_008050();

    /* JSL $80:8453 — DMA table loader A */
    printf("mim: calling DMA loader A ($80:8453)\n");
    mim_008453();

    /* JSL $80:8471 — DMA table loader B */
    printf("mim: calling DMA loader B ($80:8471)\n");
    mim_008471();

    /* JSL $80:842C — OAM init */
    printf("mim: calling OAM init ($80:842C)\n");
    mim_00842C();

    /* JSL $80:8349 — RNG/timer seed */
    printf("mim: calling RNG seed ($80:8349)\n");
    mim_008349();

    printf("mim: boot chain complete\n");

    /* JMP $9A5E — main loop entry is handled by main.c's frame loop */
}

/*
 * $00:8050 — PPU / hardware register initialization
 *
 * Comprehensive PPU reset: disables everything, zeros all PPU registers,
 * clears VRAM, CGRAM, OAM, sets Mode 7 identity matrix, configures
 * color math, enables FastROM.
 */
void mim_008050(void) {
    op_sep(0x20);           /* 8-bit A */
    op_rep(0x10);           /* 16-bit X/Y */

    /* Disable NMI, DMA */
    op_stz_abs8(0x4200);
    op_stz_abs8(0x420C);
    op_stz_abs8(0x420B);

    /* Force blank */
    op_lda_imm8(0x8F);
    op_sta_abs8(0x2100);

    /* Set DP = $0000, DB = $00 */
    op_rep(0x20);
    op_lda_imm16(0x0000);
    OP_TCD();
    op_sep(0x20);
    OP_SET_DB(0x00);

    /* Clear VRAM and CGRAM via subroutines */
    mim_00818E();   /* JSL $80:818E — VRAM clear */
    mim_00817B();   /* JSL $80:817B — CGRAM clear */

    /* Zero all PPU registers $2101-$2133 */
    op_stz_abs8(0x2101);    /* OBSEL */
    op_stz_abs8(0x2102);    /* OAMADDL */
    op_stz_abs8(0x2103);    /* OAMADDH */
    op_stz_abs8(0x2105);    /* BGMODE */
    op_stz_abs8(0x2106);    /* MOSAIC */
    op_stz_abs8(0x2107);    /* BG1SC */
    op_stz_abs8(0x2108);    /* BG2SC */
    op_stz_abs8(0x2109);    /* BG3SC */
    op_stz_abs8(0x210A);    /* BG4SC */
    op_stz_abs8(0x210B);    /* BG12NBA */
    op_stz_abs8(0x210C);    /* BG34NBA */
    /* BG scroll registers (write twice each) */
    op_stz_abs8(0x210D); op_stz_abs8(0x210D);  /* BG1HOFS */
    op_stz_abs8(0x210E); op_stz_abs8(0x210E);  /* BG1VOFS */
    op_stz_abs8(0x210F); op_stz_abs8(0x210F);  /* BG2HOFS */
    op_stz_abs8(0x2110); op_stz_abs8(0x2110);  /* BG2VOFS */
    op_stz_abs8(0x2111); op_stz_abs8(0x2111);  /* BG3HOFS */
    op_stz_abs8(0x2112); op_stz_abs8(0x2112);  /* BG3VOFS */
    op_stz_abs8(0x2113); op_stz_abs8(0x2113);  /* BG4HOFS */
    op_stz_abs8(0x2114); op_stz_abs8(0x2114);  /* BG4VOFS */

    /* VRAM increment mode: word access, increment on high byte write */
    op_lda_imm8(0x80);
    op_sta_abs8(0x2115);    /* VMAIN */
    op_stz_abs8(0x2116);    /* VMADDL */
    op_stz_abs8(0x2117);    /* VMADDH */

    /* Mode 7 identity matrix */
    op_lda_imm8(0x01);
    op_sta_abs8(0x211B); op_stz_abs8(0x211B);  /* M7A = $0001 */
    op_stz_abs8(0x211C); op_stz_abs8(0x211C);  /* M7B = $0000 */
    op_stz_abs8(0x211D); op_stz_abs8(0x211D);  /* M7C = $0000 */
    op_lda_imm8(0x01);
    op_sta_abs8(0x211E); op_stz_abs8(0x211E);  /* M7D = $0001 */
    op_stz_abs8(0x211F); op_stz_abs8(0x211F);  /* M7X = $0000 */
    op_stz_abs8(0x2120); op_stz_abs8(0x2120);  /* M7Y = $0000 */

    /* Window and color math */
    op_stz_abs8(0x2123);    /* W12SEL */
    op_stz_abs8(0x2124);    /* W34SEL */
    op_stz_abs8(0x2125);    /* WOBJSEL */
    op_stz_abs8(0x2126);    /* WH0 */
    op_stz_abs8(0x2127);    /* WH1 */
    op_stz_abs8(0x2128);    /* WH2 */
    op_stz_abs8(0x2129);    /* WH3 */
    op_stz_abs8(0x212A);    /* WBGLOG */
    op_stz_abs8(0x212B);    /* WOBJLOG */
    op_stz_abs8(0x212C);    /* TM */
    op_stz_abs8(0x212D);    /* TS */
    op_stz_abs8(0x212E);    /* TMW */
    op_stz_abs8(0x212F);    /* TSW */

    /* Color math */
    op_lda_imm8(0x30);
    op_sta_abs8(0x2130);    /* CGWSEL */
    op_stz_abs8(0x2131);    /* CGADSUB */
    op_lda_imm8(0xE0);
    op_sta_abs8(0x2132);    /* COLDATA (fixed color = black with all planes) */
    op_stz_abs8(0x2133);    /* SETINI */

    /* Zero hardware control registers */
    op_stz_abs8(0x4200);    /* NMITIMEN */
    op_stz_abs8(0x4201);    /* WRIO */
    op_stz_abs8(0x4202);    /* WRMPYA */
    op_stz_abs8(0x4203);    /* WRMPYB */
    op_stz_abs8(0x4204);    /* WRDIVL */
    op_stz_abs8(0x4205);    /* WRDIVH */
    op_stz_abs8(0x4206);    /* WRDIVB */
    op_stz_abs8(0x4207);    /* HTIMEL */
    op_stz_abs8(0x4208);    /* HTIMEH */
    op_stz_abs8(0x4209);    /* VTIMEL */
    op_stz_abs8(0x420A);    /* VTIMEH */
    op_stz_abs8(0x420B);    /* MDMAEN */
    op_stz_abs8(0x420C);    /* HDMAEN */

    /* Enable FastROM */
    op_lda_imm8(0x01);
    op_sta_abs8(0x420D);    /* MEMSEL */
}

/*
 * $00:818E — Clear VRAM (64 KB)
 *
 * Uses DMA channel 0 to zero-fill all of VRAM.
 */
static void mim_00818E(void) {
    op_sep(0x20);
    /* VRAM address = $0000, word increment */
    op_lda_imm8(0x80);
    op_sta_abs8(0x2115);    /* VMAIN */
    op_stz_abs8(0x2116);    /* VMADDL */
    op_stz_abs8(0x2117);    /* VMADDH */

    /* DMA channel 0: fixed source, write to VRAM data port */
    /* Use a zero byte in WRAM as source (address $0000 which we just cleared) */
    op_stz_abs8(0x4300);    /* DMA mode: 1-reg, A->B, no increment (fixed) */
    /* Actually the game uses mode $09: fixed source, 2-reg write */
    op_lda_imm8(0x09);
    op_sta_abs8(0x4300);    /* DMAP0 = fixed source, write pair */
    op_lda_imm8(0x18);
    op_sta_abs8(0x4301);    /* BBAD0 = $18 (VMDATAL) */
    op_stz_abs8(0x4302);    /* A1T0L = $00 */
    op_stz_abs8(0x4303);    /* A1T0H = $00 */
    op_stz_abs8(0x4304);    /* A1B0 = $00 */
    op_stz_abs8(0x4305);    /* DAS0L = $00 */
    op_stz_abs8(0x4306);    /* DAS0H = $00 (size = $0000 = 64KB) */
    op_lda_imm8(0x01);
    op_sta_abs8(0x420B);    /* trigger DMA ch0 */
}

/*
 * $00:817B — Clear CGRAM (512 bytes)
 *
 * Zeros all 256 palette entries.
 */
static void mim_00817B(void) {
    op_sep(0x20);
    op_stz_abs8(0x2121);    /* CGADD = 0 (palette index 0) */

    /* Write 512 zeros to CGDATA ($2122) */
    op_rep(0x10);
    op_ldx_imm16(0x0200);
    /* Loop: STZ $2122 / DEX / BNE */
    while (g_cpu.X > 0) {
        op_stz_abs8(0x2122);
        g_cpu.X--;
    }
}

/*
 * $00:8349 — RNG / timer seed initialization
 *
 * Seeds WRAM $04E0-$04EC with constants.
 */
void mim_008349(void) {
    op_rep(0x20);

    op_lda_imm16(0x119A);
    bus_wram_write16(0x04E4, CPU_A16());
    bus_wram_write16(0x04E0, CPU_A16());

    op_lda_imm16(0x0E84);
    bus_wram_write16(0x04E6, CPU_A16());
    bus_wram_write16(0x04E2, CPU_A16());

    op_lda_imm16(0x4321);
    bus_wram_write16(0x04EA, CPU_A16());

    op_lda_imm16(0x8765);
    bus_wram_write16(0x04EC, CPU_A16());
}

/*
 * $00:842C — OAM sprite table initialization
 *
 * Fills OAM shadow buffer at $0200 with all sprites offscreen.
 * 128 sprites x 4 bytes = 512 bytes, plus 32-byte high table.
 */
void mim_00842C(void) {
    op_rep(0x30);           /* 16-bit A and X/Y */

    /* Fill 128 OAM entries with $E080 (Y=$E0 offscreen, X=$80) */
    op_ldx_imm16(0x0000);
    while (g_cpu.X < 0x0200) {
        op_lda_imm16(0xE080);
        bus_wram_write16(0x0200 + g_cpu.X, CPU_A16());
        op_stz_abs16(0x0200 + g_cpu.X + 2);  /* clear tile/attr */
        g_cpu.X += 4;
    }

    /* Fill high table (32 bytes) with $5555 — all sprites small, high X bit set */
    op_lda_imm16(0x5555);
    while (g_cpu.X < 0x0220) {
        bus_wram_write16(0x0200 + g_cpu.X, CPU_A16());
        g_cpu.X += 2;
    }
}

/*
 * $00:8453 — DMA table loader A
 *
 * Sets up pointer to inline data table at $80:8467,
 * then calls the DMA table engine at $80:8506.
 */
void mim_008453(void) {
    op_sep(0x20);
    op_rep(0x10);

    /* Store 24-bit pointer to data table at DP $18 */
    op_ldx_imm16(0x8467);
    bus_wram_write16(g_cpu.DP + 0x18, g_cpu.X);
    op_lda_imm8(0x80);
    bus_wram_write8(g_cpu.DP + 0x1A, CPU_A8());

    /* Call DMA table engine */
    mim_008506();
}

/*
 * $00:8471 — DMA table loader B
 *
 * Same as $8453 but points to data table at $80:8485.
 */
void mim_008471(void) {
    op_sep(0x20);
    op_rep(0x10);

    op_ldx_imm16(0x8485);
    bus_wram_write16(g_cpu.DP + 0x18, g_cpu.X);
    op_lda_imm8(0x80);
    bus_wram_write8(g_cpu.DP + 0x1A, CPU_A8());

    mim_008506();
}

/*
 * $00:8506 — DMA table engine
 *
 * Reads ONE DMA transfer entry from the 24-bit pointer at DP $18-$1A.
 * If NMI is enabled (DP $00 bit 7), queues the entry for NMI processing.
 * If NMI is disabled, executes DMA immediately.
 *
 * Table format (8 bytes):
 *   byte 0-1: VRAM/CGRAM destination address
 *   byte 2-3: DMA transfer size (bytes)
 *   byte 4:   Mode index (lookup for VMAIN, B-bus register, DMA mode)
 *             bit 7: if set, force word increment on VMAIN
 *   byte 5-7: DMA source address + bank
 *
 * Mode index lookup tables at $8681/$8692/$86A3:
 *   Mode 0: VMAIN=$80, B=$18 (VMDATAL), DMA=$01 (word, A→B) — standard VRAM
 *   Mode $0D: CGRAM transfer (B=$22)
 *   etc.
 */
void mim_008506(void) {
    /* VMAIN lookup table */
    static const uint8_t vmain_tbl[16] = {
        0x80,0x80,0x00,0x00,0x80,0x80,0x80,0x80,
        0x00,0x00,0x80,0x80,0x05,0x80,0x80,0x80
    };
    /* B-bus register lookup */
    static const uint8_t bbus_tbl[16] = {
        0x18,0x39,0x18,0x39,0x19,0x3A,0x18,0x39,
        0x18,0x39,0x19,0x3A,0x18,0x22,0x3B,0x22
    };
    /* DMA mode lookup */
    static const uint8_t dma_mode_tbl[16] = {
        0x01,0x81,0x00,0x80,0x00,0x80,0x09,0x89,
        0x08,0x88,0x08,0x88,0x00,0x02,0x82,0x0A
    };

    op_sep(0x30);

    /* Check NMI enable flag — if set, queue for NMI (simplified: just execute) */
    uint8_t nmi_flag = bus_wram_read8(g_cpu.DP + 0x00);
    /* For now, always execute immediately regardless of NMI state */

    uint16_t tbl_addr = bus_wram_read16(g_cpu.DP + 0x18);
    uint8_t tbl_bank = bus_wram_read8(g_cpu.DP + 0x1A);

    /* Read mode index from byte 4 */
    uint8_t mode_raw = bus_read8(tbl_bank, tbl_addr + 4);
    uint8_t mode_idx = mode_raw & 0x7F;
    if (mode_idx >= 16) mode_idx = 0;

    /* Look up VMAIN, B-bus register, and DMA mode */
    uint8_t vmain = vmain_tbl[mode_idx];
    if (mode_raw & 0x80) vmain |= 0x01;  /* force word increment */
    uint8_t bbus_reg = bbus_tbl[mode_idx];
    uint8_t dma_mode = dma_mode_tbl[mode_idx];

    /* Set VMAIN */
    bus_write8(0x00, 0x2115, vmain);

    /* Set destination address */
    if (mode_idx >= 0x0D) {
        /* CGRAM mode: byte 0 is CGRAM address */
        bus_write8(0x00, 0x2121, bus_read8(tbl_bank, tbl_addr));
    } else {
        /* VRAM mode: bytes 0-1 are VRAM word address */
        bus_write8(0x00, 0x2116, bus_read8(tbl_bank, tbl_addr));
        bus_write8(0x00, 0x2117, bus_read8(tbl_bank, tbl_addr + 1));
    }

    /* Set DMA size (bytes 2-3) */
    bus_write8(0x00, 0x4305, bus_read8(tbl_bank, tbl_addr + 2));
    bus_write8(0x00, 0x4306, bus_read8(tbl_bank, tbl_addr + 3));

    /* Set DMA source (bytes 5-7) */
    bus_write8(0x00, 0x4302, bus_read8(tbl_bank, tbl_addr + 5));
    bus_write8(0x00, 0x4303, bus_read8(tbl_bank, tbl_addr + 6));
    bus_write8(0x00, 0x4304, bus_read8(tbl_bank, tbl_addr + 7));

    /* Set DMA mode and B-bus register */
    bus_write8(0x00, 0x4300, dma_mode);
    bus_write8(0x00, 0x4301, bbus_reg);

    /* Trigger DMA channel 0 */
    bus_write8(0x00, 0x420B, 0x01);

    /* Reset VMAIN to standard mode */
    bus_write8(0x00, 0x2115, 0x80);

    g_cpu.flag_C = 0;  /* clear carry (success) */
}

/*
 * $00:819D — NMI handler
 *
 * Reentrance guard via DP $02 (incremented on entry, decremented on exit).
 * If $02 was already nonzero, just decrement and RTI (skip processing).
 *
 *   REP #$30
 *   INC $02          ; reentrance guard
 *   BNE skip         ; if was nonzero, skip
 *   PHA/PHX/PHY/PHD/PHB
 *   PHK / PLB        ; DB = PB
 *   LDA #$0000 / TCD ; DP = $0000
 *   check $06 flag, inc frame counters
 *   SEP #$30
 *   LDA $4210        ; read RDNMI (clear NMI flag)
 *   LDA #$80 / STA $2100  ; force blank during DMA
 *   JSL $80:85C0     ; DMA/VRAM transfer handler
 *   JSL $80:81E1     ; indirect vector dispatch
 *   SEP #$30
 *   LDA $01 / STA $2100   ; restore INIDISP
 *   JSR $81E5        ; wait for VBlank end
 *   REP #$30
 *   STZ $16          ; clear flag
 *   PLB/PLD/PLY/PLX/PLA
 * skip:
 *   DEC $02
 *   RTI
 */
void mim_00819D(void) {
    op_rep(0x30);

    /* Reentrance guard — in the recomp, NMI is called synchronously
     * from $8249 so true reentrance can't happen. But $02 must be
     * managed correctly since other code may check it. */
    uint16_t guard = bus_wram_read16(0x0002);
    bus_wram_write16(0x0002, guard + 1);
    if (guard != 0) {
        bus_wram_write16(0x0002, guard);
        return;
    }

    /* Save registers */
    uint8_t saved_db = g_cpu.DB;
    uint16_t saved_dp = g_cpu.DP;

    /* PHK/PLB — set DB to program bank */
    OP_SET_DB(0x00);
    /* LDA #$0000 / TCD — set DP to $0000 */
    g_cpu.DP = 0x0000;

    /* Frame counter logic */
    uint16_t flag06 = bus_wram_read16(0x0006);
    if (flag06 == 0) {
        uint16_t ctr04 = bus_wram_read16(0x0004) + 1;
        bus_wram_write16(0x0004, ctr04);
        if (ctr04 == 0) {
            /* Overflow — increment extended counter */
            uint16_t ext = bus_wram_read16(0x0E11) + 1;
            bus_wram_write16(0x0E11, ext);
        }
    }

    /* SEP #$30 — 8-bit everything */
    op_sep(0x30);

    /* Read RDNMI to acknowledge NMI */
    bus_read8(0x00, 0x4210);

    /* Force blank during DMA transfers */
    op_lda_imm8(0x80);
    op_sta_abs8(0x2100);

    /* JSL $80:85C0 — DMA/VRAM/OAM transfer handler */
    mim_0085C0();

    /* JSL $80:81E1 — indirect vector dispatch (JML [$08]) */
    mim_0081E1();

    /* Restore screen brightness */
    op_sep(0x30);
    uint8_t inidisp = bus_wram_read8(0x0001);
    bus_write8(0x00, 0x2100, inidisp);

    /* JSR $81E5 — wait for VBlank end */
    mim_0081E5();

    /* REP #$30 / STZ $16 */
    op_rep(0x30);
    bus_wram_write16(0x0016, 0);

    /* Restore registers */
    g_cpu.DB = saved_db;
    g_cpu.DP = saved_dp;

    /* DEC $02 — exit reentrance guard */
    bus_wram_write16(0x0002, bus_wram_read16(0x0002) - 1);
}

/*
 * $00:85C0 — NMI DMA/VRAM/OAM transfer handler
 *
 * If $10 (skip flag) is zero, transfers OAM shadow buffer ($0200, 544 bytes)
 * to PPU OAM via DMA channel 0.
 *
 * Then processes a ring buffer of DMA transfer requests:
 *   $14 = read pointer, $15 = write pointer into queue at $0420
 *   Each entry is 8 bytes: type, VRAM/CGRAM addr, length, source addr+bank
 *   Supports both VRAM and CGRAM transfers.
 */
void mim_0085C0(void) {
    uint8_t saved_db = g_cpu.DB;
    OP_SET_DB(0x00);        /* PHB / PHK / PLB */
    op_sep(0x30);

    /* OAM DMA transfer (if not skipped) */
    uint8_t skip_oam = bus_wram_read8(0x0010);
    if (skip_oam == 0) {
        /* DMA channel 0: transfer $0220 bytes from $00:0200 to OAM */
        bus_write8(0x00, 0x2102, 0x00);     /* OAMADDL = 0 */
        bus_write8(0x00, 0x2103, 0x00);     /* OAMADDH = 0 */
        bus_write8(0x00, 0x4300, 0x00);     /* DMA mode: 1-reg, increment */
        bus_write8(0x00, 0x4301, 0x04);     /* B-bus = $04 -> $2104 (OAMDATA) */
        bus_write8(0x00, 0x4302, 0x00);     /* source low = $00 */
        bus_write8(0x00, 0x4303, 0x02);     /* source high = $02 -> $0200 */
        bus_write8(0x00, 0x4304, 0x00);     /* source bank = $00 */
        bus_write8(0x00, 0x4305, 0x20);     /* size low = $20 */
        bus_write8(0x00, 0x4306, 0x02);     /* size high = $02 -> $0220 */
        bus_write8(0x00, 0x420B, 0x01);     /* trigger DMA ch0 */
    }

    /* Process DMA ring buffer */
    uint8_t read_ptr = bus_wram_read8(0x0014);
    uint8_t write_ptr = bus_wram_read8(0x0015);

    /* Lookup tables for transfer types (from ROM at $80:8681, $80:8692, $80:86A3) */
    while (read_ptr != write_ptr) {
        uint16_t entry_base = 0x0420 + (uint16_t)read_ptr * 8;

        uint8_t type = bus_wram_read8(entry_base);
        uint8_t addr_lo = bus_wram_read8(entry_base + 1);
        uint8_t addr_hi = bus_wram_read8(entry_base + 2);
        uint8_t len_lo = bus_wram_read8(entry_base + 3);
        uint8_t len_hi = bus_wram_read8(entry_base + 4);
        uint8_t src_lo = bus_wram_read8(entry_base + 5);
        uint8_t src_hi = bus_wram_read8(entry_base + 6);
        uint8_t src_bank = bus_wram_read8(entry_base + 7);

        if (type < 0x0D) {
            /* VRAM transfer */
            uint8_t vmain = (type & 0x80) ? 0x81 : 0x80;
            bus_write8(0x00, 0x2115, vmain);
            bus_write8(0x00, 0x2116, addr_lo);
            bus_write8(0x00, 0x2117, addr_hi);

            bus_write8(0x00, 0x4300, 0x01);     /* 2-reg sequential */
            bus_write8(0x00, 0x4301, 0x18);     /* B-bus = $18 (VMDATAL) */
        } else {
            /* CGRAM transfer */
            bus_write8(0x00, 0x2121, addr_lo);  /* CGADD */
            bus_write8(0x00, 0x4300, 0x00);     /* 1-reg */
            bus_write8(0x00, 0x4301, 0x22);     /* B-bus = $22 (CGDATA) */
        }

        bus_write8(0x00, 0x4302, src_lo);
        bus_write8(0x00, 0x4303, src_hi);
        bus_write8(0x00, 0x4304, src_bank);
        bus_write8(0x00, 0x4305, len_lo);
        bus_write8(0x00, 0x4306, len_hi);
        bus_write8(0x00, 0x420B, 0x01);         /* trigger DMA ch0 */

        read_ptr = (read_ptr + 1) % 24;
    }
    bus_wram_write8(0x0014, read_ptr);

    /* Reset VRAM/CGRAM/OAM addresses to 0 */
    bus_write8(0x00, 0x2116, 0x00);
    bus_write8(0x00, 0x2117, 0x00);
    bus_write8(0x00, 0x2121, 0x00);
    bus_write8(0x00, 0x2102, 0x00);
    bus_write8(0x00, 0x2103, 0x00);

    g_cpu.DB = saved_db;
}

/*
 * $00:81E1 — NMI indirect vector dispatch
 *
 * Single instruction: JML [$0008]
 * Jumps through the 24-bit vector stored at DP $08-$0A.
 * The game sets this to point to different NMI handlers depending on state.
 */
void mim_0081E1(void) {
    uint16_t addr = bus_wram_read16(0x0008);
    uint8_t bank = bus_wram_read8(0x000A);
    uint32_t full = ((uint32_t)bank << 16) | addr;

    if (full != 0) {
        snes_func_t fn = func_table_lookup(full);
        if (fn) {
            fn();
        }
        /* Silently skip unregistered NMI vectors */
    }
}

/*
 * $00:81E5 — Wait for VBlank / auto-joypad to finish
 *
 * Polls HVBJOY ($4212) until VBlank flag clears, with timeout.
 */
void mim_0081E5(void) {
    op_sep(0x30);
    /* In the recomp, VBlank timing is handled by the frame loop.
     * This is effectively a no-op. */
}

/*
 * $00:8249 — Wait for next frame (VBlank sync)
 *
 * Original: spins until NMI increments DP $04 (frame counter).
 * Recomp: this IS the frame boundary. We render the current frame,
 * poll input, and run the NMI handler before returning.
 *
 * The game's infinite main loop calls this whenever it needs a frame
 * to pass. X is preserved across the call (PHA/PHP ... PLP/PLA).
 */
jmp_buf mim_quit_jmp;

void mim_008249(void) {
    /* Save registers (original does PHA/PHP) */
    uint16_t saved_a = CPU_A16();

    /* === Frame boundary === */

    /* End current frame: render PPU scanlines, present, sync */
    snesrecomp_end_frame();

    /* Start next frame: poll events, check quit */
    if (!snesrecomp_begin_frame()) {
        printf("mim: quit requested, longjmp\n");
        longjmp(mim_quit_jmp, 1);
    }

    /* Run NMI handler only when the game has enabled NMI ($00 bit 7).
     * When NMI is disabled, just bump the frame counter so wait loops work. */
    uint8_t nmi_flag = bus_wram_read8(0x0000);
    if (nmi_flag & 0x80) {
        bus_wram_write16(0x0002, 0);
        mim_00819D();
    } else {
        /* Bump frame counter */
        uint16_t ctr = bus_wram_read16(0x0004);
        bus_wram_write16(0x0004, ctr + 1);
        /* Still update brightness from $01 */
        bus_write8(0x00, 0x2100, bus_wram_read8(0x0001));
    }

    /* Restore A */
    g_cpu.C = saved_a;
}

/*
 * $00:82A7 — Screen disable / cleanup
 *
 * Waits 15 frames with force blank, disables all display layers,
 * disables NMI, and clears the NMI enable flag at $00.
 */
void mim_0082A7(void) {
    op_sep(0x30);

    /* Wait 15 frames with force blank fading */
    for (int i = 0x0F; i >= 0; i--) {
        g_cpu.X = (uint16_t)i;
        mim_008249();
        bus_wram_write8(g_cpu.DP + 0x01, (uint8_t)g_cpu.X);
    }

    /* Disable all display layers */
    bus_write8(0x00, 0x212C, 0x00);  /* TM = 0 */
    bus_write8(0x00, 0x212D, 0x00);  /* TS = 0 */
    bus_write8(0x00, 0x212E, 0x00);  /* TMW = 0 */
    bus_write8(0x00, 0x212F, 0x00);  /* TSW = 0 */

    /* Force blank + max brightness, disable NMI */
    bus_wram_write8(g_cpu.DP + 0x01, 0x8F);
    mim_008249();
    bus_write8(0x00, 0x4200, 0x00);  /* NMITIMEN = 0 */
    bus_wram_write8(g_cpu.DP + 0x00, 0x00);  /* clear NMI enable flag */
}

/*
 * $00:90AF — VBlank wait loop + SPC700 sync commands
 *
 * Sends SPC700 command $10, waits ~42 frames polling VBlank,
 * then sends commands $1A and $1C to sync audio state.
 * Returns with carry reflecting the SPC700 response.
 */
void mim_0090AF(void) {
    op_sep(0x20);
    op_rep(0x10);

    /* SPC700 command $10 with Y=$0301 */
    CPU_SET_A8(0x10);
    g_cpu.Y = 0x0301;
    mim_009203();

    /* Wait ~42 frames (original polls $4212 for VBlank 42 times) */
    for (int i = 0; i < 42; i++) {
        mim_008249();
    }

    /* SPC700 command $1A with Y=$0000 */
    CPU_SET_A8(0x1A);
    g_cpu.Y = 0x0000;
    mim_009203();

    /* SPC700 command $1C with Y=$FF00 */
    CPU_SET_A8(0x1C);
    g_cpu.Y = 0xFF00;
    mim_009203();

    /* SPC700 command $1C with Y=$8000 */
    CPU_SET_A8(0x1C);
    g_cpu.Y = 0x8000;
    mim_009203();
}

/* ================================================================
 * Graphics / Decompression Engine
 * ================================================================ */

/*
 * $00:8BF5 — Compressed data load to WRAM
 *
 * Decompresses data from ROM source ([$5D-$5F]) to a WRAM destination.
 * Entry: X = WRAM dest offset, A = WRAM bank bit (0=$7E, 1=$7F)
 * Source pointer at DP $5D-$5F (24-bit).
 *
 * Compression format (mode 0 / byte-level):
 *   Header: type byte (1=byte, 2=word, 3=LZ)
 *   Mode 0: 5 dictionary bytes, then command stream:
 *     Command byte: [EEE XXXXX]
 *       E=0: fill X+1 bytes with dict[0]
 *       E=1-4: fill X+1 bytes with dict[E]
 *       E=5 ($A0): copy X+1 literal bytes
 *       E=6 ($C0): read seed, fill X+1 bytes incrementing
 *       E=7 ($E0): copy X+1 literal bytes
 *     $00 = end of stream
 */
static void mim_008BF5(void) {
    op_sep(0x20);
    op_rep(0x10);

    /* Set WRAM destination address ($2181-$2183) */
    uint16_t wram_offset = g_cpu.X;
    uint8_t wram_bank_bit = CPU_A8() & 0x01;
    bus_write8(0x00, 0x2181, (uint8_t)(wram_offset & 0xFF));
    bus_write8(0x00, 0x2182, (uint8_t)(wram_offset >> 8));
    bus_write8(0x00, 0x2183, wram_bank_bit);

    /* Source pointer from DP $5D-$5F */
    uint16_t src_lo = bus_wram_read16(g_cpu.DP + 0x5D);
    uint8_t src_bank = bus_wram_read8(g_cpu.DP + 0x5F);

    /* Read compression type byte */
    uint8_t comp_type = bus_read8(src_bank, src_lo);
    if (comp_type < 1 || comp_type > 3) return;  /* unsupported or raw */

    if (comp_type == 1) {
        /* Mode 0: byte-level decompression */
        uint16_t ptr = src_lo;

        /* Read 5 dictionary bytes at src+1..src+5 */
        uint8_t dict[5];
        for (int i = 0; i < 5; i++) {
            dict[i] = bus_read8(src_bank, ptr + 1 + i);
        }

        /* Command stream starts at src+6 */
        uint16_t y = 6;
        while (1) {
            uint8_t cmd = bus_read8(src_bank, ptr + y);
            if (cmd == 0) break;  /* end of stream */
            y++;

            uint8_t count = cmd & 0x1F;
            if (count == 0) count = 32;  /* X register is count, 0 means wrapped */
            uint8_t mode = cmd & 0xE0;

            if (mode < 0xA0) {
                /* RLE fill from dictionary */
                uint8_t dict_idx = (mode == 0) ? 0 : (mode >> 5);
                uint8_t fill_val = dict[dict_idx];
                for (int i = 0; i < count; i++) {
                    bus_write8(0x00, 0x2180, fill_val);
                }
            } else if (mode == 0xA0) {
                /* Copy literal bytes from source */
                for (int i = 0; i < count; i++) {
                    bus_write8(0x00, 0x2180, bus_read8(src_bank, ptr + y));
                    y++;
                }
            } else if (mode == 0xC0) {
                /* Incrementing fill: read seed, write count times with INC */
                uint8_t val = bus_read8(src_bank, ptr + y);
                y++;
                for (int i = 0; i < count; i++) {
                    bus_write8(0x00, 0x2180, val);
                    val++;
                }
            } else {
                /* $E0: copy literal bytes (same as $A0) */
                for (int i = 0; i < count; i++) {
                    bus_write8(0x00, 0x2180, bus_read8(src_bank, ptr + y));
                    y++;
                }
            }

            /* Page boundary adjustment (original checks Y >= $DC) */
            if (y >= 0xDC) {
                ptr += y;
                y = 0;
            }
        }
    } else if (comp_type == 3) {
        /* LZ bitstream decompression (mode 3) */
        /* Mode 2 (type byte 3): LZ bitstream with 12-bit ring buffer.
         *
         * Ring buffer at $7F:F000 (4096 bytes, wraps with & 0xFFF).
         * Control bytes: 8 bits, MSB first.
         *   Bit 1 = literal: read 1 byte, write to output + ring buf.
         *   Bit 0 = back-ref: read 2 bytes, decode ring offset + length.
         *     If 2-byte value is 0, end of stream.
         *     ring_offset = ((byte0 << 4) | (byte1 >> 4)) & 0xFFF
         *     length = (byte1 & 0x0F) + 2
         */
        uint8_t *wram = bus_get_wram();
        if (!wram) return;

        /* Ring buffer in WRAM at $7F:F000 = offset $1F000 */
        uint8_t *ring = wram + 0x1F000;
        uint16_t ring_write = 1;  /* write position starts at 1 */
        uint32_t total_output = 0;

        uint16_t ptr = src_lo;
        /* Skip type byte, read control byte address */
        uint16_t ctrl_pos = ptr + 1;
        uint16_t data_pos = ptr + 2;
        uint16_t y = 0;  /* offset into data from data_pos */
        int bits_left = 8;
        uint8_t ctrl = bus_read8(src_bank, ctrl_pos);

        while (1) {
            /* Shift out next bit */
            int bit = (ctrl >> 7) & 1;
            ctrl <<= 1;

            if (bit) {
                /* Literal byte */
                uint8_t val = bus_read8(src_bank, data_pos + y);
                y++;
                bus_write8(0x00, 0x2180, val);
                ring[ring_write] = val;
                ring_write = (ring_write + 1) & 0x0FFF;
                total_output++;
            } else {
                /* Back-reference: read 2-byte descriptor */
                uint8_t byte0 = bus_read8(src_bank, data_pos + y);
                uint8_t byte1 = bus_read8(src_bank, data_pos + y + 1);
                uint16_t ref16 = (uint16_t)byte0 | ((uint16_t)byte1 << 8);

                if (ref16 == 0) break;  /* end of stream */
                y += 2;

                /* Decode: after XBA, value = byte1 | (byte0 << 8)
                 * ring_offset = (byte1 | (byte0 << 8)) >> 4 = (byte0 << 4) | (byte1 >> 4)
                 * length = (byte1 & 0x0F) + 2 */
                uint16_t ring_read = ((byte0 << 4) | (byte1 >> 4)) & 0x0FFF;
                int length = (byte1 & 0x0F) + 2;

                for (int i = 0; i < length; i++) {
                    uint8_t val = ring[ring_read];
                    bus_write8(0x00, 0x2180, val);
                    ring[ring_write] = val;
                    ring_write = (ring_write + 1) & 0x0FFF;
                    ring_read = (ring_read + 1) & 0x0FFF;
                    total_output++;
                }
            }

            bits_left--;
            if (bits_left == 0) {
                /* Reload control byte */
                bits_left = 8;
                ctrl_pos = data_pos + y;
                y++;
                ctrl = bus_read8(src_bank, ctrl_pos);
            }
        }
        (void)total_output;
    } else {
        printf("mim: TODO: compression mode %d not yet implemented\n", comp_type);
    }
}

/*
 * $00:8BB7 — Decompress to WRAM $7F:8000
 *
 * Source pointer at DP $5D-$5F. Decompresses data and writes to
 * WRAM starting at $7F:8000 via the WRAM port ($2180).
 */
static void mim_008BB7(void) {
    op_sep(0x30);

    /* Set WRAM destination to $7F:8000 */
    bus_write8(0x00, 0x2181, 0x00);
    bus_write8(0x00, 0x2182, 0x80);
    bus_write8(0x00, 0x2183, 0x01);  /* bank $7F → bit 0 = 1 */

    /* Reuse $8BF5's decompression with the destination already set */
    uint16_t src_lo = bus_wram_read16(g_cpu.DP + 0x5D);
    uint8_t src_bank = bus_wram_read8(g_cpu.DP + 0x5F);
    uint8_t comp_type = bus_read8(src_bank, src_lo);

    if (comp_type < 1 || comp_type > 3) return;

    /* Call $8BF5 with pre-set WRAM destination */
    g_cpu.X = 0x0000;
    CPU_SET_A8(0x01);  /* bank $7F → bit = 1 */
    /* Save and override X with $8000 for the WRAM offset */
    bus_write8(0x00, 0x2181, 0x00);
    bus_write8(0x00, 0x2182, 0x80);
    bus_write8(0x00, 0x2183, 0x01);
    g_cpu.X = 0x8000;
    mim_008BF5();
}

/*
 * $00:8781 — Decompress ROM data to VRAM
 *
 * Entry: A = VRAM word address for DMA dest, [$5D] = compressed data size,
 *        source data follows the 2-byte size at [$5D+2].
 * Decompresses to WRAM $7F:8000, then DMA transfers to VRAM.
 */
static void mim_008781(void) {
    op_rep(0x20);
    op_sep(0x10);

    /* A = VRAM destination word address */
    uint16_t vram_dest = CPU_A16();
    bus_wram_write16(g_cpu.DP + 0x2A, vram_dest);

    /* Read compressed data size from [$5D] (16-bit indirect long) */
    uint16_t src_lo = bus_wram_read16(g_cpu.DP + 0x5D);
    uint8_t src_bank = bus_wram_read8(g_cpu.DP + 0x5F);
    uint16_t data_size = bus_read16(src_bank, src_lo);
    bus_wram_write16(g_cpu.DP + 0x2C, data_size);

    /* printf("mim: $8781 VRAM load: src=$%02X:%04X size=%u vram=$%04X\n",
           src_bank, src_lo, data_size, vram_dest); */

    /* Set DMA source = $7F:8000 (decompressed data) */
    bus_wram_write8(g_cpu.DP + 0x2E, 0x00);
    bus_wram_write16(g_cpu.DP + 0x2F, 0x8000);
    bus_wram_write8(g_cpu.DP + 0x31, 0x7F);

    /* Advance source pointer past the 2-byte size field */
    src_lo += 2;
    bus_wram_write16(g_cpu.DP + 0x5D, src_lo);

    /* Decompress to WRAM $7F:8000 */
    mim_008BB7();

    /* Set up DMA transfer table pointer at DP $18-$1A */
    op_rep(0x20);
    bus_wram_write16(g_cpu.DP + 0x18, 0x002A);
    bus_wram_write8(g_cpu.DP + 0x1A, 0x00);

    /* Execute DMA transfer to VRAM */
    mim_008506();
}

/*
 * $00:D18B — Fill tilemap buffer ($7F:F000) via DMA
 *
 * DMA channel 7: fills 2048 bytes at WRAM $7F:F000 with data from
 * a fixed source at $80:D1BD (likely zeroes/spaces for clearing).
 */
static void mim_00D18B(void) {
    op_rep(0x20);
    op_sep(0x10);

    /* Set WRAM destination to $7F:F000 */
    bus_write8(0x00, 0x2181, 0x00);
    bus_write8(0x00, 0x2182, 0xF0);
    bus_write8(0x00, 0x2183, 0x01);  /* $7F */

    /* DMA channel 7: fixed source → WRAM port ($2180) */
    bus_write8(0x00, 0x4371, 0x80);  /* B-bus dest = $2180 */
    bus_write8(0x00, 0x4370, 0x08);  /* DMA mode: fixed source, A→B */
    bus_write16(0x00, 0x4375, 0x0800);  /* transfer size = 2048 */
    bus_write16(0x00, 0x4372, 0xD1BD);  /* source addr */
    bus_write8(0x00, 0x4374, 0x80);  /* source bank $80 */

    /* Trigger DMA channel 7 */
    bus_write8(0x00, 0x420B, 0x80);
}

/*
 * $00:D11F — Tilemap writer
 *
 * Reads a compressed tilemap from the source pointed to by X (bank = DB)
 * and writes it to WRAM $7F:F000 as 16-bit tilemap entries.
 *
 * Format: [row][col][tile bytes...]
 *   Row/col encode starting VRAM position.
 *   $00 = end, $F0-$FE = set palette, $FF = newline, else = tile char.
 */
static void mim_00D11F(void) {
    op_rep(0x30);

    uint16_t src = g_cpu.X;
    uint8_t bank = g_cpu.DB;
    uint16_t palette = 0x2100;  /* default palette bits */
    uint16_t y = 0;

    /* Read row (byte 0) and col (byte 1) to compute tilemap offset */
    uint8_t row = bus_read8(bank, src + y) & 0xFF;
    y++;
    uint16_t base_offset = (uint16_t)(row << 6);  /* row * 64 bytes */

    uint8_t col = bus_read8(bank, src + y) & 0xFF;
    y++;
    base_offset += col * 2;

    uint16_t x_offset = base_offset;

    /* Process tile stream */
    while (1) {
        uint8_t ch = bus_read8(bank, src + y);
        y++;

        if (ch == 0x00) break;  /* end */

        if (ch >= 0xF0) {
            if (ch == 0xFF) {
                /* Newline: advance to next row */
                base_offset += 0x0040;
                x_offset = base_offset;
            } else {
                /* Set palette: low 3 bits → palette number */
                uint8_t pal = ch & 0x07;
                palette = 0x2100 | ((uint16_t)pal << 10);
            }
            continue;
        }

        /* Regular tile: subtract $20 (ASCII offset), OR with palette */
        uint16_t tile = ((uint16_t)ch - 0x20) | palette;
        bus_write8(0x00, 0x2180, (uint8_t)(tile & 0xFF));
        bus_write8(0x00, 0x2181, (uint8_t)(tile >> 8));

        /* Write directly to WRAM $7F:F000 + x_offset */
        uint8_t *wram = bus_get_wram();
        if (wram) {
            uint32_t addr = 0x1F000 + x_offset;  /* $7F:F000 in WRAM = offset $1F000 */
            wram[addr] = (uint8_t)(tile & 0xFF);
            wram[addr + 1] = (uint8_t)(tile >> 8);
        }
        x_offset += 2;
    }
}

/*
 * $00:8F27 — Sprite/tile DMA setup
 *
 * Sets up a DMA transfer table entry for sprite tile data.
 * Entry: A = source bank, X = source addr, Y = VRAM dest word addr.
 * Reads first byte of source to determine tile count, then calls $8506.
 */
static void mim_008F27(void) {
    op_sep(0x20);
    op_rep(0x10);

    uint8_t bank = CPU_A8();
    g_cpu.DB = bank;  /* PHB; PHA; PLB */

    /* Store VRAM dest and source */
    bus_wram_write16(g_cpu.DP + 0x2A, g_cpu.Y);
    bus_wram_write16(g_cpu.DP + 0x60, g_cpu.X);
    bus_wram_write8(g_cpu.DP + 0x62, bank);

    /* Source+1 = DMA source addr, bank = same */
    uint16_t src_plus_1 = g_cpu.X + 1;
    bus_wram_write16(g_cpu.DP + 0x2F, src_plus_1);
    bus_wram_write8(g_cpu.DP + 0x31, bank);

    /* Read tile count from source[0] via [$60] indirect long */
    uint8_t tile_count = bus_read8(bank, g_cpu.X);
    uint16_t byte_count;
    if (tile_count >= 0x80) {
        byte_count = (uint16_t)(tile_count << 1);  /* large: count * 2 */
        bus_wram_write8(g_cpu.DP + 0x2C, (uint8_t)(byte_count & 0xFF));
        bus_wram_write8(g_cpu.DP + 0x2D, 0x01);
    } else {
        byte_count = (uint16_t)(tile_count << 1);
        bus_wram_write8(g_cpu.DP + 0x2C, (uint8_t)(byte_count & 0xFF));
        bus_wram_write8(g_cpu.DP + 0x2D, 0x00);
    }

    /* DMA mode = $0D (word write, A→B, auto-increment) */
    bus_wram_write8(g_cpu.DP + 0x2E, 0x0D);

    /* Set DMA table pointer at DP $18-$1A = $002A */
    bus_wram_write16(g_cpu.DP + 0x18, 0x002A);
    bus_wram_write8(g_cpu.DP + 0x1A, 0x00);

    /* Execute DMA */
    mim_008506();
}

/*
 * $00:87DF — Decompress ROM data to VRAM (alt staging at $7F:F800)
 */
static void mim_0087DF(void) {
    op_rep(0x20);
    op_sep(0x10);

    uint16_t vram_dest = CPU_A16();
    bus_wram_write16(g_cpu.DP + 0x2A, vram_dest);

    uint16_t src_lo = bus_wram_read16(g_cpu.DP + 0x5D);
    uint8_t src_bank = bus_wram_read8(g_cpu.DP + 0x5F);

    uint16_t data_size = bus_read16(src_bank, src_lo);
    bus_wram_write16(g_cpu.DP + 0x2C, data_size);

    bus_wram_write8(g_cpu.DP + 0x2E, 0x00);
    bus_wram_write16(g_cpu.DP + 0x2F, 0xF800);
    bus_wram_write8(g_cpu.DP + 0x31, 0x7F);

    src_lo += 2;
    bus_wram_write16(g_cpu.DP + 0x5D, src_lo);

    /* Read and skip transfer method word */
    src_lo += 2;
    bus_wram_write16(g_cpu.DP + 0x5D, src_lo);

    /* Decompress to WRAM $7F:F800 */
    bus_write8(0x00, 0x2181, 0x00);
    bus_write8(0x00, 0x2182, 0xF8);
    bus_write8(0x00, 0x2183, 0x01);
    g_cpu.X = 0xF800;
    CPU_SET_A8(0x01);
    mim_008BF5();

    /* DMA to VRAM */
    op_rep(0x20);
    bus_wram_write16(g_cpu.DP + 0x18, 0x002A);
    bus_wram_write8(g_cpu.DP + 0x1A, 0x00);
    mim_008506();
}

/*
 * $00:8E9D — CGRAM (palette) DMA loader
 *
 * Source format: [u8 cgram_addr] [u8 color_count] [count*2 bytes data]
 */
static void mim_008E9D(void) {
    op_sep(0x20);
    op_rep(0x10);

    uint8_t bank = CPU_A8();
    g_cpu.DB = bank;

    uint8_t cgram_addr = bus_read8(bank, g_cpu.X);
    bus_write8(0x00, 0x2121, cgram_addr);

    uint8_t color_count = bus_read8(bank, g_cpu.X + 1);
    uint16_t byte_count = (uint16_t)color_count << 1;

    uint16_t src_data = g_cpu.X + 2;
    bus_write8(0x00, 0x4312, (uint8_t)(src_data & 0xFF));
    bus_write8(0x00, 0x4313, (uint8_t)(src_data >> 8));
    bus_write8(0x00, 0x4314, bank);
    bus_write8(0x00, 0x4315, (uint8_t)(byte_count & 0xFF));
    bus_write8(0x00, 0x4316, (uint8_t)(byte_count >> 8));
    bus_write8(0x00, 0x4310, 0x02);
    bus_write8(0x00, 0x4311, 0x22);
    bus_write8(0x00, 0x420B, 0x02);
}

/*
 * $00:828C — Screen fade-in (0 to max brightness over 16 frames)
 */
static void mim_00828C(void) {
    op_sep(0x30);
    bus_wram_write8(g_cpu.DP + 0x01, 0x00);

    /* Clear DMA ring buffer before enabling NMI to prevent
     * $85C0 from processing garbage entries */
    bus_wram_write8(0x0014, 0);  /* read pointer */
    bus_wram_write8(0x0015, 0);  /* write pointer */

    bus_write8(0x00, 0x4200, 0x81);
    bus_wram_write8(g_cpu.DP + 0x00, 0x81);

    for (uint8_t x = 0; x < 0x10; x++) {
        mim_008249();
        bus_wram_write8(g_cpu.DP + 0x01, x);
    }
}

/*
 * $00:D3D2 — Shared game graphics loader
 */
static void mim_00D3D2(void) {
    uint8_t saved_db = g_cpu.DB;
    g_cpu.DB = 0x00;
    op_sep(0x20);
    op_rep(0x10);

    uint8_t param = CPU_A8();

    if (!(param & 0x80)) {
        op_rep(0x20);
        bus_wram_write16(g_cpu.DP + 0x5D, 0xDE50);
        bus_wram_write16(g_cpu.DP + 0x5F, 0x0091);
        g_cpu.C = 0x3800;
        mim_008781();

        op_sep(0x20);
        g_cpu.Y = 0x0000;
        g_cpu.X = 0xD482;
        CPU_SET_A8(0x80);
        mim_008F27();

        mim_00D18B();
    }

    op_rep(0x30);
    uint16_t tilemap_src = (param == 0) ? 0xD42E : 0xD454;
    g_cpu.X = tilemap_src;
    g_cpu.DB = 0x80;
    mim_00D11F();

    g_cpu.DB = saved_db;
}

/*
 * $00:9203 — SPC700 communication port write
 *
 * Sends a command to the SPC700 audio processor.
 * A = command byte, Y = data parameter.
 * Uses handshake protocol on APU ports $2140-$2143.
 */
void mim_009203(void) {
    op_sep(0x20);
    op_rep(0x10);

    /* Increment transfer-in-progress flag */
    uint8_t flag = bus_wram_read8(0x006A);
    bus_wram_write8(0x006A, flag + 1);

    /* The command is in A (8-bit), data in Y (16-bit) */
    op_xba();   /* swap command to high byte */

    /* Wait for SPC700 echo handshake */
    uint8_t expected = bus_wram_read8(0x0069);
    /* In the recomp, we just write the ports directly since
     * LakeSnes SPC700 runs alongside us */
    uint8_t echo = bus_read8(0x00, 0x2142);

    /* Increment echo value (skip $FF) */
    echo = expected + 1;
    if (echo == 0xFF) echo++;
    bus_wram_write8(0x0069, echo);

    /* Write data to APU ports 0-1 */
    op_rep(0x20);
    bus_write16(0x00, 0x2140, g_cpu.Y);

    /* Write command + echo to ports 2-3 */
    uint16_t cmd_echo = (CPU_A16() & 0xFF00) | echo;
    bus_write16(0x00, 0x2142, cmd_echo);

    op_sep(0x20);

    /* Decrement transfer flag */
    flag = bus_wram_read8(0x006A);
    bus_wram_write8(0x006A, flag - 1);
}

/*
 * $00:915A — SPC700 IPL upload routine (called via JSR, not JSL)
 *
 * Implements the standard SNES SPC700 boot protocol:
 *   1. Wait for IPL ready signal ($BBAA on ports $2140-$2141)
 *   2. For each block in the data stream:
 *      - Read 16-bit block size + 16-bit destination address
 *      - Upload bytes with echo handshake on port $2140
 *      - Handle bank boundary crossing
 *   3. Final block (size=0) sends execution start address
 *
 * Data format at source: [u16 size][u16 dest][size bytes]...
 * Terminated by a block with size=0 (dest becomes execution address).
 *
 * Parameters: src_addr = source address low (X), src_bank = bank byte (A)
 */

/*
 * SPC700 upload helper — reads sequential bytes from ROM,
 * tracking position across LoROM bank boundaries.
 */
typedef struct {
    uint16_t ptr_lo;
    uint8_t  ptr_bank;
    uint16_t y;
} SpcReadState;

static uint8_t spc_read_next(SpcReadState *s) {
    uint8_t b = bus_read8(s->ptr_bank, s->ptr_lo + s->y);
    s->y++;
    if ((uint32_t)(s->ptr_lo + s->y) > 0xFFFF) {
        s->y = 0;
        s->ptr_lo = 0x8000;
        s->ptr_bank++;
    }
    return b;
}

static uint16_t spc_read_next16(SpcReadState *s) {
    uint8_t lo = spc_read_next(s);
    uint8_t hi = spc_read_next(s);
    return (uint16_t)(lo | (hi << 8));
}

/*
 * Hybrid SPC700 upload: uses the real IPL handshake for the initial
 * boot, then writes data directly to SPC RAM for reliability.
 *
 * The polled byte-by-byte protocol has cycle-level timing dependencies
 * that are hard to satisfy in a recomp environment. Direct RAM writes
 * achieve the same result (data in SPC RAM) without the timing issues.
 */
static void mim_00915A(uint16_t src_addr, uint8_t src_bank) {
    SpcReadState rs;
    rs.ptr_lo = src_addr;
    rs.ptr_bank = src_bank;
    rs.y = 0;

    int max_spins = 200000;
    int spins;

    printf("mim: SPC700 IPL upload from $%02X:%04X\n", src_bank, src_addr);

    /* Wait for SPC700 ready signal ($BBAA on ports 0-1).
     * This works for both the IPL ROM and uploaded programs that
     * signal ready with the same $BBAA convention. */
    spins = 0;
    while (1) {
        bus_run_cycles(32);
        uint8_t lo = bus_read8(0x00, 0x2140);
        uint8_t hi = bus_read8(0x00, 0x2141);
        if (lo == 0xAA && hi == 0xBB) break;
        if (++spins > max_spins) {
            printf("mim: WARNING: SPC700 ready timeout, proceeding with direct write\n");
            break;
        }
    }

    /* Process blocks: read headers from ROM, write data directly to SPC RAM */
    while (1) {
        uint16_t block_size = spc_read_next16(&rs);
        uint16_t dest_addr = spc_read_next16(&rs);

        if (block_size == 0) {
            printf("mim: SPC700 upload done, execution at $%04X\n", dest_addr);

            /* Tell the SPC700 to jump to the execution address.
             * Write dest to ports 2-3, transfer flag=0 to port 1,
             * and a command byte to port 0. */
            bus_write8(0x00, 0x2142, (uint8_t)(dest_addr & 0xFF));
            bus_write8(0x00, 0x2143, (uint8_t)(dest_addr >> 8));
            bus_write8(0x00, 0x2141, 0x00);  /* transfer flag = 0 (execute) */
            bus_write8(0x00, 0x2140, 0xCC);  /* command */

            /* Give the SPC700 time to process the execute command,
             * start the program, and signal ready with $BBAA. */
            bus_run_cycles(17088);
            break;
        }

        printf("mim:   block: %u bytes -> SPC $%04X\n", block_size, dest_addr);

        /* Copy block data directly into SPC700 RAM */
        uint16_t spc_ptr = dest_addr;
        for (uint16_t i = 0; i < block_size; i++) {
            uint8_t byte = spc_read_next(&rs);
            bus_apu_write_ram(spc_ptr, byte);
            spc_ptr++;
        }
    }
}

/*
 * $00:90EA — Initial asset load
 *
 * Loads graphics/data from ROM banks $86-$87 via a transfer routine,
 * and sends initialization commands to the SPC700.
 */
void mim_0090EA(void) {
    printf("mim: loading initial assets ($00:90EA)\n");
    op_sep(0x20);
    op_rep(0x10);

    /* JSR $915A — upload from $86:8000 (main audio engine) */
    mim_00915A(0x8000, 0x86);

    /* JSR $915A — upload from $86:80DA (secondary data) */
    mim_00915A(0x80DA, 0x86);

    /* SPC700 command $0E (init audio engine) */
    op_sep(0x20);
    CPU_SET_A8(0x0E);
    g_cpu.Y = 0x0000;
    mim_009203();

    /* JSR $915A — upload from $87:EFB8 (audio sample data) */
    mim_00915A(0xEFB8, 0x87);

    /* Set game state: $6B = 2 (assets loaded) */
    op_rep(0x20);
    op_ldx_imm16(0x0002);
    bus_wram_write16(0x006B, g_cpu.X);

    /* SPC700 command $02 (start music) */
    op_sep(0x20);
    CPU_SET_A8(0x02);
    g_cpu.Y = 0x0000;
    mim_009203();
}

/*
 * $00:913A — Asset reload (audio data)
 *
 * Reloads audio data from $87:EFB8 and sends SPC700 commands.
 */
void mim_00913A(void) {
    op_sep(0x20);
    op_rep(0x10);

    /* SPC700 command $0E (re-init audio engine) */
    CPU_SET_A8(0x0E);
    g_cpu.Y = 0x0000;
    mim_009203();

    /* JSR $915A — reload audio data from $87:EFB8 */
    mim_00915A(0xEFB8, 0x87);

    /* SPC700 command $02 (restart music) */
    op_sep(0x20);
    CPU_SET_A8(0x02);
    g_cpu.Y = 0x0000;
    mim_009203();

    /* Set $6B = 2 (assets loaded) */
    op_rep(0x20);
    op_ldx_imm16(0x0002);
    bus_wram_write16(0x006B, g_cpu.X);
}

/*
 * $00:911A — Secondary asset load (audio data reload)
 *
 * Called from gameplay loop at $9B1F. Uploads audio data from $87:A31F
 * and sends SPC700 commands.
 */
void mim_00911A(void) {
    op_sep(0x20);
    op_rep(0x10);

    /* SPC700 command $0E */
    CPU_SET_A8(0x0E);
    g_cpu.Y = 0x0000;
    mim_009203();

    /* JSR $915A — upload from $87:A31F */
    mim_00915A(0xA31F, 0x87);

    /* SPC700 command $02 */
    op_sep(0x20);
    CPU_SET_A8(0x02);
    g_cpu.Y = 0x0000;
    mim_009203();

    /* Set $6B = 1 */
    op_rep(0x20);
    op_ldx_imm16(0x0001);
    bus_wram_write16(0x006B, g_cpu.X);
}

/* === Stub functions for not-yet-recompiled subroutines === */

/*
 * $00:D24B — Title screen / intro sequence
 *
 * Sets up Mode 1, loads title screen graphics, displays logo with
 * DMA transfers, waits for button press. Complex enough to need
 * its own recompilation pass.
 */
static void mim_00D24B(void) {
    printf("mim: title screen ($00:D24B)\n");

    uint8_t saved_db = g_cpu.DB;
    g_cpu.DB = 0x00;  /* PHK; PLB */

    op_sep(0x20);
    op_rep(0x10);

    /* JSL $80:8453 — DMA loader A */
    mim_008453();

    /* BG character base addresses: all zero */
    bus_write8(0x00, 0x210B, 0x00);
    bus_write8(0x00, 0x210C, 0x00);

    /* BG tilemap addresses: all at $6000 (word addr $3000) */
    bus_write8(0x00, 0x2107, 0x60);
    bus_write8(0x00, 0x2108, 0x60);
    bus_write8(0x00, 0x2109, 0x60);

    /* Mode 1 */
    bus_write8(0x00, 0x2105, 0x01);

    /* Enable BG3 on main screen */
    bus_write8(0x00, 0x212C, 0x04);

    /* Load compressed title graphics from $91:DE50 to VRAM $0800 */
    op_rep(0x20);
    bus_wram_write16(g_cpu.DP + 0x5D, 0xDE50);
    bus_wram_write16(g_cpu.DP + 0x5F, 0x0091);
    g_cpu.C = 0x0800;  /* A = VRAM dest word address */
    mim_008781();

    /* Load sprite tile data from $80:D482 to VRAM $0000 */
    op_sep(0x20);
    g_cpu.Y = 0x0000;
    g_cpu.X = 0xD482;
    CPU_SET_A8(0x80);
    mim_008F27();

    /* Wait one frame */
    mim_008249();

    /* Fill tilemap buffer at $7F:F000 */
    mim_00D18B();

    /* Write tilemap from data at $80:D2D6 */
    g_cpu.X = 0xD2D6;
    g_cpu.DB = 0x80;
    mim_00D11F();
    g_cpu.DB = 0x00;

    /* Set up DMA transfer table at $80:D3CA for tilemap → VRAM */
    bus_wram_write16(g_cpu.DP + 0x18, 0xD3CA);
    bus_wram_write8(g_cpu.DP + 0x1A, 0x80);
    mim_008506();

    /* $82:8DA7 — sprite positioning (stub: just wait a frame) */
    mim_008249();

    /* Enable display: set brightness to max ($0F), NMI handler writes this to $2100 */
    bus_wram_write8(g_cpu.DP + 0x01, 0x0F);

    /* Enable NMI */
    bus_wram_write8(g_cpu.DP + 0x00, 0x81);

    /* Display loop: wait up to 480 frames, exit on Start button */
    for (uint16_t x = 0; x < 0x01E0; x++) {
        g_cpu.X = x;
        mim_008249();

        if (x >= 0x00F0) {
            /* Check joypad at WRAM $38 for Start button (bit 4) */
            uint8_t joy = bus_wram_read8(g_cpu.DP + 0x38);
            if (joy & 0x10) break;
        }
    }

    /* Clean up */
    mim_0082A7();
    mim_008050();

    g_cpu.DB = saved_db;
}

/*
 * $00:D4A3 — Game state machine / world map logic
 *
 * Handles the world map, city selection, and game state transitions.
 * Very large routine with many subroutines.
 */
static void mim_00D4A3(void) {
    printf("mim: STUB $00:D4A3 (game state machine)\n");
    op_rep(0x30);

    /* Call $90AF for VBlank sync */
    mim_0090AF();

    /* Initialize some game state variables */
    bus_wram_write16(0x0E9F, 0x0000);
    bus_wram_write16(0x0EA1, 0x0000);

    /* The real $D4A3 sets up and runs the world map.
     * For now, just advance game state to loop back to title. */
    bus_wram_write16(0x058F, 0x0004);
}

/*
 * $01:8320 — State setup / transition
 *
 * Called with A=0 or A=1 to set up different game states.
 * Configures PPU mode, loads graphics, sets up tilemaps.
 */
static void mim_018320(void) {
    uint16_t param = CPU_A16() & 0x0001;
    printf("mim: $01:8320 state setup (param=%d)\n", param);

    op_rep(0x30);

    /* Save stack or $FFFF based on param */
    if (param == 1) {
        bus_wram_write16(0x12A5, 0xFFFF);
    } else {
        bus_wram_write16(0x12A5, g_cpu.S);
    }

    uint8_t saved_db = g_cpu.DB;

    op_sep(0x20);

    /* PPU init + DMA loaders + OAM init */
    mim_008050();
    g_cpu.DB = 0x01;  /* PHK; PLB for bank $01 */
    mim_008453();
    mim_008471();
    mim_00842C();
    mim_008249();

    /* Configure PPU for Mode 9 (Mode 1 + BG3 priority) */
    bus_write8(0x00, 0x2101, 0x63);  /* OBJ size + name base */
    bus_write8(0x00, 0x2105, 0x09);  /* Mode 1, BG3 priority */
    bus_write8(0x00, 0x2107, 0x00);  /* BG1 tilemap at $0000 */
    bus_write8(0x00, 0x2108, 0x04);  /* BG2 tilemap at $0400 */
    bus_write8(0x00, 0x2109, 0x0A);  /* BG3 tilemap at $0A00 */
    bus_write8(0x00, 0x212C, 0x17);  /* Main screen: OBJ+BG1+BG2+BG3 */
    bus_write8(0x00, 0x210B, 0x22);  /* BG1/2 tile base */
    bus_write8(0x00, 0x210C, 0x33);  /* BG3/4 tile base */

    /* Load BG graphics from $82:8DDB to VRAM $2000 */
    op_rep(0x30);
    bus_wram_write16(g_cpu.DP + 0x5D, 0x8DDB);
    bus_wram_write16(g_cpu.DP + 0x5F, 0x0082);
    g_cpu.C = 0x2000;
    mim_0087DF();

    /* Load more graphics from $89:FBD6 to VRAM $0400 */
    bus_wram_write16(g_cpu.DP + 0x5D, 0xFBD6);
    bus_wram_write16(g_cpu.DP + 0x5F, 0x0089);
    g_cpu.C = 0x0400;
    mim_008781();

    /* Load BG3 graphics from $81:8D41 to VRAM $6000 */
    bus_wram_write16(g_cpu.DP + 0x5D, 0x8D41);
    bus_wram_write16(g_cpu.DP + 0x5F, 0x0081);
    g_cpu.C = 0x6000;
    mim_0087DF();

    /* Load palettes from $84:FF72, $84:FF50, $84:FF2E */
    op_sep(0x20);
    g_cpu.X = 0xFF72;
    CPU_SET_A8(0x84);
    mim_008E9D();

    g_cpu.X = 0xFF50;
    CPU_SET_A8(0x84);
    mim_008E9D();

    g_cpu.X = 0xFF2E;
    CPU_SET_A8(0x84);
    mim_008E9D();

    if (param == 1) {
        /* Additional setup for param=1 */
        g_cpu.X = 0xFEEC;
        CPU_SET_A8(0x84);
        mim_008E9D();

        /* Load shared game graphics + tilemap */
        op_rep(0x20);
        g_cpu.C = 0x0000;
        mim_00D3D2();

        /* Transfer tilemap buffer ($7F:F000) to VRAM at BG3 tilemap ($0A00).
         * $D3D2 wrote text to WRAM, now DMA it to VRAM. */
        bus_write8(0x00, 0x2115, 0x80);  /* VMAIN: word increment */
        /* BG3 tilemap register $2109=$0A → bits 7-2 = 000010 → addr $0800 */
        bus_write8(0x00, 0x2116, 0x00);  /* VRAM addr = $0800 (BG3 tilemap) */
        bus_write8(0x00, 0x2117, 0x08);
        bus_write8(0x00, 0x4300, 0x01);  /* DMA mode: word, A→B */
        bus_write8(0x00, 0x4301, 0x18);  /* B-bus = VMDATAL */
        bus_write8(0x00, 0x4302, 0x00);  /* source = $7F:F000 */
        bus_write8(0x00, 0x4303, 0xF0);
        bus_write8(0x00, 0x4304, 0x7F);
        bus_write8(0x00, 0x4305, 0x00);  /* size = $0800 (2KB tilemap) */
        bus_write8(0x00, 0x4306, 0x08);
        bus_write8(0x00, 0x420B, 0x01);  /* trigger DMA ch0 */

        /* Only show BG3 (text layer) until BG1/2 tilemaps are properly loaded */
        bus_write8(0x00, 0x212C, 0x14);  /* BG3 + OBJ only */

        /* Screen fade-in */
        mim_00828C();

        /* Original has a 1200-frame display loop here. */
        op_rep(0x30);
        for (uint16_t x = 0; x < 0x04B0; x++) {
            mim_008249();
            if (x == 5) {
                uint8_t wram01 = bus_wram_read8(0x0001);
                uint8_t wram00 = bus_wram_read8(0x0000);
                uint8_t wram02 = bus_wram_read8(0x0002);
                printf("mim: frame 5: WRAM $00=%02X $01=%02X $02=%02X\n", wram00, wram01, wram02);
                snesrecomp_dump_ppu("mim_attract_debug.txt");
            }
            uint8_t joy = bus_wram_read8(g_cpu.DP + 0x38);
            if (joy & 0x10) break;
        }
    }

    /* Screen disable + cleanup */
    mim_0082A7();

    g_cpu.DB = saved_db;
    g_cpu.flag_C = 0;
}

/*
 * $02:8000 — Bank 2 entry point (game logic dispatcher)
 *
 * Handles game logic flow, sets up function pointers for
 * the main game state machine.
 */
static void mim_028000(void) {
    printf("mim: STUB $02:8000 (game logic dispatcher)\n");
    op_rep(0x30);

    /* Save stack pointer */
    bus_wram_write16(0x12A5, g_cpu.S);

    /* Check $058D for game state */
    uint16_t state = bus_wram_read16(0x058D);
    if (state != 0) {
        bus_wram_write16(0x1215, 0xFFFF);
    } else {
        bus_wram_write16(0x1215, 0x0000);
    }

    /* Clear carry (success) */
    g_cpu.flag_C = 0;
}

/* ($00:8BF5 is now implemented above in the Graphics section) */

/*
 * $00:9A5E — Main game loop
 *
 * This is the heart of the game. It runs forever (until quit),
 * with frame boundaries at mim_008249() calls.
 *
 * Flow:
 *   1. One-time init: load assets ($90EA), PPU init
 *   2. Title screen ($D24B)
 *   3. Game setup: SPC commands, state transitions
 *   4. Gameplay loop ($9B1F onwards)
 */
void mim_009A5E(void) {
    op_rep(0x30);
    g_cpu.DB = 0x00;  /* PHK; PLB */

    /* === One-time initialization === */

    /* JSL $80:90EA — load initial assets (SPC700 uploads) */
    mim_0090EA();

game_restart:
    /* JSL $80:8050 — PPU init */
    mim_008050();

    /* JSL $80:8453, $80:8471, $80:842C — DMA loaders, OAM init */
    mim_008453();
    mim_008471();
    mim_00842C();

    /* Check $6B: if != 2, reload assets */
    {
        uint16_t val_6b = bus_wram_read16(0x006B);
        if (val_6b != 0x0002) {
            mim_00913A();
        }
    }

    /* SPC700 command $12 with Y=$7F01 (set master volume?) */
    op_rep(0x30);
    CPU_SET_A8(0x12);
    g_cpu.Y = 0x7F01;
    mim_009203();

    /* SPC700 command $04 with Y=$0000 */
    CPU_SET_A8(0x04);
    g_cpu.Y = 0x0000;
    mim_009203();

    /* JSL $80:D24B — title screen */
    mim_00D24B();

    /* Store $0004 to $0591 (game state = title done) */
    op_rep(0x20);
    bus_wram_write16(0x0591, 0x0004);

title_loop:
    /* SPC700 command $12 with Y=$7F01 */
    op_rep(0x30);
    CPU_SET_A8(0x12);
    g_cpu.Y = 0x7F01;
    mim_009203();

    /* Wait 30 frames */
    for (uint16_t x = 0; x < 0x001E; x++) {
        g_cpu.X = x;
        mim_008249();
    }

    /* SPC700 command $06 with Y=$0007 (start title music?) */
    CPU_SET_A8(0x06);
    g_cpu.Y = 0x0007;
    mim_009203();

    /* JSL $81:8320 with A=0 (state setup) */
    g_cpu.C = 0x0000;
    mim_018320();

    /* If carry clear, call $82:8000 */
    if (!g_cpu.flag_C) {
        mim_028000();
    }

    /* JSL $81:8320 with A=1 */
    g_cpu.C = 0x0001;
    mim_018320();

    /* JSL $80:90AF — wait/sync */
    mim_0090AF();

    /* If carry set, loop back to title_loop */
    if (g_cpu.flag_C) {
        goto title_loop;
    }

    /* Load tilemap data via $8BF5 */
    op_rep(0x20);

    /* Source: $85:8000, dest WRAM $5000, bank $7F */
    bus_wram_write16(g_cpu.DP + 0x5D, 0x8000);
    bus_wram_write16(g_cpu.DP + 0x5F, 0x0085);
    g_cpu.X = 0x5000;
    g_cpu.C = 0x007F;
    mim_008BF5();

    /* Source: $85:890E, dest WRAM $6000, bank $7F */
    bus_wram_write16(g_cpu.DP + 0x5D, 0x890E);
    bus_wram_write16(g_cpu.DP + 0x5F, 0x0085);
    g_cpu.X = 0x6000;
    g_cpu.C = 0x007F;
    mim_008BF5();

    /* Set game state $058F = 3 */
    bus_wram_write16(0x058F, 0x0003);

    /* Reload audio assets */
    mim_00913A();

gameplay_loop:
    /* JSL $80:D4A3 — game state machine */
    mim_00D4A3();

    /* JSL $80:90AF — wait/sync */
    mim_0090AF();

    /* Check $058F: if == 4, restart from PPU init */
    {
        uint16_t state = bus_wram_read16(0x058F);
        if (state == 0x0004) {
            goto game_restart;
        }
    }

    /* === $9B1F — Gameplay loop === */

    /* Reload audio data */
    mim_00911A();

    /* Wait/sync */
    mim_0090AF();

    /* Read game state table index:
     * index = $0504 * 5 + $0593, lookup in table at $9B5E */
    {
        uint16_t val_0504 = bus_wram_read16(0x0504);
        uint16_t val_0593 = bus_wram_read16(0x0593);
        uint16_t index = (val_0504 * 5 + val_0593) * 2;

        /* Read state handler address from jump table at $9B5E */
        uint16_t handler = bus_read16(0x00, 0x9B5E + index);
        bus_wram_write16(0x0502, handler);

        printf("mim: gameplay state dispatch: $0504=%04X $0593=%04X handler=$%04X\n",
               val_0504, val_0593, handler);
    }

    /* TODO: Call the state-specific handler subroutines:
     * JSL $80:9BCE, $80:9B7C, $80:9D4B, $80:9D7F, $80:9E83
     * JSL $80:BE03, $81:9EB9
     * These implement the actual gameplay (world map, city,
     * Q&A, artifact collection, etc.)
     */

    /* Wait/sync and loop back to gameplay */
    mim_0090AF();

    /* Reload audio and loop back through $D4A3 */
    mim_00913A();
    goto gameplay_loop;
}
