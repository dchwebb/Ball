#include <BleApp.h>
#include "led.h"
#include "HidService.h"

LED led;

void LED::LedOn(bool on){
	if (on) {
		GPIOA->ODR |= GPIO_ODR_OD3;								// Turn on connected LED
	} else {
		GPIOA->ODR &= ~GPIO_ODR_OD3;							// Turn off connected LED
	}
	lit = on;
}


void LED::Update()
{
	auto oldState = state;
	if (bleApp.sleepState != BleApp::SleepState::Awake) {
		state = State::Off;
	} else if (bleApp.connectionStatus == BleApp::ConnStatus::FastAdv) {
		state = State::FastFlash;
	} else if (bleApp.connectionStatus == BleApp::ConnStatus::LPAdv) {
		state = State::SlowFlash;
	} else if (bleApp.connectionStatus == BleApp::ConnStatus::Connected) {
		state = hidService.moving ? State::On : State::FastFlash;
	}

	if (oldState != state) {
		flashCount = 0;
		flashUpdated = SysTickVal;
		switch (state) {
		case State::FastFlash:
			flashOn = 50;
			flashOff = 1000;
			break;
		case State::SlowFlash:
			flashOn = 10;
			flashOff = 2000;
			break;
		case State::On:
			LedOn(true);
			break;
		case State::Off:
			LedOn(false);
			break;
		}
	}


	if (state == State::FastFlash || state == State::SlowFlash) {
		if ((lit && SysTickVal > flashUpdated + flashOn) || (!lit && SysTickVal > flashUpdated + flashOff)) {
			flashUpdated = SysTickVal;
			LedOn(!lit);
		}
	}
}


