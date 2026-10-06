#include "icons.h"
#include <QPainter>
namespace aeris {
static void drawGlyph(QPainter &p, Glyph glyph) {
    if (glyph == Glyph::Search) {
        p.drawEllipse(QRectF(3, 3, 11, 11));
        p.drawLine(QPointF(12, 12), QPointF(18, 18));
    } else if (glyph == Glyph::Cards) {
        for (int y : {3, 12})
            for (int x : {3, 12})
                p.drawRoundedRect(QRectF(x, y, 6, 6), 1, 1);
    } else if (glyph == Glyph::Palette) {
        p.drawEllipse(QRectF(2, 2, 16, 16));
        p.setBrush(p.pen().color());
        p.drawEllipse(QPointF(7, 7), 1, 1);
        p.drawEllipse(QPointF(13, 7), 1, 1);
        p.drawEllipse(QPointF(7, 13), 1, 1);
    } else {
        const int step = glyph == Glyph::Compact ? 4 : 7;
        for (int y = 4; y < 18; y += step) {
            p.drawLine(3, y, 5, y);
            p.drawLine(9, y, 18, y);
        }
    }
}
QIcon actionIcon(Glyph glyph, QColor color) {
    QIcon icon;
    for (int size : {20, 40, 60}) {
        QPixmap pixmap(size, size);
        pixmap.fill(Qt::transparent);
        QPainter p(&pixmap);
        p.setRenderHint(QPainter::Antialiasing);
        p.scale(size / 20.0, size / 20.0);
        p.setPen(QPen(color, 1.5, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        drawGlyph(p, glyph);
        p.end();
        icon.addPixmap(pixmap);
    }
    return icon;
}
} // namespace aeris
