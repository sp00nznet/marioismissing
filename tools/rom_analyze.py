#!/usr/bin/env python3
"""
Mario is Missing! (SNES) — ROM header analyzer and boot chain disassembler.
"""
import struct
import sys
import os
import glob

def find_rom():
    """Find the ROM file."""
    # Try common locations
    candidates = []
    for pattern in ["*.smc", "*.sfc", "../*.smc", "../*.sfc"]:
        candidates.extend(glob.glob(pattern))

    # Also check /tmp on Windows (via APPDATA)
    tmp = os.environ.get("TEMP", os.environ.get("TMP", ""))
    if tmp:
        for pattern in ["*.smc", "*.sfc"]:
            candidates.extend(glob.glob(os.path.join(tmp, "mim_rom", pattern)))

    if len(sys.argv) > 1:
        return sys.argv[1]

    for c in candidates:
        if "mario" in c.lower() or "missing" in c.lower():
            return c

    if candidates:
        return candidates[0]

    print("Usage: python rom_analyze.py <rom_path>")
    sys.exit(1)

def lorom_to_offset(bank, addr):
    """Convert LoROM bank:addr to ROM file offset."""
    return (bank & 0x7F) * 0x8000 + (addr - 0x8000)

def main():
    rom_path = find_rom()
    print(f"ROM: {rom_path}")

    with open(rom_path, "rb") as f:
        data = f.read()

    size = len(data)
    print(f"File size: {size} bytes (0x{size:X})")

    has_header = (size % 1024) == 512
    print(f"SMC header: {has_header}")

    offset = 512 if has_header else 0
    rom = data[offset:]
    print(f"ROM data: {len(rom)} bytes (0x{len(rom):X})")

    # LoROM header at 0x7FC0 (title starts here)
    print("\n=== LoROM Header (0x7FC0) ===")
    hdr = rom[0x7FC0:0x8000]
    title = hdr[0x00:0x15].decode("ascii", errors="replace").rstrip()
    print(f"  Title: [{title}]")

    map_mode = hdr[0x15]
    is_lorom = (map_mode & 0x01) == 0
    print(f"  Map mode: 0x{map_mode:02X} ({'LoROM' if is_lorom else 'HiROM'})")

    rom_type = hdr[0x16]
    print(f"  ROM type: 0x{rom_type:02X}")

    rom_size_exp = hdr[0x17]
    if rom_size_exp < 20:
        print(f"  ROM size: 0x{rom_size_exp:02X} ({1 << rom_size_exp} KB)")
    else:
        print(f"  ROM size: 0x{rom_size_exp:02X} (invalid)")

    sram_size = hdr[0x18]
    print(f"  SRAM size: 0x{sram_size:02X} ({1 << sram_size if sram_size else 0} KB)")

    region = hdr[0x19]
    regions = {0: "Japan", 1: "USA", 2: "Europe"}
    print(f"  Region: 0x{region:02X} ({regions.get(region, 'Unknown')})")

    dev_id = hdr[0x1A]
    print(f"  Developer ID: 0x{dev_id:02X}")

    version = hdr[0x1B]
    print(f"  Version: 1.{version}")

    comp = struct.unpack("<H", hdr[0x1C:0x1E])[0]
    cksum = struct.unpack("<H", hdr[0x1E:0x20])[0]
    print(f"  Checksum complement: 0x{comp:04X}")
    print(f"  Checksum: 0x{cksum:04X}")
    print(f"  Valid: {comp ^ cksum == 0xFFFF}")

    # Vectors at 0x7FE0-0x7FFF
    vecs = rom[0x7FE0:0x8000]
    print("\n=== Interrupt Vectors ===")
    # Native mode vectors (0x7FE4-0x7FEF)
    native_nmi = struct.unpack("<H", vecs[0x0A:0x0C])[0]
    native_irq = struct.unpack("<H", vecs[0x0E:0x10])[0]
    # Emulation mode vectors (0x7FF4-0x7FFF)
    emu_nmi = struct.unpack("<H", vecs[0x1A:0x1C])[0]
    emu_reset = struct.unpack("<H", vecs[0x1C:0x1E])[0]
    emu_irq = struct.unpack("<H", vecs[0x1E:0x20])[0]

    print(f"  NMI (native):  ${native_nmi:04X}")
    print(f"  IRQ (native):  ${native_irq:04X}")
    print(f"  NMI (emu):     ${emu_nmi:04X}")
    print(f"  RESET:         ${emu_reset:04X}")
    print(f"  IRQ (emu):     ${emu_irq:04X}")

    reset = emu_reset
    nmi = native_nmi

    # Disassemble from reset vector
    print(f"\n=== Disassembly: RESET vector ${reset:04X} ===")
    disasm(rom, 0x00, reset, 120)

    print(f"\n=== Disassembly: NMI handler ${nmi:04X} ===")
    disasm(rom, 0x00, nmi, 80)

def disasm(rom, bank, start_pc, max_insns):
    """Simple 65816 disassembler — enough to trace the boot chain."""
    pc = start_pc
    m_flag = True   # 8-bit A after reset
    x_flag = True   # 8-bit X/Y after reset

    for _ in range(max_insns):
        rom_off = lorom_to_offset(bank, pc)
        if rom_off < 0 or rom_off >= len(rom):
            print(f"  ${bank:02X}:{pc:04X}  (out of range)")
            break

        b = rom[rom_off]

        # Helper to read bytes
        def rb(off):
            o = rom_off + off
            return rom[o] if o < len(rom) else 0

        def rw(off):
            return rb(off) | (rb(off+1) << 8)

        def rl(off):
            return rb(off) | (rb(off+1) << 8) | (rb(off+2) << 16)

        def branch_target(off):
            v = rb(off)
            rel = v if v < 128 else v - 256
            return pc + 2 + rel

        # Decode instruction
        prefix = f"  ${bank:02X}:{pc:04X}  "

        # --- Implied / Accumulator ---
        implied = {
            0x78: "SEI", 0x58: "CLI", 0x18: "CLC", 0x38: "SEC",
            0xD8: "CLD", 0xF8: "SED", 0xB8: "CLV", 0xFB: "XCE",
            0xEB: "XBA", 0xEA: "NOP",
            0x1B: "TCS", 0x3B: "TSC", 0x5B: "TCD", 0x7B: "TDC",
            0xAA: "TAX", 0xA8: "TAY", 0x8A: "TXA", 0x98: "TYA",
            0x9A: "TXS", 0xBA: "TSX", 0x9B: "TXY", 0xBB: "TYX",
            0x48: "PHA", 0x68: "PLA", 0xDA: "PHX", 0xFA: "PLX",
            0x5A: "PHY", 0x7A: "PLY", 0x08: "PHP", 0x28: "PLP",
            0x4B: "PHK", 0xAB: "PLB", 0x0B: "PHD", 0x2B: "PLD",
            0x0A: "ASL A", 0x4A: "LSR A", 0x2A: "ROL A", 0x6A: "ROR A",
            0x1A: "INC A", 0x3A: "DEC A",
            0xCA: "DEX", 0x88: "DEY", 0xE8: "INX", 0xC8: "INY",
            0x60: "RTS", 0x6B: "RTL", 0x40: "RTI", 0xDB: "STP",
            0xCB: "WAI",
        }

        if b in implied:
            print(f"{prefix}{implied[b]}")

            # Track flag changes
            if b == 0xFB:  # XCE
                pass  # complex, assume native mode after CLC;XCE

            if b in (0x60, 0x6B, 0x40, 0xDB):  # RTS/RTL/RTI/STP
                break
            pc += 1
            continue

        # --- REP/SEP ---
        if b == 0xC2:  # REP
            v = rb(1)
            print(f"{prefix}REP #${v:02X}")
            if v & 0x20: m_flag = False
            if v & 0x10: x_flag = False
            pc += 2; continue
        if b == 0xE2:  # SEP
            v = rb(1)
            print(f"{prefix}SEP #${v:02X}")
            if v & 0x20: m_flag = True
            if v & 0x10: x_flag = True
            pc += 2; continue

        # --- Immediate (size depends on M/X flags) ---
        # LDA imm
        if b == 0xA9:
            if m_flag:
                print(f"{prefix}LDA #${rb(1):02X}")
                pc += 2
            else:
                print(f"{prefix}LDA #${rw(1):04X}")
                pc += 3
            continue
        # LDX imm
        if b == 0xA2:
            if x_flag:
                print(f"{prefix}LDX #${rb(1):02X}")
                pc += 2
            else:
                print(f"{prefix}LDX #${rw(1):04X}")
                pc += 3
            continue
        # LDY imm
        if b == 0xA0:
            if x_flag:
                print(f"{prefix}LDY #${rb(1):02X}")
                pc += 2
            else:
                print(f"{prefix}LDY #${rw(1):04X}")
                pc += 3
            continue
        # CMP imm
        if b == 0xC9:
            if m_flag:
                print(f"{prefix}CMP #${rb(1):02X}")
                pc += 2
            else:
                print(f"{prefix}CMP #${rw(1):04X}")
                pc += 3
            continue
        # CPX imm
        if b == 0xE0:
            if x_flag:
                print(f"{prefix}CPX #${rb(1):02X}")
                pc += 2
            else:
                print(f"{prefix}CPX #${rw(1):04X}")
                pc += 3
            continue
        # CPY imm
        if b == 0xC0:
            if x_flag:
                print(f"{prefix}CPY #${rb(1):02X}")
                pc += 2
            else:
                print(f"{prefix}CPY #${rw(1):04X}")
                pc += 3
            continue
        # ADC imm
        if b == 0x69:
            if m_flag:
                print(f"{prefix}ADC #${rb(1):02X}")
                pc += 2
            else:
                print(f"{prefix}ADC #${rw(1):04X}")
                pc += 3
            continue
        # SBC imm
        if b == 0xE9:
            if m_flag:
                print(f"{prefix}SBC #${rb(1):02X}")
                pc += 2
            else:
                print(f"{prefix}SBC #${rw(1):04X}")
                pc += 3
            continue
        # AND imm
        if b == 0x29:
            if m_flag:
                print(f"{prefix}AND #${rb(1):02X}")
                pc += 2
            else:
                print(f"{prefix}AND #${rw(1):04X}")
                pc += 3
            continue
        # ORA imm
        if b == 0x09:
            if m_flag:
                print(f"{prefix}ORA #${rb(1):02X}")
                pc += 2
            else:
                print(f"{prefix}ORA #${rw(1):04X}")
                pc += 3
            continue
        # EOR imm
        if b == 0x49:
            if m_flag:
                print(f"{prefix}EOR #${rb(1):02X}")
                pc += 2
            else:
                print(f"{prefix}EOR #${rw(1):04X}")
                pc += 3
            continue
        # BIT imm
        if b == 0x89:
            if m_flag:
                print(f"{prefix}BIT #${rb(1):02X}")
                pc += 2
            else:
                print(f"{prefix}BIT #${rw(1):04X}")
                pc += 3
            continue

        # --- Direct Page ---
        dp_ops = {
            0x85: "STA", 0x86: "STX", 0x84: "STY", 0x64: "STZ",
            0xA5: "LDA", 0xA6: "LDX", 0xA4: "LDY",
            0x65: "ADC", 0xE5: "SBC", 0x25: "AND", 0x05: "ORA", 0x45: "EOR",
            0xC5: "CMP", 0xE4: "CPX", 0xC4: "CPY",
            0x24: "BIT", 0x06: "ASL", 0x46: "LSR", 0x26: "ROL", 0x66: "ROR",
            0xE6: "INC", 0xC6: "DEC",
        }
        if b in dp_ops:
            print(f"{prefix}{dp_ops[b]} ${rb(1):02X}")
            pc += 2; continue

        # DP,X / DP,Y
        dpx_ops = {0x95: "STA", 0xB5: "LDA", 0x75: "ADC", 0xF5: "SBC",
                    0x35: "AND", 0x15: "ORA", 0x55: "EOR", 0xD5: "CMP",
                    0x94: "STY", 0xB4: "LDY", 0x74: "STZ",
                    0x96: "STX", 0xB6: "LDX"}
        if b in dpx_ops:
            idx = "Y" if b in (0x96, 0xB6) else "X"
            print(f"{prefix}{dpx_ops[b]} ${rb(1):02X},{idx}")
            pc += 2; continue

        # (DP) indirect
        if b == 0xB2:
            print(f"{prefix}LDA (${rb(1):02X})")
            pc += 2; continue
        if b == 0x92:
            print(f"{prefix}STA (${rb(1):02X})")
            pc += 2; continue

        # (DP,X) indexed indirect
        if b == 0xA1:
            print(f"{prefix}LDA (${rb(1):02X},X)")
            pc += 2; continue
        if b == 0x81:
            print(f"{prefix}STA (${rb(1):02X},X)")
            pc += 2; continue

        # (DP),Y indirect indexed
        if b == 0xB1:
            print(f"{prefix}LDA (${rb(1):02X}),Y")
            pc += 2; continue
        if b == 0x91:
            print(f"{prefix}STA (${rb(1):02X}),Y")
            pc += 2; continue

        # [DP] indirect long
        if b == 0xA7:
            print(f"{prefix}LDA [${rb(1):02X}]")
            pc += 2; continue
        if b == 0x87:
            print(f"{prefix}STA [${rb(1):02X}]")
            pc += 2; continue

        # [DP],Y indirect long indexed
        if b == 0xB7:
            print(f"{prefix}LDA [${rb(1):02X}],Y")
            pc += 2; continue
        if b == 0x97:
            print(f"{prefix}STA [${rb(1):02X}],Y")
            pc += 2; continue

        # --- Absolute ---
        abs_ops = {
            0x8D: "STA", 0x8E: "STX", 0x8C: "STY", 0x9C: "STZ",
            0xAD: "LDA", 0xAE: "LDX", 0xAC: "LDY",
            0x6D: "ADC", 0xED: "SBC", 0x2D: "AND", 0x0D: "ORA", 0x4D: "EOR",
            0xCD: "CMP", 0xEC: "CPX", 0xCC: "CPY",
            0x2C: "BIT", 0x0E: "ASL", 0x4E: "LSR", 0x2E: "ROL", 0x6E: "ROR",
            0xEE: "INC", 0xCE: "DEC", 0x1C: "TRB", 0x0C: "TSB",
        }
        if b in abs_ops:
            print(f"{prefix}{abs_ops[b]} ${rw(1):04X}")
            pc += 3; continue

        # Absolute,X / Absolute,Y
        absx_ops = {0x9D: "STA", 0xBD: "LDA", 0x7D: "ADC", 0xFD: "SBC",
                     0x3D: "AND", 0x1D: "ORA", 0x5D: "EOR", 0xDD: "CMP",
                     0xBC: "LDY", 0x9E: "STZ", 0x1E: "ASL", 0x5E: "LSR",
                     0x3E: "ROL", 0x7E: "ROR", 0xDE: "DEC", 0xFE: "INC"}
        if b in absx_ops:
            print(f"{prefix}{absx_ops[b]} ${rw(1):04X},X")
            pc += 3; continue

        absy_ops = {0x99: "STA", 0xB9: "LDA", 0x79: "ADC", 0xF9: "SBC",
                     0x39: "AND", 0x19: "ORA", 0x59: "EOR", 0xD9: "CMP",
                     0xBE: "LDX"}
        if b in absy_ops:
            print(f"{prefix}{absy_ops[b]} ${rw(1):04X},Y")
            pc += 3; continue

        # --- Long ---
        if b == 0x8F:
            print(f"{prefix}STA ${rl(1):06X}")
            pc += 4; continue
        if b == 0xAF:
            print(f"{prefix}LDA ${rl(1):06X}")
            pc += 4; continue
        if b == 0x6F:
            print(f"{prefix}ADC ${rl(1):06X}")
            pc += 4; continue
        if b == 0xEF:
            print(f"{prefix}SBC ${rl(1):06X}")
            pc += 4; continue
        if b == 0x2F:
            print(f"{prefix}AND ${rl(1):06X}")
            pc += 4; continue
        if b == 0x0F:
            print(f"{prefix}ORA ${rl(1):06X}")
            pc += 4; continue
        if b == 0x4F:
            print(f"{prefix}EOR ${rl(1):06X}")
            pc += 4; continue
        if b == 0xCF:
            print(f"{prefix}CMP ${rl(1):06X}")
            pc += 4; continue

        # Long,X
        if b == 0x9F:
            print(f"{prefix}STA ${rl(1):06X},X")
            pc += 4; continue
        if b == 0xBF:
            print(f"{prefix}LDA ${rl(1):06X},X")
            pc += 4; continue

        # --- Jumps ---
        if b == 0x4C:
            addr = rw(1)
            print(f"{prefix}JMP ${addr:04X}")
            pc = addr; continue
        if b == 0x5C:
            lo = rw(1); bk = rb(3)
            print(f"{prefix}JML ${bk:02X}:{lo:04X}")
            bank = bk; pc = lo; continue
        if b == 0x6C:
            print(f"{prefix}JMP (${rw(1):04X})")
            break  # can't follow indirect
        if b == 0x7C:
            print(f"{prefix}JMP (${rw(1):04X},X)")
            break
        if b == 0xDC:
            print(f"{prefix}JML [${rw(1):04X}]")
            break
        if b == 0x20:
            addr = rw(1)
            print(f"{prefix}JSR ${addr:04X}")
            pc += 3; continue
        if b == 0x22:
            lo = rw(1); bk = rb(3)
            print(f"{prefix}JSL ${bk:02X}:{lo:04X}")
            pc += 4; continue
        if b == 0xFC:
            print(f"{prefix}JSR (${rw(1):04X},X)")
            pc += 3; continue

        # --- Branches ---
        branch_ops = {
            0x80: "BRA", 0xD0: "BNE", 0xF0: "BEQ",
            0xB0: "BCS", 0x90: "BCC", 0x10: "BPL", 0x30: "BMI",
            0x50: "BVC", 0x70: "BVS",
        }
        if b in branch_ops:
            target = branch_target(1)
            print(f"{prefix}{branch_ops[b]} ${target:04X}")
            pc += 2; continue

        # BRL (16-bit relative)
        if b == 0x82:
            v = rw(1)
            rel = v if v < 0x8000 else v - 0x10000
            target = pc + 3 + rel
            print(f"{prefix}BRL ${target:04X}")
            pc += 3; continue

        # Block moves
        if b == 0x54:
            print(f"{prefix}MVN ${rb(1):02X},{rb(2):02X}")
            pc += 3; continue
        if b == 0x44:
            print(f"{prefix}MVP ${rb(1):02X},{rb(2):02X}")
            pc += 3; continue

        # PEA/PEI/PER
        if b == 0xF4:
            print(f"{prefix}PEA ${rw(1):04X}")
            pc += 3; continue
        if b == 0xD4:
            print(f"{prefix}PEI (${rb(1):02X})")
            pc += 2; continue
        if b == 0x62:
            print(f"{prefix}PER ${rw(1):04X}")
            pc += 3; continue

        # Stack relative
        if b == 0x63:
            print(f"{prefix}ADC ${rb(1):02X},S")
            pc += 2; continue
        if b == 0xA3:
            print(f"{prefix}LDA ${rb(1):02X},S")
            pc += 2; continue
        if b == 0x83:
            print(f"{prefix}STA ${rb(1):02X},S")
            pc += 2; continue
        if b == 0x23:
            print(f"{prefix}AND ${rb(1):02X},S")
            pc += 2; continue

        # (SR),Y
        if b == 0xB3:
            print(f"{prefix}LDA (${rb(1):02X},S),Y")
            pc += 2; continue
        if b == 0x93:
            print(f"{prefix}STA (${rb(1):02X},S),Y")
            pc += 2; continue

        # WDM (2 bytes)
        if b == 0x42:
            print(f"{prefix}WDM #${rb(1):02X}")
            pc += 2; continue

        # Fallback
        print(f"{prefix}.db ${b:02X}")
        pc += 1

if __name__ == "__main__":
    main()
