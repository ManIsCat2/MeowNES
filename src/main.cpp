#include "nes/nes_cpu.hpp"
#include "nes/nes_apu.hpp"
#include "nes/nes_rom.hpp"
#include "nes/nes_console.hpp"
#include "gb/gb_cpu.hpp"
#include "gb/gb_ppu.hpp"
#include "gb/gb_rom.hpp"
#include "gb/gb_console.hpp"
#include "audio.hpp"

#include "main.hpp"
#include "qt/screen_widget.hpp"
#include "qt/input_manager.hpp"
#include "qt/palette_editor.hpp"
#include "config.hpp"
#include "rom.hpp"
#include "savestate.hpp"

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
#include <array>
#include <string_view>

#ifdef _WIN32
#include <direct.h>
#define makeDirectory(dir) _mkdir(dir)
#else
#include <sys/stat.h>
#include <sys/types.h>
#define makeDirectory(dir) mkdir(dir, 0777)
#endif

bool romIsLoaded = false;
int hoveredPaletteIndex = -1;

void *globalQTWin;

const unsigned char EMeowlatorIcon[] = {
    0x89, 0x50, 0x4e, 0x47, 0x0d, 0x0a, 0x1a, 0x0a, 0x00, 0x00, 0x00, 0x0d,
    0x49, 0x48, 0x44, 0x52, 0x00, 0x00, 0x00, 0x10, 0x00, 0x00, 0x00, 0x10,
    0x08, 0x06, 0x00, 0x00, 0x00, 0x1f, 0xf3, 0xff, 0x61, 0x00, 0x00, 0x00,
    0x01, 0x73, 0x52, 0x47, 0x42, 0x00, 0xae, 0xce, 0x1c, 0xe9, 0x00, 0x00,
    0x00, 0x04, 0x67, 0x41, 0x4d, 0x41, 0x00, 0x00, 0xb1, 0x8f, 0x0b, 0xfc,
    0x61, 0x05, 0x00, 0x00, 0x00, 0x09, 0x70, 0x48, 0x59, 0x73, 0x00, 0x00,
    0x0e, 0xc3, 0x00, 0x00, 0x0e, 0xc3, 0x01, 0xc7, 0x6f, 0xa8, 0x64, 0x00,
    0x00, 0x00, 0x73, 0x49, 0x44, 0x41, 0x54, 0x38, 0x4f, 0xcd, 0x91, 0xcb,
    0x09, 0xc0, 0x30, 0x0c, 0x43, 0xb3, 0x93, 0x27, 0xf1, 0x28, 0x99, 0x29,
    0x83, 0xa6, 0xe8, 0x90, 0x62, 0xcb, 0xce, 0xaf, 0xa7, 0x0a, 0x1e, 0x94,
    0xf0, 0xa4, 0xb4, 0xb4, 0x94, 0x5f, 0xa7, 0xd6, 0xda, 0xc1, 0xe9, 0x79,
    0x08, 0xa4, 0xde, 0x9a, 0x93, 0x47, 0xf9, 0x7a, 0x20, 0x83, 0xfd, 0x10,
    0x2b, 0x8b, 0x48, 0x78, 0x66, 0x3f, 0xe4, 0x68, 0x60, 0xf5, 0x3a, 0x9f,
    0x07, 0x6c, 0x71, 0x45, 0x3a, 0xc0, 0xd2, 0x0c, 0x7b, 0xa1, 0x1b, 0x61,
    0x71, 0x5a, 0xb2, 0x61, 0x11, 0xa8, 0xea, 0xcb, 0x76, 0x00, 0xe1, 0x81,
    0x19, 0xdc, 0x73, 0x61, 0x39, 0x83, 0x3b, 0x2e, 0x2c, 0x03, 0xfb, 0x09,
    0xdb, 0x11, 0xfc, 0xdf, 0x0c, 0x8c, 0xf0, 0xd9, 0xe8, 0x3c, 0x77, 0x43,
    0x1b, 0x22, 0x0c, 0x2c, 0xbb, 0xb7, 0x00, 0x00, 0x00, 0x00, 0x49, 0x45,
    0x4e, 0x44, 0xae, 0x42, 0x60, 0x82
};

std::string joinLines(const std::vector<std::string> &lines) {
    std::string result;
    size_t totalSize = 0;
    for (const auto &line : lines) totalSize += line.size() + 1;
    result.reserve(totalSize);
    
    for (size_t i = 0; i < lines.size(); ++i) {
        result += lines[i];
        if (i != lines.size() - 1) result += "\n";
    }
    return result;
}

QAction *makeQBool(const QString &text, QObject *parent, bool defaultBool) {
    QAction *newAction = new QAction(text, parent);
    newAction->setCheckable(true);
    newAction->setChecked(defaultBool);
    return newAction;
}

QTimer cpuTimer;

const std::array<std::string_view, 3> allowedExts = {
    ".nes",
    ".gb",
    ".gbc"
};

bool loadConsoleWithGame(const std::string &file) {
    QMainWindow *win = (QMainWindow*)(globalQTWin);

    size_t dotPos = file.find_last_of('.');
    if (dotPos == std::string::npos) {
        romIsLoaded = false;
        return false;
    }

    std::string rawExt = file.substr(dotPos);
    for (char &c : rawExt) c = (char)(std::tolower(c));
    
    bool allow = false;
    std::string matchedExt = "";
    
    for (const auto &ext : allowedExts) {
        if (rawExt == ext) {
            allow = true;
            matchedExt = std::string(ext);
            break;
        }
    }

    if (!allow) {
        romIsLoaded = false;
        DebugPrintLog("LOADER", "Console not supported");
        QMessageBox::critical(win, "Error", "Console not supported");
        return false;
    }

    delete emuConsole;
    emuConsole = nullptr;

    if (matchedExt == ".nes") {
        emuConsole = new NESConsole;
        win->setWindowTitle("EMeowlator - NES");
    } else if (matchedExt == ".gb" || matchedExt == ".gbc") {
        emuConsole = new GBConsole;
        win->setWindowTitle("EMeowlator - Game Boy");
    } else {
        romIsLoaded = false;
        return false;
    }

    if (!emuConsole) {
        romIsLoaded = false;
        return false;
    }

    emuConsole->init();

    if (!emuConsole->loadGame(file)) {
        romIsLoaded = false;
        delete emuConsole;
        emuConsole = nullptr;
        return false;
    }

    romIsLoaded = true;
    return true;
}

void startCPUTimer(void) {
    if (getRom()->Region == ConsoleRegion::NTSC) {
        cpuTimer.start(16);
    } else {
        cpuTimer.start(20);
    }
}

void ShowRomInfoDialog(QWidget* parent) {
    std::string fullInfo = "ROM isn't loaded!";
    QDialog* dialog = new QDialog(parent);
    dialog->setWindowTitle("ROM Info");
    dialog->setFixedSize(350, 250);

    if (emuConsole && romIsLoaded) {
        const ConsoleType type = emuConsole->getConsoleType();
        const bool isNES = (type == ConsoleType::NES);
        const bool isGB  = (type == ConsoleType::GAMEBOY);

        std::string fileStr = "File: " + getRom()->Name;
        if (isNES) {
            NesROM *rom = getNESRom();
            std::string HeaderHexStr;
            HeaderHexStr.reserve(32);
            for (int i = 0; i < 8; i++) {
                char Buf[4];
                snprintf(Buf, sizeof(Buf), "%02X", rom->Header[i]);
                HeaderHexStr += Buf;
                if (i < 7) HeaderHexStr += " ";
            }
            HeaderHexStr = "Header: " + HeaderHexStr;
            std::string headVerStr = "Header Version: " + std::string(rom->Version == HeaderVersion::NES2_0 ? "NES2.0" : "INES");
            char PRGSizeStr[128];
            snprintf(PRGSizeStr, sizeof(PRGSizeStr), "PRG Size: %uKiB (%u x 16KiB)", rom->PRGNumPages * 16, rom->PRGNumPages);
            char CHRSizeStr[128];
            snprintf(CHRSizeStr, sizeof(CHRSizeStr), "CHR Size: %uKiB (%u x 8KiB)", rom->CHRNumPages * 8, rom->CHRNumPages);
            std::string mapperStr = "Mapper: " + std::string(rom->mapper ? rom->mapper->getName() : (rom->MapperID ? "Unknown" : "NROM")) + " (Mapper " + std::to_string(rom->MapperID)+")";
            std::string subMapperStr = "Sub Mapper: " + std::to_string(rom->SubMapperID);
            std::string mirrorStr = "Mirroring: " + std::string(rom->Mirroring == MirrorMode::HORIZONTAL ? "Horizontal" : "Vertical");
            std::string batteryStr = "Battery: " + std::string(rom->hasBattery ? "Yes" : "No");
            std::string CHRRamStr = "CHR-RAM: " + std::string(rom->CHRRomSize == 0 ? "Yes" : "No");
            char batterySizeStr[128];
            size_t SRAMSize = rom->hasBattery ? rom->mapper->getSRAMSize() : 0x0000; 
            snprintf(batterySizeStr, sizeof(batterySizeStr), "SRAM/Battery Size: 0x%zx (%zu)", SRAMSize, SRAMSize);
            char RESETVecStr[128];
            snprintf(RESETVecStr, sizeof(RESETVecStr), "RESET Vector: 0x%x", rom->ResetVec);

            fullInfo = joinLines({fileStr, HeaderHexStr, headVerStr, PRGSizeStr, CHRSizeStr, mapperStr, subMapperStr, mirrorStr, batteryStr, CHRRamStr, batterySizeStr, RESETVecStr});
        } else if (isGB) {
            GbROM *rom = getGBRom();

            std::string titleStr = "Title: " + rom->Title;
            std::string mapperStr = "Mapper: " + std::string(rom->mbc->getName()) + " (Mapper " + std::to_string(rom->cartType)+")";
            std::string batteryStr = "Battery: " + std::string(rom->hasBattery() ? "Yes" : "No");
            std::string RTCStr = "RTC: " + std::string(rom->hasRTC() ? "Yes" : "No");
            std::string RAMStr = "External RAM: " + std::string(rom->hasRAM() ? "Yes" : "No");

            char ROMSizeStr[128];
            snprintf(ROMSizeStr, sizeof(ROMSizeStr), "ROM Size: %uKiB", rom->RomSize / 1024);
            char RAMSizeStr[128];
            snprintf(RAMSizeStr, sizeof(RAMSizeStr), "RAM Size: %uKiB", rom->ramSize / 1024);

            fullInfo = joinLines({fileStr, titleStr, mapperStr, batteryStr, RTCStr, RAMStr, ROMSizeStr, RAMSizeStr});
        }
    }
    QLabel* label = new QLabel(QString::fromStdString(fullInfo), dialog);
    label->setAlignment(Qt::AlignCenter);

    QVBoxLayout* layout = new QVBoxLayout(dialog);
    layout->addWidget(label);

    dialog->setLayout(layout);
    dialog->exec();
    delete dialog;
}

void ShowAudioConfigDialog(QWidget* parent) {
    if (!emuConsole) return;
    const ConsoleType type = emuConsole->getConsoleType();
    const bool isNES = (type == ConsoleType::NES);

    QDialog *dialog = new QDialog(parent);
    dialog->setWindowTitle("Audio Config");
    dialog->setFixedSize(500, 340);

    QVBoxLayout *mainLayout = new QVBoxLayout(dialog);
    QGroupBox *volumeBox = new QGroupBox("Volume", dialog);
    QHBoxLayout *volumeLayout = new QHBoxLayout(volumeBox);
    volumeLayout->setSpacing(5);
    volumeLayout->setContentsMargins(10, 10, 10, 10);

    if (isNES) {
        const std::array<const char*, 6> volumeNames = {"Square 1", "Square 2", "Triangle", "Noise", "DMC", "Master"};
        float *volumePtrs[] = { &nesApu.pulse1Volume, &nesApu.pulse2Volume, &nesApu.triangleVolume, &nesApu.noiseVolume, &nesApu.dmcVolume, &nesApu.masterVolume };

        for (int i = 0; i < 6; ++i) {
            QWidget *pairWidget = new QWidget(volumeBox);
            QVBoxLayout *pairLayout = new QVBoxLayout(pairWidget);
            pairLayout->setContentsMargins(0, 0, 0, 0);
            pairLayout->setSpacing(5);

            QLabel *label = new QLabel(volumeNames[i], pairWidget);
            label->setAlignment(Qt::AlignHCenter);
            pairLayout->addWidget(label);

            QSlider *slider = new QSlider(Qt::Vertical, pairWidget);
            slider->setRange(0, 50);
            slider->setValue((int)(*volumePtrs[i]));
            slider->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Expanding);
            slider->setTickPosition(QSlider::TicksRight);
            slider->setTickInterval(10);
            pairLayout->addWidget(slider);

            QObject::connect(slider, &QSlider::valueChanged, [volumePtrs, i](int value) {
                *volumePtrs[i] = (float)(value);
            });

            volumeLayout->addWidget(pairWidget);
        }
    } else if (type == ConsoleType::GAMEBOY) {
    }

    volumeBox->setLayout(volumeLayout);
    mainLayout->addWidget(volumeBox);
    dialog->setLayout(mainLayout);
    dialog->exec();
    delete dialog;
}

void ShowInputConfigDialog(QWidget* parent) {
    if (!emuConsole) return;
    const ConsoleType type = emuConsole->getConsoleType();
    const bool isNES = (type == ConsoleType::NES);
    const bool isGB  = (type == ConsoleType::GAMEBOY);

    QDialog *dialog = new QDialog(parent);
    dialog->setWindowTitle("Input Config");
    dialog->setFixedSize(300, 340);

    QVBoxLayout *layout = new QVBoxLayout(dialog);

    if (isNES || isGB) {
        std::vector<KeyCaptureButton*> buttons;
        buttons.reserve(sizeof(nesKeyBinds) / sizeof(nesKeyBinds[0]));
        
        for (auto &bind : nesKeyBinds) {
            QHBoxLayout *row = new QHBoxLayout();
            QLabel *label = new QLabel(bind.name);

            KeyCaptureButton *button = new KeyCaptureButton(
                QKeySequence(bind.key).toString(),
                &bind.key,
                dialog
            );
            buttons.push_back(button);

            QObject::connect(button, &QPushButton::clicked, [button]() {
                button->setText("Press key...");
                button->waitingForKey = true;
                button->setFocus();
            });

            row->addWidget(label);
            row->addWidget(button);
            layout->addLayout(row);
        }

        QPushButton *resetButton = new QPushButton("Reset Binds", dialog);
        QObject::connect(resetButton, &QPushButton::clicked, [buttons]() {
            memcpy(nesKeyBinds, nesKeyBindsDefault, sizeof(nesKeyBinds));
            for (size_t i = 0; i < buttons.size(); i++) {
                buttons[i]->setText(QKeySequence(nesKeyBindsDefault[i].key).toString());
            }
        });

        layout->addStretch();
        layout->addWidget(resetButton);
    }

    dialog->setLayout(layout);
    dialog->exec();
    delete dialog;
}

void ShowDisplayConfigDialog(QWidget* parent) {
    if (!emuConsole) return;
    const ConsoleType type = emuConsole->getConsoleType();
    const bool isNES = (type == ConsoleType::NES);

    QDialog *dialog = new QDialog(parent);
    dialog->setWindowTitle("Display Config");
    dialog->setFixedSize(560, isNES ? 420 : 280);

    QGridLayout *mainLayout = new QGridLayout(dialog);
    QGroupBox *settingsBox = new QGroupBox("Settings", dialog);
    QGridLayout *settingsLayout = new QGridLayout(settingsBox);
    settingsBox->setMinimumSize(270, 120);

    QGroupBox *regionBox = new QGroupBox("Region/Model", dialog);
    QVBoxLayout *regionLayout = new QVBoxLayout(regionBox);
    QComboBox *tvtypeComboBox = new QComboBox(regionBox);

    const std::vector<std::string> regionStrs = isNES 
        ? std::vector<std::string>{"NTSC", "PAL", "Dendy"} 
        : std::vector<std::string>{"Game Boy (DMG)", "Game Boy Color (CGB)"};

    for (size_t i = 0; i < regionStrs.size(); ++i) {
        tvtypeComboBox->addItem(QString::fromStdString(regionStrs[i]), (int)(i));
    }

    if (isNES) {
        tvtypeComboBox->setCurrentIndex((int)(getNESRom()->Region));
    } else {
        tvtypeComboBox->setCurrentIndex((int)(getGBRom()->isCGB));
    }

    QObject::connect(tvtypeComboBox, &QComboBox::currentIndexChanged, [isNES](int index) {
        if (isNES) {
            getNESRom()->Region = (ConsoleRegion)(index);
            startCPUTimer();
        } else {
            getGBRom()->isCGB = (bool)(index);
        }
        DebugPrintLog("SETTINGS", "Set Region/Model to %d", index);
    });
    regionLayout->addWidget(tvtypeComboBox);

    if (isNES) {
        QGroupBox *shadowBox = new QGroupBox("Shadows", dialog);
        QVBoxLayout *shadowLayout = new QVBoxLayout(shadowBox);
        QComboBox *shadowComboBox = new QComboBox(shadowBox);

        const std::array<const char*, 4> shadowStrs = {"None", "Sprites", "Tiles", "Sprites & Tiles"};

        for (size_t i = 0; i < shadowStrs.size(); ++i) {
            shadowComboBox->addItem(shadowStrs[i], (int)(i));
        }

        shadowComboBox->setCurrentIndex(nesPpu.AddShadows);

        QObject::connect(shadowComboBox, &QComboBox::currentIndexChanged, [](int index) {
            nesPpu.AddShadows = index;
        });

        shadowLayout->addWidget(shadowComboBox);
        settingsLayout->addWidget(shadowBox, 3, 0);
    }
    
    QGroupBox *ppuSettingsBox = new QGroupBox("PPU Settings", dialog);
    ppuSettingsBox->setFixedSize(270, isNES ? 150 : 65);

    QGridLayout *ppuSettingsLayout = new QGridLayout(ppuSettingsBox);
    
    QCheckBox *VRAMCorruptCheckBox = new QCheckBox("VRAM Corruption", ppuSettingsBox);
    VRAMCorruptCheckBox->setChecked(isNES ? nesPpu.VRAMCorruption : gbPpu.VRAMCorruption);

    QCheckBox *disableSpritesCheckBox = new QCheckBox("Disable Sprites", ppuSettingsBox);
    disableSpritesCheckBox->setChecked(isNES ? nesPpu.DisableSprites : gbPpu.DisableSprites);

    ppuSettingsLayout->addWidget(VRAMCorruptCheckBox, 0, 0, Qt::AlignLeft | Qt::AlignTop);
    ppuSettingsLayout->addWidget(disableSpritesCheckBox, 0, 1, Qt::AlignLeft | Qt::AlignTop);

    QObject::connect(VRAMCorruptCheckBox, &QCheckBox::toggled, [isNES](bool checked) {
        if (isNES) {
            nesPpu.VRAMCorruption = checked;
        } else {
            gbPpu.VRAMCorruption = checked;
        }
    });
    QObject::connect(disableSpritesCheckBox, &QCheckBox::toggled, [isNES](bool checked) {
        if (isNES) {
            nesPpu.DisableSprites = checked;
        } else {
            gbPpu.DisableSprites = checked;
        }
    });

    if (isNES) {
        QGroupBox *ntmirrorBox = new QGroupBox("NT Mirroring", dialog);
        QVBoxLayout *ntmirrorLayout = new QVBoxLayout(ntmirrorBox);
        QComboBox *ntmirrorComboBox = new QComboBox(ntmirrorBox);

        constexpr std::array<const char*, 5> ntmirrorStrs = {"Horizontal", "Vertical", "Screen A", "Screen B", "Fourscreen"};

        for (size_t i = 0; i < ntmirrorStrs.size(); ++i) {
            ntmirrorComboBox->addItem(ntmirrorStrs[i], (int)(i));
        }

        ntmirrorComboBox->setCurrentIndex((int)(nesPpu.Mirroring));

        QObject::connect(ntmirrorComboBox, &QComboBox::currentIndexChanged, [](int index) {
            nesPpu.Mirroring = (MirrorMode)(index);
            DebugPrintLog("SETTINGS", "Set NT Mirroring to %d", index);
        });

        ntmirrorLayout->addWidget(ntmirrorComboBox);
        ppuSettingsLayout->addWidget(ntmirrorBox, 1, 0);
    }

    QGroupBox *paletteBox = new QGroupBox("Palette", dialog);
    QGridLayout *paletteLayout = new QGridLayout(paletteBox);

    paletteBox->setFixedHeight(isNES ? 400 : 200);

    const int palCount = isNES ? 64 : 4;
    uint32_t *palTable = isNES ? nesPalette : gbPalette;
    uint32_t *palTableOg = isNES ? nesPaletteDefault : gbPaletteDefault;

    std::vector<PaletteButton*> buttons(palCount, nullptr);

    for (int i = 0; i < palCount; i++) {
        buttons[i] = new PaletteButton(i, dialog);

        auto updateButtonColor = [i, &buttons, palTable]() {
            QColor color(
                (palTable[i] >> 16) & 0xFF,
                (palTable[i] >> 8) & 0xFF,
                palTable[i] & 0xFF
            );
            buttons[i]->setStyleSheet(QString("background-color: %1").arg(color.name()));
        };

        updateButtonColor();

        QObject::connect(buttons[i], &QPushButton::clicked, [i, dialog, palTable, updateButtonColor, isNES]() {
            QColor current(
                (palTable[i] >> 16) & 0xFF,
                (palTable[i] >> 8) & 0xFF,
                palTable[i] & 0xFF
            );

            QColor newColor = QColorDialog::getColor(current, dialog);

            if (newColor.isValid()) {
                palTable[i] =
                    (newColor.red() << 16) |
                    (newColor.green() << 8) |
                    newColor.blue();
                updateButtonColor();
                if (isNES && nesPpu.filtering == VideoFilter::NTSC && nesPpu.vfilter) {
                    nesPpu.vfilter->initialize();
                }
            }
        });

        paletteLayout->addWidget(buttons[i], i / 8, i % 8);
    }

    auto updateAllButtonsColor = [buttons, palCount, palTable, isNES]() {
        for (int i = 0; i < palCount; i++) {
            QColor color(
                (palTable[i] >> 16) & 0xFF,
                (palTable[i] >> 8) & 0xFF,
                palTable[i] & 0xFF
            );
            buttons[i]->setStyleSheet(QString("background-color: %1").arg(color.name()));
        }
        if (isNES && nesPpu.filtering == VideoFilter::NTSC && nesPpu.vfilter) {
            nesPpu.vfilter->initialize();
        }
    };

    QPushButton *resetButton = new QPushButton("Reset Palette", dialog);
    QPushButton *randomButton = new QPushButton("Randomize Palette", dialog);
    QPushButton *exportButton = new QPushButton("Export Palette", dialog);
    QPushButton *importButton = new QPushButton("Import Palette", dialog);
    paletteLayout->addWidget(resetButton, 8, 0, 1, 8);
    paletteLayout->addWidget(randomButton, 9, 0, 1, 8);
    paletteLayout->addWidget(exportButton, 10, 0, 1, 8);
    paletteLayout->addWidget(importButton, 11, 0, 1, 8);
    resetButton->setFixedHeight(25);
    randomButton->setFixedHeight(25);
    exportButton->setFixedHeight(25);
    importButton->setFixedHeight(25);
    
    QObject::connect(resetButton, &QPushButton::clicked, [palTable, palTableOg, palCount, updateAllButtonsColor]() {
        memcpy(palTable, palTableOg, palCount * sizeof(uint32_t));
        updateAllButtonsColor();
    });
    QObject::connect(randomButton, &QPushButton::clicked, [palTable, palCount, updateAllButtonsColor]() {
        for (int i = 0; i < palCount; i++) {
            palTable[i] = 0xFF000000 | (rand() << 16) | (rand() << 8) | rand();
        }
        updateAllButtonsColor();
    });
    QObject::connect(exportButton, &QPushButton::clicked, [palTable, palCount, dialog, updateAllButtonsColor]() {
        QString file = QFileDialog::getSaveFileName(
            dialog,
            "Export Palette",
            "",
            "EMeowlator Palette (*.paw)"
        );

        if (!file.isEmpty()) {
            FILE* f = fopen(file.toStdString().c_str(), "wb");
            if (!f) return;
            fwrite(palTable, 1, palCount * sizeof(uint32_t), f);
            fclose(f);
            updateAllButtonsColor();
        }
    });
    QObject::connect(importButton, &QPushButton::clicked, [palTable, palCount, dialog, updateAllButtonsColor]() {
        QString file = QFileDialog::getOpenFileName(
            dialog,
            "Import Palette",
            "",
            "EMeowlator Palette (*.paw)"
        );

        if (!file.isEmpty()) {
            FILE* f = fopen(file.toStdString().c_str(), "rb");
            if (!f) return;
            fread(palTable, 1, palCount * sizeof(uint32_t), f);
            fclose(f);
            updateAllButtonsColor();
        }
    });

    QGroupBox *filterBox = new QGroupBox("Video Filter", dialog);
    QVBoxLayout *filterLayout = new QVBoxLayout(filterBox);
    QComboBox *filterComboBox = new QComboBox(filterBox);

    const std::array<const char*, 4> videoFiltersStr = {"None", "NTSC", "Chroma", "Grayscale"};

    for (size_t i = 0; i < videoFiltersStr.size(); ++i) {
        filterComboBox->addItem(videoFiltersStr[i], (int)(i));
    }

    if (isNES) {
        filterComboBox->setCurrentIndex((int)(nesPpu.filtering));
    } else {
        filterComboBox->setCurrentIndex((int)(gbPpu.filtering));
    }

    QObject::connect(filterComboBox, &QComboBox::currentIndexChanged, [](int index) {
        if (emuConsole) {
            emuConsole->setVideoFilter(index);
            DebugPrintLog("SETTINGS", "Set Filter to %d", index);
        }
    });

    filterLayout->addWidget(filterComboBox);
    filterLayout->addStretch();

    settingsBox->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    ppuSettingsBox->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);

    QVBoxLayout *leftLayout = new QVBoxLayout();
    leftLayout->setAlignment(Qt::AlignTop);

    leftLayout->addWidget(settingsBox);
    leftLayout->addWidget(ppuSettingsBox);
    leftLayout->addStretch();

    mainLayout->addLayout(leftLayout, 0, 0);
    mainLayout->addWidget(paletteBox, 0, 1);

    settingsLayout->addWidget(regionBox, 2, 0);
    settingsLayout->addWidget(filterBox, 2, 1);
    dialog->setLayout(mainLayout);
    dialog->exec();
    delete dialog;
}

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);
    app.setStyle(QStyleFactory::create("Fusion"));
    QMainWindow window;
    ScreenWidget *screen = new ScreenWidget(&window);
    globalQTWin = (void*)(&window);

    makeDirectory("saves");

    Config::Load("meowconf.txt");

    QMenuBar *menuBar = window.menuBar();
    QMenu *fileMenu = menuBar->addMenu("File");
    QMenu *gameMenu = menuBar->addMenu("Game");
    QMenu *settingsMenu = menuBar->addMenu("Options");
    QMenu *miscMenu = menuBar->addMenu("Misc");

    QAction *openAction = new QAction("Open ROM", &window);
    QAction *closeAction = new QAction("Close ROM", &window);

    QAction *gameResetAction = new QAction("Reset", &window);
    QAction *gamePauseAction = new QAction("Pause", &window);

    QAction *keyEditAction = new QAction("Input Config", &window);
    QAction *audioConfAction = new QAction("Audio Config", &window);
    QAction *displayConfAction = new QAction("Display Config", &window);

    QAction *romInfoAction = new QAction("ROM Info", &window);
    QAction *exitAction = new QAction("Exit", &window);

    fileMenu->addAction(openAction);
    fileMenu->addAction(closeAction);
    fileMenu->addSeparator();

    gameMenu->addAction(gameResetAction);
    gameMenu->addAction(gamePauseAction);

    settingsMenu->addAction(keyEditAction);
    settingsMenu->addAction(audioConfAction);
    settingsMenu->addAction(displayConfAction);

    miscMenu->addAction(romInfoAction);
    miscMenu->addAction(exitAction);

    QObject::connect(exitAction, &QAction::triggered, &window, &QMainWindow::close);
    QObject::connect(openAction, &QAction::triggered, [&]() {
        QString file = QFileDialog::getOpenFileName(
            &window,
            "Open ROM",
            "",
            "Supported ROMs (*.nes *.gb *.gbc)"
        );

        if (!file.isEmpty()) {
            if (loadConsoleWithGame(file.toStdString())) {
                startCPUTimer();
                emuConsole->reset();

                int w = (int)(emuConsole->getDisplayWidth() * 2.5);
                int h = (int)(emuConsole->getDisplayHeight() * 2.5);
                screen->resize(w, h);
                window.resize(w, h + window.menuBar()->height());
            }
        }
    });
    QObject::connect(closeAction, &QAction::triggered, [&]() {
        if (romIsLoaded) {
            if (emuConsole) {
                emuConsole->writeSave();
                emuConsole->reset();
            }
            romIsLoaded = false;
            window.setWindowTitle("EMeowlator");
        }
    });
    QObject::connect(gameResetAction, &QAction::triggered, [&]() {
        if (emuConsole) emuConsole->reset();
        DebugPrintLog("GAME", "Reset Game");
    });
    QObject::connect(gamePauseAction, &QAction::triggered, [&]() {
        if (emuConsole) emuConsole->pause();
    });
    
    QObject::connect(keyEditAction, &QAction::triggered, [&]() { ShowInputConfigDialog(&window); });
    QObject::connect(audioConfAction, &QAction::triggered, [&]() { ShowAudioConfigDialog(&window); });
    QObject::connect(displayConfAction, &QAction::triggered, [&]() { ShowDisplayConfigDialog(&window); });
    QObject::connect(romInfoAction, &QAction::triggered, [&]() { ShowRomInfoDialog(&window); });

    window.setCentralWidget(screen);
    InputManager inputMgr;
    inputMgr.install(&window);

    audioSystem.init();
    cpuTimer.setTimerType(Qt::PreciseTimer);
    
    QObject::connect(&cpuTimer, &QTimer::timeout, [&]() {
        screen->update();
        if (romIsLoaded && emuConsole) {
            emuConsole->runFrame();
            screen->image = emuConsole->getOutputImage();
            rainbowHoverPhase += 0.002f;
            if (rainbowHoverPhase > 1.0f) rainbowHoverPhase -= 1.0f;
        }
    });

    window.resize(256 * 2, 240 * 2);

    QPixmap pixmap;
    pixmap.loadFromData(EMeowlatorIcon, sizeof(EMeowlatorIcon) / sizeof(EMeowlatorIcon[0]));
    window.setWindowTitle("EMeowlator");
    window.setWindowIcon(QIcon(pixmap));
    window.show();

    QObject::connect(&app, &QApplication::aboutToQuit, [&]() {
        if (emuConsole) {
            emuConsole->writeSave();
            delete emuConsole;
            emuConsole = nullptr;
        }
        audioSystem.close();
        Config::Write("meowconf.txt");
    });

    if (argc > 1) {
        if (loadConsoleWithGame(argv[1])) {
            startCPUTimer();
            emuConsole->reset();

            int w = (int)(emuConsole->getDisplayWidth() * 2.5);
            int h = (int)(emuConsole->getDisplayHeight() * 2.5);
            screen->resize(w, h);
            window.resize(w, h + window.menuBar()->height());
        }
    }

    return app.exec();
}