// Unit tests for MappingModel::isValidInput and ::isValidOutput.
//
// Standalone — only Qt6::Core is linked. The two validation methods are
// inline in model.h (they use just QString/QChar), so the test doesn't need
// model.cpp or fcitx-utils. Static-method calls on the class don't require
// the Q_OBJECT vtable, so no MOC processing is needed either.

#include "model.h"

#include <QCoreApplication>
#include <QString>

#include <cstdio>
#include <cstdlib>

using fcitx::MappingModel;

#define EXPECT(cond)                                                           \
    do {                                                                       \
        if (!(cond)) {                                                         \
            std::fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__,       \
                         #cond);                                               \
            std::abort();                                                      \
        }                                                                      \
    } while (0)

// -- isValidOutput: F3 guard -------------------------------------------------

// Embedded '\n' is the mappings.txt entry separator. fgets would split one
// mapping into two lines on reload, silently dropping the tail. Must be
// rejected wherever it occurs in the value.
void testOutputRejectsNewline() {
    EXPECT(!MappingModel::isValidOutput(QStringLiteral("hello\nworld")));
}
void testOutputRejectsNewlineAtStart() {
    EXPECT(!MappingModel::isValidOutput(QStringLiteral("\nhello")));
}
void testOutputRejectsNewlineAtEnd() {
    EXPECT(!MappingModel::isValidOutput(QStringLiteral("hello\n")));
}

// '\r' at the end of a line would be trimmed by the parser's trailing-
// whitespace loop; in the middle it would pass through unchanged and
// reach commitString where it renders erratically. Reject both cases.
void testOutputRejectsCarriageReturn() {
    EXPECT(!MappingModel::isValidOutput(QStringLiteral("hello\rworld")));
}
void testOutputRejectsCrLf() {
    EXPECT(!MappingModel::isValidOutput(QStringLiteral("line1\r\nline2")));
}
void testOutputRejectsLoneCarriageReturn() {
    EXPECT(!MappingModel::isValidOutput(QStringLiteral("\r")));
}

// -- isValidOutput: values that must still be accepted ----------------------

void testOutputAcceptsPlainAscii() {
    EXPECT(MappingModel::isValidOutput(QStringLiteral("hello")));
}
void testOutputAcceptsMultiByteUtf8() {
    EXPECT(MappingModel::isValidOutput(QString::fromUtf8("ä")));
    EXPECT(MappingModel::isValidOutput(QString::fromUtf8("€")));
}
// Tabs and leading/trailing spaces are intentional — see editor.cpp:117
// ("Output is NOT trimmed"). They are not file-format hazards.
void testOutputAcceptsTabs() {
    EXPECT(MappingModel::isValidOutput(QStringLiteral("\tindent")));
    EXPECT(MappingModel::isValidOutput(QStringLiteral("col1\tcol2")));
}
void testOutputAcceptsLeadingAndTrailingSpace() {
    EXPECT(MappingModel::isValidOutput(QStringLiteral(" ls")));
    EXPECT(MappingModel::isValidOutput(QStringLiteral("hello ")));
}
// Cycling variant separator — handled by splitOutputs at a later stage,
// not a format-level issue for isValidOutput.
void testOutputAcceptsCommas() {
    EXPECT(MappingModel::isValidOutput(QStringLiteral("ä,à,á,â")));
}
void testOutputAcceptsEmoji() {
    EXPECT(MappingModel::isValidOutput(QString::fromUtf8("😀")));
    EXPECT(MappingModel::isValidOutput(QString::fromUtf8("hi 😀 bye")));
}
// Empty output is rejected by addMapping() via isEmpty(), not by
// isValidOutput itself. Keep the contract crisp.
void testOutputAcceptsEmptyString() {
    EXPECT(MappingModel::isValidOutput(QString()));
}

// -- isValidInput: regression guard around the F3 change --------------------

void testInputAcceptsSingleAscii() {
    EXPECT(MappingModel::isValidInput(QStringLiteral("a")));
    EXPECT(MappingModel::isValidInput(QStringLiteral("Z")));
    EXPECT(MappingModel::isValidInput(QStringLiteral("=")));
}
void testInputAcceptsSingleMultiByte() {
    EXPECT(MappingModel::isValidInput(QString::fromUtf8("ä")));
    EXPECT(MappingModel::isValidInput(QString::fromUtf8("€")));
}
// Single codepoint emojis (U+1F600 etc.) must pass — important for users
// with custom layouts that emit emoji keysyms.
void testInputAcceptsSingleEmoji() {
    EXPECT(MappingModel::isValidInput(QString::fromUtf8("😀")));
}
void testInputRejectsEmpty() { EXPECT(!MappingModel::isValidInput(QString())); }
void testInputRejectsMultipleCodepoints() {
    EXPECT(!MappingModel::isValidInput(QStringLiteral("ab")));
    // ZWJ sequence (👨‍💻) is 4 codepoints — input field requires
    // exactly 1.
    EXPECT(!MappingModel::isValidInput(QString::fromUtf8("\xf0\x9f\x91\xa8"
                                                         "\xe2\x80\x8d"
                                                         "\xf0\x9f\x92\xbb")));
}
void testInputRejectsWhitespace() {
    EXPECT(!MappingModel::isValidInput(QStringLiteral(" ")));
    EXPECT(!MappingModel::isValidInput(QStringLiteral("\t")));
    EXPECT(!MappingModel::isValidInput(QStringLiteral("\n")));
    EXPECT(!MappingModel::isValidInput(QStringLiteral("\r")));
}

int main(int argc, char *argv[]) {
    QCoreApplication app(argc, argv);

    testOutputRejectsNewline();
    testOutputRejectsNewlineAtStart();
    testOutputRejectsNewlineAtEnd();
    testOutputRejectsCarriageReturn();
    testOutputRejectsCrLf();
    testOutputRejectsLoneCarriageReturn();

    testOutputAcceptsPlainAscii();
    testOutputAcceptsMultiByteUtf8();
    testOutputAcceptsTabs();
    testOutputAcceptsLeadingAndTrailingSpace();
    testOutputAcceptsCommas();
    testOutputAcceptsEmoji();
    testOutputAcceptsEmptyString();

    testInputAcceptsSingleAscii();
    testInputAcceptsSingleMultiByte();
    testInputAcceptsSingleEmoji();
    testInputRejectsEmpty();
    testInputRejectsMultipleCodepoints();
    testInputRejectsWhitespace();

    std::printf("All mapping model tests passed.\n");
    return 0;
}
