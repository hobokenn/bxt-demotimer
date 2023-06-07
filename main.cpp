#include <cstdint>
#include <fstream>
#include <sstream>
#include <iostream>
#include <iomanip>
#include <vector>
#include <string>
#include <unordered_map>
#include <cassert>
#include "demo.h"

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

			getOriginalBytes(packedBxtBytes);

			for (size_t i = 0; i < packedBxtBytes.size(); i += 8)
				TEA_Decrypt(reinterpret_cast<uint32_t*>(packedBxtBytes.data() + i), KEY);

			assert(!packedBxtBytes.empty());
			while (packedBxtBytes.back() == FILL_BYTE)
				packedBxtBytes.pop_back();

			getLastBxtTime(packedBxtBytes);

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

void getOriginalBytes(std::vector<uint8_t> &bytes)
{
	for (auto it = bytes.begin(); it != bytes.end(); ++it) {

		if (std::next(it, 1) == bytes.end())
			break;

		uint8_t nextChar = *std::next(it, 1);
		auto escaped = ESCAPE_CHARACTERS.find(nextChar);
		if (it[0] == ESCAPE_BYTE && nextChar == escaped->first) {
			// is this slow? probably
			it = bytes.erase(it, std::next(it, 2));
			it = bytes.emplace(it, escaped->second);
		}
	}
}

void getLastBxtTime(const std::vector<uint8_t> &bytes)
{
	// unless BXT code changes and time is not the last thing written
	// to a demo on CL_Stop_f(), this code should work https://cdn.7tv.app/emote/6268904f4f54759b7184fa72/4x.webp
	// until then this is the laziest solution i could think of.
	char buf[15];
	std::memcpy(&buf, &bytes.back() - 14, 15);
	std::string str = std::string(buf, 15);
	
	std::istringstream ss(str, std::ios::binary);
	Time time; char type;
	ss.read(&type, 1);
	assert(type == '\3' && "what the hell is going on?");

	ss.read(reinterpret_cast<char*>(&time.hours), sizeof(uint32_t));
	ss.read(reinterpret_cast<char*>(&time.minutes), sizeof(uint8_t));
	ss.read(reinterpret_cast<char*>(&time.seconds), sizeof(uint8_t));
	ss.read(reinterpret_cast<char*>(&time.remainder), sizeof(double));

	// pretty ugly
	if (time.hours == 0 && time.minutes == '\0' && time.seconds == '\0')
		std::cout << std::fixed << std::setprecision(3)
		<< "bxt demo time: " <<  time.remainder << '\n';
	else if (time.hours == 0 && time.minutes == '\0')
		std::cout << std::fixed << std::setprecision(3)
		<< "bxt demo time: " << time.seconds + time.remainder << '\n';
	else if (time.hours == 0)
		std::cout << std::fixed << std::setprecision(3)
		<< "bxt demo time: " << static_cast<int>(time.minutes) << ":"
		<< time.seconds + time.remainder << '\n';
	else
		std::cout << std::fixed << std::setprecision(3)
		<< "bxt demo time: " << time.hours << ":" << static_cast<int>(time.minutes)
		<< ":" << time.seconds + time.remainder << '\n';
}
