#pragma once
#include <QString>
namespace OxitaTheme {
inline QString styleSheet() {
return QStringLiteral(R"(
QWidget { font-family:"Inter","Segoe UI",sans-serif; font-size:13px; color:#E7EDF4; }
QMainWindow,QWidget#appRoot { background:#101720; color:#E7EDF4; }
QLabel { background:transparent; color:#E7EDF4; }
QLabel#brand { color:#F4F6F8; font-size:24px; font-weight:900; letter-spacing:1px; }
QLabel#productName { color:#AEB9C6; font-size:10px; font-weight:800; letter-spacing:1.35px; }
QLabel#brandTagline { color:#9AA8B7; font-size:10px; font-weight:700; }
QLabel#muted { color:#A0ADBB; font-size:12px; }
QLabel#sectionTitle { color:#F1F4F7; font-size:15px; font-weight:800; }
QLabel#deviceDetails,QLabel#selectedDeviceDetails { background:#141F2B; border:1px solid #2B3948; border-radius:8px; padding:10px; color:#D8E1EA; }
QLabel#inventoryEmptyState { background:#141F2B; color:#AAB7C5; border:1px dashed #3A4A5C; border-radius:9px; padding:18px; font-size:13px; }
QFrame#topBar,QFrame#surface,QFrame#toolbar,QFrame#metricCard,QFrame#rightWorkspacePanel { background:#192431; border:1px solid #2B3948; border-radius:10px; }
QFrame#topBar { border-radius:11px; }
QGroupBox#actionPanel { background:#192431; border:1px solid #2B3948; border-radius:9px; margin-top:13px; padding:14px 11px 11px; color:#E7EDF4; font-weight:750; }
QGroupBox#actionPanel::title { subcontrol-origin:margin; left:12px; padding:0 7px; color:#E7EDF4; background:#192431; font-size:12px; font-weight:800; }
QLineEdit,QComboBox,QSpinBox,QDoubleSpinBox { background:#111B26; color:#EDF2F7; border:1px solid #3A4A5C; border-radius:6px; padding:7px 9px; min-height:21px; selection-background-color:#C62836; selection-color:#FFFFFF; }
QLineEdit:hover,QComboBox:hover,QSpinBox:hover,QDoubleSpinBox:hover { border-color:#60758B; }
QLineEdit:focus,QComboBox:focus,QSpinBox:focus,QDoubleSpinBox:focus { border:1px solid #C62836; background:#14202C; }
QLineEdit:disabled,QComboBox:disabled,QSpinBox:disabled,QDoubleSpinBox:disabled { background:#1A2531; color:#728194; border-color:#2C3947; }
QComboBox::drop-down { border:0; width:25px; }
QComboBox QAbstractItemView { background:#192431; color:#EDF2F7; border:1px solid #3A4A5C; selection-background-color:#3B2630; selection-color:#FFFFFF; outline:0; padding:5px; }
QPushButton { background:#243242; color:#E2E9F0; border:1px solid #405165; border-radius:6px; padding:7px 11px; min-height:20px; font-weight:650; }
QPushButton:hover { background:#2D4053; border-color:#657A90; color:#FFFFFF; }
QPushButton:pressed { background:#1C2937; }
QPushButton:focus { border:1px solid #C62836; }
QPushButton#primaryButton { background:#C62836; color:#FFFFFF; border:1px solid #C62836; padding:8px 15px; font-weight:800; }
QPushButton#primaryButton:hover { background:#AD202D; border-color:#AD202D; }
QPushButton#primaryButton:pressed { background:#901A25; border-color:#901A25; }
QPushButton:disabled { color:#718092; background:#1A2531; border-color:#2B3948; }
QTableWidget { background:#131E29; alternate-background-color:#182431; color:#DCE5EE; border:0; gridline-color:transparent; selection-background-color:#3B2630; selection-color:#FFFFFF; font-size:12px; outline:0; }
QTableWidget::item { padding:7px 8px; border-bottom:1px solid #263442; }
QTableWidget::item:hover { background:#263747; }
QTableWidget::item:selected { background:#3B2630; color:#FFFFFF; }
QHeaderView { background:#1D2A37; }
QHeaderView::section { background:#1D2A37; color:#AEBBC8; border:0; border-bottom:1px solid #344455; padding:9px 8px; font-size:10px; font-weight:800; letter-spacing:.3px; }
QTabWidget::pane { border:1px solid #2B3948; background:#192431; border-radius:8px; top:-1px; }
QTabBar::tab { background:transparent; color:#9EADBC; padding:10px 13px; border:0; border-bottom:2px solid transparent; margin-right:2px; font-weight:700; }
QTabBar::tab:hover { color:#FFFFFF; background:#223140; }
QTabBar::tab:selected { color:#FFFFFF; border-bottom:2px solid #C62836; background:#192431; }
QCheckBox { color:#D5DEE7; spacing:8px; }
QCheckBox::indicator { width:16px; height:16px; border:1px solid #607084; border-radius:4px; background:#111B26; }
QCheckBox::indicator:hover { border-color:#C62836; }
QCheckBox::indicator:checked { background:#C62836; border-color:#C62836; }
QSplitter::handle:horizontal { background:#2B3948; width:7px; margin:8px 2px; border-radius:3px; }
QSplitter::handle:vertical { background:#2B3948; height:7px; margin:2px 8px; border-radius:3px; }
QSplitter::handle:hover { background:#C62836; }
QScrollArea { border:0; background:transparent; }
QScrollBar:vertical { background:transparent; width:9px; margin:3px; }
QScrollBar::handle:vertical { background:#45576A; border-radius:4px; min-height:24px; }
QScrollBar::handle:vertical:hover { background:#63788E; }
QScrollBar:horizontal { background:transparent; height:9px; margin:3px; }
QScrollBar::handle:horizontal { background:#45576A; border-radius:4px; min-width:24px; }
QScrollBar::add-line:vertical,QScrollBar::sub-line:vertical { height:0; }
QScrollBar::add-line:horizontal,QScrollBar::sub-line:horizontal { width:0; }
QStatusBar { background:#151F2A; color:#AEBAC7; border-top:1px solid #2B3948; padding:5px 10px; }
QMenuBar { background:#192431; color:#DCE5EE; border-bottom:1px solid #2B3948; }
QMenuBar::item { padding:8px 12px; background:transparent; }
QMenuBar::item:selected { background:#3B2630; color:#FFFFFF; }
QMenu { background:#192431; color:#E7EDF4; border:1px solid #3A4A5C; padding:5px; }
QMenu::item { padding:8px 22px; border-radius:5px; }
QMenu::item:selected { background:#3B2630; color:#FFFFFF; }
QDialog,QMessageBox { background:#192431; color:#E7EDF4; }
QDialogButtonBox QPushButton { min-width:88px; }
QToolTip { background:#0D141C; color:#FFFFFF; border:1px solid #45576A; padding:6px 8px; border-radius:5px; }
)");
}
} // namespace OxitaTheme
