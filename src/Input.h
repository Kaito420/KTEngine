//=====================================================================================
// Input.h
// Author:Kaito Aoki
// Date:2025/07/22
//=====================================================================================

#ifndef _INPUT_H
#define _INPUT_H

#include <Windows.h>
#include <array>
#include <utility>

namespace Input {

	enum class MouseButton {
		Left,
		Right,
		Middle,
		Button4,
		Button5
	};

	void Initialize(HWND hwnd);

	void Update();

	void Finalize();

	void ProcessRawInput(LPARAM lParam);

	bool IsKeyDown(UINT key);
	bool IsKeyPressed(UINT key);
	bool IsKeyReleased(UINT key);

	bool IsMouseButtonDown(MouseButton btn);
	bool IsMouseButtonPressed(MouseButton btn);
	bool IsMouseButtonReleased(MouseButton btn);

	std::pair<LONG, LONG> GetMousePosition();
	std::pair<LONG, LONG> GetMouseDelta();

}


#endif //!_INPUT_H