#include "BleApp.h"
#include "led.h"

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
	if (bleApp.action == BleApp::RequestAction::ScanConnect || bleApp.action == BleApp::RequestAction::ScanInfo) {
		state = State::SlowFlash;
	} else if (bleApp.deviceConnectionStatus == BleApp::ConnectionStatus::Connecting) {
		state = State::FastFlash;
	} else if (bleApp.deviceConnectionStatus == BleApp::ConnectionStatus::ClientConnected) {
		state = State::On;
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
		case State::MediumFlash:
			flashOn = 400;
			flashOff = 400;
			break;
		case State::On:
			LedOn(true);
			break;
		case State::Off:
			LedOn(false);
			break;
		}
	}


	if (state == State::FastFlash || state == State::MediumFlash || state == State::SlowFlash) {
		if ((lit && SysTickVal > flashUpdated + flashOn) || (!lit && SysTickVal > flashUpdated + flashOff)) {
			flashUpdated = SysTickVal;
			LedOn(!lit);
		}
	}
}


