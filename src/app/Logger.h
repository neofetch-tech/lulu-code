#pragma once

namespace lulu {

/// Installs a qInstallMessageHandler that writes a compact, readable log to
/// the console and to <data>/logs/lulu-code.log (rotated at ~2 MB).
class Logger
{
public:
    static void install();
};

} // namespace lulu
