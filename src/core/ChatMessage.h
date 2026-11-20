#pragma once

#include <QString>
#include <QStringList>
#include <QVector>
#include <QMetaType>

namespace lulu {

enum class Role {
    System,
    User,
    Assistant,
};

struct ChatMessage {
    Role role = Role::User;
    QString content;

    static ChatMessage system(const QString& text) { return {Role::System, text}; }
    static ChatMessage user(const QString& text) { return {Role::User, text}; }
    static ChatMessage assistant(const QString& text) { return {Role::Assistant, text}; }
};

struct GenerationParams {
    int maxTokens = 768;
    double temperature = 0.7;
    double topP = 0.95;
    int topK = 40;
    double repeatPenalty = 1.08;
    quint32 seed = 0xFFFFFFFFu;
    QStringList stopSequences;
};

struct TokenStats {
    int promptTokens = 0;
    int generatedTokens = 0;
    double tokensPerSecond = 0.0;
};

QString roleName(Role role);

} // namespace lulu

Q_DECLARE_METATYPE(lulu::ChatMessage)
Q_DECLARE_METATYPE(QVector<lulu::ChatMessage>)
Q_DECLARE_METATYPE(lulu::GenerationParams)
Q_DECLARE_METATYPE(lulu::TokenStats)
