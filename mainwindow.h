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
#include <QVector>

class MinimapWidget;

QT_BEGIN_NAMESPACE
namespace Ui {
class MainWindow;
}
QT_END_NAMESPACE

// Forward declarations
class QPropertyAnimation;
class QShortcut;
class QPoint;
class QProgressDialog;

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

    // 主坐标类型枚举
    enum PrimaryCoordinate {
        PrimaryPixel,      // 像素坐标为主
        PrimaryNormalized  // 归一化坐标为主
    };
    PrimaryCoordinate primaryCoord;  // 主坐标类型

    // 默认构造函数
    DetectionPoint() : x(0), y(0), r(0), g(0), b(0), normX(0.0), normY(0.0), hasNormalized(false), primaryCoord(PrimaryPixel) {}

    // 带参数的构造函数（像素坐标）
    DetectionPoint(int x, int y, int r, int g, int b)
        : x(x), y(y), r(r), g(g), b(b), normX(0.0), normY(0.0), hasNormalized(false), primaryCoord(PrimaryPixel) {}

    // 带参数的构造函数（像素坐标 + 归一化坐标）
    DetectionPoint(int x, int y, int r, int g, int b, double normX, double normY)
        : x(x), y(y), r(r), g(g), b(b), normX(normX), normY(normY), hasNormalized(true), primaryCoord(PrimaryPixel) {}

    // toJson: 根据格式设置导出为对应格式
    QJsonArray toJson(CoordinateFormat xyFormat, ColorFormat colorFormat,
                     int imgWidth, int imgHeight, int normDecimals, int colorDecimals) const;

    // fromJson: 从任意格式解析回内部表示（像素坐标 + RGB）
    static DetectionPoint fromJson(const QJsonArray& arr,
                                   CoordinateFormat xyFormat, ColorFormat colorFormat,
                                   int imgWidth, int imgHeight);

    // fromJsonLegacy: 向后兼容旧格式（没有格式信息的 JSON）
    static DetectionPoint fromJsonLegacy(const QJsonArray& arr);

    // 辅助方法
    void ensurePixelCoords(const QSize& imgSize);
    void ensureNormalizedCoords(const QSize& imgSize);
    bool isPixelPrimary() const { return primaryCoord == PrimaryPixel; }
    bool isNormalizedPrimary() const { return primaryCoord == PrimaryNormalized; }
};

class MainWindow : public QMainWindow
{
    Q_OBJECT

    // Property for smooth zoom animation
    Q_PROPERTY(double currentZoom READ currentZoom WRITE setCurrentZoom NOTIFY currentZoomChanged)

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

    // Getter for currentZoom property (used by QPropertyAnimation)
    double currentZoom() const { return m_currentZoom; }

protected:
    bool eventFilter(QObject *obj, QEvent *event) override;
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
    void saveAsConfig();
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
    void onCopyCoordClicked();
    void onCopyColorClicked();
    void loadFolder();
    void previousImage();
    void nextImage();
    void onOpenInExplorerClicked();
    void onSelectInExplorerClicked();
    void onCopyImageClicked();
    void onCopyImageFileClicked();
    void onRenameImageFileClicked();
    void openImageGroupsDialog();

signals:
    void currentZoomChanged(double newZoom);

private:
    Ui::MainWindow *ui;
    QImage currentImage;
    QList<DetectionPoint> detectionPoints;
    double m_currentZoom = 1.0;

    static constexpr double ZOOM_MIN = 0.1;
    static constexpr double ZOOM_MAX = 32.0;
    static constexpr double ZOOM_PIXEL_PERFECT_THRESHOLD = 2.0;

    double calculateZoomStep(double currentZoom) const;
    QString formatZoomLabel(double zoom) const;

    // Image file information
    QString currentImageFileName;
    qint64 currentImageFileSize = 0;

    // Image list management
    QStringList imageFileList;      // 所有图片文件路径列表
    int currentImageIndex = -1;     // 当前图片索引（-1表示单张图片模式）
    QString currentFolderPath;      // 当前打开的文件夹路径

    struct ImageGroupColorRange {
        int pointIndex = -1;
        DetectionPoint point;
        double normX = 0.0;
        double normY = 0.0;
        int sampleCount = 0;
        QStringList componentNames;
        QList<double> minValues;
        QList<double> maxValues;
        QList<double> avgValues;
    };

    QVector<QStringList> imageGroups;
    QStringList imageGroupNames;

    // Drag functionality
    bool isDragging = false;
    QPoint dragStartPos;
    QPoint scrollStartPos;

    // Selected point index in the list (-1 means no selection)
    int selectedPointIndex = -1;
    QString pointSearchText;
    bool isPickingPointCoordinate = false;
    int pickingPointIndex = -1;

    // Current config file path (empty if not loaded/saved yet)
    QString currentConfigFilePath;

    void setupConnections();
    void updatePointsList();
    void applyPointSearchFilter();
    QString pointSearchHaystack(int pointIndex);
    void focusDetectionPoint(int pointIndex, bool centerImage);
    bool imagePointFromGlobalPosition(const QPoint& globalPos, QPoint* imagePos) const;
    void addDetectionPoint(const QPoint& pos);
    void startPointCoordinatePick(int pointIndex);
    void confirmPointCoordinatePick(const QPoint& pos);
    void moveDetectionPointTo(int pointIndex, const QPoint& pos, CoordinateFormat coordFormat);
    QColor getPixelColor(const QPoint& pos) const;
    void drawDetectionPoints();
    bool loadImageFile(const QString& filePath, bool keepFolderNavigation = false);
    bool loadImageFolder(const QString& folderPath);
    bool loadJsonConfig(const QString& filePath);
    bool saveJsonConfig(const QString& filePath);
    void updateImageDisplay();
    QSize fitToScreenAvailableSize() const;
    void setZoom(double zoom);
    void updatePointsRGBFromImage();
    void updateImageInfoDisplay();
    void focusImageFileNameEditor();
    void commitImageFileRenameFromEditors();
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
    int pointsListVisibleRows = 5;  // 检测点列表可见行数

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

    // Smooth zoom animation
    QPropertyAnimation* zoomAnimation = nullptr;
    QShortcut* prevImageShortcut = nullptr;
    QShortcut* nextImageShortcut = nullptr;
    QShortcut* saveShortcut = nullptr;
    QShortcut* saveAsShortcut = nullptr;
    QShortcut* loadImageShortcut = nullptr;
    QShortcut* loadFolderShortcut = nullptr;
    QShortcut* loadConfigShortcut = nullptr;
    QShortcut* zoomInShortcut = nullptr;
    QShortcut* zoomOutShortcut = nullptr;
    QShortcut* fitToScreenShortcut = nullptr;
    QShortcut* actualSizeShortcut = nullptr;
    QShortcut* deletePointShortcut = nullptr;
    QShortcut* renameImageShortcut = nullptr;
    QShortcut* pointSearchShortcut = nullptr;
    QVector<QShortcut*> imageGroupShortcuts;
    void setCurrentZoom(double zoom);
    void animatedZoomTo(double targetZoom, const QPoint& centerPos = QPoint());
    void setupZoomAnimation();
    void updatePointsListHeight();

    QString defaultImageGroupName(int groupIndex) const;
    void addCurrentImageToGroup(int groupIndex);
    QList<ImageGroupColorRange> calculateImageGroupColorRanges(
        const QStringList& files,
        ColorFormat colorFormat,
        QStringList* failedFiles,
        QProgressDialog* progress = nullptr) const;
    bool exportImageGroupCsv(int groupIndex, const QString& filePath, QString* errorMessage) const;

    // Minimap
    MinimapWidget* minimapWidget = nullptr;
    void updateMinimap();
};
#endif // MAINWINDOW_H
