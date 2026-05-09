#include "minimapwidget.h"

#include <QScrollArea>
#include <QScrollBar>
#include <QPainter>
#include <QPaintEvent>
#include <QMouseEvent>

MinimapWidget::MinimapWidget(QScrollArea* scrollArea, QWidget* parent)
    : QWidget(parent)
    , m_scrollArea(scrollArea)
{
    setAttribute(Qt::WA_TransparentForMouseEvents, false);
    setCursor(Qt::CrossCursor);
    hide();
}

void MinimapWidget::setImage(const QImage& image)
{
    if (image.isNull()) {
        m_fullImage = QImage();
        m_thumbnail = QImage();
        hide();
        return;
    }

    m_fullImage = image;

    // Calculate minimap size preserving aspect ratio
    double aspect = static_cast<double>(image.width()) / image.height();
    int w, h;
    if (aspect >= 1.0) {
        w = MINIMAP_MAX_WIDTH;
        h = static_cast<int>(w / aspect);
    } else {
        h = MINIMAP_MAX_HEIGHT;
        w = static_cast<int>(h * aspect);
    }

    m_thumbnail = image.scaled(w, h, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    setFixedSize(m_thumbnail.size());

    show();
    reposition();
    update();
}

void MinimapWidget::setZoom(double zoom)
{
    m_zoom = zoom;
    update();
}

void MinimapWidget::updateViewportRect()
{
    update();
}

void MinimapWidget::reposition()
{
    if (!m_scrollArea) return;

    QWidget* viewport = m_scrollArea->viewport();
    int x = MARGIN;
    int y = viewport->height() - height() - MARGIN;

    // Ensure minimap stays within viewport bounds
    if (y < MARGIN) y = MARGIN;
    if (x + width() > viewport->width() - MARGIN)
        x = viewport->width() - width() - MARGIN;

    move(x, y);
}

void MinimapWidget::paintEvent(QPaintEvent* event)
{
    Q_UNUSED(event);

    if (m_thumbnail.isNull()) return;

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    // Draw border/shadow
    painter.setPen(QPen(QColor(0, 0, 0, 100), 2));
    painter.setBrush(Qt::NoBrush);
    painter.drawRect(0, 0, width() - 1, height() - 1);

    // Draw thumbnail
    painter.drawImage(0, 0, m_thumbnail);

    // Draw viewport rectangle
    QRectF vpRect = viewportRectOnMinimap();
    if (vpRect.isValid()) {
        // Semi-transparent fill
        painter.setPen(QPen(QColor(30, 120, 255), 2));
        painter.setBrush(QColor(30, 120, 255, 40));
        painter.drawRect(vpRect);
    }

    // Outer border
    painter.setPen(QPen(QColor(80, 80, 80, 180), 1));
    painter.setBrush(Qt::NoBrush);
    painter.drawRect(0, 0, width() - 1, height() - 1);
}

QRectF MinimapWidget::viewportRectOnMinimap() const
{
    if (m_fullImage.isNull() || !m_scrollArea) return QRectF();

    QWidget* viewport = m_scrollArea->viewport();
    QScrollBar* hBar = m_scrollArea->horizontalScrollBar();
    QScrollBar* vBar = m_scrollArea->verticalScrollBar();

    int scrollX = hBar->value();
    int scrollY = vBar->value();
    int vpW = viewport->width();
    int vpH = viewport->height();

    // The scaled image size = fullImage * zoom
    double scaledW = m_fullImage.width() * m_zoom;
    double scaledH = m_fullImage.height() * m_zoom;

    // Scale factor from scaled image to minimap
    double scaleX = static_cast<double>(width()) / scaledW;
    double scaleY = static_cast<double>(height()) / scaledH;

    // Viewport rect in minimap coords
    double rx = scrollX * scaleX;
    double ry = scrollY * scaleY;
    double rw = vpW * scaleX;
    double rh = vpH * scaleY;

    return QRectF(rx, ry, rw, rh);
}

void MinimapWidget::mousePressEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton) {
        m_dragging = true;
        navigateToPoint(event->pos());
    }
}

void MinimapWidget::mouseMoveEvent(QMouseEvent* event)
{
    if (m_dragging) {
        navigateToPoint(event->pos());
    }
}

void MinimapWidget::mouseReleaseEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton) {
        m_dragging = false;
    }
}

void MinimapWidget::navigateToPoint(const QPoint& pos)
{
    if (m_fullImage.isNull() || !m_scrollArea) return;

    QWidget* viewport = m_scrollArea->viewport();
    QScrollBar* hBar = m_scrollArea->horizontalScrollBar();
    QScrollBar* vBar = m_scrollArea->verticalScrollBar();

    // Convert minimap click pos to scaled image coords
    double scaledW = m_fullImage.width() * m_zoom;
    double scaledH = m_fullImage.height() * m_zoom;

    double scaleX = static_cast<double>(width()) / scaledW;
    double scaleY = static_cast<double>(height()) / scaledH;

    // Center of click in scaled image coords
    double imgX = pos.x() / scaleX;
    double imgY = pos.y() / scaleY;

    // Scroll so that this point is centered in the viewport
    int targetH = static_cast<int>(imgX - viewport->width() / 2.0);
    int targetV = static_cast<int>(imgY - viewport->height() / 2.0);

    hBar->setValue(qBound(hBar->minimum(), targetH, hBar->maximum()));
    vBar->setValue(qBound(vBar->minimum(), targetV, vBar->maximum()));
}
