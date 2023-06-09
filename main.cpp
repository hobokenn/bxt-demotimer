#include <cstdint>
#include <fstream>
#include <sstream>
#include <iostream>
#include <iomanip>
#include <vector>
#include <string>
#include <unordered_map>

#include "demo.h"

Time bxtTime;

int main(int argc, char* argv[])
{
	if (argc != 2) {
		std::cout << "bxt-demotimer.exe <demoname>\n";
		return 1;
	}
	
	std::ifstream input(argv[1], std::ios::binary);
	
	if (!input.is_open()) {
		std::cout << "couldn't open the file.\n";
		return 1;
	}

	demoheader_t demoHeader;
	input.read(demoHeader.szFileStamp, sizeof(demoHeader.szFileStamp));
	if (std::strncmp(demoHeader.szFileStamp, "HLDEMO", 6)) {
		std::cout << "not a goldsrc demo.\n";
		return 1;
	}

	input.seekg(offsetof(demoheader_t, nNetProtocol), std::ios_base::beg);
	input.read(reinterpret_cast<char*>(&demoHeader.nNetProtocol), sizeof(int));
	
	if (demoHeader.nNetProtocol < 43) {
		std::cout << "unsupported net protocol: " << demoHeader.nNetProtocol << std::endl;
		return 1;
	}

	// steam = 436, won 1712 = 436 - 24 + 120
	int netMsgSize = demoHeader.nNetProtocol == 43 ? 532 : 436;

	input.seekg(offsetof(demoheader_t, nDirectoryOffset), std::ios_base::beg);
	input.read(reinterpret_cast<char*>(&demoHeader.nDirectoryOffset), sizeof(int));

	input.seekg(demoHeader.nDirectoryOffset, std::ios_base::beg);

	demodirectory_t directoryEntry;
	input.read(reinterpret_cast<char*>(&directoryEntry.nEntries), sizeof(int));
	std::cout << "parsing the demo file...\n";

	for (int i = 0; i < directoryEntry.nEntries; ++i) {
		demoentry_t demoEntry;	
		input.seekg(offsetof(demoentry_t, nFrames), std::ios_base::cur);
		input.read(reinterpret_cast<char*>(&demoEntry.nFrames), sizeof(int));
		input.read(reinterpret_cast<char*>(&demoEntry.nOffset), sizeof(int));

		input.seekg(demoEntry.nOffset, std::ios_base::beg);

		demo_command_t demoFrame;
		int sampleCount = 0, bufferSize = 0, msglen = 0;
		BxtData bxtData; std::vector<uint8_t> packedBxtBytes; static bool isBxtDemo;

		do {
			input.read(reinterpret_cast<char*>(&demoFrame.cmd), sizeof(uint8_t));
			input.seekg(8, std::ios_base::cur);

			switch (demoFrame.cmd) {
			case DemoFrameType::DEMO_START:
				break;
			case DemoFrameType::CONSOLE_COMMAND:
				// handle //BXTD0 here
				input.read(bxtData.header, 7);
				if (!std::strncmp(bxtData.header, "//BXTD0", 7)) {
					isBxtDemo = true;
					input.read(bxtData.data, 56);
					for (int i = 0; i < sizeof(bxtData.data); ++i) {
						if (bxtData.data[i] == '\0') break;
						// is std::vector the best container here? if i want to parse all of
						// bxt data later and not just last bxt time i need to think of something else
						packedBxtBytes.emplace_back(bxtData.data[i]);
					}
					// null terminator
					input.seekg(1, std::ios_base::cur);
				} else {
					input.seekg(57, std::ios_base::cur);
				}
				break;
			case DemoFrameType::CLIENT_DATA:
				input.seekg(32, std::ios_base::cur);
				break;
			case DemoFrameType::NEXT_SECTION:
				break;
			case DemoFrameType::EVENT:
				input.seekg(84, std::ios_base::cur);
				break;
			case DemoFrameType::WEAPON_ANIM:
				input.seekg(8, std::ios_base::cur);
				break;
			case DemoFrameType::SOUND:
				input.seekg(4, std::ios_base::cur);
				input.read(reinterpret_cast<char*>(&sampleCount), sizeof(int));
				input.seekg(sampleCount, std::ios_base::cur);
				input.seekg(16, std::ios_base::cur);
				break;
			case DemoFrameType::DEMO_BUFFER:
				input.read(reinterpret_cast<char*>(&bufferSize), sizeof(int));
				input.seekg(bufferSize, std::ios_base::cur);
				break;
			default:
				input.seekg(netMsgSize, std::ios_base::cur);
				input.seekg(28, std::ios_base::cur);
				input.read(reinterpret_cast<char*>(&msglen), sizeof(int));
				input.seekg(msglen, std::ios_base::cur);
				break;
			}
		} while (demoFrame.cmd != DemoFrameType::NEXT_SECTION);

		if (i == 0) {
			input.seekg(demoHeader.nDirectoryOffset, std::ios_base::beg);
			input.seekg(4, std::ios_base::cur);
			input.seekg(sizeof(demoentry_t), std::ios_base::cur);
		} else {
			std::cout << "reached the end of the demo\n";

			if (!isBxtDemo) {
				input.seekg(-8, std::ios_base::cur);
				input.read(reinterpret_cast<char*>(&demoFrame.time), sizeof(float));
				input.read(reinterpret_cast<char*>(&demoFrame.frame), sizeof(int));
				std::cout << "this demo was not recorded with BXT, however here's some information the game provides itself when you stop a demo:\n";
				std::cout << "Recording time: " << std::fixed << std::setprecision(2) << demoFrame.time << '\n';
				std::cout << "Host frames: " << demoFrame.frame << '\n';
				break;
			}

			auto unEscapedBytes = getOriginalBytes(packedBxtBytes);

			for (size_t i = 0; i < unEscapedBytes.size(); i += 8)
				TEA_Decrypt(reinterpret_cast<uint32_t*>(unEscapedBytes.data() + i), KEY);

#if 0
			std::string outFile = "bxt-data.bin";
			std::ofstream ostrm(outFile, std::ios::binary);

			for (auto byte : unEscapedBytes)
				ostrm << byte;
#endif
			parseBxtData(unEscapedBytes);
			printBxtTime();

			break;
		}
	}
	return 0;
}

void TEA_Decrypt(uint32_t v[2], const uint32_t k[4]) {
	uint32_t v0 = v[0], v1 = v[1], sum = 0xC6EF3720, i;  /* set up; sum is (delta << 5) & 0xFFFFFFFF */
	uint32_t delta = 0x9E3779B9;                     /* a key schedule constant */
	uint32_t k0 = k[0], k1 = k[1], k2 = k[2], k3 = k[3];   /* cache key */
	for (i = 0; i < 32; i++) {                         /* basic cycle start */
		v1 -= ((v0 << 4) + k2) ^ (v0 + sum) ^ ((v0 >> 5) + k3);
		v0 -= ((v1 << 4) + k0) ^ (v1 + sum) ^ ((v1 >> 5) + k1);
		sum -= delta;
	}                                              /* end cycle */
	v[0] = v0; v[1] = v1;
}

std::vector<uint8_t> getOriginalBytes(const std::vector<uint8_t> &bytes)
{
	std::vector <uint8_t> out;
	for (auto it = bytes.cbegin(); it != bytes.cend(); ++it) {

		if (std::next(it) == bytes.cend()) {
			out.emplace_back(*it);
			break;
		}

		auto nextChar = *std::next(it);
		auto escaped = ESCAPE_CHARACTERS.find(nextChar);
		if (*it == ESCAPE_BYTE && nextChar == escaped->first) {
			out.emplace_back(escaped->second);
			it++;
			continue;
		}
		out.emplace_back(*it);
	}
	return out;
}

void parseBxtData(const std::vector<uint8_t> &bytes)
{
	std::stringstream ss;
	std::copy(bytes.cbegin(), bytes.cend(), std::ostream_iterator<uint8_t>(ss));

	unsigned numDataTypes = 0, numElements = 0;
	RuntimeDataType dataType{};

	for (;;) {
		while (ss.tellg() % 8 != 0)
			ss.seekg(1, std::ios_base::cur);

		ss.read(reinterpret_cast<char*>(&numDataTypes), sizeof(unsigned));

		if (ss.eof())
			break;

		for (unsigned i = 0; i < numDataTypes; ++i) {
			ss.read(reinterpret_cast<char*>(&dataType), sizeof(uint8_t));

			switch (dataType) {
			case RuntimeDataType::VERSION_INFO:
				ss.seekg(4, std::ios_base::cur);
				skipString(ss);
				break;
			case RuntimeDataType::CVAR_VALUES:
				ss.read(reinterpret_cast<char*>(&numElements), sizeof(unsigned));
				for (unsigned j = 0; j < numElements; ++j) {
					skipString(ss);
					skipString(ss);
				}
				break;
			case RuntimeDataType::TIME:
				ss.read(reinterpret_cast<char*>(&bxtTime.hours), sizeof(int));
				ss.read(reinterpret_cast<char*>(&bxtTime.minutes), sizeof(uint8_t));
				ss.read(reinterpret_cast<char*>(&bxtTime.seconds), sizeof(uint8_t));
				ss.read(reinterpret_cast<char*>(&bxtTime.remainder), sizeof(double));
				break;
			case RuntimeDataType::BOUND_COMMAND:
				skipString(ss);
				break;
			case RuntimeDataType::ALIAS_EXPANSION:
				skipString(ss);
				skipString(ss);
				break;
			case RuntimeDataType::SCRIPT_EXECUTION:
				skipString(ss);
				skipString(ss);
				break;
			case RuntimeDataType::COMMAND_EXECUTION:
				skipString(ss);
				break;
			case RuntimeDataType::GAME_END_MARKER:
				break;
			case RuntimeDataType::LOADED_MODULES:
				ss.read(reinterpret_cast<char*>(&numElements), sizeof(unsigned));
				for (unsigned j = 0; j < numElements; ++j)
					skipString(ss);
				break;
			case RuntimeDataType::CUSTOM_TRIGGER_COMMAND:
				ss.seekg(24, std::ios_base::cur);
				skipString(ss);
				break;
			case RuntimeDataType::EDICTS:
				ss.seekg(4, std::ios_base::cur);
				break;
			case RuntimeDataType::PLAYERHEALTH:
				ss.seekg(4, std::ios_base::cur);
				break;
			case RuntimeDataType::SPLIT_MARKER:
				ss.seekg(24, std::ios_base::cur);
				skipString(ss);
				skipString(ss);
				break;
			}
		}
	}
}

inline void skipString(std::stringstream &ss)
{
	int strLen = 0;
	ss.read(reinterpret_cast<char*>(&strLen), sizeof(int));
	ss.seekg(strLen, std::ios_base::cur);
}

void printBxtTime()
{
	// pretty ugly
	if (bxtTime.hours == 0 && bxtTime.minutes == '\0' && bxtTime.seconds == '\0')
		std::cout << std::fixed << std::setprecision(3)
		<< "bxt demo time: " <<  bxtTime.remainder << '\n';
	else if (bxtTime.hours == 0 && bxtTime.minutes == '\0')
		std::cout << std::fixed << std::setprecision(3)
		<< "bxt demo time: " << bxtTime.seconds + bxtTime.remainder << '\n';
	else if (bxtTime.hours == 0)
		std::cout << std::fixed << std::setprecision(3)
		<< "bxt demo time: " << static_cast<int>(bxtTime.minutes) << ":"
		<< bxtTime.seconds + bxtTime.remainder << '\n';
	else
		std::cout << std::fixed << std::setprecision(3)
		<< "bxt demo time: " << bxtTime.hours << ":" << static_cast<int>(bxtTime.minutes)
		<< ":" << bxtTime.seconds + bxtTime.remainder << '\n';
}
