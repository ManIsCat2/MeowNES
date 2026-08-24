#pragma once

#include "../main.hpp"
#include <QTimer>

struct MemoryRegion {
    std::string name;
    uint8_t *data = nullptr;
    size_t size = 0;
};

class MemoryHexView : public QWidget {
public:
    MemoryHexView(MemoryRegion region, QWidget *parent = nullptr) : QWidget(parent), region(region) {
        QVBoxLayout *layout = new QVBoxLayout(this);

        QHBoxLayout *topRow = new QHBoxLayout();
        QLabel *gotoLabel = new QLabel("Goto (hex):", this);
        gotoEdit = new QLineEdit(this);
        gotoEdit->setPlaceholderText("0000");
        gotoEdit->setFixedWidth(70);
        QPushButton *gotoButton = new QPushButton("Go", this);

        autoRefreshCheck = new QCheckBox("Auto Refresh", this);
        autoRefreshCheck->setChecked(true);
        QPushButton *refreshButton = new QPushButton("Refresh", this);

        topRow->addWidget(gotoLabel);
        topRow->addWidget(gotoEdit);
        topRow->addWidget(gotoButton);
        topRow->addStretch();
        topRow->addWidget(autoRefreshCheck);
        topRow->addWidget(refreshButton);

        hexEdit = new QPlainTextEdit(this);
        hexEdit->setReadOnly(true);
        hexEdit->setFont(QFont("Courier New", 10));
        hexEdit->setLineWrapMode(QPlainTextEdit::NoWrap);

        QHBoxLayout *pokeRow = new QHBoxLayout();
        QLabel *addrLabel = new QLabel("Poke addr (hex):", this);
        pokeAddrEdit = new QLineEdit(this);
        pokeAddrEdit->setFixedWidth(70);
        QLabel *valLabel = new QLabel("Value (hex):", this);
        pokeValEdit = new QLineEdit(this);
        pokeValEdit->setFixedWidth(50);
        QPushButton *pokeButton = new QPushButton("Poke", this);

        pokeRow->addWidget(addrLabel);
        pokeRow->addWidget(pokeAddrEdit);
        pokeRow->addWidget(valLabel);
        pokeRow->addWidget(pokeValEdit);
        pokeRow->addWidget(pokeButton);
        pokeRow->addStretch();

        layout->addLayout(topRow);
        layout->addWidget(hexEdit);
        layout->addLayout(pokeRow);

        QObject::connect(gotoButton, &QPushButton::clicked, this, &MemoryHexView::gotoAddress);
        QObject::connect(refreshButton, &QPushButton::clicked, this, &MemoryHexView::refresh);
        QObject::connect(pokeButton, &QPushButton::clicked, this, &MemoryHexView::pokeByte);

        refreshTimer = new QTimer(this);
        QObject::connect(refreshTimer, &QTimer::timeout, this, [this]() {
            if (autoRefreshCheck->isChecked() && isVisible()) refresh();
        });
        refreshTimer->start(200);

        refresh();
    }

    void refresh() {
        int scrollValue = hexEdit->verticalScrollBar()->value();

        QString text;
        text.reserve((int)(region.size * 4));

        for (size_t row = 0; row < region.size; row += 16) {
            text += QString::asprintf("%04zX: ", row);

            QString hexPart, asciiPart;
            for (size_t col = 0; col < 16; col++) {
                size_t idx = row + col;
                if (idx < region.size) {
                    uint8_t b = region.data[idx];
                    hexPart += QString::asprintf("%02X ", b);
                    asciiPart += (b >= 0x20 && b < 0x7F) ? QChar((char)(b)) : QChar('.');
                } else {
                    hexPart += "   ";
                    asciiPart += ' ';
                }
                if (col == 7) hexPart += " ";
            }

            text += hexPart + " " + asciiPart + "\n";
        }

        hexEdit->setPlainText(text);
        hexEdit->verticalScrollBar()->setValue(scrollValue);
    }

private:
    void gotoAddress() {
        bool ok = false;
        unsigned int addr = gotoEdit->text().toUInt(&ok, 16);
        if (!ok) return;

        int row = (int)(addr / 16);
        QTextCursor cursor(hexEdit->document()->findBlockByNumber(row));
        hexEdit->setTextCursor(cursor);
        hexEdit->centerCursor();
    }

    void pokeByte() {
        bool okAddr = false, okVal = false;
        unsigned int addr = pokeAddrEdit->text().toUInt(&okAddr, 16);
        unsigned int val = pokeValEdit->text().toUInt(&okVal, 16);

        if (!okAddr || !okVal || addr >= region.size || val > 0xFF) return;

        region.data[addr] = (uint8_t)(val);
        refresh();
    }

    MemoryRegion region;
    QPlainTextEdit *hexEdit;
    QLineEdit *gotoEdit;
    QLineEdit *pokeAddrEdit;
    QLineEdit *pokeValEdit;
    QCheckBox *autoRefreshCheck;
    QTimer *refreshTimer;
};