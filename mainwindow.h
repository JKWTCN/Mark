#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QImage>
#include <QMap>
#include <QJsonObject>
#include <QJsonArray>
#include <QListWidgetItem>
#include <QColor>
#include <QSplitter>
#include <QClipboard>
#include <QGuiApplication>

QT_BEGIN_NAMESPACE
namespace Ui {
class MainWindow;
}
QT_END_NAMESPACE

struct DetectionPoint {
    int x;          // 像素坐标 x
    int y;          // 像素坐标 y
    double normX;   // 归一化坐标 x (0.0-1.0)
    double normY;   // 归一化坐标 y (0.0-1.0)
    int r, g, b;    // 颜色值

    // 默认构造函数
    DetectionPoint() : x(0), y(0), normX(0.0), normY(0.0), r(0), g(0), b(0) {}

    // 带参数的构造函数（像素坐标）
    DetectionPoint(int x, int y, int r, int g, int b, double normX = 0.0, double normY = 0.0)
        : x(x), y(y), normX(normX), normY(normY), r(r), g(g), b(b) {}

    // toJson: 保存所有坐标信息
    QJsonArray toJson() const {
        QJsonArray arr;
        arr.append(x);
        arr.append(y);
        arr.append(r);
        arr.append(g);
        arr.append(b);
        arr.append(normX);  // 新增
        arr.append(normY);  // 新增
        return arr;
    }

    // fromJson: 加载所有坐标信息（向后兼容旧格式）
    static DetectionPoint fromJson(const QJsonArray& arr) {
        // 旧格式: [x, y, r, g, b]
        // 新格式: [x, y, r, g, b, normX, normY]
        double normX = (arr.size() > 5) ? arr[5].toDouble() : 0.0;
        double normY = (arr.size() > 6) ? arr[6].toDouble() : 0.0;
        return DetectionPoint(
            arr[0].toInt(),
            arr[1].toInt(),
            arr[2].toInt(),
            arr[3].toInt(),
            arr[4].toInt(),
            normX,
            normY
        );
    }
};

enum class ColorFormat {
    RGB,
    HEX,
    HSL,
    HSV,
    CMYK
};

enum class CoordinateFormat {
    Pixel,      // 像素坐标 (0 到 width-1, height-1)
    Normalized  // 归一化坐标 (0.0 到 1.0)
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
    void keyPressEvent(QKeyEvent *event) override;

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
    void onCopyPointClicked();
    void loadFolder();
    void previousImage();
    void nextImage();

private:
    Ui::MainWindow *ui;
    QImage currentImage;
    QList<DetectionPoint> detectionPoints;
    double currentZoom = 1.0;

    // Image file information
    QString currentImageFileName;
    qint64 currentImageFileSize = 0;

    // Image list management
    QStringList imageFileList;      // 所有图片文件路径列表
    int currentImageIndex = -1;     // 当前图片索引（-1表示单张图片模式）
    QString currentFolderPath;      // 当前打开的文件夹路径

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
    void updateImageInfoDisplay();
    QString formatFileSize(qint64 bytes);

    // Color format conversion functions
    QString formatColorToString(int r, int g, int b, ColorFormat format);
    QString rgbToHex(int r, int g, int b);
    QString rgbToHsl(int r, int g, int b);
    QString rgbToHsv(int r, int g, int b);
    QString rgbToCmyk(int r, int g, int b);
    ColorFormat getCurrentColorFormat() const;

    // User preference
    int lastUsedColorFormat = 0;  // 0 = RGB (default)
    int lastUsedCoordinateFormat = 0;  // 0 = Pixel (默认)

    // Coordinate format state tracking
    int currentCoordinateFormat = 0;  // Track the current coordinate format display

    // Display settings - decimal places
    int normalizedDecimals = 6;
    int colorDecimals = 0;
    int fileSizeDecimals = 2;

    // Coordinate conversion functions
    QString pixelToNormalizedString(int pixel, int maxValue);
    int normalizedStringToPixel(const QString& normalizedStr, int maxValue);
    QString formatCoordinates(int x, int y, CoordinateFormat format);
    CoordinateFormat getCurrentCoordinateFormat() const;
    void convertCoordinateFormat(CoordinateFormat newFormat);

    // Image navigation helper functions
    void loadImageAtIndex(int index);
    void updateNavigationButtons();
    void updateImageCounterDisplay();
};
#endif // MAINWINDOW_H
