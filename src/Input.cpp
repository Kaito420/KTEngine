//=====================================================================================
// Input.h
// Author:Kaito Aoki
// Date:2025/07/22
//=====================================================================================

#include "Input.h"
#include "ImGuiLayer.h"
#include "imgui.h"

namespace {
	HWND _hWnd = nullptr;
	
	std::array<bool, 256> _currentKeys = { false };
	std::array<bool, 256> _previousKeys = { false };

	std::array<bool, 5> _currentMouseButtons = { false };
	std::array<bool, 5> _previousMouseButtons = { false };

	LONG _mouseX = 0;
	LONG _mouseY = 0;
	LONG _deltaX = 0;
	LONG _deltaY = 0;

	int _wheelDelta = 0;

	bool _isGameViewHovered = false;
}




void Input::SetGameViewHovered(bool hovered){
	_isGameViewHovered = hovered;
}

bool Input::IsGameViewHovered(){
	return _isGameViewHovered;
}

void Input::Initialize(HWND hwnd){
	_hWnd = hwnd;

	//キーボードとマウス
	RAWINPUTDEVICE rid[2];

	//キーボード
	rid[0].usUsagePage = 0x01;
	rid[0].usUsage = 0x06;
	rid[0].dwFlags = RIDEV_INPUTSINK;
	rid[0].hwndTarget = hwnd;

	//マウス
	rid[1].usUsagePage = 0x01;
	rid[1].usUsage = 0x02;
	rid[1].dwFlags = RIDEV_INPUTSINK;
	rid[1].hwndTarget = hwnd;

	RegisterRawInputDevices(rid, 2, sizeof(RAWINPUTDEVICE));
}

void Input::Update(){
	_previousKeys = _currentKeys;
	_previousMouseButtons = _currentMouseButtons;

	_deltaX = 0;
	_deltaY = 0;
	_wheelDelta = 0;
}

void Input::Finalize(){

}

void Input::ProcessRawInput(LPARAM lParam){
	UINT dwSize;
	GetRawInputData((HRAWINPUT)lParam, RID_INPUT, nullptr, &dwSize, sizeof(RAWINPUTHEADER));
	BYTE* lpb = new BYTE[dwSize];

	if (GetRawInputData((HRAWINPUT)lParam, RID_INPUT, lpb, &dwSize, sizeof(RAWINPUTHEADER)) != dwSize) {
		OutputDebugString("GetRawInputData size mismatch\n");
		delete[] lpb;
		return;
	}

	RAWINPUT* raw = (RAWINPUT*)lpb;

	if (raw->header.dwType == RIM_TYPEKEYBOARD) {
		RAWKEYBOARD& kbd = raw->data.keyboard;
		USHORT key = kbd.VKey;
		USHORT flags = kbd.Flags;

		if (key < 256) {
			if (!(flags & RI_KEY_BREAK))
				_currentKeys[key] = true;
			else
				_currentKeys[key] = false;
		}
	}
	else if (raw->header.dwType == RIM_TYPEMOUSE) {
		RAWMOUSE& mouse = raw->data.mouse;

		_deltaX = mouse.lLastX;
		_deltaY = mouse.lLastY;

		if (mouse.usButtonFlags & RI_MOUSE_LEFT_BUTTON_DOWN)
			_currentMouseButtons[(int)MouseButton::Left] = true;
		if (mouse.usButtonFlags & RI_MOUSE_LEFT_BUTTON_UP)
			_currentMouseButtons[(int)MouseButton::Left] = false;

		if (mouse.usButtonFlags & RI_MOUSE_RIGHT_BUTTON_DOWN)
			_currentMouseButtons[(int)MouseButton::Right] = true;
		if (mouse.usButtonFlags & RI_MOUSE_RIGHT_BUTTON_UP)
			_currentMouseButtons[(int)MouseButton::Right] = false;

		if (mouse.usButtonFlags & RI_MOUSE_MIDDLE_BUTTON_DOWN)
			_currentMouseButtons[(int)MouseButton::Middle] = true;
		if (mouse.usButtonFlags & RI_MOUSE_MIDDLE_BUTTON_UP)
			_currentMouseButtons[(int)MouseButton::Middle] = false;

		if (mouse.usButtonFlags & RI_MOUSE_WHEEL) {
			SHORT wheel = (SHORT)mouse.usButtonData;
			_wheelDelta += wheel;	//WHEEL_DELTA単位（通常120）
		}
	}

	delete[] lpb;

}

bool Input::IsKeyDown(UINT key){
	return (key < 256) ? _currentKeys[key] : false;
}

bool Input::IsKeyPressed(UINT key){
	return (key < 256) ? (_currentKeys[key] && !_previousKeys[key]) : false;
}

bool Input::IsKeyReleased(UINT key){
	return (key < 256) ? (!_currentKeys[key] && _previousKeys[key]) : false;
}

bool Input::IsMouseButtonDown(MouseButton btn){
	return _currentMouseButtons[(int)btn];
}

bool Input::IsMouseButtonPressed(MouseButton btn){
	return _currentMouseButtons[(int)btn] && !_previousMouseButtons[(int)btn];
}

bool Input::IsMouseButtonReleased(MouseButton btn){
	return !_currentMouseButtons[(int)btn] && _previousMouseButtons[(int)btn];
}

std::pair<LONG, LONG> Input::GetMousePosition(){
	POINT p;
	GetCursorPos(&p);
	ScreenToClient(_hWnd, &p);
	return { p.x, p.y };
}

std::pair<LONG, LONG> Input::GetMouseDelta(){
	return { _deltaX, _deltaY };
}

int Input::GetMouseWheelDelta()
{
	return _wheelDelta;
}
