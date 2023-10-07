#include "led.h"
#include "BleApp.h"
#include "HidApp.h"

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
	if (bleApp.connectionStatus == BleApp::ConnectionStatus::Connecting) {
		state = State::FastFlash;
	} else if (bleApp.connectionStatus == BleApp::ConnectionStatus::ClientConnected) {
		// Check if low battery
		if (hidApp.batteryLevel <= hidApp.settings.batteryWarning) {
			state = State::LowBattery;
		} else {
			state = State::On;
		}
	} else if (bleApp.action == BleApp::RequestAction::ScanConnect || bleApp.action == BleApp::RequestAction::ScanInfo) {
		state = State::SlowFlash;
	} else {
		state = State::Off;
	}

	if (oldState != state) {
		flashCount = 0;
		flashUpdated = SysTickVal;
		switch (state) {
		case State::FastFlash:
			flashOn = 200;
			flashOff = 200;
			break;
		case State::SlowFlash:
			flashOn = 600;
			flashOff = 600;
			break;
		case State::LowBattery:
			flashOn = 1000;
			flashOff = 100;
			break;
		case State::On:
			break;
		case State::Off:
			break;
		}
		LedOn(state != State::Off);
	}


	if (state == State::FastFlash || state == State::SlowFlash || state == State::LowBattery) {
		if ((lit && SysTickVal > flashUpdated + flashOn) || (!lit && SysTickVal > flashUpdated + flashOff)) {
			flashUpdated = SysTickVal;
			LedOn(!lit);
		}
	}
}


