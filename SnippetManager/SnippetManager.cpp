#define _CRT_SECURE_NO_WARNINGS
#define _SILENCE_EXPERIMENTAL_FILESYSTEM_DEPRECATION_WARNING
#define MIDNIGHT_STYLE
#include <iostream>
#include <raylib.h>
#include <imgui.h>
#include <imgui_stdlib.h>
#include <string>
#include <vector>
#include <direct.h>
#include <fstream>
#include <cstdio>
#include <experimental/filesystem>
#include "rImGui.h"
#include "darktheme_style.h"
#include "windows_bridge.h"
#include "TextEditorFork.h"

namespace fs = std::experimental::filesystem;

Color dark = { 45, 45, 45, 255 };
Color dark_gray = { 50, 51, 50, 255 };
Color linecolor = { 58, 58, 58, 255 };
Color right_dark = { 43, 43, 43, 255 };
Color bottom_dark = { 36, 35, 34, 255 };

float ScreenWidth = 1200;
float ScreenHeight = 600;
bool quit = false;
bool deleting = false;
bool delete_confirmed = false;
bool save = false;
bool close_tab = false;

std::string search_bar = "";

std::vector<int> index_cache;
std::vector<std::string> snippets;
std::vector<std::string> snippet_paths;
std::vector<const char*> filtered_snippets;
int selected_snippet = -1;

bool creating_snippet = false;
std::string snippet_name = "";
std::string snippet_code = "";
std::string snippet_description = "";

//new snippet editor
TextEditor text_editor;
int open_snippets = 0;
const char* current_snippet_name = { 0 };
int currently_open_snippet = -1;
int old_open_snippet = -1;

auto lang = TextEditor::LanguageDefinition::CPlusPlus();
std::vector<TextEditor*> text_editors;

//snippet editor tab
TextEditor edit_editor;
std::vector<std::string> open_snippet_names;
std::vector<std::string> open_snippet_paths;
std::string current_code = "";
std::string current_description = "";
std::string description_cache = "";
bool update_tab = false;

//floating windows
int floating_windows = 0;
bool float_window = false;
std::vector<std::string> floating_window_names;
std::vector<std::string> floating_window_code;

//git integration
bool git_not_found = false;
bool cloning_git = false;
std::string gist_link = "";
bool command_executed = false;
bool command_fail = false;
bool command_success = false;

//settings variables
bool autosave = false;
float size_offset = 0;

void FilterSnippets() {
    size_t filter = 0;
    int index = 0;
    filtered_snippets.clear();
    index_cache.clear();
    for (const std::string& snippet : snippets) {
        if ((filter = snippet.find((std::string)search_bar)) != std::string::npos) {
            filtered_snippets.push_back(snippet.c_str());
            index_cache.push_back(index);
        }
        index++;
    }
}

void BuildDir(const std::string& path) {
    std::vector<std::string> dirs;
    std::string folder;
    for (char c : path) {
        if (c != '/') {
            folder += c;
        }
        else {
            if (!folder.empty()) {
                dirs.push_back(folder);
                folder = "";
            }
        }
    }
    if (!folder.empty()) {
        dirs.push_back(folder);
    }

    std::string dirPath;
    for (const auto& dir : dirs) {
        dirPath += dir + '/';
        if (_mkdir(dirPath.c_str()) != 0) {
            if (errno == EEXIST) {
                std::cout << "Directory " << dirPath << " already exists\n";
            }
            else {
                std::cout << "Failed to create directory " << dirPath << "\n";
            }
        }
    }
}
 
void SaveSnippet() {
    if (snippet_name != "" && std::find(snippets.begin(), snippets.end(), snippet_name) == snippets.end()) {
        if (!DirectoryExists("snippets/"))
            BuildDir("snippets/");

        std::ofstream out("snippets/" + snippet_name + ".snippet");
        out << snippet_name << std::endl;
        out << snippet_code;
        out << "[SNIPPET_CODE_END]" << std::endl;
        out << snippet_description;
        out.close();

        snippets.push_back(snippet_name);
        snippet_paths.push_back("snippets/" + snippet_name + ".snippet");
    }

    snippet_name.clear();
    snippet_description.clear();
    snippet_code.clear();
}

void CopyFromGist(std::string path) {
    snippet_description.clear();
    for (const fs::v1::path& entry : fs::directory_iterator(path)) {
        if (fs::is_regular_file(entry)) {
            snippet_name.clear();
            snippet_code.clear();
            snippet_name = GetFileNameWithoutExt(entry.filename().string().c_str());
            std::ifstream in(entry);
            std::string temp;
            while (std::getline(in, temp)) {
                snippet_code += temp + "\n";
            }
            in.close();
            SaveSnippet();
        }
    }
}

void SetupTextEditor(TextEditor* editor) {
    editor->SetLanguageDefinition(lang);
    editor->SetHandleKeyboardInputs(true);
    editor->SetHandleMouseInputs(true);
    editor->SetColorizerEnable(true);
    editor->SetReadOnly(false);
    editor->SetPalette(TextEditor::GetDarkPalette());
}

void LoadSnippets() {
    if (!DirectoryExists("snippets/"))
        return;

    for (const fs::v1::path& entry : fs::directory_iterator("snippets/")) {
        if (IsFileExtension(entry.filename().string().c_str(), ".snippet")) {
            std::ifstream in(entry.string());
            std::string temp;
            std::getline(in, temp);
            snippets.push_back(temp);
            snippet_paths.push_back(entry.string());
            in.close();
        }
    }
}

void GetCodeAndDescription(const std::string& path, std::string& code, std::string& description) {
    std::ifstream in(path);
    code = "";
    description = "";
    std::string temp;
    std::getline(in, temp);
    size_t code_getter;
    while (std::getline(in, temp) && (code_getter = temp.find("[SNIPPET_CODE_END]")) == std::string::npos)
        code += (code == "" ? temp : "\n" + temp);
    while (std::getline(in, temp))
        description += (description == "" ? temp : "\n" + temp);
    in.close();
}

void UpdateFileContents(const std::string& path, const std::string& name, const std::string& code, const std::string& description) {
    std::ofstream out(path);
    out << name << std::endl;
    out << code;
    out << "[SNIPPET_CODE_END]" << std::endl;
    out << description;
    out.close();
}

void OpenSnippet() {
    if (selected_snippet > index_cache.size() - 1 || selected_snippet < 0)
        return;

    for (const fs::v1::path& entry : fs::directory_iterator("snippets/")) {
        if (IsFileExtension(entry.filename().string().c_str(), ".snippet")) {
            if (entry.filename().string() == snippets[index_cache[selected_snippet]] + ".snippet") {
                if (std::find(open_snippet_names.begin(), open_snippet_names.end(), snippets[index_cache[selected_snippet]]) != open_snippet_names.end())
                    return;
                open_snippets++;
                open_snippet_names.push_back(snippets[index_cache[selected_snippet]]);
                open_snippet_paths.push_back(snippet_paths[index_cache[selected_snippet]]);
                break;
            }
        }
    }
}

void UpdateLanguageDefinition(std::string langdef) {
    if (langdef == "C")
        for (TextEditor* editor : text_editors)
            editor->SetLanguageDefinition(TextEditor::LanguageDefinition::C());
    else if (langdef == "C++")
        for (TextEditor* editor : text_editors)
            editor->SetLanguageDefinition(TextEditor::LanguageDefinition::CPlusPlus());
    else if (langdef == "HLSL")
        for (TextEditor* editor : text_editors)
            editor->SetLanguageDefinition(TextEditor::LanguageDefinition::HLSL());
    else if (langdef == "GLSL")
        for (TextEditor* editor : text_editors)
            editor->SetLanguageDefinition(TextEditor::LanguageDefinition::GLSL());
    else if (langdef == "Python")
        for (TextEditor* editor : text_editors)
            editor->SetLanguageDefinition(TextEditor::LanguageDefinition::Python());
    else if (langdef == "SQL")
        for (TextEditor* editor : text_editors)
            editor->SetLanguageDefinition(TextEditor::LanguageDefinition::SQL());
    else if (langdef == "AngelScript")
        for (TextEditor* editor : text_editors)
            editor->SetLanguageDefinition(TextEditor::LanguageDefinition::AngelScript());
    else if (langdef == "Lua")
        for (TextEditor* editor : text_editors)
            editor->SetLanguageDefinition(TextEditor::LanguageDefinition::Lua());
    else if (langdef == "C#")
        for (TextEditor* editor : text_editors)
            editor->SetLanguageDefinition(TextEditor::LanguageDefinition::CSharp());
    else if (langdef == "Json")
        for (TextEditor* editor : text_editors)
            editor->SetLanguageDefinition(TextEditor::LanguageDefinition::Json());
    else if (langdef == "Java")
        for (TextEditor* editor : text_editors)
            editor->SetLanguageDefinition(TextEditor::LanguageDefinition::Java());
}

void UpdateColorPalette(std::string palette) {
    if (palette == "mariana")
        for (TextEditor* editor : text_editors)
            editor->SetPalette(TextEditor::GetMarianaPalette());
    else if (palette == "dark")
        for (TextEditor* editor : text_editors)
            editor->SetPalette(TextEditor::GetDarkPalette());
    else if (palette == "light")
        for (TextEditor* editor : text_editors)
            editor->SetPalette(TextEditor::GetLightPalette());
    else if (palette == "retroblue")
        for (TextEditor* editor : text_editors)
            editor->SetPalette(TextEditor::GetRetroBluePalette());
}

void SaveSettings() {
    std::ofstream out("settings.ini");
    out << autosave << std::endl;
    out << size_offset << std::endl;
    out << edit_editor.GetLanguageDefinitionName() << std::endl;
    if (edit_editor.GetPalette() == TextEditor::GetMarianaPalette())
        out << "mariana\n";
    else if (edit_editor.GetPalette() == TextEditor::GetDarkPalette())
        out << "dark\n";
    else if (edit_editor.GetPalette() == TextEditor::GetLightPalette())
        out << "light\n";
    else if (edit_editor.GetPalette() == TextEditor::GetRetroBluePalette())
        out << "retroblue\n";
    out.close();
}

void LoadSettings() {
    if (FileExists("settings.ini")) {
        std::string temp;
        std::ifstream in("settings.ini");
        in >> autosave;
        in >> size_offset;
        in >> temp;
        UpdateLanguageDefinition(temp);
        in >> temp;
        UpdateColorPalette(temp);
        in.close();
    }
}

int main()
{
    Image icon = LoadImage("icon.png");
    InitWindow(1200, 600, "Snippet Manager");
    SetWindowMinSize(400, 200);
    SetWindowState(FLAG_WINDOW_RESIZABLE | FLAG_VSYNC_HINT);
    SetWindowIcon(icon);
    rlImGuiSetup(true);
    SetGuiStyle();
    SetupTextEditor(&text_editor);
    SetupTextEditor(&edit_editor);

    text_editor.SetTabSize(3);
    edit_editor.SetTabSize(3);
    text_editors.push_back(&text_editor);
    text_editors.push_back(&edit_editor);
    
    LoadSnippets();
    LoadSettings();

    while (!WindowShouldClose() && !quit) {

        ClearBackground(bottom_dark);
        rlImGuiBegin();

        // Window scale handling //
        if (!IsWindowFullscreen()) {
            ScreenWidth = GetScreenWidth();
            ScreenHeight = GetScreenHeight();
        }
        else {
            ScreenWidth = GetMonitorWidth(GetCurrentMonitor());
            ScreenHeight = GetMonitorHeight(GetCurrentMonitor());
        }
        if (IsKeyPressed(KEY_F11)) {
            if (!IsWindowFullscreen()) {
                MaximizeWindow();
                ToggleFullscreen();
            }
            else
                ToggleFullscreen();
        }
        //---------------------------------//

        // Main Toolbar //
        if (ImGui::BeginMainMenuBar()) {
            if (ImGui::BeginMenu("File")) {
                if (ImGui::MenuItem("New", "(Ctrl+E)")) {
                    if (!creating_snippet) {
                        creating_snippet = true;
                        text_editor.SetText("");
                    }
                }
                if (ImGui::MenuItem("Save", "(Ctrl+S)")) {
                    if (open_snippets > 0 && !save) save = true;
                }
                if (ImGui::MenuItem("Close", "(Ctrl+W)")) {
                    if (open_snippets > 0 && !close_tab) close_tab = true;
                }
                if (ImGui::MenuItem("Delete", "(Ctrl+Q)")) {
                    if (!deleting) deleting = true;
                }
                ImGui::Separator();
                if (ImGui::MenuItem("Copy", "(Ctrl+C)")) {
                    ImGuiIO& io = ImGui::GetIO();
                    if (!io.WantTextInput && open_snippets > 0)
                        ImGui::SetClipboardText(edit_editor.GetText().c_str());
                }
                if (ImGui::MenuItem("Paste", "(Ctrl+V)")) {
                    if (!creating_snippet && open_snippets <= 0 && floating_windows <= 0) {
                        creating_snippet = true;
                        text_editor.SetText(ImGui::GetClipboardText());
                    }
                }
                ImGui::Separator();
                if (ImGui::MenuItem("Exit", "(Esc)")) {
                    quit = true;
                }
                ImGui::EndMenu();
            }
            if (ImGui::BeginMenu("Settings")) {
                if (ImGui::MenuItem(autosave ? "Disable automatic saves" : "Enable automatic saves")) {
                    autosave = !autosave;
                }
                if (ImGui::BeginMenu("Code Editor")) {
                    if (ImGui::BeginMenu("Syntax")) {
                        if (ImGui::MenuItem("C")) {
                            for (TextEditor* editor : text_editors)
                                editor->SetLanguageDefinition(TextEditor::LanguageDefinition::C());
                        }
                        if (ImGui::MenuItem("C++")) {
                            for (TextEditor* editor : text_editors)
                                editor->SetLanguageDefinition(TextEditor::LanguageDefinition::CPlusPlus());
                        }
                        if (ImGui::MenuItem("HLSL")) {
                            for (TextEditor* editor : text_editors)
                                editor->SetLanguageDefinition(TextEditor::LanguageDefinition::HLSL());
                        }
                        if (ImGui::MenuItem("GLSL")) {
                            for (TextEditor* editor : text_editors)
                                editor->SetLanguageDefinition(TextEditor::LanguageDefinition::GLSL());
                        }
                        if (ImGui::MenuItem("Python")) {
                            for (TextEditor* editor : text_editors)
                                editor->SetLanguageDefinition(TextEditor::LanguageDefinition::Python());
                        }
                        if (ImGui::MenuItem("SQL")) {
                            for (TextEditor* editor : text_editors)
                                editor->SetLanguageDefinition(TextEditor::LanguageDefinition::SQL());
                        }
                        if (ImGui::MenuItem("AngelScript")) {
                            for (TextEditor* editor : text_editors)
                                editor->SetLanguageDefinition(TextEditor::LanguageDefinition::AngelScript());
                        }
                        if (ImGui::MenuItem("Lua")) {
                            for (TextEditor* editor : text_editors)
                                editor->SetLanguageDefinition(TextEditor::LanguageDefinition::Lua());
                        }
                        if (ImGui::MenuItem("C#")) {
                            for (TextEditor* editor : text_editors)
                                editor->SetLanguageDefinition(TextEditor::LanguageDefinition::CSharp());
                        }
                        if (ImGui::MenuItem("Json")) {
                            for (TextEditor* editor : text_editors)
                                editor->SetLanguageDefinition(TextEditor::LanguageDefinition::Json());
                        }
                        if (ImGui::MenuItem("Java")) {
                            for (TextEditor* editor : text_editors)
                                editor->SetLanguageDefinition(TextEditor::LanguageDefinition::Java());
                        }
                        ImGui::EndMenu();
                    }
                    if (ImGui::BeginMenu("Palette")) {
                        if (ImGui::MenuItem("Mariana")) {
                            for (TextEditor* editor : text_editors)
                                editor->SetPalette(TextEditor::GetMarianaPalette());
                        }
                        if (ImGui::MenuItem("Dark")) {
                            for (TextEditor* editor : text_editors)
                                editor->SetPalette(TextEditor::GetDarkPalette());
                        }
                        if (ImGui::MenuItem("Light")) {
                            for (TextEditor* editor : text_editors)
                                editor->SetPalette(TextEditor::GetLightPalette());
                        }
                        if (ImGui::MenuItem("Retro Blue")) {
                            for (TextEditor* editor : text_editors)
                                editor->SetPalette(TextEditor::GetRetroBluePalette());
                        }
                        ImGui::EndMenu();
                    }
                    ImGui::EndMenu();
                }
                ImGui::EndMenu();
            }
            if (ImGui::BeginMenu("Git")) {
                if (ImGui::MenuItem("Add snippets from gist")) {
                    int returnCode = std::system("git --version");
                    if (returnCode == 0)
                        cloning_git = true;
                    else
                        git_not_found = true;
                }
                ImGui::EndMenu();
            }
            ImGui::EndMainMenuBar();
        }
        //----------------------------------//

        FilterSnippets();

        // Right side window //
        ImGui::SetNextWindowPos({ 0, 19 });
        ImGui::SetNextWindowSizeConstraints({ 20, ScreenHeight }, { ScreenWidth - 30, ScreenHeight });
        ImGui::SetNextWindowSize({ 200, ScreenHeight }, ImGuiCond_FirstUseEver);
        if (ImGui::Begin("Snippets", NULL, ImGuiWindowFlags_NoCollapse)) {
            size_offset = ImGui::GetWindowWidth() - 200;
            ImGui::SetNextItemWidth(ImGui::GetColumnWidth());
            ImGui::InputText(" ", &search_bar);
            ImGui::Spacing();
            ImGui::BeginChild("snips", { 0, ScreenHeight - 77 });
            ImGui::SetNextItemWidth(ImGui::GetColumnWidth());
            ImGui::ListBox(" ", &selected_snippet, filtered_snippets.data(), filtered_snippets.size(), snippets.size());
            ImGui::EndChild();
            ImGui::End();
        }
        //----------------------------------//

        // Snippet viewer tabs //
        ImGui::SetNextWindowPos({ 200 + size_offset, 19 });
        ImGui::SetNextWindowSize({ ScreenWidth - 200 - size_offset,  ScreenHeight - 15 });
        if (ImGui::Begin("Snippet Editor", NULL, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoBringToFrontOnFocus)) {
            if (ImGui::BeginTabBar("snippets", ImGuiTabBarFlags_FittingPolicyScroll)) {
                if (open_snippets == 0) {
                    currently_open_snippet = -1;
                    description_cache = "";
                }
                for (int i = 0; i < open_snippets; i++) {
                    bool tab_open = true;
                    if (ImGui::BeginTabItem(open_snippet_names[i].c_str(), &tab_open)) {
                        if (i != currently_open_snippet || update_tab) { // update handling
                            currently_open_snippet = i;
                            edit_editor.SetText("");
                            GetCodeAndDescription(open_snippet_paths[i], current_code, current_description);
                            edit_editor.SetText(current_code);
                            update_tab = false;
                        }
                        if (delete_confirmed) tab_open = false;
                        edit_editor.Render("id_edit");
                        ImGui::Spacing();
                        ImGui::InputTextMultiline(" ", &current_description, { ImGui::GetColumnWidth(), 100 }, ImGuiInputTextFlags_AllowTabInput);
                        if (autosave ? edit_editor.IsTextChanged() || (current_description != description_cache) : save) {
                            UpdateFileContents(open_snippet_paths[i], open_snippet_names[i], edit_editor.GetText(), current_description);
                            save = false;
                        }
                        description_cache = current_description;
                        if (float_window) {
                            float_window = false;
                            if (std::find(floating_window_names.begin(), floating_window_names.end(), open_snippet_names[i]) == floating_window_names.end()) {
                                floating_windows++;
                                floating_window_names.push_back(open_snippet_names[i]);
                                floating_window_code.push_back(edit_editor.GetText());
                            }
                        }
                        ImGui::EndTabItem();
                    }
                    if (close_tab) {
                        close_tab = false;
                        tab_open = false;
                    }
                    if (!tab_open) {
                        update_tab = true;
                        std::vector<std::string> new_open_names;
                        std::vector<std::string> new_open_paths;
                        for (int j = 0; j < open_snippets; j++) {
                            if (j != i) {
                                new_open_names.push_back(open_snippet_names[j]);
                                new_open_paths.push_back(open_snippet_paths[j]);
                            }
                            else
                                if (j == i && delete_confirmed) {
                                    delete_confirmed = false;
                                    std::vector<std::string> new_snippet_names;
                                    for (const std::string snippet : snippets) {
                                        if (snippet == open_snippet_names[j]) {
                                            std::remove(open_snippet_paths[j].c_str());
                                        }
                                        else
                                            new_snippet_names.push_back(snippet);
                                    }
                                    snippets.clear();
                                    snippets = new_snippet_names;
                                }
                        }
                        open_snippet_names.clear();
                        open_snippet_paths.clear();
                        open_snippet_names = new_open_names;
                        open_snippet_paths = new_open_paths;
                        open_snippets--;
                    }
                }
                ImGui::EndTabBar();
            }
            ImGui::End();
        }
        //---------------------------//

        // Floating windows //
        for (int i = 0; i < floating_windows; i++) {
            bool tab_open = true;
            ImGui::SetNextWindowSize({ 300, 150 }, ImGuiCond_Once);
            if (ImGui::Begin(floating_window_names[i].c_str(), &tab_open)) {
                ImGui::InputTextMultiline(" ", &floating_window_code[i], { ImGui::GetColumnWidth(), ImGui::GetWindowHeight() });
            }
            ImGui::End();
            if (!tab_open) {
                std::vector<std::string> new_names;
                std::vector<std::string> new_code;
                for (int j = 0; j < floating_windows; j++) {
                    if (j != i) {
                        new_names.push_back(floating_window_names[j]);
                        new_code.push_back(floating_window_code[j]);
                    }
                }
                floating_window_names.clear();
                floating_window_code.clear();
                floating_window_names = new_names;
                floating_window_code = new_code;
                floating_windows--;
            }
        }
        //--------------------------//

        // Add snippet dialog //
        if (creating_snippet) {
            ImGui::SetNextWindowSize({ 400, 400 }, ImGuiCond_Once);
            ImGui::SetNextWindowCollapsed(false);
            ImGui::SetNextWindowSizeConstraints({ 100, 100 }, { 1000, 1000 });
            if (ImGui::Begin("Snippet Creation")) {
                ImGui::Text("Snippet Name");
                ImGui::SameLine();
                ImGui::SetNextItemWidth(ImGui::GetColumnWidth());
                ImGui::InputText(" ", &snippet_name);
                ImGui::Spacing();
                ImGui::Text("Snippet Code");
                text_editor.Render("editor");
                ImGui::Spacing(); ImGui::Spacing();
                ImGui::Text("Snippet Description");
                ImGui::PushID(1);
                ImGui::InputTextMultiline(" ", &snippet_description, { ImGui::GetColumnWidth(), 100 }, ImGuiInputTextFlags_AllowTabInput);
                ImGui::PopID();
                ImGui::Spacing();
                if (ImGui::Button("Save Snippet", { ImGui::GetColumnWidth(), 25 })) {
                    if (snippet_name != "" && snippet_name.size() <= 100 && std::find(snippets.begin(), snippets.end(), snippet_name) == snippets.end()) {
                        snippet_code = text_editor.GetText();
                        SaveSnippet();
                        text_editor.SetText(snippet_code);
                        creating_snippet = false;
                    }
                }
            }
            else {
                creating_snippet = false;
                snippet_name.clear();
                snippet_description.clear();
                snippet_code.clear();
                text_editor.SetText(snippet_code);
            }
            ImGui::End();
        }
        //---------------------------//

        // Delete file confirmation popup //
        if (deleting && open_snippets > 0) {
            ImGui::SetNextWindowSize({ 350, 150 }, ImGuiCond_Once);
            ImGui::SetNextWindowPos({ (ScreenWidth - 350) / 2, (ScreenHeight - 150) / 2 }, ImGuiCond_Once);
            if (ImGui::Begin("Delete file confirmation", &deleting, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize)) {
                ImGui::NewLine();
                ImGui::SetCursorPosX(25);
                ImGui::Text("Are you sure you want to delete this file?");
                ImGui::NewLine(); ImGui::NewLine();
                ImGui::SetCursorPosX(90);
                if (ImGui::Button("Yes", { 70, 25 })) { delete_confirmed = true; deleting = false; }
                ImGui::SameLine();
                ImGui::SetCursorPosX(190);
                if (ImGui::Button("No", { 70, 25 })) deleting = false;
                ImGui::End();
            }
        }
        else
            deleting = false;
        //--------------------------------//

        // Git not found popup //
        if (git_not_found) {
            ImGui::SetNextWindowSize({ 350, 150 }, ImGuiCond_Once);
            ImGui::SetNextWindowPos({ (ScreenWidth - 350) / 2, (ScreenHeight - 150) / 2 }, ImGuiCond_Once);
            if (ImGui::Begin("Git version check failed", &git_not_found, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize)) {
                ImGui::NewLine();
                ImGui::Spacing();
                ImGui::SetCursorPosX(60);
                ImGui::Text("Failed to verify git installation");
                ImGui::NewLine(); ImGui::NewLine();
                ImGui::SetCursorPosX(150);
                if (ImGui::Button("Ok", { 50, 25 })) git_not_found = false;
                ImGui::End();
            }
        }
        //---------------------------//

        // Clone gist popup & actions //
        if (cloning_git) {
            ImGui::SetNextWindowSize({ 350, 150 }, ImGuiCond_Once);
            ImGui::SetNextWindowPos({ (ScreenWidth - 350) / 2, (ScreenHeight - 150) / 2 }, ImGuiCond_Once);
            if (ImGui::Begin("Gist snippet cloning", &cloning_git, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize)) {
                ImGui::NewLine(); ImGui::Spacing(); ImGui::Spacing();
                ImGui::SetNextItemWidth(ImGui::GetColumnWidth());
                ImGui::InputTextWithHint(" ", "https://gist.github.com/address.git", &gist_link);
                ImGui::NewLine(); ImGui::NewLine();
                ImGui::SetCursorPosX(90);
                if (ImGui::Button("Clone", { 70, 25 })) {
                    size_t is_valid_1 = gist_link.find("https://gist.github.com");
                    size_t is_valid_2 = gist_link.find(".git");
                    if (is_valid_1 != std::string::npos && is_valid_2 != std::string::npos) {
                        cloning_git = false;
                        command_executed = true;
                    }
                }
                ImGui::SameLine();
                ImGui::SetCursorPosX(190);
                if (ImGui::Button("Cancel", { 70, 25 })) { cloning_git = false; gist_link.clear(); }
                ImGui::End();
            }
        }

        if (command_executed) {
            command_executed = false;
            std::string command = "git clone " + gist_link + " " + (std::string)GetWorkingDirectory() + "/gist_snippets/";
            int result = std::system(command.c_str());
            if (result == 0) {
                CopyFromGist("gist_snippets/");
                std::string remover = "attrib -r " + (std::string)GetWorkingDirectory() + "\\*.* /s";
                std::system(remover.c_str());
                fs::remove_all("gist_snippets/");
                command_success = true;
            }
            else {
                command_fail = true;
            }
        }

        if (command_fail || command_success) {
            std::string error = (command_fail ? "Failed to clone gist " : "Successfully cloned gist ") + gist_link;
            ImGui::SetNextWindowSize({ 25 + ImGui::CalcTextSize(error.c_str()).x, 150});
            ImGui::SetNextWindowPos({ (ScreenWidth - 25 - ImGui::CalcTextSize(error.c_str()).x) / 2, (ScreenHeight - 150) / 2 }, ImGuiCond_Once);
            if (ImGui::Begin((command_fail ? "Command execution failed" : "Command executed successfully"), NULL, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize)) {
                ImGui::NewLine();
                ImGui::Spacing();
                ImVec2 content_max = ImGui::GetWindowContentRegionMax();
                ImVec2 content_min = ImGui::GetWindowContentRegionMin();
                ImGui::SetCursorPosX((content_min.x + content_max.x - ImGui::CalcTextSize(error.c_str()).x) / 2.0f);
                ImGui::Text(error.c_str());
                ImGui::NewLine(); ImGui::NewLine();
                ImGui::SetCursorPosX((content_min.x + content_max.x - 50) / 2.0f);
                if (ImGui::Button("Ok", { 50, 25 })) {
                    command_fail = false; 
                    command_success = false; 
                    gist_link.clear();
                }
                ImGui::End(); //https://gist.github.com/.git
            }
        }
        //--------------------------------//

        // Some shortcuts //
        if (IsKeyDown(KEY_LEFT_CONTROL) && IsKeyPressed(KEY_E) && !creating_snippet) { creating_snippet = true; text_editor.SetText(""); }
        if (IsMouseButtonReleased(MOUSE_BUTTON_LEFT)) { OpenSnippet(); selected_snippet = -1; }
        if (IsKeyDown(KEY_LEFT_CONTROL) && IsKeyPressed(KEY_F)) float_window = true;
        if (IsKeyDown(KEY_LEFT_CONTROL) && IsKeyPressed(KEY_V)) { if (!cloning_git && !creating_snippet && open_snippets <= 0 && floating_windows <= 0) { creating_snippet = true; text_editor.SetText(ImGui::GetClipboardText()); } }
        if (IsKeyDown(KEY_LEFT_CONTROL) && IsKeyPressed(KEY_C)) {
            ImGuiIO& io = ImGui::GetIO();
            if (!io.WantTextInput && open_snippets > 0)
                ImGui::SetClipboardText(edit_editor.GetText().c_str());
        }
        if (IsKeyDown(KEY_LEFT_CONTROL) && IsKeyPressed(KEY_Q)) if (!deleting) deleting = true;
        if (IsKeyDown(KEY_LEFT_CONTROL) && IsKeyPressed(KEY_S)) if (open_snippets > 0 && !save) save = true;
        if (IsKeyDown(KEY_LEFT_CONTROL) && IsKeyPressed(KEY_W)) if (open_snippets > 0 && !close_tab) close_tab = true;
        //------------------//

        rlImGuiEnd();
        BeginDrawing();
        EndDrawing();
    }

    UnloadImage(icon);
    SaveSettings();
    rlImGuiShutdown();
    CloseWindow();

    return 0;
}
