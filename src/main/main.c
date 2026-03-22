/*
 * Mario is Missing! — Static Recompilation
 *
 * Powered by snesrecomp (LakeSnes backend) for real SNES hardware.
 * Recompiled game code drives the hardware via bus_read8/bus_write8.
 */

#include <snesrecomp/snesrecomp.h>
#include "mim/functions.h"
#include "mim/cpu_ops.h"
#include <stdio.h>
#include <stdlib.h>

static const char *find_rom_path(int argc, char *argv[]) {
    if (argc >= 2) return argv[1];
    return "Mario is Missing! (U).sfc";
}

int main(int argc, char *argv[]) {
    printf("Mario is Missing! - Static Recompilation\n");
    printf("=========================================\n");
    printf("Powered by snesrecomp (LakeSnes backend)\n\n");

    /* Initialize snesrecomp (LakeSnes hardware + SDL2 platform) */
    if (!snesrecomp_init("Mario is Missing!", 3)) {
        fprintf(stderr, "Failed to initialize snesrecomp. Exiting.\n");
        return 1;
    }

    /* Load ROM (LakeSnes auto-detects LoROM, sets up cart mapping) */
    const char *rom_path = find_rom_path(argc, argv);
    printf("Loading ROM: %s\n", rom_path);
    if (!snesrecomp_load_rom(rom_path)) {
        fprintf(stderr, "Failed to load ROM. Exiting.\n");
        snesrecomp_shutdown();
        return 1;
    }
    printf("\n");

    /* Register all recompiled functions */
    mim_register_all();

    /* === Run the boot chain === */
    printf("--- Running boot chain ---\n");
    mim_reset();
    printf("--- Boot chain done ---\n\n");

    printf("Running... (press Escape to quit)\n\n");

    /* === Main frame loop === */
    while (snesrecomp_begin_frame()) {
        /* Run NMI handler */
        mim_nmi();

        /* Run one main loop iteration */
        mim_main_loop();

        /* Render PPU and present */
        snesrecomp_end_frame();
    }

    printf("Shutting down...\n");
    snesrecomp_shutdown();

    return 0;
}
