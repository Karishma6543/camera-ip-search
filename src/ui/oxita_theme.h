#pragma once

#include <QString>

namespace OxitaTheme
{
inline QString styleSheet()
{
    return QStringLiteral(R"(
        QMainWindow, QWidget#appRoot {
            background: #F3F4F6;
            color: #252A32;
        }
        QWidget {
            font-family: "Inter", "Segoe UI", sans-serif;
            font-size: 12px;
            color: #303640;
        }
        QLabel { background: transparent; color: #303640; }
        QLabel#brand {
            color: #20242B;
            font-size: 24px;
            font-weight: 900;
            letter-spacing: 1.2px;
        }
        QLabel#productName {
            color: #747B86;
            font-size: 9px;
            font-weight: 850;
            letter-spacing: 1.45px;
        }
        QLabel#brandTagline {
            color: #8A909A;
            font-size: 9px;
            font-weight: 800;
            letter-spacing: 1.1px;
        }
        QLabel#muted {
            color: #7B828E;
            font-size: 11px;
        }
        QLabel#sectionTitle {
            color: #242932;
            font-size: 15px;
            font-weight: 800;
        }
        QLabel#metric {
            color: #242932;
            font-size: 16px;
            font-weight: 850;
        }
        QLabel#eyebrow {
            color: #89909B;
            font-size: 9px;
            font-weight: 850;
            letter-spacing: 1.1px;
        }
        QLabel#deviceDetails {
            background: #FBFCFD;
            border: 1px solid #E9ECF0;
            border-radius: 8px;
            padding: 12px;
        }
        QFrame#topBar, QFrame#surface, QFrame#toolbar, QFrame#metricCard {
            background: #FFFFFF;
            border: 1px solid #E5E8ED;
            border-radius: 13px;
        }
        QFrame#topBar { border-radius: 15px; }
        QFrame#metricCard { border-radius: 12px; }
        QFrame#toolbar { border-radius: 12px; }
        QGroupBox#actionPanel {
            background: #FFFFFF;
            border: 1px solid #E5E8ED;
            border-radius: 12px;
            margin-top: 14px;
            padding: 17px 13px 13px 13px;
            color: #292E37;
            font-weight: 800;
        }
        QGroupBox#actionPanel::title {
            subcontrol-origin: margin;
            left: 13px;
            padding: 0 7px;
            color: #303640;
            background: #FFFFFF;
            font-size: 11px;
            font-weight: 850;
        }
        QLineEdit, QComboBox, QSpinBox, QDoubleSpinBox {
            background: #FFFFFF;
            color: #303640;
            border: 1px solid #DCE1E8;
            border-radius: 8px;
            padding: 9px 11px;
            min-height: 19px;
            selection-background-color: #C92D3B;
            selection-color: #FFFFFF;
        }
        QLineEdit:hover, QComboBox:hover, QSpinBox:hover, QDoubleSpinBox:hover {
            border-color: #BEC5CF;
        }
        QLineEdit:focus, QComboBox:focus, QSpinBox:focus, QDoubleSpinBox:focus {
            border: 1px solid #C92D3B;
            background: #FFFFFF;
        }
        QLineEdit:disabled, QComboBox:disabled, QSpinBox:disabled,
        QDoubleSpinBox:disabled {
            background: #F3F4F6;
            color: #A3A9B3;
            border-color: #E8EAEE;
        }
        QComboBox::drop-down { border: 0; width: 25px; }
        QComboBox QAbstractItemView {
            background: #FFFFFF;
            color: #303640;
            border: 1px solid #E1E5EB;
            selection-background-color: #FCEBED;
            selection-color: #A91F2D;
            outline: 0;
            padding: 5px;
        }
        QPushButton {
            background: #FFFFFF;
            color: #414853;
            border: 1px solid #DCE1E8;
            border-radius: 8px;
            padding: 9px 14px;
            min-height: 18px;
            font-weight: 700;
        }
        QPushButton:hover {
            background: #F8F9FA;
            border-color: #C4CAD3;
            color: #252A32;
        }
        QPushButton:pressed { background: #EEF0F3; }
        QPushButton:focus { border: 1px solid #C92D3B; }
        QPushButton#primaryButton {
            background: #C92D3B;
            color: #FFFFFF;
            border: 1px solid #C92D3B;
            padding: 10px 17px;
            font-weight: 800;
        }
        QPushButton#primaryButton:hover {
            background: #B52331;
            border-color: #B52331;
        }
        QPushButton#primaryButton:pressed {
            background: #981D29;
            border-color: #981D29;
        }
        QPushButton:disabled {
            color: #A8AEB7;
            background: #F1F2F4;
            border-color: #E6E9ED;
        }
        QTableWidget {
            background: #FFFFFF;
            alternate-background-color: #FAFBFC;
            color: #353B45;
            border: 0;
            gridline-color: transparent;
            selection-background-color: #FCEBED;
            selection-color: #A8202D;
            font-size: 11px;
            outline: 0;
        }
        QTableWidget::item {
            padding: 10px 9px;
            border-bottom: 1px solid #F0F2F5;
        }
        QTableWidget::item:hover { background: #FFF8F8; }
        QTableWidget::item:selected {
            background: #FCEBED;
            color: #A8202D;
        }
        QHeaderView { background: #F6F7F9; }
        QHeaderView::section {
            background: #F6F7F9;
            color: #777F8B;
            border: 0;
            border-bottom: 1px solid #E7EAF0;
            padding: 12px 9px;
            font-size: 9px;
            font-weight: 850;
            letter-spacing: 0.35px;
        }
        QTabWidget::pane {
            border: 1px solid #E5E8ED;
            background: #FFFFFF;
            border-radius: 10px;
            top: -1px;
        }
        QTabBar::tab {
            background: transparent;
            color: #858C97;
            padding: 12px 17px;
            border: 0;
            border-bottom: 2px solid transparent;
            margin-right: 4px;
            font-weight: 750;
        }
        QTabBar::tab:hover { color: #3A404A; background: #FAFAFB; }
        QTabBar::tab:selected {
            color: #B92533;
            border-bottom: 2px solid #C92D3B;
            background: #FFFFFF;
        }
        QCheckBox { color: #4B525D; spacing: 8px; }
        QCheckBox::indicator {
            width: 16px;
            height: 16px;
            border: 1px solid #C9CFD8;
            border-radius: 4px;
            background: #FFFFFF;
        }
        QCheckBox::indicator:hover { border-color: #C92D3B; }
        QCheckBox::indicator:checked {
            background: #C92D3B;
            border-color: #C92D3B;
        }
        QScrollArea { border: 0; background: transparent; }
        QScrollBar:vertical {
            background: transparent;
            width: 9px;
            margin: 3px;
        }
        QScrollBar::handle:vertical {
            background: #D5DAE2;
            border-radius: 4px;
            min-height: 24px;
        }
        QScrollBar::handle:vertical:hover { background: #B9C0CB; }
        QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; }
        QScrollBar:horizontal {
            background: transparent;
            height: 9px;
            margin: 3px;
        }
        QScrollBar::handle:horizontal {
            background: #D5DAE2;
            border-radius: 4px;
            min-width: 24px;
        }
        QScrollBar::add-line:horizontal, QScrollBar::sub-line:horizontal { width: 0; }
        QStatusBar {
            background: #FFFFFF;
            color: #727A86;
            border-top: 1px solid #E7EAF0;
            padding: 5px 12px;
        }
        QMenuBar {
            background: #FFFFFF;
            color: #4C535E;
            border-bottom: 1px solid #ECEEF2;
        }
        QMenuBar::item { padding: 8px 12px; background: transparent; }
        QMenuBar::item:selected { background: #FCEBED; color: #A8202D; }
        QMenu {
            background: #FFFFFF;
            color: #303640;
            border: 1px solid #E1E5EB;
            padding: 5px;
        }
        QMenu::item { padding: 8px 22px; border-radius: 5px; }
        QMenu::item:selected { background: #FCEBED; color: #A8202D; }
        QDialog, QMessageBox { background: #FFFFFF; }
        QDialogButtonBox QPushButton { min-width: 86px; }
        QToolTip {
            background: #252A32;
            color: #FFFFFF;
            border: 1px solid #252A32;
            padding: 6px 8px;
            border-radius: 5px;
        }
    )");
}
} // namespace OxitaTheme
