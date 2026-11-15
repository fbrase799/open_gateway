#include "display_config.hpp"

#include <QFile>

namespace {

constexpr auto kDefaultTitle = "Open Gateway";

QString unquote(const QString& value) {
    if (value.size() < 2) {
        return {};
    }
    const auto quote = value.at(0);
    if (quote != QLatin1Char('"') && quote != QLatin1Char('\'')) {
        return {};
    }
    QString result;
    bool escape = false;
    for (int i = 1; i < value.size(); ++i) {
        const auto ch = value.at(i);
        if (escape) {
            switch (ch.unicode()) {
            case 'n':
                result += QLatin1Char('\n');
                break;
            case 't':
                result += QLatin1Char('\t');
                break;
            case 'r':
                result += QLatin1Char('\r');
                break;
            default:
                result += ch;
                break;
            }
            escape = false;
            continue;
        }
        if (quote == QLatin1Char('"') && ch == QLatin1Char('\\')) {
            escape = true;
            continue;
        }
        if (ch == quote) {
            return result;
        }
        result += ch;
    }
    return {};
}

}  // namespace

QHash<QString, DeviceEntry>* device_table(DisplaySettings& settings, const QString& group) {
    if (group == QLatin1String("simulators")) {
        return &settings.simulators;
    }
    if (group == QLatin1String("devices")) {
        return &settings.devices;
    }
    return nullptr;
}

bool is_false(const QString& raw) {
    return raw.compare(QLatin1String("false"), Qt::CaseInsensitive) == 0;
}

bool is_true(const QString& raw) {
    return raw.compare(QLatin1String("true"), Qt::CaseInsensitive) == 0;
}

DisplaySettings display_settings_from_toml(const QString& text) {
    DisplaySettings settings;
    settings.title = QString::fromLatin1(kDefaultTitle);
    QHash<QString, DeviceEntry>* table = nullptr;
    QString entryId;
    const auto lines = text.split(QLatin1Char('\n'));
    for (auto line : lines) {
        if (line.endsWith(QLatin1Char('\r'))) {
            line.chop(1);
        }
        const auto trimmed = line.trimmed();
        if (trimmed.isEmpty() || trimmed.startsWith(QLatin1Char('#'))) {
            continue;
        }
        if (trimmed.startsWith(QLatin1Char('[')) && trimmed.endsWith(QLatin1Char(']'))) {
            const auto header = trimmed.mid(1, trimmed.size() - 2).trimmed();
            const auto dot = header.indexOf(QLatin1Char('.'));
            if (dot > 0 && dot + 1 < header.size()) {
                table = device_table(settings, header.left(dot));
                entryId = header.mid(dot + 1).trimmed();
                if (table != nullptr && !entryId.isEmpty()) {
                    const auto& created = (*table)[entryId];
                    (void)created;
                }
            } else {
                table = device_table(settings, header);
                entryId.clear();
            }
            continue;
        }
        const auto eq = trimmed.indexOf(QLatin1Char('='));
        if (eq <= 0) {
            continue;
        }
        const auto key = trimmed.left(eq).trimmed();
        const auto raw = trimmed.mid(eq + 1).trimmed();
        if (table == nullptr && key == QLatin1String("title")) {
            const auto title = unquote(raw);
            if (!title.isNull()) {
                settings.title = title;
            }
            continue;
        }
        if (table == nullptr) {
            continue;
        }
        if (!entryId.isEmpty()) {
            auto& entry = table->operator[](entryId);
            if (key == QLatin1String("enabled") && (is_true(raw) || is_false(raw))) {
                entry.enabled = is_true(raw);
            } else if (key == QLatin1String("name")) {
                const auto name = unquote(raw);
                if (!name.isNull()) {
                    entry.name = name;
                }
            }
            continue;
        }
        if (is_true(raw) || is_false(raw)) {
            table->operator[](key).enabled = is_true(raw);
        }
    }
    return settings;
}

DisplaySettings load_display_settings(const QString& path) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return display_settings_from_toml({});
    }
    return display_settings_from_toml(QString::fromUtf8(file.readAll()));
}

QString display_title_from_toml(const QString& text) {
    return display_settings_from_toml(text).title;
}
