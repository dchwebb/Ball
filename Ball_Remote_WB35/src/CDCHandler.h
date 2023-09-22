#pragma once

#include "initialisation.h"
#include "USBHandler.h"
#include <string>

class CDCHandler : public USBHandler {
public:
	CDCHandler(USBMain* usb, uint8_t inEP, uint8_t outEP, int8_t interface) : USBHandler(usb, inEP, outEP, interface) {
		outBuff = xfer_buff;
	}

	void DataIn() override;
	void DataOut() override;
	void ActivateEP() override;
	void ClassSetup(usbRequest& req) override;
	void ClassSetupData(usbRequest& req, const uint8_t* data) override;
	uint32_t GetInterfaceDescriptor(const uint8_t** buffer) override;

	void ProcessCommand();
	void PrintString(const char* format, ...);
	char* HexToString(const uint8_t* v, uint32_t len, const bool spaces = false);
	char* HexToString(const uint16_t v);
	int32_t ParseInt(const std::string_view cmd, const char precedingChar, const int32_t low, const int32_t high);

	bool cmdPending = false;
	static constexpr uint32_t maxCmdLen = 40;
	char comCmd[maxCmdLen];
	static constexpr uint32_t maxStrLen = 100;
	char stringBuf[maxStrLen];

	struct LineCoding {
		uint32_t bitrate;    		// Data terminal rate in bits per sec.
		uint8_t format;      		// Stop Bits: 0-1 Stop Bit; 1-1.5 Stop Bits; 2-2 Stop Bits
		uint8_t paritytype;  		// Parity: 0 = None; 1 = Odd; 2 = Even; 3 = Mark; 4 = Space; 6 bDataBits 1 Data bits
		uint8_t datatype;    		// Data bits (5, 6, 7,	8 or 16)
	} lineCoding;

private:
	enum {SetLineCoding = 0x20, GetLineCoding = 0x21};						// See CDC documentation rev 1.2 p.30
	enum {HtoD_Class_Interface = 0x21, DtoH_Class_Interface = 0xA1};		// A1 = [1|01|00001] Device to host | Class | Interface;
	uint32_t xfer_buff[64] __attribute__ ((aligned (4)));					// Receive data buffer - must be aligned to allow copying to other structures
	uint32_t buffPos = 0;
	static constexpr uint32_t bufSize = 1024;
	char buf[bufSize];

	static const uint8_t Descriptor[];

	float ParseFloat(const std::string_view cmd, const char precedingChar, float low, float high);

	// State machine for multi-stage commands
	enum class serialState {pending, dfuConfirm, calibConfirm, cancelAudioTest};
	serialState state = serialState::pending;

};


