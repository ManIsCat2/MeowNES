#pragma once

#include <cstdint>
#include <iostream>
#include <array>
#include <string>
#include <fstream>
#include <vector>
#include <cstring>
#include <filesystem>

#include "nes.hpp"
#include "nes/nes_cpu.hpp"
#include "nes/nes_controller.hpp"

#include "nes/mappers/mappers.hpp"

#include "nes/nes_rom.hpp"

#include <QApplication>
#include <QStyleFactory>
#include <QMainWindow>
#include <QMenuBar>
#include <QFileDialog>
#include <QMessageBox>
#include <QVBoxLayout>
#include <QDialog>
#include <QSlider>
#include <QGroupBox>
#include <QLabel>
#include <QComboBox>
#include <QCheckBox>
#include <QColorDialog>
#include <QTabWidget>
#include <QLineEdit>
#include <QTextBlock>
#include <QPlainTextEdit>
#include <QScrollBar>
#include <QPushButton>
#include <QFont>

extern bool romIsLoaded;
extern void *globalQTWin;
extern int hoveredPaletteIndex;

//extern QTimer cpuTimer;