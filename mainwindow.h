#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QImage>
#include <QMap>
#include <QJsonObject>
#include <QJsonArray>
#include <QListWidgetItem>

QT_BEGIN_NAMESPACE
namespace Ui {
class MainWindow;
}
QT_END_NAMESPACE

struct DetectionPoint {
    int x;
    int y;
    int r;
    int g;
    int b;

    DetectionPoint() : x(0), y(0), r(0), g(0), b(0) {}
    DetectionPoint(int x, int y, int r, int g, int b) : x(x), y(y), r(r), g(g), b(b) {}

    QJsonArray toJson() const {
        QJsonArray arr;
        arr.append(x);
        arr.append(y);
        arr.append(r);
        arr.append(g);
        arr.append(b);
        return arr;
    }

    static DetectionPoint fromJson(const QJsonArray& arr) {
        return DetectionPoint(
            arr[0].toInt(),
            arr[1].toInt(),
            arr[2].toInt(),
            arr[3].toInt(),
            arr[4].toInt()
        );
    }
};

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

protected:
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;
    void dragEnterEvent(QDragEnterEvent *event) override;
    void dropEvent(QDropEvent *event) override;

private slots:
    void loadImage();
    void loadConfig();
    void saveConfig();
    void clearAllPoints();
    void onPointsListItemDoubleClicked(QListWidgetItem *item);
    void onPointsListSelectionChanged();
    void zoomIn();
    void zoomOut();
    void zoomChanged(int value);
    void fitToScreen();
    void actualSize();
    void onAddPointManuallyClicked();
    void onDeletePointClicked();
    void onEditPointClicked();

private:
    Ui::MainWindow *ui;
    QImage currentImage;
    QList<DetectionPoint> detectionPoints;
    double currentZoom = 1.0;

    // Drag functionality
    bool isDragging = false;
    QPoint dragStartPos;
    QPoint scrollStartPos;

    // Selected point index in the list (-1 means no selection)
    int selectedPointIndex = -1;

    void setupConnections();
    void updatePointsList();
    void addDetectionPoint(const QPoint& pos);
    QColor getPixelColor(const QPoint& pos) const;
    void drawDetectionPoints();
    bool loadJsonConfig(const QString& filePath);
    bool saveJsonConfig(const QString& filePath);
    void updateImageDisplay();
    void setZoom(double zoom);
    void updatePointsRGBFromImage();
};
#endif // MAINWINDOW_H
