#pragma once
#include <vector>
#include <cstdio>
#include <iostream>
#include <fstream>
#include <cstdint>
#include <stdexcept>

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

	void reset(const char* rom_filename) {
		std::ifstream file(rom_filename, std::ios::binary | std::ios::ate);
		if (!file)
			throw std::runtime_error("Failed to open the ROM.");

		std::streamsize size = file.tellg();
		file.seekg(0, std::ios::beg);

		std::vector<uint8_t> buffer(static_cast<size_t>(size));
		if (!file.read(reinterpret_cast<char*>(buffer.data()), size))
			throw std::runtime_error("Failed to read the ROM.");

		std::vector<uint8_t> header(buffer.begin(), buffer.begin() + 16);
		std::vector<uint8_t> rest(buffer.begin() + 16, buffer.end());

		INesHeader = header;
		ROM = rest;

		uint8_t PCL = read(0xFFFC);
		uint8_t PCH = read(0xFFFD);

		ProgramCounter = static_cast<uint16_t>((static_cast<uint16_t>(PCH) << 8) | PCL);

		flag_InterruptDisable = true;

		run();
	}

	uint8_t read(uint16_t addr) {
		if (addr <= 0x1FFF) {
			return RAM[addr & 0x07FF];
		}
		if (addr >= 0x8000) {
			uint32_t romIndex = static_cast<uint32_t>(addr - 0x8000);
			if (romIndex < ROM.size())
				return ROM[romIndex];
			return 0;
		}
		return 0x0;
	}

	void write(uint16_t addr, uint8_t value) {
		RAM[addr & 0x07FF] = value;
	}

	void run() {
		while (!CpuHalted) {
			emulate_cpu();
		}
	}

	void emulate_cpu() {
		int cycles = 0;

		uint8_t addr;
		uint8_t addr_low;
		uint8_t addr_high;

		uint8_t opcode = read(ProgramCounter);
		ProgramCounter++;

		switch (opcode) {
			case 0x02: // HTL
				CpuHalted = true;
				break;

			/* 
			 * 	Load Instructions 
			 */

			case 0xA9: // LDA Immediate
				A = read(ProgramCounter);
				flag_Zero = A == 0;
				flag_Negative = A > 127;
				ProgramCounter++;
				cycles = 2;
				break;
			case 0xA5: // LDA Zero Page
				addr = read(ProgramCounter);
				ProgramCounter++;
				A = read(addr);
				flag_Zero = A == 0;
				flag_Negative = A > 127;
				cycles = 3;
				break;
			case 0xAD: // LDA Absolute
				addr_low = read(ProgramCounter);
				ProgramCounter++;
				addr_high = read(ProgramCounter);
				ProgramCounter++;
				A = read((uint16_t)(addr_high * 256 + addr_low));
				flag_Zero = A == 0;
				flag_Negative = A > 127;
				cycles = 4;
				break;

			case 0xA2: // LDX Immediate
				X = read(ProgramCounter);
				flag_Zero = X == 0;
				flag_Negative = X > 127;
				ProgramCounter++;
				cycles = 2;
				break;
			case 0xA6: // LDX Zero Page
				addr = read(ProgramCounter);
				ProgramCounter++;
				X = read(addr);
				flag_Zero = X == 0;
				flag_Negative = X > 127;
				cycles = 3;
				break;
			case 0xAE: // LDX Absolute
				addr_low = read(ProgramCounter);
				ProgramCounter++;
				addr_high = read(ProgramCounter);
				ProgramCounter++;
				X = read((uint16_t)(addr_high * 256 + addr_low));
				flag_Zero = X == 0;
				flag_Negative = X > 127;
				cycles = 4;
				break;

			case 0xA0: // LDY Immediate
				Y = read(ProgramCounter);
				flag_Zero = Y == 0;
				flag_Negative = Y > 127;
				ProgramCounter++;
				cycles = 2;
				break;
			case 0xA4: // LDY Zero Page
				addr = read(ProgramCounter);
				ProgramCounter++;
				Y = read(addr);
				flag_Zero = Y == 0;
				flag_Negative = Y > 127;
				cycles = 3;
				break;
			case 0xAC: // LDY Absolute
				addr_low = read(ProgramCounter);
				ProgramCounter++;
				addr_high = read(ProgramCounter);
				ProgramCounter++;
				Y = read((uint16_t)(addr_high * 256 + addr_low));
				flag_Zero = Y == 0;
				flag_Negative = Y > 127;
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
				addr_low = read(ProgramCounter);
				ProgramCounter++;
				addr_high = read(ProgramCounter);
				ProgramCounter++;
				write((uint16_t)(addr_high * 256 + addr_low), A);
				cycles = 4;
				break;
			case 0x86: // STX Zero Page
				addr = read(ProgramCounter);
				ProgramCounter++;
				write(addr, X);
				cycles = 3;
				break;
			case 0x8E: // STX Absolute
				addr_low = read(ProgramCounter);
				ProgramCounter++;
				addr_high = read(ProgramCounter);
				ProgramCounter++;
				write((uint16_t)(addr_high * 256 + addr_low), X);
				cycles = 4;
				break;
			case 0x84: // STY Zero Page
				addr = read(ProgramCounter);
				ProgramCounter++;
				write(addr, Y);
				cycles = 3;
				break;
			case 0x8C: // STY Absolute
				addr_low = read(ProgramCounter);
				ProgramCounter++;
				addr_high = read(ProgramCounter);
				ProgramCounter++;
				write((uint16_t)(addr_high * 256 + addr_low), Y);
				cycles = 4;
				break;

			/*
			 * Branch Instructions
			 */
			case 0x10: // BPL (Branch on PLus)
				addr = read(ProgramCounter);
				ProgramCounter++;
				if (!flag_Negative) {
					int8_t signedVal = (int8_t)addr;
					ProgramCounter = (uint16_t)(ProgramCounter + signedVal);
					cycles = 3;
				}
				else {
					cycles = 2;
				}
				break;
			case 0x30: // BMI (Branch on MInus)
				addr = read(ProgramCounter);
				ProgramCounter++;
				if (flag_Negative) {
					int8_t signedVal = (int8_t)addr;
					ProgramCounter = (uint16_t)(ProgramCounter + signedVal);
					cycles = 3;
				}
				else {
					cycles = 2;
				}
				break;
			case 0x50: // BVC (Branch on oVerflow Clear)
				addr = read(ProgramCounter);
				ProgramCounter++;
				if (!flag_Overflow) {
					int8_t signedVal = (int8_t)addr;
					ProgramCounter = (uint16_t)(ProgramCounter + signedVal);
					cycles = 3;
				}
				else {
					cycles = 2;
				}
				break;
			case 0x70: // BVS (Branch on oVerflow Set)
				addr = read(ProgramCounter);
				ProgramCounter++;
				if (flag_Overflow) {
					int8_t signedVal = (int8_t)addr;
					ProgramCounter = (uint16_t)(ProgramCounter + signedVal);
					cycles = 3;
				}
				else {
					cycles = 2;
				}
				break;
			case 0x90: // BCC (Branch on Carry Clear)
				addr = read(ProgramCounter);
				ProgramCounter++;
				if (!flag_Carry) {
					int8_t signedVal = (int8_t)addr;
					ProgramCounter = (uint16_t)(ProgramCounter + signedVal);
					cycles = 3;
				}
				else {
					cycles = 2;
				}
				break;
			case 0xB0: // BCS (Branch on Carry Set)
				addr = read(ProgramCounter);
				ProgramCounter++;
				if (flag_Carry) {
					int8_t signedVal = (int8_t)addr;
					ProgramCounter = (uint16_t)(ProgramCounter + signedVal);
					cycles = 3;
				}
				else {
					cycles = 2;
				}
				break;
			case 0xD0: // BNE (Branch on Not Equal)
				addr = read(ProgramCounter);
				ProgramCounter++;
				if (!flag_Zero) {
					int8_t signedVal = (int8_t)addr;
					ProgramCounter = (uint16_t)(ProgramCounter + signedVal);
					cycles = 3;
				}
				else {
					cycles = 2;
				}
				break;
			case 0xF0: // BEQ (Branch on EQual)
				addr = read(ProgramCounter);
				ProgramCounter++;
				if (flag_Zero) {
					int8_t signedVal = (int8_t)addr;
					ProgramCounter = (uint16_t)(ProgramCounter + signedVal);
					cycles = 3;
				}
				else {
					cycles = 2;
				}
				break;
			
			default:
				std::cout << "Unknown opcode 0x" << std::hex << static_cast<unsigned int>(opcode)
				          << ", bailing out, you're on your own!" << std::endl;
				CpuHalted = true;
				break;
		}
	}

private:
	uint16_t ProgramCounter;
	bool CpuHalted = false;

	uint8_t A;
	uint8_t X;
	uint8_t Y;

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