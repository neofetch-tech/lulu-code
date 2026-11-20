#pragma once

#include <QString>

namespace lulu {

/// Central place for every on-disk location Lulu Code uses.
/// All paths are created on first use.
class AppPaths
{
public:
    /// Per-user data directory (settings live in QSettings, big data lives here).
    static QString dataDir();
    /// Downloaded GGUF models.
    static QString modelsDir();
    /// Log files.
    static QString logsDir();
    /// Model manifest (installed models).
    static QString manifestFile();

    static void ensure();

    /// Human-readable location shown in the UI.
    static QString displayDataDir();
};

} // namespace lulu
