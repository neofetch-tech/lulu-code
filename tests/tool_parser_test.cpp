#include <QtTest>
#include <QJsonDocument>
#include <QJsonObject>
#include <QString>
#include <QVector>

#include "agent/ToolCall.h"

using namespace lulu;

class ToolCallParserTest : public QObject
{
    Q_OBJECT

private slots:
    void parseValidToolBlock()
    {
        // Given
        QString input = QStringLiteral(R"(
```lulu-tool
{"tool": "read_file", "args": {"path": "src/main.cpp"}}
```
)");

        // When
        auto parsed = ToolCallParser::parse(input);

        // Then
        QCOMPARE(parsed.calls.size(), 1u);
        QCOMPARE(parsed.calls[0].name, QStringLiteral("read_file"));
        QCOMPARE(parsed.calls[0].args[QStringLiteral("path")].toString(), QStringLiteral("src/main.cpp"));
        QCOMPARE(parsed.cleanText.isEmpty(), true);
    }

    void parseMultipleToolBlocks()
    {
        // Given
        QString input = QStringLiteral(R"(
```lulu-tool
{"tool": "list_files", "args": {"path": "."}}
{"tool": "read_file", "args": {"path": "src/main.cpp"}}
```
)");

        // When
        auto parsed = ToolCallParser::parse(input);

        // Then
        QCOMPARE(parsed.calls.size(), 2u);
        QCOMPARE(parsed.calls[0].name, QStringLiteral("list_files"));
        QCOMPARE(parsed.calls[1].name, QStringLiteral("read_file"));
    }

    void parseJsonFences()
    {
        // Given
        QString input = QStringLiteral(R"(
```json
{"tool": "read_file", "args": {"path": "src/main.cpp"}}
```
)");

        // When
        auto parsed = ToolCallParser::parse(input);

        // Then
        QCOMPARE(parsed.calls.size(), 1u);
        QCOMPARE(parsed.calls[0].name, QStringLiteral("read_file"));
    }

    void parseMalformedJsonTolerant()
    {
        // Given - model output with raw newlines in strings, trailing commas
        QString input = QStringLiteral(R"(
```lulu-tool
{"tool": "read_file", "args": {"path": "src/main.cpp", "start_line": 1, }}
```
)");

        // When
        auto parsed = ToolCallParser::parse(input);

        // Then
        // Should still parse the tool call even with trailing comma
        QVERIFY(parsed.calls.size() >= 1);
    }

    void containsToolBlock()
    {
        // Given
        QString text = QStringLiteral("Here is a tool call: ```lulu-tool {\"tool\": \"read_file\", \"args\": {\"path\": \"x\"}} ``` and some prose.");

        // When
        bool hasBlock = ToolCallParser::containsToolBlock(text);

        // Then
        QVERIFY(hasBlock);
    }

    void noToolBlockWhenPlain()
    {
        // Given
        QString text = QStringLiteral("Just some regular text without any tool blocks.");

        // When
        bool hasBlock = ToolCallParser::containsToolBlock(text);

        // Then
        QVERIFY(!hasBlock);
    }
};

QTEST_MAIN(ToolCallParserTest)
#include "moc_ToolCallParserTest.cpp"