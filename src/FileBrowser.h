//=====================================================================================
// FileBrowser.h
// Author:Kaito Aoki
// Date:2025/06/23
//=====================================================================================

#ifndef _FILEBROWSER_H
#define _FILEBROWSER_H

#include <filesystem>
#include <string>
#include "imgui.h"


class FileBrowser {
private:
	std::filesystem::path _currentPath;


public:
	FileBrowser() {
		_currentPath = std::filesystem::current_path();
	}
};


#endif // !_FILEBROWSER_H