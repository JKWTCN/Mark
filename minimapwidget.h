#ifndef MINIMAPWIDGET_H
#define MINIMAPWIDGET_H

#include <QWidget>
#include <QImage>

class QScrollArea;
class QResizeEvent;
class QMouseEvent;
class QPaintEvent;

class MinimapWidget : public QWidget
{
    Q_OBJECT

public:
    explicit MinimapWidget(QScrollArea* scrollArea, QWidget* parent = nullptr);

    void setImage(const QImage& image);
    void setZoom(double zoom);
    void updateViewportRect();
    void reposition();

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;

private:
    void navigateToPoint(const QPoint& pos);
    QRectF viewportRectOnMinimap() const;

    QScrollArea* m_scrollArea;
    QImage m_thumbnail;
    QImage m_fullImage;
    double m_zoom = 1.0;
    bool m_dragging = false;

    static constexpr int MINIMAP_MAX_WIDTH = 200;
    static constexpr int MINIMAP_MAX_HEIGHT = 150;
    static constexpr int MARGIN = 10;
};

#endif // MINIMAPWIDGET_H
