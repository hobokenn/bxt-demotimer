#pragma once

std::vector<uint8_t> getOriginalBytes(const std::vector<uint8_t> &bytes);
void TEA_Decrypt(uint32_t v[2], const uint32_t k[4]);
void getLastBxtTime(const std::vector<uint8_t> &bytes);

struct demoheader_t
{
	char szFileStamp[6];
	int nDemoProtocol;
	int nNetProtocol;
	char szMapName[260];
	char szDllDir[260];
	uint32_t mapCRC;
	int nDirectoryOffset;
};

struct demoentry_t
{
	int nEntryType;
	char szDescription[64];
	int nFlags;
	int nCDTrack;
	float fTrackTime;
	int nFrames;
	int nOffset;
	int nFileLength;
};

struct demodirectory_t
{
	int nEntries;
	demoentry_t *p_rgEntries;
};

enum class DemoFrameType : uint8_t {
	DEMO_START = 2,
	CONSOLE_COMMAND = 3,
	CLIENT_DATA = 4,
	NEXT_SECTION = 5,
	EVENT = 6,
	WEAPON_ANIM = 7,
	SOUND = 8,
	DEMO_BUFFER = 9
};

struct demo_command_t
{
	DemoFrameType cmd;
	float time;
	int frame;
};

struct BxtData
{
	char header[7];
	char data[56];
};

enum class RuntimeDataType : uint8_t {
	VERSION_INFO = 1,
	CVAR_VALUES,
	TIME,
	BOUND_COMMAND,
	ALIAS_EXPANSION,
	SCRIPT_EXECUTION,
	COMMAND_EXECUTION,
	GAME_END_MARKER,
	LOADED_MODULES,
	CUSTOM_TRIGGER_COMMAND,
	EDICTS,
	PLAYERHEALTH,
	SPLIT_MARKER,
};

struct Time {
	uint32_t hours;
	uint8_t minutes;
	uint8_t seconds;
	double remainder;
};

const uint32_t KEY[4] = { 0x1337FACE, 0x12345678, 0xDEADBEEF, 0xFEEDABCD };
constexpr uint8_t ESCAPE_BYTE = 0xFF;
constexpr uint8_t FILL_BYTE = 0xFE;

const std::unordered_map<uint8_t, uint8_t> ESCAPE_CHARACTERS = {
	{ 0x01, 0x00 },
	{ 0x02 , '"' },
	{ 0x03, '\n' },
	{ 0x04, ';' },
	{ 0xFF, 0xFF },
};
