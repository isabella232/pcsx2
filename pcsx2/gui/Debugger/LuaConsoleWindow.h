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

#pragma once

#include "App.h"

#include "Utilities/wxGuiTools.h"

#ifdef WIN32
#include "windows/Win32.h"
#endif

#include <wx/fswatcher.h>

#include <map>

class LuaConsoleWindow;
class LuaConsoleWindow : public wxFrame
{
public:
	LuaConsoleWindow(wxWindow* parent, wxString filename = wxEmptyString);

	void RunScript();
	void RestartScript(wxString reason);

	static LuaConsoleWindow* FindByFileName(wxString filename);
	static LuaConsoleWindow* FindByUID(int uid);
	static void New();
	static wxString OpenLuaScript(wxString filename, wxString extraDirToCheck);
	static void CloseAll();

	wxDECLARE_EVENT_TABLE();

protected:
	wxTextCtrl* m_scriptPathText;
	wxButton* m_browseButton;
	wxButton* m_editButton;
	wxButton* m_stopButton;
	wxButton* m_runButton;
	wxTextCtrl* m_consoleText;

	static int uidCounter;
	static std::map<int, LuaConsoleWindow *> UidToLuaConsoleWindows;

	int m_uid;
	bool m_running;
	bool m_closeOnStop;
	wxFileName m_filename;
	wxFileSystemWatcher m_fileWatcher;
	wxDateTime m_lastModified;

	void RunScript(wxString filename);

	void OnDropFiles(wxDropFilesEvent& event);
	void OnLuaPathChange(wxCommandEvent& event);
	void OnLuaBrowse(wxCommandEvent& event);
	void OnLuaEdit(wxCommandEvent& event);
	void OnLuaRun(wxCommandEvent& event);
	void OnLuaStop(wxCommandEvent& event);
	void OnFileSystemEvent(wxFileSystemWatcherEvent& event);
	void OnClose(wxCloseEvent& event);
	virtual bool Destroy() override;

	void UpdateFileSystemWatcher();
	void UpdateEditButtonLabel();

	void Print(wxString str);
	void PrintLine(wxString str);
	void ClearConsole();
	void OnStart();
	void OnStop(bool statusOK);

	static void Print_C(int uid, const char* str);
	static void OnStart_C(int uid);
	static void OnStop_C(int uid, bool statusOK);

	static bool DemandLua();
};
