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

    printf("mim: registered 12 recompiled functions (24 with mirrors)\n");
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
 * Reads a data table from the 24-bit pointer at DP $18-$1A.
 * Each entry describes a DMA transfer to VRAM.
 * Table format (per entry):
 *   byte 0: VRAM increment mode (for $2115)
 *   byte 1-2: VRAM destination address
 *   byte 3-4: DMA transfer size
 *   byte 5-6: source address (within current bank)
 *   byte 7: source bank
 * Table ends when byte 0 == $00.
 *
 * TODO: This needs proper disassembly to confirm the exact table format.
 *       For now, we read the table from ROM and execute DMA transfers.
 */
void mim_008506(void) {
    op_sep(0x20);
    op_rep(0x10);

    uint16_t tbl_addr = bus_wram_read16(g_cpu.DP + 0x18);
    uint8_t tbl_bank = bus_wram_read8(g_cpu.DP + 0x1A);

    for (;;) {
        uint8_t vmain = bus_read8(tbl_bank, tbl_addr);
        if (vmain == 0x00) break;  /* end of table */

        uint16_t vram_dst = bus_read16(tbl_bank, tbl_addr + 1);
        uint16_t dma_size = bus_read16(tbl_bank, tbl_addr + 3);
        uint16_t src_addr = bus_read16(tbl_bank, tbl_addr + 5);
        uint8_t src_bank = bus_read8(tbl_bank, tbl_addr + 7);

        /* Set VRAM address and increment mode */
        bus_write8(0x00, 0x2115, vmain);        /* VMAIN */
        bus_write8(0x00, 0x2116, vram_dst & 0xFF);
        bus_write8(0x00, 0x2117, vram_dst >> 8);

        /* DMA channel 0: word write to VMDATAL/VMDATAH */
        bus_write8(0x00, 0x4300, 0x01);         /* 2-reg sequential */
        bus_write8(0x00, 0x4301, 0x18);         /* B-bus = $18 (VMDATAL) */
        bus_write8(0x00, 0x4302, src_addr & 0xFF);
        bus_write8(0x00, 0x4303, src_addr >> 8);
        bus_write8(0x00, 0x4304, src_bank);
        bus_write8(0x00, 0x4305, dma_size & 0xFF);
        bus_write8(0x00, 0x4306, dma_size >> 8);
        bus_write8(0x00, 0x420B, 0x01);         /* trigger DMA ch0 */

        tbl_addr += 8;  /* next entry */
    }
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

    /* Reentrance guard */
    uint16_t guard = bus_wram_read16(g_cpu.DP + 0x02);
    bus_wram_write16(g_cpu.DP + 0x02, guard + 1);
    if (guard != 0) {
        /* Already in NMI — skip, just decrement and return */
        bus_wram_write16(g_cpu.DP + 0x02, guard);
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
    uint16_t g = bus_wram_read16(0x0002);
    bus_wram_write16(0x0002, g - 1);
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
    uint16_t addr = bus_wram_read16(g_cpu.DP + 0x08);
    uint8_t bank = bus_wram_read8(g_cpu.DP + 0x0A);
    uint32_t full = ((uint32_t)bank << 16) | addr;

    if (full != 0) {
        func_table_call(full);
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
 * $00:90EA — Initial asset load
 *
 * Loads graphics/data from ROM banks $86-$87 via a transfer routine,
 * and sends initialization commands to the SPC700.
 */
void mim_0090EA(void) {
    printf("mim: loading initial assets ($00:90EA)\n");
    op_sep(0x20);
    op_rep(0x10);

    /* Transfer from $86:8000 */
    /* TODO: implement transfer subroutine at $00:915A */
    /* For now, the ROM data is already mapped by LakeSnes */

    /* SPC700 command $0E (likely: upload audio engine) */
    op_lda_imm8(0x0E);
    g_cpu.Y = 0x0000;
    /* mim_009203(); — skip for now, audio init needs more work */

    /* Transfer from $87:EFB8 */
    /* TODO: implement $00:915A */

    /* Set game state */
    op_rep(0x20);
    op_ldx_imm16(0x0002);
    bus_wram_write16(0x006B, g_cpu.X);

    /* SPC700 command $02 (likely: start music) */
    op_lda_imm8(0x02);
    g_cpu.Y = 0x0000;
    /* mim_009203(); */
}

/*
 * $00:913A — Asset reload (audio data)
 *
 * Reloads audio data from $87:EFB8 and sends SPC700 commands.
 */
void mim_00913A(void) {
    op_sep(0x20);
    op_rep(0x10);

    /* SPC700 command $0E */
    /* TODO: needs $915A transfer routine */

    /* SPC700 command $02 */

    /* Set $6B = 2 */
    op_rep(0x20);
    op_ldx_imm16(0x0002);
    bus_wram_write16(0x006B, g_cpu.X);
}

/*
 * $00:9A5E — Main loop entry / game state dispatcher
 *
 * This is the main game loop. Called once per frame from main.c.
 * In the original ROM, this is an infinite loop. In the recomp,
 * each call processes one frame.
 *
 * TODO: Recompile the full main loop logic (JSL calls to
 *       $80:8050, $80:8453, $80:8471, $80:842C, state dispatch, etc.)
 */
void mim_009A5E(void) {
    /* The main loop calls many subroutines per frame.
     * This will be filled in as we recompile more game logic.
     * For now, the NMI handler runs the DMA transfers each frame. */
}
