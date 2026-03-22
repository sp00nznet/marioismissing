/*
 * Mario is Missing! — Recompiled function declarations
 */
#ifndef MIM_FUNCTIONS_H
#define MIM_FUNCTIONS_H

/* Register all recompiled functions in the function table */
void mim_register_all(void);

/* === Boot chain === */
void mim_reset(void);       /* Reset vector */
void mim_hw_init(void);     /* Hardware init + main loop entry */
void mim_nmi(void);         /* NMI handler */
void mim_main_loop(void);   /* Main loop (one iteration) */

#endif /* MIM_FUNCTIONS_H */
