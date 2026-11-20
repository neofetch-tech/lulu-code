#include "core/ChatMessage.h"

namespace lulu {

QString roleName(Role role)
{
    switch (role) {
    case Role::System:
        return QStringLiteral("system");
    case Role::User:
        return QStringLiteral("user");
    case Role::Assistant:
        return QStringLiteral("assistant");
    }
    return QStringLiteral("user");
}

} // namespace lulu
