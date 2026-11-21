#pragma once

#include <QString>

namespace OxitaTheme
{
inline QString styleSheet()
{
    return QStringLiteral(R"(
        QMainWindow, QWidget#appRoot {
            background: #F4F5F7;
            color: #252830;
        }
        QWidget {
            font-family: "Inter", "Segoe UI", sans-serif;
            font-size: 12px;
            color: #30343B;
        }
        QLabel { color: #30343B; }
        QLabel#brand {
            color: #252830;
            font-size: 23px;
            font-weight: 900;
            letter-spacing: 1px;
        }
        QLabel#productName {
            color: #747984;
            font-size: 10px;
            font-weight: 800;
            letter-spacing: 1.25px;
        }
        QLabel#brandTagline {
            color: #9297A0;
            font-size: 10px;
            font-weight: 700;
            letter-spacing: 0.8px;
        }
        QLabel#muted {
            color: #7C828D;
            font-size: 11px;
        }
        QLabel#sectionTitle {
            color: #252830;
            font-size: 15px;
            font-weight: 800;
        }
        QLabel#metric {
            color: #252830;
            font-size: 16px;
            font-weight: 850;
        }
        QFrame#topBar, QFrame#surface, QFrame#toolbar, QFrame#metricCard {
            background: #FFFFFF;
            border: 1px solid #E6E8EC;
            border-radius: 14px;
        }
        QFrame#metricCard {
            border-radius: 12px;
        }
        QGroupBox#actionPanel {
            background: #FFFFFF;
            border: 1px solid #E6E8EC;
            border-radius: 13px;
            margin-top: 14px;
            padding: 17px 13px 13px 13px;
            color: #282C34;
            font-weight: 800;
        }
        QGroupBox#actionPanel::title {
            subcontrol-origin: margin;
            left: 14px;
            padding: 0 7px;
            color: #292D35;
            background: #FFFFFF;
            font-size: 12px;
            font-weight: 800;
        }
        QLineEdit, QComboBox, QSpinBox, QDoubleSpinBox {
            background: #FFFFFF;
            color: #30343B;
            border: 1px solid #DDE1E7;
            border-radius: 8px;
            padding: 9px 11px;
            min-height: 20px;
            selection-background-color: #C92D3B;
            selection-color: #FFFFFF;
        }
        QLineEdit:hover, QComboBox:hover, QSpinBox:hover, QDoubleSpinBox:hover {
            border-color: #C3C8D0;
        }
        QLineEdit:focus, QComboBox:focus, QSpinBox:focus, QDoubleSpinBox:focus {
            border: 1px solid #C92D3B;
            background: #FFFFFF;
        }
        QLineEdit:disabled, QComboBox:disabled {
            background: #F2F3F5;
            color: #A5AAB3;
            border-color: #E7E9ED;
        }
        QComboBox::drop-down {
            border: 0;
            width: 25px;
        }
        QComboBox QAbstractItemView {
            background: #FFFFFF;
            color: #30343B;
            border: 1px solid #E1E4E9;
            selection-background-color: #FCEBED;
            selection-color: #A91F2D;
            outline: 0;
            padding: 5px;
        }
        QPushButton {
            background: #FFFFFF;
            color: #414650;
            border: 1px solid #DDE1E7;
            border-radius: 8px;
            padding: 9px 14px;
            font-weight: 700;
        }
        QPushButton:hover {
            background: #F8F8FA;
            border-color: #C8CDD5;
            color: #252830;
        }
        QPushButton:pressed {
            background: #F0F1F4;
        }
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
        }
        QPushButton:disabled {
            color: #A8ADB5;
            background: #F1F2F4;
            border-color: #E6E8EC;
        }
        QTableWidget {
            background: #FFFFFF;
            alternate-background-color: #FAFAFB;
            color: #353A43;
            border: 0;
            gridline-color: transparent;
            selection-background-color: #FCEBED;
            selection-color: #A8202D;
            font-size: 11px;
            outline: 0;
        }
        QTableWidget::item {
            padding: 9px 8px;
            border-bottom: 1px solid #F0F1F3;
        }
        QTableWidget::item:selected {
            background: #FCEBED;
            color: #A8202D;
        }
        QHeaderView::section {
            background: #F6F7F9;
            color: #777D88;
            border: 0;
            border-bottom: 1px solid #E8EAEE;
            padding: 12px 9px;
            font-size: 10px;
            font-weight: 850;
        }
        QTabWidget::pane {
            border: 1px solid #E6E8EC;
            background: #FFFFFF;
            border-radius: 10px;
            top: -1px;
        }
        QTabBar::tab {
            background: transparent;
            color: #858B95;
            padding: 12px 18px;
            border: 0;
            border-bottom: 2px solid transparent;
            margin-right: 5px;
            font-weight: 750;
        }
        QTabBar::tab:hover {
            color: #3A3F48;
            background: #FAFAFB;
        }
        QTabBar::tab:selected {
            color: #B92533;
            border-bottom: 2px solid #C92D3B;
            background: #FFFFFF;
        }
        QCheckBox {
            color: #4B505A;
            spacing: 8px;
        }
        QCheckBox::indicator {
            width: 16px;
            height: 16px;
            border: 1px solid #C9CED6;
            border-radius: 4px;
            background: #FFFFFF;
        }
        QCheckBox::indicator:checked {
            background: #C92D3B;
            border-color: #C92D3B;
        }
        QScrollArea {
            border: 0;
            background: transparent;
        }
        QScrollBar:vertical {
            background: transparent;
            width: 9px;
            margin: 3px;
        }
        QScrollBar::handle:vertical {
            background: #D5D9E0;
            border-radius: 4px;
            min-height: 24px;
        }
        QScrollBar::handle:vertical:hover { background: #B9BEC8; }
        QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; }
        QStatusBar {
            background: #FFFFFF;
            color: #727884;
            border-top: 1px solid #E7E9ED;
            padding: 5px 12px;
        }
        QMenuBar {
            background: #FFFFFF;
            color: #4C515B;
            border-bottom: 1px solid #ECEEF1;
        }
        QMenuBar::item {
            padding: 8px 12px;
            background: transparent;
        }
        QMenuBar::item:selected { background: #FCEBED; color: #A8202D; }
        QMenu {
            background: #FFFFFF;
            color: #30343B;
            border: 1px solid #E1E4E9;
            padding: 5px;
        }
        QMenu::item { padding: 8px 22px; border-radius: 5px; }
        QMenu::item:selected { background: #FCEBED; color: #A8202D; }
        QDialog { background: #FFFFFF; }
        QDialogButtonBox QPushButton { min-width: 86px; }
        QMessageBox { background: #FFFFFF; }
        QToolTip {
            background: #252830;
            color: #FFFFFF;
            border: 1px solid #252830;
            padding: 6px 8px;
            border-radius: 5px;
        }
    )");
}
} // namespace OxitaTheme
