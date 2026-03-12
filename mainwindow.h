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

struct DetectionPoint {
    int x;          // 像素坐标 x
    int y;          // 像素坐标 y
    int r, g, b;    // RGB 颜色值（内部统一使用 RGB）
    double normX;   // 归一化坐标 x (0.0-1.0)
    double normY;   // 归一化坐标 y (0.0-1.0)
    bool hasNormalized; // 标记是否包含归一化坐标

    // 默认构造函数
    DetectionPoint() : x(0), y(0), r(0), g(0), b(0), normX(0.0), normY(0.0), hasNormalized(false) {}

    // 带参数的构造函数（像素坐标）
    DetectionPoint(int x, int y, int r, int g, int b)
        : x(x), y(y), r(r), g(g), b(b), normX(0.0), normY(0.0), hasNormalized(false) {}

    // 带参数的构造函数（像素坐标 + 归一化坐标）
    DetectionPoint(int x, int y, int r, int g, int b, double normX, double normY)
        : x(x), y(y), r(r), g(g), b(b), normX(normX), normY(normY), hasNormalized(true) {}

    // toJson: 根据格式设置导出为对应格式
    QJsonArray toJson(CoordinateFormat xyFormat, ColorFormat colorFormat,
                     int imgWidth, int imgHeight, int normDecimals, int colorDecimals) const;

    // fromJson: 从任意格式解析回内部表示（像素坐标 + RGB）
    static DetectionPoint fromJson(const QJsonArray& arr,
                                   CoordinateFormat xyFormat, ColorFormat colorFormat,
                                   int imgWidth, int imgHeight);

    // fromJsonLegacy: 向后兼容旧格式（没有格式信息的 JSON）
    static DetectionPoint fromJsonLegacy(const QJsonArray& arr);
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

    // New color conversion helpers for JSON import/export (return numeric values)
    QList<double> rgbToHsvValues(int r, int g, int b) const;
    QList<double> rgbToHslValues(int r, int g, int b) const;
    QList<double> rgbToCmykValues(int r, int g, int b) const;
    QList<int> hsvToRgb(double h, double s, double v) const;
    QList<int> hslToRgb(double h, double s, double l) const;
    QList<int> cmykToRgb(double c, double m, double y, double k) const;
    QList<int> hexToRgb(const QString& hex) const;

    // Format string conversion
    QString colorFormatToString(ColorFormat format) const;
    ColorFormat stringToColorFormat(const QString& str) const;

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
