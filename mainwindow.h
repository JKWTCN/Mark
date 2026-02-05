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

    QJsonObject toJson() const {
        QJsonObject obj;
        obj["x"] = x;
        obj["y"] = y;
        obj["r"] = r;
        obj["g"] = g;
        obj["b"] = b;
        return obj;
    }

    static DetectionPoint fromJson(const QJsonObject& obj) {
        return DetectionPoint(
            obj["x"].toInt(),
            obj["y"].toInt(),
            obj["r"].toInt(),
            obj["g"].toInt(),
            obj["b"].toInt()
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
    void dragEnterEvent(QDragEnterEvent *event) override;
    void dropEvent(QDropEvent *event) override;

private slots:
    void loadImage();
    void loadConfig();
    void saveConfig();
    void clearCurrentSkillPoints();
    void onSkillChanged(int index);
    void onPointsListItemDoubleClicked(QListWidgetItem *item);
    void zoomIn();
    void zoomOut();
    void zoomChanged(int value);
    void fitToScreen();
    void actualSize();

private:
    Ui::MainWindow *ui;
    QImage currentImage;
    QString currentSkill;
    QMap<QString, QList<DetectionPoint>> skillPoints;
    double currentZoom = 1.0;

    void setupConnections();
    void updatePointsList();
    QString getCurrentSkill() const;
    void addDetectionPoint(const QPoint& pos);
    QColor getPixelColor(const QPoint& pos) const;
    void drawDetectionPoints();
    bool loadJsonConfig(const QString& filePath);
    bool saveJsonConfig(const QString& filePath);
    void updateImageDisplay();
    void setZoom(double zoom);
};
#endif // MAINWINDOW_H
