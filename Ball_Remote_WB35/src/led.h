#pragma once
#include "initialisation.h"

class LED {
public:
	void Update();
private:
	enum class State {Off, FastFlash, MediumFlash, SlowFlash, On} state {State::Off};
	void LedOn(bool on);

	uint32_t flashTimer = 0;
	uint32_t flashOn = 0;
	uint32_t flashOff = 0;
	uint32_t flashCount = 0;
	uint32_t flashUpdated = 0;
	bool lit = false;
};

extern LED led;
