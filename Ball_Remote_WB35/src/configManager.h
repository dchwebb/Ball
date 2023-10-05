#pragma once

#include "initialisation.h"
#include <stddef.h>


#define FLASH_ALL_ERRORS (FLASH_SR_OPERR  | FLASH_SR_PROGERR | FLASH_SR_WRPERR | FLASH_SR_PGAERR | FLASH_SR_SIZERR | FLASH_SR_PGSERR | FLASH_SR_MISERR | FLASH_SR_FASTERR | FLASH_SR_RDERR  | FLASH_SR_OPTVERR)

class Config {
public:
	static constexpr uint32_t configVersion = 3;
	static constexpr uint32_t BufferSize = 2048;
	static constexpr bool eraseConfig = true;
	static constexpr uint32_t flashConfigPage = 53;		// 54 pages for WB35; Page address: 4096 * (page - 1)
	uint32_t* const flashConfigAddr = reinterpret_cast<uint32_t* const>(0x08000000 + 4096 * (flashConfigPage - 1));

	void ScheduleSave();				// called whenever a config setting is changed to schedule a save after waiting to see if any more changes are being made
	bool SaveConfig(bool eraseOnly = false);
	void RestoreConfig();				// gets config from Flash, checks and updates settings accordingly
	void CheckSave();					// Check if a scheduled save is ready

private:
	uint32_t SetConfig();				// Serialise configuration data into buffer

	void FlashUnlock();
	void FlashLock();
	void FlashErasePage(uint8_t Sector);
	bool FlashWaitForLastOperation();
	bool FlashProgram(uint32_t* dest_addr, uint32_t* src_addr, size_t size);

	bool scheduleSave = false;
	uint32_t saveBooked;
	uint8_t configBuffer[BufferSize];


};

extern Config config;
