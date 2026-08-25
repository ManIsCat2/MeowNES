#pragma once

#include "../main.hpp"

#include <QTimer>
#include <QKeyEvent>
#include <QScrollBar>
#include <QTextBlock>

struct MemoryRegion {
    std::string name;
    bool readOnly = false;
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
        
        hexEdit->installEventFilter(this);

        layout->addLayout(topRow);
        layout->addWidget(hexEdit);

        QObject::connect(gotoButton, &QPushButton::clicked, this, &MemoryHexView::gotoAddress);
        QObject::connect(refreshButton, &QPushButton::clicked, this, &MemoryHexView::refresh);

        refreshTimer = new QTimer(this);
        QObject::connect(refreshTimer, &QTimer::timeout, this, [this]() {
            if (autoRefreshCheck->isChecked() && isVisible()) refresh();
        });
        refreshTimer->start(200);

        refresh(true);
    }

    void refresh(bool force=false) {
        if (region.readOnly && !force) return;
        int scrollValue = hexEdit->verticalScrollBar()->value();
        
        QTextCursor currentCursor = hexEdit->textCursor();
        int bNum = currentCursor.blockNumber();
        int bPos = currentCursor.positionInBlock();

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
        
        QTextBlock block = hexEdit->document()->findBlockByNumber(bNum);
        if (block.isValid()) {
            QTextCursor newCursor(block);
            newCursor.setPosition(block.position() + std::min(bPos, block.length() - 1));
            hexEdit->setTextCursor(newCursor);
        }
        
        hexEdit->verticalScrollBar()->setValue(scrollValue);
    }
protected:
    bool eventFilter(QObject *obj, QEvent *event) override {
        if (obj == hexEdit && event->type() == QEvent::KeyPress) {
            QKeyEvent *keyEvent = static_cast<QKeyEvent *>(event);
            
            switch (keyEvent->key()) {
                case Qt::Key_Up:
                case Qt::Key_Down:
                case Qt::Key_Left:
                case Qt::Key_Right:
                case Qt::Key_PageUp:
                case Qt::Key_PageDown:
                    return false;
            }

            QString text = keyEvent->text();
            if (text.isEmpty()) return false;

            char c = text[0].toUpper().toLatin1();
            if ((c >= '0' && c <= '9') || (c >= 'A' && c <= 'F')) {
                uint8_t nibble = (c >= '0' && c <= '9') ? (c - '0') : (c - 'A' + 10);
                
                QTextCursor cursor = hexEdit->textCursor();
                
                if (cursor.hasSelection()) {
                    cursor.clearSelection();
                    hexEdit->setTextCursor(cursor);
                }
                
                int row = cursor.blockNumber();
                int col = cursor.positionInBlock();
                
                int byteIdx = -1;
                bool isHigh = false;
                
                if (col >= 6 && col <= 28) {
                    int rel = col - 6;
                    if (rel % 3 != 2) {
                        byteIdx = rel / 3;
                        isHigh = (rel % 3 == 0);
                    }
                } else if (col >= 31 && col <= 53) {
                    int rel = col - 31;
                    if (rel % 3 != 2) {
                        byteIdx = 8 + (rel / 3);
                        isHigh = (rel % 3 == 0);
                    }
                }
                
                if (byteIdx != -1) {
                    size_t memAddr = (row * 16) + byteIdx;
                    if (memAddr < region.size) {
                        uint8_t currentVal = region.data[memAddr];
                        if (isHigh) {
                            region.data[memAddr] = (currentVal & 0x0F) | (nibble << 4);
                        } else {
                            region.data[memAddr] = (currentVal & 0xF0) | nibble;
                        }
                        
                        hexEdit->setReadOnly(false);
                        cursor.deleteChar(); 
                        cursor.insertText(QString(c));
                        
                        if (!isHigh) {
                            if (byteIdx == 7) {
                                cursor.movePosition(QTextCursor::Right, QTextCursor::MoveAnchor, 2);
                            } else if (byteIdx == 15) {
                                if (row < hexEdit->document()->blockCount() - 1) {
                                    cursor.movePosition(QTextCursor::NextBlock, QTextCursor::MoveAnchor);
                                    cursor.movePosition(QTextCursor::Right, QTextCursor::MoveAnchor, 6);
                                } else {
                                    cursor.movePosition(QTextCursor::Right, QTextCursor::MoveAnchor, 1);
                                }
                            } else {
                                cursor.movePosition(QTextCursor::Right, QTextCursor::MoveAnchor, 1);
                            }
                        }
                        hexEdit->setTextCursor(cursor);
                        
                        int asciiCol = 56 + byteIdx; 
                        QTextCursor asciiCursor = hexEdit->textCursor();
                        asciiCursor.setPosition(asciiCursor.block().position() + asciiCol);
                        
                        uint8_t newVal = region.data[memAddr];
                        QChar asciiChar = (newVal >= 0x20 && newVal < 0x7F) ? QChar((char)newVal) : QChar('.');
                        
                        asciiCursor.deleteChar();
                        asciiCursor.insertText(QString(asciiChar));
                        hexEdit->setReadOnly(true);
                    }
                }
                return true;
            }
            return true;
        }
        return QWidget::eventFilter(obj, event);
    }
private:
    void gotoAddress() {
        bool ok = false;
        unsigned int addr = gotoEdit->text().toUInt(&ok, 16);
        if (!ok) return;

        int row = (int)(addr / 16);
        QTextBlock block = hexEdit->document()->findBlockByNumber(row);
        if (block.isValid()) {
            QTextCursor cursor(block);
            cursor.setPosition(block.position() + 6);
            hexEdit->setTextCursor(cursor);
            hexEdit->centerCursor();
            hexEdit->setFocus();
        }
    }

    MemoryRegion region;
    QPlainTextEdit *hexEdit;
    QLineEdit *gotoEdit;
    QCheckBox *autoRefreshCheck;
    QTimer *refreshTimer;
};