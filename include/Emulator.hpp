#pragma once
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <format>
#include <fstream>
#include <iostream>
#include <ostream>
#include <stdexcept>
#include <sys/types.h>
#include <vector>

#include "Tracelogger.cpp"

class Emulator {
  public:
    Emulator() {
        ProgramCounter = 0;
        A = 0;
        X = 0;
        Y = 0;
        CpuHalted = false;
        RAM.assign(0x0800, 0);
    }

    void reset(const char *rom_filename) {
        std::ifstream file(rom_filename, std::ios::binary | std::ios::ate);
        if (!file)
            throw std::runtime_error("Failed to open the ROM.");

        std::streamsize size = file.tellg();
        file.seekg(0, std::ios::beg);

        std::vector<uint8_t> buffer(static_cast<size_t>(size));
        if (!file.read(reinterpret_cast<char *>(buffer.data()), size))
            throw std::runtime_error("Failed to read the ROM.");

        std::vector<uint8_t> header(buffer.begin(), buffer.begin() + 16);
        std::vector<uint8_t> rest(buffer.begin() + 16, buffer.end());

        INesHeader = header;
        ROM = rest;

        uint8_t PCL = read(0xFFFC);
        uint8_t PCH = read(0xFFFD);

        ProgramCounter =
            static_cast<uint16_t>((static_cast<uint16_t>(PCH) << 8) | PCL);
        stackPointer = 0xFD;

        flag_InterruptDisable = true;

        run();
    }

    uint8_t read(const uint16_t addr) const {
        if (addr <= 0x1FFF) {
            return RAM[addr & 0x07FF];
        }
        if (addr >= 0x8000) {
            const auto romIndex = static_cast<uint32_t>(addr - 0x8000);
            if (romIndex < ROM.size())
                return ROM[romIndex];
            return 0;
        }
        return 0x0;
    }

    void write(uint16_t addr, uint8_t value) { RAM[addr & 0x07FF] = value; }

    void push(uint8_t value) {
        write(static_cast<uint16_t>(0x100 + stackPointer), value);
        stackPointer--;
    }

    uint8_t pull() {
        stackPointer++;
        return read(static_cast<uint16_t>(0x100 + stackPointer));
    }

    void run() {
        while (!CpuHalted) {
            emulate_cpu();
        }
        return;
    }

    void readAbsolute(uint16_t *addr) {
        *addr = read(ProgramCounter);
        ProgramCounter++;
        *addr = static_cast<uint16_t>(read(ProgramCounter) << 8 | *addr);
        ProgramCounter++;
    }

    void readAbsoluteIndexed(uint16_t *addr, uint8_t reg) {
        *addr = read(ProgramCounter);
        ProgramCounter++;
        *addr = static_cast<uint16_t>(read(ProgramCounter) << 8 | *addr);
        ProgramCounter++;
        *addr += reg;
    }

    void readZeroPage(uint8_t *addr) {
        *addr = read(ProgramCounter);
        ProgramCounter++;
    }

    void flagZN(uint8_t *reg) {
        flag_Zero = *reg == 0;
        flag_Negative = *reg > 127;
    }

    void emulate_cpu() {
        int cycles = 0;

        uint8_t addr;
        uint8_t addr_low;
        uint8_t addr_high;
        uint16_t addr_abs;
        uint8_t value;
        int sum;
        bool oldCarry;

        uint8_t opcode = read(ProgramCounter);
        ProgramCounter++;

        switch (opcode) {
        case 0x02: // HTL - Unofficial Instruction
            CpuHalted = true;
            break;

        case 0xEA: // NOP
            cycles = 2;
            break;

            /*
             * 	Load Instructions
             */

        case 0xA9: // LDA Immediate
            A = read(ProgramCounter);
            flagZN(&A);
            ProgramCounter++;
            cycles = 2;
            break;
        case 0xA5: // LDA Zero Page
            addr = read(ProgramCounter);
            ProgramCounter++;
            A = read(addr);
            flagZN(&A);
            cycles = 3;
            break;
        case 0xAD: // LDA Absolute
            readAbsolute(&addr_abs);
            A = read(addr_abs);
            flagZN(&A);
            cycles = 4;
            break;
        case 0xBD: // LDA Absolute,X
            readAbsoluteIndexed(&addr_abs, X);
            A = read(addr_abs);
            flagZN(&A);
            cycles = 4;
            break;
        case 0xB9: // LDA Absolute,Y
            readAbsoluteIndexed(&addr_abs, Y);
            A = read(addr_abs);
            flagZN(&A);
            cycles = 4;
            break;

        case 0xA2: // LDX Immediate
            X = read(ProgramCounter);
            flagZN(&X);
            ProgramCounter++;
            cycles = 2;
            break;
        case 0xA6: // LDX Zero Page
            addr = read(ProgramCounter);
            ProgramCounter++;
            X = read(addr);
            flagZN(&X);
            cycles = 3;
            break;
        case 0xAE: // LDX Absolute
            readAbsolute(&addr_abs);
            X = read(static_cast<uint16_t>(addr_high * 256 + addr_low));
            flagZN(&X);
            cycles = 4;
            break;
        case 0xBE: // LDX Absolute,Y
            readAbsoluteIndexed(&addr_abs, Y);
            X = read(static_cast<uint16_t>(addr_high * 256 + addr_low));
            flagZN(&X);
            cycles = 4;
            break;

        case 0xA0: // LDY Immediate
            Y = read(ProgramCounter);
            flagZN(&Y);
            ProgramCounter++;
            cycles = 2;
            break;
        case 0xA4: // LDY Zero Page
            addr = read(ProgramCounter);
            ProgramCounter++;
            Y = read(addr);
            flagZN(&Y);
            cycles = 3;
            break;
        case 0xAC: // LDY Absolute
            readAbsolute(&addr_abs);
            Y = read(static_cast<uint16_t>(addr_high * 256 + addr_low));
            flagZN(&Y);
            cycles = 4;
            break;
        case 0xBC: // LDY Absolute,X
            readAbsoluteIndexed(&addr_abs, X);
            Y = read(static_cast<uint16_t>(addr_high * 256 + addr_low));
            flagZN(&Y);
            cycles = 4;
            break;

            /*
             * Store Instructions
             */

        case 0x85: // STA Zero Page
            addr = read(ProgramCounter);
            ProgramCounter++;
            write(addr, A);
            cycles = 3;
            break;
        case 0x8D: // STA Absolute
            readAbsolute(&addr_abs);
            write(addr_abs, A);
            cycles = 4;
            break;
        case 0x9D: // STA Absolute,X
            readAbsoluteIndexed(&addr_abs, X);
            write(addr_abs, A);
            cycles = 4;
            break;
        case 0x99: // STA Absolute,Y
            readAbsoluteIndexed(&addr_abs, Y);
            write(addr_abs, A);
            cycles = 4;
            break;

        case 0x86: // STX Zero Page
            addr = read(ProgramCounter);
            ProgramCounter++;
            write(addr, X);
            cycles = 3;
            break;
        case 0x8E: // STX Absolute
            readAbsolute(&addr_abs);
            write(addr_abs, X);
            cycles = 4;
            break;

        case 0x84: // STY Zero Page
            addr = read(ProgramCounter);
            ProgramCounter++;
            write(addr, Y);
            cycles = 3;
            break;
        case 0x8C: // STY Absolute
            readAbsolute(&addr_abs);
            write(addr_abs, Y);
            cycles = 4;
            break;

        /*
         * Branch Instructions
         */
        case 0x10: // BPL (Branch on PLus)
            addr = read(ProgramCounter);
            ProgramCounter++;
            if (!flag_Negative) {
                auto signedVal = static_cast<int8_t>(addr);
                ProgramCounter =
                    static_cast<uint16_t>(ProgramCounter + signedVal);
                cycles = 3;
            } else {
                cycles = 2;
            }
            break;

        case 0x30: // BMI (Branch on MInus)
            addr = read(ProgramCounter);
            ProgramCounter++;
            if (flag_Negative) {
                auto signedVal = static_cast<int8_t>(addr);
                ProgramCounter =
                    static_cast<uint16_t>(ProgramCounter + signedVal);
                cycles = 3;
            } else {
                cycles = 2;
            }
            break;

        case 0x50: // BVC (Branch on oVerflow Clear)
            addr = read(ProgramCounter);
            ProgramCounter++;
            if (!flag_Overflow) {
                auto signedVal = static_cast<int8_t>(addr);
                ProgramCounter =
                    static_cast<uint16_t>(ProgramCounter + signedVal);
                cycles = 3;
            } else {
                cycles = 2;
            }
            break;

        case 0x70: // BVS (Branch on oVerflow Set)
            addr = read(ProgramCounter);
            ProgramCounter++;
            if (flag_Overflow) {
                auto signedVal = static_cast<int8_t>(addr);
                ProgramCounter =
                    static_cast<uint16_t>(ProgramCounter + signedVal);
                cycles = 3;
            } else {
                cycles = 2;
            }
            break;

        case 0x90: // BCC (Branch on Carry Clear)
            addr = read(ProgramCounter);
            ProgramCounter++;
            if (!flag_Carry) {
                auto signedVal = static_cast<int8_t>(addr);
                ProgramCounter =
                    static_cast<uint16_t>(ProgramCounter + signedVal);
                cycles = 3;
            } else {
                cycles = 2;
            }
            break;

        case 0xB0: // BCS (Branch on Carry Set)
            addr = read(ProgramCounter);
            ProgramCounter++;
            if (flag_Carry) {
                auto signedVal = static_cast<int8_t>(addr);
                ProgramCounter =
                    static_cast<uint16_t>(ProgramCounter + signedVal);
                cycles = 3;
            } else {
                cycles = 2;
            }
            break;

        case 0xD0: // BNE (Branch on Not Equal)
            addr = read(ProgramCounter);
            ProgramCounter++;
            if (!flag_Zero) {
                auto signedVal = static_cast<int8_t>(addr);
                ProgramCounter =
                    static_cast<uint16_t>(ProgramCounter + signedVal);
                cycles = 3;
            } else {
                cycles = 2;
            }
            break;

        case 0xF0: // BEQ (Branch on EQual)
            addr = read(ProgramCounter);
            ProgramCounter++;
            if (flag_Zero) {
                const auto signedVal = static_cast<int8_t>(addr);
                ProgramCounter =
                    static_cast<uint16_t>(ProgramCounter + signedVal);
                cycles = 3;
            } else {
                cycles = 2;
            }
            break;

            /*
             * Stack Instructions
             */

        case 0x48: // PHA
            push(A);
            cycles = 3;
            break;

        case 0x68: // PLA
            A = pull();
            flag_Zero = A == 0;
            flag_Negative = A >= 0x80;
            cycles = 4;
            break;

        case 0x9A: // TXS - Transfer X to Stack Pointer
            stackPointer = X;
            cycles = 2;
            break;

        case 0xBA: // TSX - Transfer Stack Pointer to X
            X = stackPointer;
            flagZN(&X);
            cycles = 2;
            break;

            /*
             * Subroutine operations
             */

        case 0x20: // JSR
            addr_low = read(ProgramCounter);
            ProgramCounter++;
            addr_high = read(ProgramCounter);
            push(static_cast<uint8_t>(ProgramCounter / 256));
            push(static_cast<uint8_t>(ProgramCounter));
            ProgramCounter = static_cast<uint16_t>(addr_high * 256 + addr_low);
            cycles = 6;
            break;

        case 0x60: // RTS
            addr_low = pull();
            addr_high = pull();
            ProgramCounter = static_cast<uint16_t>(addr_high * 256 + addr_low);
            ProgramCounter++;
            cycles = 6;
            break;

        case 0x4C: // JMP
            addr_low = read(ProgramCounter);
            ProgramCounter++;
            addr_high = read(ProgramCounter);
            ProgramCounter = static_cast<uint16_t>(addr_high * 256 + addr_low);
            cycles = 3;
            break;

        case 0x6C: // JMP - Indirect
        	addr_low = read(ProgramCounter);
        	ProgramCounter++;
        	addr_high = read(ProgramCounter);
        	ProgramCounter = read(static_cast<uint16_t>(addr_high * 256 + addr_low));
         	cycles = 5;
          	break;

            /*
             * Register Instructions
             */

        case 0xE8: // INX - Increment X
            X++;
            flagZN(&X);
            cycles = 2;
            break;

        case 0xC8: // INY - Increment Y
            Y++;
            flagZN(&Y);
            cycles = 2;
            break;

        case 0xCA: // DEX - Decrement X
            X--;
            flagZN(&X);
            cycles = 2;
            break;

        case 0x88: // DEY - Decrement Y
            Y--;
            flagZN(&Y);
            cycles = 2;
            break;

        case 0xAA: // TAX - Transfer A to X
            X = A;
            flagZN(&X);
            cycles = 2;
            break;

        case 0x8A: // TXA - Transfer X to A
            A = X;
            flag_Zero = A == 0;
            flag_Negative = A >= 0x80;
            cycles = 2;
            break;

        case 0xA8: // TAY - Transfer A to Y
            Y = A;
            flagZN(&Y);
            cycles = 2;
            break;

        case 0x98: // TYA - Transfer Y to A
            A = Y;
            flag_Zero = A == 0;
            flag_Negative = A >= 0x80;
            cycles = 2;
            break;

        case 0x0A: // ASL - Arithmetic Shift Left
            flag_Carry = A >= 0x80;
            A <<= 1;
            flag_Zero = A == 0;
            flag_Negative = A > 127;
            cycles = 2;
            break;
        case 0x0E: // ASL Absolute
            readAbsolute(&addr_abs);
            addr = read(addr_abs);
            flag_Carry = addr >= 0x80;
            addr <<= 1;
            flag_Zero = addr == 0;
            flag_Negative = addr > 127;
            write(addr_abs, addr);
            cycles = 6;
            break;
        case 0x1E: // ASL Absolute,X
            readAbsoluteIndexed(&addr_abs, X);
            addr = read(addr_abs);
            flag_Carry = addr >= 0x80;
            addr <<= 1;
            flag_Zero = addr == 0;
            flag_Negative = addr > 127;
            write(addr_abs, addr);
            cycles = 6;
            break;
        case 0x06:                   // ASL Zero Page
            readZeroPage(&addr_low); // We use low and high to have two
                                     // different addresses
            addr_high = read(addr_low);
            flag_Carry = addr >= 0x80;
            addr_high <<= 1;
            flag_Zero = addr == 0;
            flag_Negative = addr > 127;
            write(addr_low, addr_high);
            cycles = 6;
            break;

        case 0x2A: // ROL - ROtate Left (Accumulator)
            oldCarry = flag_Carry;
            flag_Carry = A >= 0x80;
            A <<= 1;
            if (oldCarry) {
                A |= 1;
            }
            flagZN(&A);
            cycles = 2;
            break;
        case 0x26: // ROL Zero Page
            readZeroPage(&addr);
            value = read(addr);
            oldCarry = flag_Carry;
            flag_Carry = value >= 0x80;
            value <<= 1;
            if (oldCarry) {
                value |= 1;
            }
            write(addr, value);
            flagZN(&A);
            cycles = 6;
            break;
        case 0x2E: // ROL Absolute
            readAbsolute(&addr_abs);
            value = read(addr_abs);
            oldCarry = flag_Carry;
            flag_Carry = value >= 0x80;
            value <<= 1;
            if (oldCarry) {
                value |= 1;
            }
            write(addr_abs, value);
            flagZN(&A);
            cycles = 6;
            break;
        case 0x3E: // ROL Absolute,X
            readAbsoluteIndexed(&addr_abs, X);
            value = read(addr_abs);
            oldCarry = flag_Carry;
            flag_Carry = value >= 0x80;
            value <<= 1;
            if (oldCarry) {
                value |= 1;
            }
            write(addr_abs, value);
            flagZN(&A);
            cycles = 6;
            break;

        case 0x4A: // LSR - Logical Shift Right
            flag_Carry = !(A % 2);
            A >>= 1;
            flag_Zero = A == 0;
            flag_Negative = A > 127;
            cycles = 2;
            break;
        case 0x4E: // LSR Absolute
            readAbsolute(&addr_abs);
            addr = read(addr_abs);
            flag_Carry = !(addr % 2);
            addr >>= 1;
            flag_Zero = addr == 0;
            flag_Negative = addr > 127;
            write(addr_abs, addr);
            cycles = 6;
            break;
        case 0x5E: // LSR Absolute,X
            readAbsoluteIndexed(&addr_abs, X);
            addr = read(addr_abs);
            flag_Carry = !(addr % 2);
            addr >>= 1;
            flag_Zero = addr == 0;
            flag_Negative = addr > 127;
            write(addr_abs, addr);
            cycles = 6;
            break;
        case 0x46:                   // LSR Zero Page
            readZeroPage(&addr_low); // We use low and high to have two
                                     // different addresses
            addr_high = read(addr_low);
            flag_Carry = !(addr % 2);
            addr_high >>= 1;
            flag_Zero = addr == 0;
            flag_Negative = addr > 127;
            write(addr_low, addr_high);
            cycles = 6;
            break;

        case 0x6A: // ROR - ROtate Right (Accumulator)
            oldCarry = flag_Carry;
            flag_Carry = !(A % 2);
            A >>= 1;
            if (oldCarry) {
                A |= 0x80;
            }
            flagZN(&A);
            cycles = 2;
            break;
        case 0x66: // ROR Zero Page
            readZeroPage(&addr);
            value = read(addr);
            oldCarry = flag_Carry;
            flag_Carry = !(value % 2);
            value >>= 1;
            if (oldCarry) {
                value |= 0x80;
            }
            write(addr, value);
            flagZN(&A);
            cycles = 6;
            break;
        case 0x6E: // ROR Absolute
            readAbsolute(&addr_abs);
            value = read(addr_abs);
            oldCarry = flag_Carry;
            flag_Carry = !(value % 2);
            value >>= 1;
            if (oldCarry) {
                value |= 0x80;
            }
            write(addr_abs, value);
            flagZN(&A);
            cycles = 6;
            break;
        case 0x7E: // ROR Absolute,X
            readAbsoluteIndexed(&addr_abs, X);
            value = read(addr_abs);
            oldCarry = flag_Carry;
            flag_Carry = !(value % 2);
            value >>= 1;
            if (oldCarry) {
                value |= 0x80;
            }
            write(addr_abs, value);
            flagZN(&A);
            cycles = 6;
            break;

            /*
             * Increment / Decrement
             */

        case 0xE6: // INC Zero Page - Increment
            readZeroPage(&addr);
            value = read(addr);
            value++;
            write(addr, value);
            flagZN(&value); // WARNING : MUST TEST
            cycles = 5;
            break;

        case 0xEE: // INC Absolute
            readAbsolute(&addr_abs);
            value = read(addr_abs);
            value++;
            write(addr_abs, value);
            flagZN(&value);
            cycles = 6;
            break;

        case 0xFE: // INC Absolute,X
            readAbsoluteIndexed(&addr_abs, X);
            value = read(addr_abs);
            value++;
            write(addr_abs, value);
            flagZN(&value);
            cycles = 6;
            break;

        case 0xC6: // DEC Zero Page - Decrement
            readZeroPage(&addr);
            value = read(addr);
            value--;
            write(addr, value);
            flagZN(&value); // WARNING : MUST TEST
            cycles = 5;
            break;

        case 0xCE: // DEC Absolute
            readAbsolute(&addr_abs);
            value = read(addr_abs);
            value--;
            write(addr_abs, value);
            flagZN(&value);
            cycles = 6;
            break;

        case 0xDE: // DEC Absolute,X
            readAbsoluteIndexed(&addr_abs, X);
            value = read(addr_abs);
            value--;
            write(addr_abs, value);
            flagZN(&value);
            cycles = 6;
            break;

            /*
             * Stack Processor Flags
             */

        case 0x08: // PHP - Push Processor Flags
            addr = 0;
            addr += static_cast<uint8_t>(flag_Carry ? 1 : 0);
            addr += static_cast<uint8_t>(flag_Zero ? 2 : 0);
            addr += static_cast<uint8_t>(flag_InterruptDisable ? 4 : 0);
            addr += static_cast<uint8_t>(flag_Decimal ? 8 : 0);
            addr += 0x10;
            addr += 0x20;
            addr += static_cast<uint8_t>(flag_Overflow ? 0x40 : 0);
            addr += static_cast<uint8_t>(flag_Negative ? 0x80 : 0);
            push(addr);
            cycles = 3;
            break;

        case 0x28: // PLP - Pull Processor Flags
            addr = pull();
            flag_Carry = (addr & 1) != 0;
            flag_Zero = (addr & 2) != 0;
            flag_InterruptDisable = (addr & 4) != 0;
            flag_Decimal = (addr & 8) != 0;
            flag_Overflow = (addr & 0x40) != 0;
            flag_Negative = (addr & 0x80) != 0;
            cycles = 3;
            break;

        case 0x09: // ORA - OR Accumulator
            value = read(ProgramCounter);
            ProgramCounter++;
            A |= value;
            flagZN(&A);
            cycles = 2;
            break;

        case 0x05: // ORA Zero Page
            readZeroPage(&addr);
            value = read(addr);
            A |= value;
            flagZN(&A);
            cycles = 3;
            break;

        case 0x0D: // ORA Absolute
            readAbsolute(&addr_abs);
            value = read(addr_abs);
            A |= value;
            flagZN(&A);
            cycles = 4;
            break;
        case 0x1D: // ORA Absolute,X
            readAbsoluteIndexed(&addr_abs, X);
            value = read(addr_abs);
            A |= value;
            flagZN(&A);
            cycles = 4;
            break;
        case 0x19: // ORA Absolute,Y
            readAbsoluteIndexed(&addr_abs, Y);
            value = read(addr_abs);
            A |= value;
            flagZN(&A);
            cycles = 4;
            break;

        case 0x29: // AND - AND Accumulator
            value = read(ProgramCounter);
            ProgramCounter++;
            A &= value;
            flagZN(&A);
            cycles = 2;
            break;

        case 0x25: // AND Zero Page
            readZeroPage(&addr);
            value = read(addr);
            A &= value;
            flagZN(&A);
            cycles = 3;
            break;

        case 0x2D: // AND Absolute
            readAbsolute(&addr_abs);
            value = read(addr_abs);
            A &= value;
            flagZN(&A);
            cycles = 4;
            break;
        case 0x3D: // AND Absolute,X
            readAbsoluteIndexed(&addr_abs, X);
            value = read(addr_abs);
            A &= value;
            flagZN(&A);
            cycles = 4;
            break;
        case 0x39: // AND Absolute,Y
            readAbsoluteIndexed(&addr_abs, Y);
            value = read(addr_abs);
            A &= value;
            flagZN(&A);
            cycles = 4;
            break;

        case 0x49: // EOR - XOR Accumulator
            value = read(ProgramCounter);
            ProgramCounter++;
            A ^= value;
            flagZN(&A);
            cycles = 2;
            break;

        case 0x45: // EOR Zero Page
            readZeroPage(&addr);
            value = read(addr);
            A ^= value;
            flagZN(&A);
            cycles = 3;
            break;

        case 0x4D: // EOR Absolute
            readAbsolute(&addr_abs);
            value = read(addr_abs);
            A ^= value;
            flagZN(&A);
            cycles = 4;
            break;

        case 0x5D: // EOR Absolute,X
            readAbsoluteIndexed(&addr_abs, X);
            value = read(addr_abs);
            A ^= value;
            flagZN(&A);
            cycles = 4;
            break;

        case 0x59: // EOR Absolute,Y
            readAbsoluteIndexed(&addr_abs, Y);
            value = read(addr_abs);
            A ^= value;
            flagZN(&A);
            cycles = 4;
            break;

        case 0x69: // ADC Immediate
            value = read(ProgramCounter);
            ProgramCounter++;
            opADC(value);
            cycles = 2;
            break;
        case 0x6D: // ADC Absolute
            readAbsolute(&addr_abs);
            value = read(addr_abs);
            opADC(value);
            cycles = 2;
            break;
        case 0x7D: // ADC Absolute,X
            readAbsoluteIndexed(&addr_abs, X);
            value = read(addr_abs);
            opADC(value);
            cycles = 2;
            break;
        case 0x79: // ADC Absolute,Y
            readAbsoluteIndexed(&addr_abs, Y);
            value = read(addr_abs);
            opADC(value);
            cycles = 2;
            break;

        case 0xE9: // SBC Immediate
            value = read(ProgramCounter);
            ProgramCounter++;
            opSBC(value);
            cycles = 2;
            break;
        case 0xED: // SBC Absolute
            readAbsolute(&addr_abs);
            value = read(addr_abs);
            opSBC(value);
            cycles = 3;
            break;
        case 0xFD: // SBC Absolute,X
            readAbsoluteIndexed(&addr_abs,X);
            value = read(addr_abs);
            opSBC(value);
            cycles = 3;
            break;
        case 0xF9: // SBC Absolute,Y
            readAbsoluteIndexed(&addr_abs, Y);
            value = read(addr_abs);
            opSBC(value);
            cycles = 3;
            break;

        case 0xC9: // CMP Immediate
            value = read(ProgramCounter);
            ProgramCounter++;
            opCMP(value, A);
            cycles = 2;
            break;

        case 0xCD: // CMP Absolute
            readAbsolute(&addr_abs);
            value = read(addr_abs);
            opCMP(value, A);
            cycles = 2;
            break;
        case 0xDD: // CMP Absolute,X
            readAbsoluteIndexed(&addr_abs, X);
            value = read(addr_abs);
            opCMP(value, A);
            cycles = 2;
            break;
        case 0xD9: // CMP Absolute,Y
            readAbsoluteIndexed(&addr_abs, Y);
            value = read(addr_abs);
            opCMP(value, A);
            cycles = 2;
            break;

        case 0xE0: // CPX Immediate
            value = read(ProgramCounter);
            ProgramCounter++;
            opCMP(value, X);
            cycles = 2;
            break;

        case 0xC0: // CPY Immediate
            value = read(ProgramCounter);
            ProgramCounter++;
            opCMP(value, Y);
            cycles = 2;
            break;

        case 0x24: // BIT Zero Page
            readZeroPage(&addr);
            value = read(addr);
            opBIT(value);
            cycles = 3;
            break;

        case 0x2C: // BIT Absolute
            readAbsolute(&addr_abs);
            value = read(addr_abs);
            opBIT(value);
            cycles = 4;
            break;

        case 0x00: // BRK
            ProgramCounter++;
            push(static_cast<uint8_t>(ProgramCounter >> 8));
            push(static_cast<uint8_t>(ProgramCounter));
            addr = 0;
            addr += static_cast<uint8_t>(flag_Carry ? 1 : 0);
            addr += static_cast<uint8_t>(flag_Zero ? 2 : 0);
            addr += static_cast<uint8_t>(flag_InterruptDisable ? 4 : 0);
            addr += static_cast<uint8_t>(flag_Decimal ? 8 : 0);
            addr += 0x10;
            addr += 0x20;
            addr += static_cast<uint8_t>(flag_Overflow ? 0x40 : 0);
            addr += static_cast<uint8_t>(flag_Negative ? 0x80 : 0);
            push(addr);
            addr_low = read(0xFFFE);
            addr_high = read(0xFFFF);
            ProgramCounter =
                static_cast<uint16_t>((addr_high * 0x100) + addr_low);
            cycles = 7;
            break;

        case 0x40: // RTI
            addr = pull();
            flag_Carry = (addr & 1) != 0;
            flag_Zero = (addr & 2) != 0;
            flag_InterruptDisable = (addr & 4) != 0;
            flag_Decimal = (addr & 8) != 0;
            flag_Overflow = (addr & 0x40) != 0;
            flag_Negative = (addr & 0x80) != 0;
            addr_low = pull();
            addr_high = pull();
            ProgramCounter =
                static_cast<uint16_t>((addr_high * 0x100) + addr_low);
            cycles = 6;
            break;

        case 0x38: // SEC - SEt Carry
            flag_Carry = 1;
            cycles = 2;
            break;

        case 0xF8: // SED - SEt Decimal
            flag_Decimal = 1;
            cycles = 2;
            break;

        case 0x78: // SEI - SEt Interrupt Disable
            flag_InterruptDisable = 1;
            cycles = 2;
            break;

        case 0x18: // CLC - CLear Carry
            flag_Carry = 0;
            cycles = 2;
            break;

        case 0xD8: // CLD - CLear Decimal
            flag_Decimal = 0;
            cycles = 2;
            break;

        case 0x58: // CLI - CLear Interrupt disable
            flag_InterruptDisable = 0;
            cycles = 2;
            break;

        case 0xB8: // CLV - CLear oVerflow
            flag_Overflow = 0;
            cycles = 2;
            break;

        default:
            std::cout << "Unknown opcode 0x" << std::hex
                      << static_cast<unsigned int>(opcode)
                      << ", bailing out, you're on your own!" << std::endl;
            CpuHalted = true;
            break;
        }

        tracelog(opcode);
    }

    void opADC(uint8_t input) {
        int sum;
        sum = input + A + (flag_Carry ? 1 : 0);

        flag_Overflow = (~(A ^ input) & (A ^ sum) & 0x80) != 0;
        flag_Carry = sum > 0xFF;

        A = sum;

        flagZN(&A);
    }

    void opSBC(uint8_t input) {
        int sum;
        sum = A - input - (flag_Carry ? 0 : 1);

        flag_Overflow = ((A ^ input) & (A ^ sum) & 0x80) != 0;
        flag_Carry = sum >= 0;

        A = sum;

        flagZN(&A);
    }

    void opCMP(uint8_t input, uint8_t reg) {
        flag_Carry = input <= reg;
        flag_Zero = input == reg;
        flag_Negative = static_cast<uint8_t>(reg - input) > 127;
    }

    void opBIT(uint8_t input) {
        flag_Zero = (A & input) == 0;
        flag_Negative = (input & 0x80) != 0;
        flag_Overflow = (input & 0x40) != 0;
    }

    void tracelog(uint8_t opcode) {
        std::string line = std::format(
            "{:04X} \t {:02X} \t {:<4} \t A:{:02X} X:{:02X} Y:{:02X}\t "
            "{}{}{}{}{}{}{} \n",
            ProgramCounter, opcode, opCodesArr[opcode], A, X, Y,
            (flag_Negative ? "N" : "n"), (flag_Overflow ? "V" : "v"), "--",
            (flag_Decimal ? "D" : "d"), (flag_InterruptDisable ? "I" : "i"),
            (flag_Zero ? "Z" : "z"), (flag_Carry ? "C" : "c"));
        std::cout << line;
    }

  private:
    uint16_t ProgramCounter;
    bool CpuHalted = false;
    uint16_t stackPointer{};

    uint8_t A; // Accumulator
    uint8_t X; // X register
    uint8_t Y; // Y register

    std::vector<uint8_t> RAM;
    std::vector<uint8_t> INesHeader;
    std::vector<uint8_t> ROM;

    bool flag_Carry = false;
    bool flag_Zero = false;
    bool flag_InterruptDisable = false;
    bool flag_Decimal = false;
    bool flag_Overflow = false;
    bool flag_Negative = false;
};
