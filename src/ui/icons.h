#pragma once
#include <QColor>
#include <QIcon>
namespace aeris {
enum class Glyph { Search, List, Compact, Cards, Palette };
QIcon actionIcon(Glyph glyph, QColor color);
} // namespace aeris
