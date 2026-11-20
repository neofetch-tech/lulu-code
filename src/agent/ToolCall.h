#pragma once

#include <QJsonObject>
#include <QString>
#include <QVector>

namespace lulu {

struct ToolCall {
    QString id;
    QString name;
    QJsonObject args;
};

struct ToolResult {
    bool ok = false;
    QString output;
    QString error;

    static ToolResult success(QString text)
    {
        return ToolResult{true, std::move(text), {}};
    }
    static ToolResult failure(QString text)
    {
        return ToolResult{false, {}, std::move(text)};
    }
};

struct ToolSpec {
    QString name;
    QString signature;
    QString description;
};

/// Parses the small, model-agnostic tool protocol Lulu Code teaches models:
///
///     ```lulu-tool
///     {"tool": "read_file", "args": {"path": "src/main.cpp"}}
///     ```
///
/// Tolerates ```json fences, <lulu-tool> tags, OpenAI-style
/// {"function": {"name": ..., "arguments": ...}} objects and the usual
/// malformed JSON that small local models produce (raw newlines in strings,
/// trailing commas).
class ToolCallParser
{
public:
    struct Parsed {
        QString cleanText;
        QVector<ToolCall> calls;
    };

    static Parsed parse(const QString& text);
    /// True when the text looks like it is trying to call a tool.
    static bool containsToolBlock(const QString& text);
};

} // namespace lulu
