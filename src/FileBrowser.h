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

namespace fs = std::filesystem;

class FileBrowser {
private:
	fs::path _currentPath;


public:
	FileBrowser() {
		_currentPath = fs::current_path();
	}

	void Render() {
		ImGui::Begin("Content Browser");
		{
			if (ImGui::Button("<-")) {
				if (_currentPath.has_parent_path()) {//親ディレクトリが存在したら移動
					_currentPath = _currentPath.parent_path();
				}
			}
			ImGui::SameLine();
			ImGui::Text("%s", _currentPath.string().c_str());
			ImGui::Separator();

			//ディレクトリ内のファイルとフォルダを表示
			try {
				for (const auto& entry : fs::directory_iterator(_currentPath)) {
					const auto& path = entry.path();
					std::string filename = path.filename().string();

					if (entry.is_directory()) {
						//フォルダの場合
						ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.9f, 0.5f, 1.0f));
						if (ImGui::Selectable(("[Dir]" + filename).c_str(), false, ImGuiSelectableFlags_AllowDoubleClick)) {
							if(ImGui::IsMouseDoubleClicked(0)) {
								_currentPath = path; //ダブルクリックでフォルダに移動
							}
						}
						ImGui::PopStyleColor();
					}
					else {
						//ファイルの場合
						ImGui::Text("   %s", filename.c_str());
					}
				}
			}
			catch(const std::exception& e){
				ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), "Error accessing directory: %s", e.what());
			}
			ImGui::End();
		}
	}
};


#endif // !_FILEBROWSER_H