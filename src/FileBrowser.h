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

			//アイコン表示の設定
			float padding = 16.0f;
			float thumbnailSize = 64.0f;
			float cellSize = thumbnailSize + padding;

			float panelWidth = ImGui::GetContentRegionAvail().x;
			int columnCount = (int)(panelWidth / cellSize);
			if (columnCount < 1)columnCount = 1;
			
			ImGui::Columns(columnCount, 0, false);

			//ディレクトリ内のファイルとフォルダを表示
			try {
				for (const auto& entry : fs::directory_iterator(_currentPath)) {
					const auto& path = entry.path();
					std::string filename = path.filename().string();
					std::string extension = path.extension().string();

					//拡張子の小文字変換
					std::transform(extension.begin(), extension.end(), extension.begin(), ::tolower);

					//アイコンの色とラベルを設定（将来的に画像に変更）
					ImVec4 iconColor;
					std::string iconLabel;	//画像がない代わり

					bool isDir = entry.is_directory();

					if (isDir) {
						iconColor = ImVec4(1.0f, 0.8f, 0.2f, 1.0f);//黄色
						iconLabel = "DIR";
					}
					else if (extension == ".json") {
						iconColor = ImVec4(0.5f, 1.0f, 0.5f, 1.0f);//緑
						iconLabel = "SCENE";
					}
					else if (extension == ".cpp") {
						iconColor = ImVec4(0.2f, 0.4f, 0.8f, 1.0f);//青
						iconLabel = "C++";
					}
					else if (extension == ".h" || extension == ".hpp") {
						iconColor = ImVec4(0.6f, 0.2f, 0.6f, 1.0f);//紫
						iconLabel = "H";
					}
					else {
						iconColor = ImVec4(0.7f, 0.7f, 0.7f, 1.0f);//グレー
						iconLabel = "FILE";
					}

					ImGui::PushID(filename.c_str());

					//ボタン描画処理
					ImGui::PushStyleColor(ImGuiCol_Button, iconColor);

					bool clicked = ImGui::Button(iconLabel.c_str(), ImVec2(thumbnailSize, thumbnailSize));
					
					if (isDir) {//ディレクトリならクリックで移動
						if (clicked) {
							_currentPath /= filename;	//パスを結合
							ImGui::PopStyleColor();
							ImGui::PopID();
							ImGui::Columns(1);
							ImGui::End();
							return;
						}
					}
					else {//ファイル
						if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
							if (extension == ".json") {
								//string()でパス引き渡し
								Manager::OpenScene(path.string());
							}
						}
					}
					
					ImGui::PopStyleColor();

					//下にファイル名表示
					ImGui::TextWrapped("%s", filename.c_str());
					
					ImGui::PopID();

					ImGui::NextColumn();
				}
			}
			catch(const std::exception& e){
				//エラー時
				ImGui::Columns(1);
				ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), "Error accessing directory: %s", e.what());
			}
			ImGui::Columns(1);	//カラム設定解除

			ImGui::End();
		}
	}
};


#endif // !_FILEBROWSER_H