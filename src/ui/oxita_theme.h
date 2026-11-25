#pragma once

#include <QString>

namespace OxitaTheme
{
inline QString styleSheet()
{
    // Oxita product palette: restrained red accent, neutral surfaces and
    // high-contrast charcoal typography. Semantic states remain distinguishable.
    return QStringLiteral(R"(
        QWidget {
            font-family: "Inter", "Segoe UI", sans-serif;
            font-size: 13px;
            color: #24272D;
        }
        QMainWindow, QWidget#appRoot {
            background: #F2F3F5;
            color: #24272D;
        }
        QLabel { background: transparent; color: #24272D; }
        QLabel#brand { color: #17191D; font-size: 24px; font-weight: 900; letter-spacing: 1px; }
        QLabel#productName {
            color: #5E646D; font-size: 10px; font-weight: 800; letter-spacing: 1.35px;
        }
        QLabel#brandTagline { color: #777D86; font-size: 10px; font-weight: 700; }
        QLabel#muted { color: #737A84; font-size: 12px; }
        QLabel#sectionTitle { color: #202329; font-size: 15px; font-weight: 800; }
        QLabel#metric { color: #17191D; font-size: 21px; font-weight: 850; }
        QLabel#eyebrow {
            color: #777E88; font-size: 10px; font-weight: 800; letter-spacing: 1px;
        }
        QLabel#deviceDetails, QLabel#selectedDeviceDetails {
            background: #F7F8F9; border: 1px solid #E5E7EB;
            border-radius: 9px; padding: 12px; color: #30353D;
        }
        QLabel#inventoryEmptyState {
            background: #FAFAFB; color: #666D77;
            border: 1px dashed #C9CDD4; border-radius: 10px;
            padding: 20px; font-size: 13px;
        }
        QFrame#topBar, QFrame#surface, QFrame#toolbar, QFrame#metricCard {
            background: #FFFFFF; border: 1px solid #E1E4E8; border-radius: 12px;
        }
        QFrame#topBar { border-radius: 13px; }
        QFrame#metricCard { border-radius: 10px; }
        QFrame#toolbar { border-radius: 11px; }
        QGroupBox#actionPanel {
            background: #FFFFFF; border: 1px solid #E1E4E8;
            border-radius: 10px; margin-top: 13px; padding: 15px 12px 12px;
            color: #24272D; font-weight: 750;
        }
        QGroupBox#actionPanel::title {
            subcontrol-origin: margin; left: 12px; padding: 0 7px;
            color: #292D34; background: #FFFFFF; font-size: 12px; font-weight: 800;
        }
        QLineEdit, QComboBox, QSpinBox, QDoubleSpinBox {
            background: #FFFFFF; color: #252930; border: 1px solid #D6DAE0;
            border-radius: 7px; padding: 8px 10px; min-height: 22px;
            selection-background-color: #C62836; selection-color: #FFFFFF;
        }
        QLineEdit:hover, QComboBox:hover, QSpinBox:hover, QDoubleSpinBox:hover {
            border-color: #AEB4BD;
        }
        QLineEdit:focus, QComboBox:focus, QSpinBox:focus, QDoubleSpinBox:focus {
            border: 1px solid #C62836; background: #FFFFFF;
        }
        QLineEdit:disabled, QComboBox:disabled, QSpinBox:disabled,
        QDoubleSpinBox:disabled {
            background: #F0F1F3; color: #969DA7; border-color: #E2E4E8;
        }
        QComboBox::drop-down { border: 0; width: 25px; }
        QComboBox QAbstractItemView {
            background: #FFFFFF; color: #252930; border: 1px solid #DDE0E5;
            selection-background-color: #FCEBED; selection-color: #A51F2B;
            outline: 0; padding: 5px;
        }
        QPushButton {
            background: #FFFFFF; color: #343941; border: 1px solid #D4D8DE;
            border-radius: 7px; padding: 8px 12px; min-height: 20px;
            font-weight: 700;
        }
        QPushButton:hover {
            background: #F7F8F9; border-color: #AEB4BD; color: #17191D;
        }
        QPushButton:pressed { background: #ECEEF1; }
        QPushButton:focus { border: 1px solid #C62836; }
        QPushButton#primaryButton {
            background: #C62836; color: #FFFFFF; border: 1px solid #C62836;
            padding: 9px 16px; font-weight: 800;
        }
        QPushButton#primaryButton:hover { background: #AD202D; border-color: #AD202D; }
        QPushButton#primaryButton:pressed { background: #901A25; border-color: #901A25; }
        QPushButton:disabled {
            color: #9DA3AC; background: #F0F1F3; border-color: #E3E5E9;
        }
        QTableWidget {
            background: #FFFFFF; alternate-background-color: #F8F9FA;
            color: #30353D; border: 0; gridline-color: transparent;
            selection-background-color: #FCEBED; selection-color: #8F1C27;
            font-size: 12px; outline: 0;
        }
        QTableWidget::item { padding: 9px 8px; border-bottom: 1px solid #ECEEF1; }
        QTableWidget::item:hover { background: #FFF7F8; }
        QTableWidget::item:selected { background: #FCEBED; color: #8F1C27; }
        QHeaderView { background: #F6F7F8; }
        QHeaderView::section {
            background: #F6F7F8; color: #646B75; border: 0;
            border-bottom: 1px solid #E2E5E9; padding: 11px 9px;
            font-size: 10px; font-weight: 850; letter-spacing: .35px;
        }
        QTabWidget::pane {
            border: 1px solid #E1E4E8; background: #FFFFFF;
            border-radius: 9px; top: -1px;
        }
        QTabBar::tab {
            background: transparent; color: #727983; padding: 11px 15px;
            border: 0; border-bottom: 2px solid transparent;
            margin-right: 3px; font-weight: 750;
        }
        QTabBar::tab:hover { color: #252930; background: #F7F8F9; }
        QTabBar::tab:selected {
            color: #A91F2D; border-bottom: 2px solid #C62836; background: #FFFFFF;
        }
        QCheckBox { color: #414750; spacing: 8px; }
        QCheckBox::indicator {
            width: 16px; height: 16px; border: 1px solid #BFC5CD;
            border-radius: 4px; background: #FFFFFF;
        }
        QCheckBox::indicator:hover { border-color: #C62836; }
        QCheckBox::indicator:checked { background: #C62836; border-color: #C62836; }
        QSplitter::handle:vertical {
            background: #E1E4E8; height: 7px; margin: 2px 8px;
            border-radius: 3px;
        }
        QSplitter::handle:vertical:hover { background: #C62836; }
        QScrollArea { border: 0; background: transparent; }
        QScrollBar:vertical { background: transparent; width: 9px; margin: 3px; }
        QScrollBar::handle:vertical {
            background: #C9CED6; border-radius: 4px; min-height: 24px;
        }
        QScrollBar::handle:vertical:hover { background: #AEB5BF; }
        QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; }
        QScrollBar:horizontal { background: transparent; height: 9px; margin: 3px; }
        QScrollBar::handle:horizontal {
            background: #C9CED6; border-radius: 4px; min-width: 24px;
        }
        QScrollBar::add-line:horizontal, QScrollBar::sub-line:horizontal { width: 0; }
        QStatusBar {
            background: #FFFFFF; color: #59616B;
            border-top: 1px solid #E1E4E8; padding: 6px 12px;
        }
        QMenuBar { background: #FFFFFF; color: #3F454E; border-bottom: 1px solid #E7E9ED; }
        QMenuBar::item { padding: 8px 12px; background: transparent; }
        QMenuBar::item:selected { background: #FCEBED; color: #A51F2B; }
        QMenu { background: #FFFFFF; color: #292D34; border: 1px solid #DDE0E5; padding: 5px; }
        QMenu::item { padding: 8px 22px; border-radius: 5px; }
        QMenu::item:selected { background: #FCEBED; color: #A51F2B; }
        QDialog, QMessageBox { background: #FFFFFF; }
        QDialogButtonBox QPushButton { min-width: 88px; }
        QToolTip {
            background: #202329; color: #FFFFFF; border: 1px solid #202329;
            padding: 6px 8px; border-radius: 5px;
        }
    )");
}
} // namespace OxitaTheme
