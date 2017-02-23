/*  PCSX2 - PS2 Emulator for PCs
 *  Copyright (C) 2002-2014  PCSX2 Dev Team
 *
 *  PCSX2 is free software: you can redistribute it and/or modify it under the terms
 *  of the GNU Lesser General Public License as published by the Free Software Found-
 *  ation, either version 3 of the License, or (at your option) any later version.
 *
 *  PCSX2 is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 *  without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 *  PURPOSE.  See the GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along with PCSX2.
 *  If not, see <http://www.gnu.org/licenses/>.
 */

#include "PrecompiledHeader.h"
#include "LuaConsoleWindow.h"

#include "App.h"
#include "GSFrame.h"
#include "MainFrame.h"
#include "lua-engine.h"

#include <wx/mimetype.h>

enum
{
	EDIT_LUAPATH = wxID_HIGHEST + 1,
	BUTTON_LUABROWSE,
	BUTTON_LUAEDIT,
	BUTTON_LUASTOP,
	BUTTON_LUARUN,
};

wxBEGIN_EVENT_TABLE(LuaConsoleWindow, wxFrame)
	EVT_TEXT(EDIT_LUAPATH, LuaConsoleWindow::OnLuaPathChange)
	EVT_BUTTON(BUTTON_LUABROWSE, LuaConsoleWindow::OnLuaBrowse)
	EVT_BUTTON(BUTTON_LUAEDIT, LuaConsoleWindow::OnLuaEdit)
	EVT_BUTTON(BUTTON_LUASTOP, LuaConsoleWindow::OnLuaStop)
	EVT_BUTTON(BUTTON_LUARUN, LuaConsoleWindow::OnLuaRun)
	EVT_FSWATCHER(wxID_ANY, LuaConsoleWindow::OnFileSystemEvent)
	EVT_DROP_FILES(LuaConsoleWindow::OnDropFiles)
	EVT_CLOSE(LuaConsoleWindow::OnClose)
wxEND_EVENT_TABLE()

int LuaConsoleWindow::uidCounter = 0;
std::map<int, LuaConsoleWindow *> LuaConsoleWindow::UidToLuaConsoleWindows;

LuaConsoleWindow::LuaConsoleWindow(wxWindow* parent, wxString filename)
	: wxFrame(parent, wxID_ANY, L"Lua Script"),
	m_uid(uidCounter++),
	m_running(false),
	m_closeOnStop(false),
	m_filename(filename)
{
	if (!filename.IsEmpty()) {
		SetTitle(m_filename.GetFullName());
	}

	wxPanelWithHelpers(parent, wxVERTICAL);
	wxBoxSizer* topLevelSizer = new wxBoxSizer(wxVERTICAL);
	wxPanel* panel = new wxPanel(this, wxID_ANY);
	panel->SetSizer(topLevelSizer);

	// top
	wxBoxSizer* topSizer = new wxBoxSizer(wxVERTICAL);

	wxStaticText* scriptPathLabel = new wxStaticText(panel, wxID_ANY, _("Script File"));
	m_scriptPathText = new wxTextCtrl(panel, EDIT_LUAPATH, filename);

	topSizer->Add(scriptPathLabel, 0, 0);
	topSizer->Add(m_scriptPathText, 0, wxEXPAND | wxTOP, 1);

	// middle
	wxFlexGridSizer* middleSizer = new wxFlexGridSizer(0, 2, 0, 0);

	// left buttons
	wxBoxSizer* leftButtons = new wxBoxSizer(wxHORIZONTAL);

	m_browseButton = new wxButton(panel, BUTTON_LUABROWSE, _("Browse..."));
	m_browseButton->SetMinSize(wxSize(72, -1));
	m_editButton = new wxButton(panel, BUTTON_LUAEDIT, _("Edit"));
	m_editButton->SetMinSize(wxSize(72, -1));

	leftButtons->Add(m_browseButton, 0, wxRIGHT, 4);
	leftButtons->Add(m_editButton, 0, wxRIGHT, 4);

	// right buttons
	wxBoxSizer* rightButtons = new wxBoxSizer(wxHORIZONTAL);

	m_stopButton = new wxButton(panel, BUTTON_LUASTOP, _("Stop"));
	m_stopButton->SetMinSize(wxSize(72, -1));
	m_runButton = new wxButton(panel, BUTTON_LUARUN, _("Run"));
	m_runButton->SetMinSize(wxSize(72, -1));

	rightButtons->Add(m_stopButton, 0, wxLEFT, 4);
	rightButtons->Add(m_runButton, 0, wxLEFT, 4);

	middleSizer->AddGrowableCol(1);
	middleSizer->Add(leftButtons, 0, wxTOP | wxALIGN_LEFT, 2);
	middleSizer->Add(rightButtons, 0, wxTOP | wxALIGN_RIGHT, 2);

	// bottom
	wxBoxSizer* bottomSizer = new wxBoxSizer(wxVERTICAL);

	wxStaticText* consoleLabel = new wxStaticText(panel, wxID_ANY, _("Output Console"));

	m_consoleText = new wxTextCtrl(panel, wxID_ANY, wxEmptyString, wxDefaultPosition, wxDefaultSize,
		wxTE_READONLY | wxTE_MULTILINE);
	m_consoleText->SetBackgroundColour(wxSystemSettings::GetColour(wxSYS_COLOUR_BTNFACE));

	bottomSizer->Add(consoleLabel, 0, wxTOP | wxEXPAND, 8);
	bottomSizer->Add(m_consoleText, 1, wxTOP | wxEXPAND, 1);

	topLevelSizer->Add(topSizer, 0, wxLEFT | wxRIGHT | wxTOP | wxEXPAND, 8);
	topLevelSizer->Add(middleSizer, 0, wxLEFT | wxRIGHT | wxEXPAND, 8);
	topLevelSizer->Add(bottomSizer, 1, wxLEFT | wxRIGHT | wxBOTTOM | wxEXPAND, 8);

	m_runButton->SetDefault();

	panel->SetSizer(topLevelSizer);
	Layout();
	SetInitialSize(wxSize(408, 276));
	SetMinClientSize(panel->GetBestSize());

	DragAcceptFiles(true);

	UidToLuaConsoleWindows[m_uid] = this;
	OpenLuaContext(m_uid, Print_C, OnStart_C, OnStop_C);

	m_fileWatcher.SetOwner(this);
	UpdateFileSystemWatcher();
}

void LuaConsoleWindow::RunScript()
{
#ifdef WIN32
	GSFrame* gsFrame = wxGetApp().GetGsFramePtr();
	if (gsFrame && !m_scriptPathText->HasFocus()) {
		SetActiveWindow(gsFrame->GetHWND());
	}
#endif

	if (DemandLua()) {
		// If script is already m_running, it will be stopped automatically.
		RunLuaScriptFile(m_uid, m_filename.GetFullPath().mb_str());
	}
}

void LuaConsoleWindow::RunScript(wxString filename)
{
	m_filename = filename;
	SetTitle(m_filename.GetFullName());
	RunScript();
}

void LuaConsoleWindow::RestartScript(wxString reason)
{
	RequestAbortLuaScript(m_uid, reason);
	RunScript();
}

void LuaConsoleWindow::OnLuaPathChange(wxCommandEvent& event)
{
	UpdateFileSystemWatcher();
	UpdateEditButtonLabel();

	wxFileName filename(m_scriptPathText->GetValue());
	if (filename.FileExists()) {
		RunScript(filename.GetFullPath());
	}
}

void LuaConsoleWindow::UpdateFileSystemWatcher()
{
	wxFileName filename(m_scriptPathText->GetValue());

	m_fileWatcher.RemoveAll();
	if (filename.DirExists()) {
		// Note: Start watching a directory, not a file,
		//       since the file watching is not supported by wxWidgets 3.0
		if (filename.FileExists()) {
			m_lastModified = filename.GetModificationTime();
		}
		else {
			m_lastModified.ResetTime();
		}
		m_fileWatcher.Add(filename.GetPath(), wxFSW_EVENT_MODIFY);
	}
}

void LuaConsoleWindow::UpdateEditButtonLabel()
{
	wxFileName filename(m_scriptPathText->GetValue());
	bool readonly = false;
	bool isLuaFile = filename.GetExt().IsSameAs(L"lua", false);

	if (filename.FileExists()) {
		m_editButton->SetLabel(isLuaFile ? (readonly ? _("View") : _("Edit")) : _("Open"));
		m_editButton->Enable(true);
	}
	else {
		m_editButton->SetLabel(_("Create"));
		m_editButton->Enable(isLuaFile && !readonly);
	}
}

void LuaConsoleWindow::OnLuaBrowse(wxCommandEvent& event)
{
	wxArrayString luaFilterTypes;

	luaFilterTypes.Add(pxsFmt(_("Lua Script (%s)"), L".lua"));
	luaFilterTypes.Add(L"*.lua");

	luaFilterTypes.Add(_("All Files (*.*)"));
	luaFilterTypes.Add(L"*.*");

	wxFileDialog dialog(this, _("Load Lua Script"), wxEmptyString, m_filename.GetFullPath(),
		JoinString(luaFilterTypes, L"|"), wxFD_OPEN | wxFD_FILE_MUST_EXIST);

	if (dialog.ShowModal() == wxID_CANCEL) {
		return;
	}

	m_scriptPathText->SetValue(dialog.GetPath());
}

void LuaConsoleWindow::OnLuaEdit(wxCommandEvent& event)
{
	wxFileName filename(m_scriptPathText->GetValue());

	bool exists = filename.Exists();
	if (!exists) {
		wxFile file(filename.GetFullPath(), wxFile::write);
		if (file.IsOpened()) {
			exists = true;
			file.Close();

			UpdateFileSystemWatcher();
			UpdateEditButtonLabel();
		}
	}

	if (exists) {
		// tell the OS to open the file with its associated editor,
		// without blocking on it or leaving a command window open.
		wxString command;
		wxFileType::MessageParameters parameters(filename.GetFullPath());

		std::unique_ptr<wxFileType> luaFileType(wxTheMimeTypesManager->GetFileTypeFromExtension(L"lua"));
		if (luaFileType) {
			wxArrayString verbs;
			wxArrayString commands;
			luaFileType->GetAllCommands(&verbs, &commands, parameters);

			int index = verbs.Index(L"edit", false);
			if (index != wxNOT_FOUND) {
				// 1. Edit
				command = commands[index];
			}
			else {
				// 2. Open
				command = luaFileType->GetOpenCommand(filename.GetFullPath());
			}
		}

		if (command.IsEmpty()) {
			std::unique_ptr<wxFileType> textFileType(wxTheMimeTypesManager->GetFileTypeFromMimeType(L"text/plain"));
			if (textFileType) {
				// 3. Open with generic text/plain program
				command = textFileType->GetOpenCommand(filename.GetFullPath());
			}
		}

		if (!command.IsEmpty()) {
			if (wxExecute(command, wxEXEC_ASYNC) == 0) {
				wxMessageBox(_("Unable to open the script with the associated program."), _("Error"), wxICON_ERROR);
			}
		}
		else {
			wxMessageBox(_("Lua script does not have a program associated."), _("Error"), wxICON_ERROR);
		}
	}
}

void LuaConsoleWindow::OnLuaRun(wxCommandEvent& event)
{
	RunScript(m_scriptPathText->GetValue());
}

void LuaConsoleWindow::OnLuaStop(wxCommandEvent& event)
{
	PrintLine(L"user clicked stop button");

#ifdef WIN32
	GSFrame* gsFrame = wxGetApp().GetGsFramePtr();
	if (gsFrame) {
		SetActiveWindow(gsFrame->GetHWND());
	}
#endif

	StopLuaScript(m_uid);
}

void LuaConsoleWindow::OnFileSystemEvent(wxFileSystemWatcherEvent& event)
{
	if (event.GetChangeType() == wxFSW_EVENT_MODIFY) {
		wxFileName filename(m_scriptPathText->GetValue());
		if (filename.FileExists()) {
			wxDateTime lastModified = filename.GetModificationTime();
			if (lastModified != m_lastModified) {
				m_lastModified = lastModified;
				RestartScript(L"terminated to reload the script");
			}
		}
	}
}

void LuaConsoleWindow::OnDropFiles(wxDropFilesEvent& event)
{
	if (event.GetNumberOfFiles() == 1) {
		wxString* dropped = event.GetFiles();
		m_scriptPathText->SetValue(dropped[0]);
	}
	event.Skip();
}

void LuaConsoleWindow::OnClose(wxCloseEvent& event)
{
	PrintLine(L"user closed script window");
	if (m_running) { // prevent infinite loop caused by closing request by OnStop
		StopLuaScript(m_uid);
	}

	if (event.CanVeto()) {
		if (m_running) {
			// not stopped yet, wait to close until we are, otherwise we'll crash
			m_closeOnStop = true;
			event.Veto();
			return;
		}
	}

	Destroy();
}

bool LuaConsoleWindow::Destroy()
{
	// remove self from the window list
	UidToLuaConsoleWindows.erase(m_uid);

	return wxFrame::Destroy();
}

void LuaConsoleWindow::Print(wxString str)
{
	m_consoleText->AppendText(str);
}

void LuaConsoleWindow::PrintLine(wxString str)
{
	m_consoleText->AppendText(str);
	m_consoleText->AppendText(L"\n");
}

void LuaConsoleWindow::ClearConsole()
{
	m_consoleText->SetValue(wxEmptyString);
}

void LuaConsoleWindow::OnStart()
{
	m_running = true;

	m_browseButton->Enable(false); // disable browse while m_running because it misbehaves if clicked in a frameadvance loop
	m_stopButton->Enable(true);
	m_runButton->SetLabel(_("Restart"));
	ClearConsole();
}

void LuaConsoleWindow::OnStop(bool statusOK)
{
	m_running = false;

#ifdef WIN32
	HWND prevWindow = GetActiveWindow();
	SetActiveWindow(this->GetHWND()); // bring to front among other script/secondary windows, since a stopped script will have some message for the user that would be easier to miss otherwise
	GSFrame* gsFrame = wxGetApp().GetGsFramePtr();
	if (gsFrame && prevWindow == gsFrame->GetHWND()) {
		SetActiveWindow(prevWindow);
	}
#endif

	m_browseButton->Enable(true);
	m_stopButton->Enable(false);
	m_runButton->SetLabel(_("Run"));

	if (m_closeOnStop) {
		Close();
	}
}

void LuaConsoleWindow::Print_C(int uid, const char* str)
{
	if (UidToLuaConsoleWindows.count(uid)) {
		UidToLuaConsoleWindows[uid]->Print(str);
	}
}

void LuaConsoleWindow::OnStart_C(int uid)
{
	if (UidToLuaConsoleWindows.count(uid)) {
		UidToLuaConsoleWindows[uid]->OnStart();
	}
}

void LuaConsoleWindow::OnStop_C(int uid, bool statusOK)
{
	if (UidToLuaConsoleWindows.count(uid)) {
		UidToLuaConsoleWindows[uid]->OnStop(statusOK);
	}
}

bool LuaConsoleWindow::DemandLua()
{
#ifdef WIN32
	HMODULE mod = LoadLibrary(TEXT("lua51.dll"));
	if (!mod)
	{
		wxMessageBox(_("lua51.dll was not found. Please get it into your PATH or in the same directory as pcsx2.exe"), _("Error"), wxICON_ERROR);
		return false;
	}
	FreeLibrary(mod);
	return true;
#else
	return true;
#endif
}

LuaConsoleWindow* LuaConsoleWindow::FindByFileName(wxString filename)
{
	wxFileName targetPath(filename);
	for (const auto & pair : UidToLuaConsoleWindows) {
		LuaConsoleWindow* window = pair.second;
		if (window->m_filename.SameAs(targetPath)) {
			return window;
		}
	}
	return nullptr;
}

LuaConsoleWindow* LuaConsoleWindow::FindByUID(int uid)
{
	if (UidToLuaConsoleWindows.count(uid)) {
		return UidToLuaConsoleWindows[uid];
	}
	return nullptr;
}

void LuaConsoleWindow::New()
{
#ifdef WIN32
		HWND prevWindow = GetActiveWindow();
#endif

	LuaConsoleWindow* window = new LuaConsoleWindow(wxGetApp().GetMainFramePtr());
	window->Show();

#ifdef WIN32
	SetActiveWindow(prevWindow);
#endif
}

wxString LuaConsoleWindow::OpenLuaScript(wxString filename, wxString extraDirToCheck)
{
	wxFileName path(filename);
	if (!path.Exists()) {
		bool fileExists = false;
		if (!extraDirToCheck.IsEmpty()) {
			wxFileName extraPath(extraDirToCheck, filename);
			if (extraPath.Exists()) {
				path = extraPath;
				fileExists = true;
			}
		}

		if (!fileExists) {
			return wxString::Format(L"%s (\"%s\")", _("File not found."), filename);
		}
	}

	wxString fullpath = path.GetFullPath();
	LuaConsoleWindow* existingWindow = LuaConsoleWindow::FindByFileName(fullpath);
	if (existingWindow == nullptr) {
#ifdef WIN32
		HWND prevWindow = GetActiveWindow();
#endif

		LuaConsoleWindow* window = new LuaConsoleWindow(wxGetApp().GetMainFramePtr(), path.GetFullPath());
		window->Show();
		window->RunScript();

#ifdef WIN32
		SetActiveWindow(prevWindow);
#endif
	}
	else {
		existingWindow->RestartScript(L"terminated to restart because of a call to emu.openscript");
	}

	return wxEmptyString;
}

void LuaConsoleWindow::CloseAll()
{
	std::map<int, LuaConsoleWindow *> UidToLuaConsoleWindows(LuaConsoleWindow::UidToLuaConsoleWindows);

	for (const auto & pair : UidToLuaConsoleWindows) {
		LuaConsoleWindow* window = pair.second;
		window->Close();
	}
}
