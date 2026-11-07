#pragma once

// Control IDs for the main window and dialogs.
// These must not collide with system values (0-0x7FFF).

namespace DriverNest::Ui {

// Main window controls
constexpr int IDC_SEARCH           = 1001;
constexpr int IDC_DEVICE_LIST     = 1002;
constexpr int IDC_DETAILS         = 1003;
constexpr int IDC_SPLITTER        = 1004;
constexpr int IDC_STATUS_BAR      = 1005;

// Toolbar buttons
constexpr int ID_APP_SCAN         = 2001;
constexpr int ID_APP_INSTALL      = 2002;
constexpr int ID_APP_UNDO         = 2003;
constexpr int ID_APP_SETTINGS     = 2004;
constexpr int ID_APP_THEME_TOGGLE = 2005;
constexpr int ID_APP_REFRESH      = 2006;
constexpr int ID_APP_SELECT_ALL   = 2007;

// Bottom installation progress / pipeline bar
constexpr int IDC_PROGRESS_PANEL  = 2008;
constexpr int IDC_PROGRESS_BAR    = 2009;
constexpr int IDC_PROGRESS_TEXT   = 2010;
constexpr int IDC_PROGRESS_CANCEL = 2011;
constexpr int IDC_PROGRESS_RESCAN = 2012;
constexpr int IDC_PROGRESS_CLOSE  = 2013;

// Details pane buttons
constexpr int IDC_DET_INSTALL         = 3001;
constexpr int IDC_DET_CHOOSE_ANOTHER  = 3002;
constexpr int IDC_DET_FORCE           = 3003;

// Dialog controls
constexpr int IDC_ABOUT_COPY_DIAG = 4001;
constexpr int IDC_ABOUT_CLOSE     = 4002;

// Custom window messages
constexpr int WM_APP_UPDATE_INSTALL_BUTTON = WM_APP + 1;
constexpr int WM_APP_BEGIN_INSTALL         = WM_APP + 2;
constexpr int WM_APP_BEGIN_UNDO           = WM_APP + 3;
constexpr int WM_APP_SCAN_COMPLETE        = WM_APP + 4;
constexpr int WM_APP_INSTALL_PROGRESS_UPDATE = WM_APP + 5;
constexpr int WM_APP_INSTALL_ITEM_UPDATE     = WM_APP + 6;
constexpr int WM_APP_INSTALL_ALL_COMPLETE   = WM_APP + 7;
constexpr int WM_APP_TRIGGER_RESCAN         = WM_APP + 8;

// Animation timers
constexpr UINT_PTR TIMER_SKELETON_ID = 5001;

} // namespace DriverNest::Ui
