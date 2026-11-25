#pragma once
#include <QString>
namespace OxitaTheme {
inline QString styleSheet() {
return QStringLiteral(R"(
QWidget { font-family:"Inter","Segoe UI",sans-serif; font-size:13px; color:#F4F4F4; }
QMainWindow,QWidget#appRoot { background:#171717; color:#F4F4F4; }
QLabel { background:transparent; color:#F4F4F4; }
QLabel#brand { color:#FFFFFF; font-size:24px; font-weight:900; letter-spacing:1px; }
QLabel#productName { color:#D0D0D0; font-size:10px; font-weight:800; letter-spacing:1.35px; }
QLabel#brandTagline { color:#B8B8B8; font-size:10px; font-weight:700; }
QLabel#muted { color:#BDBDBD; font-size:12px; }
QLabel#sectionTitle { color:#FFFFFF; font-size:15px; font-weight:800; }
QLabel#deviceDetails,QLabel#selectedDeviceDetails { background:#292929; border:1px solid #454545; border-radius:8px; padding:10px; color:#E5E5E5; }
QLabel#inventoryEmptyState { background:#292929; color:#BDBDBD; border:1px dashed #626262; border-radius:9px; padding:18px; font-size:13px; }
QFrame#topBar,QFrame#surface,QFrame#toolbar,QFrame#metricCard,QFrame#rightWorkspacePanel { background:#292929; border:1px solid #454545; border-radius:10px; }
QFrame#topBar { border-radius:11px; }
QGroupBox#actionPanel { background:#292929; border:1px solid #454545; border-radius:9px; margin-top:13px; padding:14px 11px 11px; color:#F4F4F4; font-weight:750; }
QGroupBox#actionPanel::title { subcontrol-origin:margin; left:12px; padding:0 7px; color:#F4F4F4; background:#292929; font-size:12px; font-weight:800; }
QLineEdit,QComboBox,QSpinBox,QDoubleSpinBox { background:#1D1D1D; color:#FFFFFF; border:1px solid #626262; border-radius:6px; padding:7px 9px; min-height:21px; selection-background-color:#C62836; selection-color:#FFFFFF; }
QLineEdit:hover,QComboBox:hover,QSpinBox:hover,QDoubleSpinBox:hover { border-color:#929292; }
QLineEdit:focus,QComboBox:focus,QSpinBox:focus,QDoubleSpinBox:focus { border:1px solid #C62836; background:#252525; }
QLineEdit:disabled,QComboBox:disabled,QSpinBox:disabled,QDoubleSpinBox:disabled { background:#303030; color:#888888; border-color:#444444; }
QComboBox { border-radius:8px; padding:8px 34px 8px 11px; min-height:24px; background:qlineargradient(x1:0,y1:0,x2:0,y2:1,stop:0 #333333,stop:1 #252525); border:1px solid #5A5A5A; }
QComboBox:hover { border-color:#888888; background:#383838; }
QComboBox:focus { border:1px solid #C62836; background:#303030; }
QComboBox::drop-down { border:0; width:28px; subcontrol-origin:padding; subcontrol-position:top right; }
QComboBox::down-arrow { width:8px; height:8px; }
QComboBox QAbstractItemView { background:#292929; color:#FFFFFF; border:1px solid #626262; selection-background-color:#5A252B; selection-color:#FFFFFF; outline:0; padding:5px; }
QPushButton { background:qlineargradient(x1:0,y1:0,x2:0,y2:1,stop:0 #414141,stop:1 #303030); color:#F0F0F0; border:1px solid #626262; border-radius:8px; padding:8px 13px; min-height:22px; font-weight:700; }
QPushButton:focus { border:1px solid #C62836; }
QPushButton#secondaryButton { background:qlineargradient(x1:0,y1:0,x2:0,y2:1,stop:0 #494949,stop:1 #343434); }
QPushButton:hover { background:#484848; border-color:#999999; color:#FFFFFF; }
QPushButton:pressed { background:#303030; }
QPushButton:focus { border:1px solid #C62836; }
QPushButton#primaryButton { background:qlineargradient(x1:0,y1:0,x2:0,y2:1,stop:0 #E13A48,stop:1 #B91F2D); color:#FFFFFF; border:1px solid #ED5260; border-radius:8px; padding:9px 16px; font-weight:800; }
QPushButton#primaryButton:hover { background:#A91E2A; border-color:#A91E2A; }
QPushButton#primaryButton:pressed { background:#8F1722; border-color:#8F1722; }
QPushButton:disabled { color:#888888; background:#303030; border-color:#454545; }
QTableWidget { background:#202020; alternate-background-color:#292929; color:#E6E6E6; border:0; gridline-color:transparent; selection-background-color:#5A252B; selection-color:#FFFFFF; font-size:12px; outline:0; }
QTableWidget::item { padding:7px 8px; border-bottom:1px solid #3B3B3B; }
QTableWidget::item:hover { background:#3A3A3A; }
QTableWidget::item:selected { background:#5A252B; color:#FFFFFF; }
QHeaderView { background:#343434; }
QHeaderView::section { background:#343434; color:#C7C7C7; border:0; border-bottom:1px solid #555555; padding:9px 8px; font-size:10px; font-weight:800; letter-spacing:.3px; }
QFrame#workspaceNavigation { background:#222222; border:1px solid #454545; border-radius:9px; }
QTabWidget::pane { border:1px solid #454545; background:#292929; border-radius:8px; top:-1px; }
QTabBar#workspaceTabs::tab { background:transparent; color:#BDBDBD; padding:12px 19px; border:0; border-bottom:3px solid transparent; margin-right:4px; font-weight:750; min-width:105px; }
QTabBar#workspaceTabs::tab:hover { color:#FFFFFF; background:#383838; border-radius:7px 7px 0 0; }
QTabBar#workspaceTabs::tab:selected { color:#FFFFFF; background:#303030; border-bottom:3px solid #C62836; }
QTabBar#workspaceTabs::tab:!selected { margin-top:2px; }
QTabBar::tab:hover { color:#FFFFFF; background:#3A3A3A; }
QTabBar::tab:selected { color:#FFFFFF; border-bottom:2px solid #C62836; background:#292929; }
QCheckBox { color:#E5E5E5; spacing:8px; }
QCheckBox::indicator { width:16px; height:16px; border:1px solid #777777; border-radius:4px; background:#1D1D1D; }
QCheckBox::indicator:hover { border-color:#C62836; }
QCheckBox::indicator:checked { background:#C62836; border-color:#C62836; }
QSplitter::handle:horizontal { background:#454545; width:7px; margin:8px 2px; border-radius:3px; }
QSplitter::handle:vertical { background:#454545; height:7px; margin:2px 8px; border-radius:3px; }
QSplitter::handle:hover { background:#C62836; }
QScrollArea { border:0; background:transparent; }
QScrollBar:vertical { background:transparent; width:9px; margin:3px; }
QScrollBar::handle:vertical { background:#777777; border-radius:4px; min-height:24px; }
QScrollBar::handle:vertical:hover { background:#999999; }
QScrollBar:horizontal { background:transparent; height:9px; margin:3px; }
QScrollBar::handle:horizontal { background:#777777; border-radius:4px; min-width:24px; }
QScrollBar::add-line:vertical,QScrollBar::sub-line:vertical { height:0; }
QScrollBar::add-line:horizontal,QScrollBar::sub-line:horizontal { width:0; }
QStatusBar { background:#242424; color:#D0D0D0; border-top:1px solid #454545; padding:5px 10px; }
QMenuBar { background:#292929; color:#E6E6E6; border-bottom:1px solid #454545; }
QMenuBar::item { padding:8px 12px; background:transparent; }
QMenuBar::item:selected { background:#5A252B; color:#FFFFFF; }
QMenu { background:#292929; color:#F4F4F4; border:1px solid #626262; padding:5px; }
QMenu::item { padding:8px 22px; border-radius:5px; }
QMenu::item:selected { background:#5A252B; color:#FFFFFF; }
QDialog,QMessageBox { background:#292929; color:#F4F4F4; }
QDialogButtonBox QPushButton { min-width:88px; }
QToolTip { background:#101010; color:#FFFFFF; border:1px solid #777777; padding:6px 8px; border-radius:5px; }
)");
}
} // namespace OxitaTheme
