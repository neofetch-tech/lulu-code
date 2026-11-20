#include "agent/ToolCall.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QRegularExpression>

namespace lulu {

namespace {

QString repairJson(const QString& input)
{
    QString out;
    out.reserve(input.size() + 32);

    bool inString = false;
    bool escaped = false;
    for (const QChar ch : input) {
        if (inString) {
            if (escaped) {
                out.append(ch);
                escaped = false;
            } else if (ch == QLatin1Char('\\')) {
                out.append(ch);
                escaped = true;
            } else if (ch == QLatin1Char('"')) {
                out.append(ch);
                inString = false;
            } else if (ch == QLatin1Char('\n')) {
                out.append(QStringLiteral("\\n"));
            } else if (ch == QLatin1Char('\r')) {
                // drop
            } else if (ch == QLatin1Char('\t')) {
                out.append(QStringLiteral("\\t"));
            } else {
                out.append(ch);
            }
        } else {
            if (ch == QLatin1Char('"')) {
                inString = true;
            } else if (ch == QChar(0x201C) || ch == QChar(0x201D)) {
                out.append(QLatin1Char('"'));
                inString = true;
                continue;
            }
            out.append(ch);
        }
    }

    out.replace(QRegularExpression(QStringLiteral(",\\s*([}\\]])")), QStringLiteral("\\1"));
    return out;
}

QJsonObject parseObjectLenient(const QString& text)
{
    QJsonParseError error{};
    QJsonDocument doc = QJsonDocument::fromJson(text.toUtf8(), &error);
    if (error.error != QJsonParseError::NoError) {
        doc = QJsonDocument::fromJson(repairJson(text).toUtf8(), &error);
    }
    return doc.object();
}

QJsonObject extractArguments(const QJsonObject& object)
{
    for (const QString& key : {QStringLiteral("args"), QStringLiteral("arguments"),
                               QStringLiteral("parameters"), QStringLiteral("input")}) {
        const QJsonValue value = object.value(key);
        if (value.isObject()) {
            return value.toObject();
        }
        if (value.isString()) {
            const QJsonObject nested = parseObjectLenient(value.toString());
            if (!nested.isEmpty()) {
                return nested;
            }
        }
    }
    return {};
}

QString extractName(const QJsonObject& object)
{
    QString name = object.value(QStringLiteral("tool")).toString();
    if (name.isEmpty()) {
        name = object.value(QStringLiteral("name")).toString();
    }
    if (name.isEmpty()) {
        const QJsonObject function = object.value(QStringLiteral("function")).toObject();
        name = function.value(QStringLiteral("name")).toString();
    }
    return name.trimmed();
}

QJsonObject unwrapArguments(const QJsonObject& object)
{
    const QJsonObject function = object.value(QStringLiteral("function")).toObject();
    if (function.contains(QStringLiteral("arguments"))) {
        return extractArguments(function);
    }
    return extractArguments(object);
}

void appendCalls(const QJsonValue& value, QVector<ToolCall>& out)
{
    if (value.isArray()) {
        for (const QJsonValue& item : value.toArray()) {
            appendCalls(item, out);
        }
        return;
    }
    if (!value.isObject()) {
        return;
    }
    const QJsonObject object = value.toObject();
    const QString name = extractName(object);
    if (name.isEmpty()) {
        // A wrapped call such as {"tool_call": {...}} or {"calls": [...]}
        for (const auto& child : object) {
            if (child.isObject() || child.isArray()) {
                appendCalls(child, out);
            }
        }
        return;
    }
    ToolCall call;
    call.name = name;
    call.args = unwrapArguments(object);
    out.append(call);
}

QVector<ToolCall> callsFromPayload(const QString& payload)
{
    QVector<ToolCall> out;

    QJsonParseError error{};
    QJsonDocument doc = QJsonDocument::fromJson(payload.toUtf8(), &error);
    if (error.error != QJsonParseError::NoError) {
        doc = QJsonDocument::fromJson(repairJson(payload).toUtf8(), &error);
    }

    if (error.error == QJsonParseError::NoError) {
        if (doc.isArray()) {
            appendCalls(doc.array(), out);
        } else if (doc.isObject()) {
            appendCalls(doc.object(), out);
        }
        if (!out.isEmpty()) {
            return out;
        }
    }

    // Some models put several JSON objects on separate lines inside one fence.
    const QStringList lines = payload.split(QLatin1Char('\n'), Qt::SkipEmptyParts);
    if (lines.size() > 1) {
        for (const QString& line : lines) {
            const QString trimmed = line.trimmed();
            if (!trimmed.startsWith(QLatin1Char('{'))) {
                continue;
            }
            const QJsonObject object = parseObjectLenient(trimmed);
            if (!object.isEmpty()) {
                appendCalls(object, out);
            }
        }
    }
    return out;
}

} // namespace

bool ToolCallParser::containsToolBlock(const QString& text)
{
    static const QRegularExpression fence(
        QStringLiteral("```\\s*(?:lulu-tool|lulu|json|tool)?\\s*\\n([\\s\\S]*?)```"),
        QRegularExpression::CaseInsensitiveOption);
    static const QRegularExpression tag(QStringLiteral("<lulu-tool>([\\s\\S]*?)</lulu-tool>"),
                                        QRegularExpression::CaseInsensitiveOption);
    return fence.match(text).hasMatch() || tag.match(text).hasMatch();
}

ToolCallParser::Parsed ToolCallParser::parse(const QString& text)
{
    Parsed result;
    result.cleanText = text;

    QVector<QPair<int, int>> ranges; // start, length

    static const QRegularExpression fence(
        QStringLiteral("```\\s*(?:lulu-tool|lulu|json|tool)?\\s*\\n([\\s\\S]*?)```"),
        QRegularExpression::CaseInsensitiveOption);
    static const QRegularExpression tag(QStringLiteral("<lulu-tool>([\\s\\S]*?)</lulu-tool>"),
                                        QRegularExpression::CaseInsensitiveOption);

    auto collect = [&](const QRegularExpression& regex) {
        auto it = regex.globalMatch(text);
        while (it.hasNext()) {
            const QRegularExpressionMatch match = it.next();
            const QVector<ToolCall> calls = callsFromPayload(match.captured(1));
            if (calls.isEmpty()) {
                continue;
            }
            result.calls += calls;
            ranges.append({static_cast<int>(match.capturedStart()), static_cast<int>(match.capturedLength())});
        }
    };

    collect(fence);
    collect(tag);

    if (result.calls.isEmpty()) {
        const QString trimmed = text.trimmed();
        if (trimmed.startsWith(QLatin1Char('{')) || trimmed.startsWith(QLatin1Char('['))) {
            const QVector<ToolCall> calls = callsFromPayload(trimmed);
            if (!calls.isEmpty()) {
                result.calls = calls;
                result.cleanText.clear();
                for (qsizetype i = 0; i < result.calls.size(); ++i) {
                    result.calls[i].id = QStringLiteral("call-%1").arg(i + 1);
                }
                return result;
            }
        }
    }

    // Remove the blocks from the human-readable text, latest first so the
    // earlier offsets stay valid.
    std::sort(ranges.begin(), ranges.end(), [](const auto& a, const auto& b) {
        return a.first > b.first;
    });
    for (const auto& range : ranges) {
        result.cleanText.remove(range.first, range.second);
    }

    // Drop the fences' leftovers and collapse excess blank lines.
    result.cleanText.replace(QRegularExpression(QStringLiteral("\\n{3,}")), QStringLiteral("\n\n"));
    result.cleanText = result.cleanText.trimmed();

    for (qsizetype i = 0; i < result.calls.size(); ++i) {
        result.calls[i].id = QStringLiteral("call-%1").arg(i + 1);
    }
    return result;
}

} // namespace lulu
