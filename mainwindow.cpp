#include "mainwindow.h"
#include "./ui_mainwindow.h"
#include "minimapwidget.h"
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QMessageBox>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QKeyEvent>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QMimeData>
#include <QPainter>
#include <QPixmap>
#include <QPalette>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QScrollBar>
#include <QDialog>
#include <QFormLayout>
#include <QDialogButtonBox>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QLabel>
#include <QLineEdit>
#include <QComboBox>
#include <QtMath>
#include <QDir>
#include <algorithm>
#include <QPropertyAnimation>
#include <QEasingCurve>
#include <QProcess>
#include <QDesktopServices>
#include <QUrl>
#include <QShortcut>
#include <QIcon>
#include <QTableWidget>
#include <QHeaderView>
#include <QPushButton>
#include <QProgressDialog>
#include <QLayout>

namespace {
bool isSupportedImageFile(const QFileInfo& fileInfo)
{
    const QString suffix = fileInfo.suffix().toLower();
    return suffix == "png" || suffix == "jpg" || suffix == "jpeg" ||
           suffix == "jfif" || suffix == "bmp" || suffix == "webp";
}
}

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    setAcceptDrops(true);

    // Set window icon with multiple resolutions
    QIcon appIcon;
    appIcon.addFile(":/Mark_16_16.ico", QSize(16, 16));
    appIcon.addFile(":/Mark_32_32.ico", QSize(32, 32));
    appIcon.addFile(":/Mark_48_48.ico", QSize(48, 48));
    appIcon.addFile(":/Mark_64_64.ico", QSize(64, 64));
    appIcon.addFile(":/Mark_128_128.ico", QSize(128, 128));
    appIcon.addFile(":/Mark_256_256.ico", QSize(256, 256));
    setWindowIcon(appIcon);

    // Setup dark mode compatible palette for scrollArea and its contents
    ui->scrollArea->setStyleSheet("");
    ui->scrollAreaContents->setStyleSheet("");
    ui->scrollArea->setAutoFillBackground(false);
    ui->scrollAreaContents->setAutoFillBackground(false);

    // Clear fixed background color from imageLabel to use system theme
    ui->imageLabel->setStyleSheet("");

    setupConnections();

    // Keyboard shortcuts for image navigation (work regardless of focus)
    prevImageShortcut = new QShortcut(Qt::Key_Left, this);
    nextImageShortcut = new QShortcut(Qt::Key_Right, this);
    prevImageShortcut->setEnabled(false);
    nextImageShortcut->setEnabled(false);
    connect(prevImageShortcut, &QShortcut::activated, this, &MainWindow::previousImage);
    connect(nextImageShortcut, &QShortcut::activated, this, &MainWindow::nextImage);

    // Keyboard shortcuts for save/load
    saveShortcut = new QShortcut(QKeySequence::Save, this);
    connect(saveShortcut, &QShortcut::activated, this, &MainWindow::saveConfig);

    saveAsShortcut = new QShortcut(QKeySequence::SaveAs, this);
    connect(saveAsShortcut, &QShortcut::activated, this, &MainWindow::saveAsConfig);

    loadImageShortcut = new QShortcut(QKeySequence::Open, this);
    connect(loadImageShortcut, &QShortcut::activated, this, &MainWindow::loadImage);

    loadFolderShortcut = new QShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_O), this);
    connect(loadFolderShortcut, &QShortcut::activated, this, &MainWindow::loadFolder);

    loadConfigShortcut = new QShortcut(QKeySequence(Qt::CTRL | Qt::Key_L), this);
    connect(loadConfigShortcut, &QShortcut::activated, this, &MainWindow::loadConfig);

    // Keyboard shortcuts for zoom
    zoomInShortcut = new QShortcut(QKeySequence::ZoomIn, this);
    connect(zoomInShortcut, &QShortcut::activated, this, &MainWindow::zoomIn);

    zoomOutShortcut = new QShortcut(QKeySequence::ZoomOut, this);
    connect(zoomOutShortcut, &QShortcut::activated, this, &MainWindow::zoomOut);

    fitToScreenShortcut = new QShortcut(QKeySequence(Qt::CTRL | Qt::Key_0), this);
    connect(fitToScreenShortcut, &QShortcut::activated, this, &MainWindow::fitToScreen);

    actualSizeShortcut = new QShortcut(QKeySequence(Qt::CTRL | Qt::Key_1), this);
    connect(actualSizeShortcut, &QShortcut::activated, this, &MainWindow::actualSize);

    // Keyboard shortcut for delete selected point
    deletePointShortcut = new QShortcut(Qt::Key_Delete, this);
    connect(deletePointShortcut, &QShortcut::activated, this, &MainWindow::onDeletePointClicked);

    renameImageShortcut = new QShortcut(Qt::Key_F2, this);
    renameImageShortcut->setEnabled(false);
    connect(renameImageShortcut, &QShortcut::activated, this, &MainWindow::onRenameImageFileClicked);

    ui->imageLabel->setMouseTracking(true);
    ui->imageLabel->installEventFilter(this);
    ui->scrollArea->installEventFilter(this);  // 为 scrollArea 安装事件过滤器
    ui->scrollArea->viewport()->installEventFilter(this);  // 为 viewport 安装事件过滤器

    // Set initial sizes for the splitter (defined in UI file)
    // control panel gets 300px, rest goes to image
    ui->splitter->setSizes({300, 900});

    // Set stretch factor so image container gets more space when resizing
    ui->splitter->setStretchFactor(1, 1);

    // Initialize image info display
    updateImageInfoDisplay();

    // Initialize display settings
    ui->normalizedDecimalsSpin->setValue(normalizedDecimals);
    ui->colorDecimalsSpin->setValue(colorDecimals);
    ui->fileSizeDecimalsSpin->setValue(fileSizeDecimals);
    ui->pointsListVisibleRowsSpin->setValue(pointsListVisibleRows);

    // Initialize coordinate format state
    currentCoordinateFormat = 0;  // Start with pixel format

    // Setup smooth zoom animation
    setupZoomAnimation();

    // Setup minimap
    minimapWidget = new MinimapWidget(ui->scrollArea, ui->scrollArea->viewport());
    connect(ui->scrollArea->horizontalScrollBar(), &QScrollBar::valueChanged, this, [this]() {
        minimapWidget->updateViewportRect();
    });
    connect(ui->scrollArea->verticalScrollBar(), &QScrollBar::valueChanged, this, [this]() {
        minimapWidget->updateViewportRect();
    });
    connect(ui->showMinimapCheckBox, &QCheckBox::toggled, this, [this](bool checked) {
        if (minimapWidget) {
            minimapWidget->setVisible(checked && !currentImage.isNull());
        }
    });

    // Connect display settings signals
    connect(ui->normalizedDecimalsSpin, QOverload<int>::of(&QSpinBox::valueChanged), this, [this](int value) {
        normalizedDecimals = value;
        updatePointsList();
    });
    connect(ui->colorDecimalsSpin, QOverload<int>::of(&QSpinBox::valueChanged), this, [this](int value) {
        colorDecimals = value;
        updatePointsList();
    });
    connect(ui->fileSizeDecimalsSpin, QOverload<int>::of(&QSpinBox::valueChanged), this, [this](int value) {
        fileSizeDecimals = value;
        updateImageInfoDisplay();
    });
    connect(ui->pointsListVisibleRowsSpin, QOverload<int>::of(&QSpinBox::valueChanged), this, [this](int value) {
        pointsListVisibleRows = value;
        updatePointsListHeight();
    });
}

MainWindow::~MainWindow()
{
    if (zoomAnimation) {
        delete zoomAnimation;
        zoomAnimation = nullptr;
    }
    delete ui;
}

bool MainWindow::eventFilter(QObject *obj, QEvent *event)
{
    // Reposition minimap when viewport resizes
    if (obj == ui->scrollArea->viewport() && event->type() == QEvent::Resize) {
        if (minimapWidget) {
            minimapWidget->reposition();
        }
    }

    // 拦截 scrollArea 或 viewport 的滚轮事件
    if ((obj == ui->scrollArea || obj == ui->scrollArea->viewport()) && event->type() == QEvent::Wheel) {
        QWheelEvent *wheelEvent = static_cast<QWheelEvent*>(event);

        // 检查是否有图片加载
        if (currentImage.isNull()) {
            return false;  // 让默认处理
        }

        // 获取滚轮滚动的角度
        QPoint angleDelta = wheelEvent->angleDelta();
        int delta = angleDelta.y();

        // 如果没有滚动，使用默认行为
        if (delta == 0) {
            return false;
        }

        // 只有按住 Ctrl 键时才响应缩放
        if (wheelEvent->modifiers() & Qt::ControlModifier) {
            // 阻止事件继续传播，避免触发滚动条
            wheelEvent->accept();

            double zoomStep = calculateZoomStep(m_currentZoom);

            // 根据滚动方向确定是放大还是缩小
            double newZoom;
            if (delta > 0) {
                newZoom = m_currentZoom + zoomStep;
            } else {
                newZoom = m_currentZoom - zoomStep;
            }

            // 限制缩放范围
            newZoom = qBound(ZOOM_MIN, newZoom, ZOOM_MAX);

            // 如果缩放比例没有变化，直接返回
            if (qFuzzyCompare(newZoom, m_currentZoom)) {
                return true;
            }

            // 以画面中心为缩放中心
            animatedZoomTo(newZoom);
            return true;
        }

        // 没有按 Ctrl 键，让默认处理（滚动条工作）
        return false;
    }

    // 高倍率时自定义绘制 imageLabel，只渲染可见区域
    if (obj == ui->imageLabel && event->type() == QEvent::Paint) {
        if (m_currentZoom > ZOOM_PIXEL_PERFECT_THRESHOLD && !currentImage.isNull()) {
            QPaintEvent* pe = static_cast<QPaintEvent*>(event);
            QPainter painter(ui->imageLabel);
            painter.setClipRect(pe->rect());

            QScrollBar* hBar = ui->scrollArea->horizontalScrollBar();
            QScrollBar* vBar = ui->scrollArea->verticalScrollBar();

            int srcX = static_cast<int>(hBar->value() / m_currentZoom);
            int srcY = static_cast<int>(vBar->value() / m_currentZoom);
            int srcW = static_cast<int>(ui->scrollArea->viewport()->width() / m_currentZoom) + 2;
            int srcH = static_cast<int>(ui->scrollArea->viewport()->height() / m_currentZoom) + 2;

            srcX = qBound(0, srcX, currentImage.width() - 1);
            srcY = qBound(0, srcY, currentImage.height() - 1);
            srcW = qMin(srcW, currentImage.width() - srcX);
            srcH = qMin(srcH, currentImage.height() - srcY);

            if (srcW > 0 && srcH > 0) {
                // 先缩放图像（不带检测点）
                QImage visibleRegion = currentImage.copy(srcX, srcY, srcW, srcH);
                QSize visScaledSize(static_cast<int>(srcW * m_currentZoom),
                                     static_cast<int>(srcH * m_currentZoom));
                QPixmap scaledPixmap = QPixmap::fromImage(visibleRegion).scaled(
                    visScaledSize, Qt::KeepAspectRatio, Qt::FastTransformation);

                int destX = static_cast<int>(srcX * m_currentZoom);
                int destY = static_cast<int>(srcY * m_currentZoom);
                painter.drawPixmap(destX, destY, scaledPixmap);

                // 在缩放后的图像上绘制检测点十字准星
                int pixelSize = static_cast<int>(m_currentZoom);
                int crossLen = qMax(pixelSize, 12);
                for (int i = 0; i < detectionPoints.size(); ++i) {
                    const auto& point = detectionPoints[i];
                    int sx = static_cast<int>(point.x * m_currentZoom);
                    int sy = static_cast<int>(point.y * m_currentZoom);

                    // 跳过不在可见区域的点
                    if (sx + crossLen < hBar->value() || sx - crossLen > hBar->value() + ui->scrollArea->viewport()->width())
                        continue;
                    if (sy + crossLen < vBar->value() || sy - crossLen > vBar->value() + ui->scrollArea->viewport()->height())
                        continue;

                    QColor color = (i == selectedPointIndex) ? Qt::yellow : Qt::red;
                    painter.setPen(QPen(color, 2));
                    // 十字线
                    painter.drawLine(sx - crossLen, sy, sx - 2, sy);
                    painter.drawLine(sx + 2, sy, sx + crossLen, sy);
                    painter.drawLine(sx, sy - crossLen, sx, sy - 2);
                    painter.drawLine(sx, sy + 2, sx, sy + crossLen);
                    // 目标像素高亮边框
                    painter.setPen(QPen(color, 1));
                    painter.drawRect(sx, sy, pixelSize - 1, pixelSize - 1);
                }
            }
            return true;
        }
    }

    // 其他事件，让默认处理
    return QMainWindow::eventFilter(obj, event);
}

QString MainWindow::rgbToHex(int r, int g, int b)
{
    return QString("#%1%2%3")
        .arg(r, 2, 16, QChar('0'))
        .arg(g, 2, 16, QChar('0'))
        .arg(b, 2, 16, QChar('0'))
        .toUpper();
}

QString MainWindow::rgbToHsl(int r, int g, int b)
{
    QColor color(r, g, b);
    int h, s, l;
    color.getHsl(&h, &s, &l);

    double sPercent = s / 2.55;
    double lPercent = l / 2.55;

    if (colorDecimals == 0) {
        return QString("HSL(%1°, %2%, %3%)")
            .arg(h < 0 ? 0 : h)
            .arg(qRound(sPercent))
            .arg(qRound(lPercent));
    } else {
        return QString("HSL(%1°, %2%, %3%)")
            .arg(h < 0 ? 0 : h)
            .arg(sPercent, 0, 'f', colorDecimals)
            .arg(lPercent, 0, 'f', colorDecimals);
    }
}

QString MainWindow::rgbToHsv(int r, int g, int b)
{
    QColor color(r, g, b);
    int h, s, v;
    color.getHsv(&h, &s, &v);

    double sPercent = s / 2.55;
    double vPercent = v / 2.55;

    if (colorDecimals == 0) {
        return QString("HSV(%1°, %2%, %3%)")
            .arg(h < 0 ? 0 : h)
            .arg(qRound(sPercent))
            .arg(qRound(vPercent));
    } else {
        return QString("HSV(%1°, %2%, %3%)")
            .arg(h < 0 ? 0 : h)
            .arg(sPercent, 0, 'f', colorDecimals)
            .arg(vPercent, 0, 'f', colorDecimals);
    }
}

QString MainWindow::rgbToCmyk(int r, int g, int b)
{
    QColor color(r, g, b);
    int c, m, y, k;
    color.getCmyk(&c, &m, &y, &k);

    return QString("CMYK(%1%, %2%, %3%, %4%)")
        .arg(c)
        .arg(m)
        .arg(y)
        .arg(k);
}

QString MainWindow::pixelToNormalizedString(int pixel, int maxValue)
{
    if (maxValue <= 0) return "0.000000";
    double normalized = static_cast<double>(pixel) / maxValue;
    return QString::number(normalized, 'f', normalizedDecimals);
}

int MainWindow::normalizedStringToPixel(const QString& normalizedStr, int maxValue)
{
    bool ok;
    double normalized = normalizedStr.toDouble(&ok);
    if (!ok || maxValue <= 0) return 0;
    normalized = qBound(0.0, normalized, 1.0);
    return qRound(normalized * maxValue);
}

QString MainWindow::formatCoordinates(int x, int y, CoordinateFormat format)
{
    if (format == CoordinateFormat::Normalized) {
        if (currentImage.isNull()) {
            return QString("(?, ?)");
        }
        QString normX = pixelToNormalizedString(x, currentImage.width() - 1);
        QString normY = pixelToNormalizedString(y, currentImage.height() - 1);
        return QString("(%1, %2)").arg(normX, normY);
    } else {
        return QString("(%1, %2)").arg(x).arg(y);
    }
}

CoordinateFormat MainWindow::getCurrentCoordinateFormat() const
{
    int index = ui->coordinateFormatCombo->currentIndex();
    return static_cast<CoordinateFormat>(index);
}

void MainWindow::convertCoordinateFormat(CoordinateFormat newFormat)
{
    // 只更新当前坐标格式状态，不再需要转换存储的坐标值
    // 因为现在同时存储归一化坐标和像素坐标
    currentCoordinateFormat = static_cast<int>(newFormat);
}

QString MainWindow::formatColorToString(int r, int g, int b, ColorFormat format)
{
    switch (format) {
        case ColorFormat::RGB:
            return QString("RGB(%1, %2, %3)").arg(r).arg(g).arg(b);
        case ColorFormat::HEX:
            return rgbToHex(r, g, b);
        case ColorFormat::HSL:
            return rgbToHsl(r, g, b);
        case ColorFormat::HSV:
            return rgbToHsv(r, g, b);
        case ColorFormat::CMYK:
            return rgbToCmyk(r, g, b);
        default:
            return QString("RGB(%1, %2, %3)").arg(r).arg(g).arg(b);
    }
}

ColorFormat MainWindow::getCurrentColorFormat() const
{
    int index = ui->colorFormatCombo->currentIndex();
    return static_cast<ColorFormat>(index);
}

// New color conversion helpers for JSON import/export (return numeric values)

QList<double> MainWindow::rgbToHsvValues(int r, int g, int b) const
{
    QColor color(r, g, b);
    int h, s, v;
    color.getHsv(&h, &s, &v);

    double hDeg = (h < 0) ? 0.0 : static_cast<double>(h);
    double sPercent = s / 2.55;
    double vPercent = v / 2.55;

    return {hDeg, sPercent, vPercent};
}

QList<double> MainWindow::rgbToHslValues(int r, int g, int b) const
{
    QColor color(r, g, b);
    int h, s, l;
    color.getHsl(&h, &s, &l);

    double hDeg = (h < 0) ? 0.0 : static_cast<double>(h);
    double sPercent = s / 2.55;
    double lPercent = l / 2.55;

    return {hDeg, sPercent, lPercent};
}

QList<double> MainWindow::rgbToCmykValues(int r, int g, int b) const
{
    QColor color(r, g, b);
    int c, m, y, k;
    color.getCmyk(&c, &m, &y, &k);

    return {static_cast<double>(c), static_cast<double>(m),
            static_cast<double>(y), static_cast<double>(k)};
}

QList<int> MainWindow::hsvToRgb(double h, double s, double v) const
{
    QColor color;
    color.setHsv(static_cast<int>(h),
                 static_cast<int>(s * 2.55),
                 static_cast<int>(v * 2.55));
    return {color.red(), color.green(), color.blue()};
}

QList<int> MainWindow::hslToRgb(double h, double s, double l) const
{
    QColor color;
    color.setHsl(static_cast<int>(h),
                 static_cast<int>(s * 2.55),
                 static_cast<int>(l * 2.55));
    return {color.red(), color.green(), color.blue()};
}

QList<int> MainWindow::cmykToRgb(double c, double m, double y, double k) const
{
    QColor color;
    color.setCmyk(static_cast<int>(c),
                  static_cast<int>(m),
                  static_cast<int>(y),
                  static_cast<int>(k));
    return {color.red(), color.green(), color.blue()};
}

QList<int> MainWindow::hexToRgb(const QString& hex) const
{
    QString h = hex;
    if (h.startsWith('#')) {
        h = h.mid(1);
    }

    bool ok;
    int r = h.mid(0, 2).toInt(&ok, 16);
    int g = h.mid(2, 2).toInt(&ok, 16);
    int b = h.mid(4, 2).toInt(&ok, 16);

    return {r, g, b};
}

QString MainWindow::colorFormatToString(ColorFormat format) const
{
    switch (format) {
        case ColorFormat::RGB: return "rgb";
        case ColorFormat::HEX: return "hex";
        case ColorFormat::HSL: return "hsl";
        case ColorFormat::HSV: return "hsv";
        case ColorFormat::CMYK: return "cmyk";
        default: return "rgb";
    }
}

ColorFormat MainWindow::stringToColorFormat(const QString& str) const
{
    QString lower = str.toLower();
    if (lower == "hex") return ColorFormat::HEX;
    if (lower == "hsl") return ColorFormat::HSL;
    if (lower == "hsv") return ColorFormat::HSV;
    if (lower == "cmyk") return ColorFormat::CMYK;
    return ColorFormat::RGB;
}

// DetectionPoint::toJson - 根据格式设置导出为对应格式
QJsonArray DetectionPoint::toJson(CoordinateFormat xyFormat, ColorFormat colorFormat,
                                  int imgWidth, int imgHeight,
                                  int normDecimals, int colorDecimals) const
{
    QJsonArray arr;

    // 根据坐标格式添加坐标
    if (xyFormat == CoordinateFormat::Normalized) {
        // 保存归一化坐标（即使主坐标是像素，也动态计算）
        double normX = isNormalizedPrimary() ? this->normX :
                       double(x) / qMax(1, imgWidth - 1);
        double normY = isNormalizedPrimary() ? this->normY :
                       double(y) / qMax(1, imgHeight - 1);

        arr.append(QString::number(normX, 'f', normDecimals).toDouble());
        arr.append(QString::number(normY, 'f', normDecimals).toDouble());
    } else {
        // 保存像素坐标（即使主坐标是归一化，也使用缓存的像素坐标）
        arr.append(x);
        arr.append(y);
    }

    // 根据颜色格式添加颜色
    switch (colorFormat) {
        case ColorFormat::RGB:
            arr.append(r);
            arr.append(g);
            arr.append(b);
            break;

        case ColorFormat::HEX: {
            QString hex = QString("#%1%2%3")
                .arg(r, 2, 16, QChar('0'))
                .arg(g, 2, 16, QChar('0'))
                .arg(b, 2, 16, QChar('0'));
            arr.append(hex);
            break;
        }

        case ColorFormat::HSV: {
            // 使用 MainWindow 的 rgbToHsvValues 方法
            // 需要通过静态获取或作为参数传入，这里手动实现转换
            QColor color(r, g, b);
            int h, s, v;
            color.getHsv(&h, &s, &v);
            double hDeg = (h < 0) ? 0.0 : static_cast<double>(h);
            double sPercent = s / 2.55;
            double vPercent = v / 2.55;
            arr.append(QString::number(hDeg, 'f', colorDecimals).toDouble());
            arr.append(QString::number(sPercent, 'f', colorDecimals).toDouble());
            arr.append(QString::number(vPercent, 'f', colorDecimals).toDouble());
            break;
        }

        case ColorFormat::HSL: {
            QColor color(r, g, b);
            int h, s, l;
            color.getHsl(&h, &s, &l);
            double hDeg = (h < 0) ? 0.0 : static_cast<double>(h);
            double sPercent = s / 2.55;
            double lPercent = l / 2.55;
            arr.append(QString::number(hDeg, 'f', colorDecimals).toDouble());
            arr.append(QString::number(sPercent, 'f', colorDecimals).toDouble());
            arr.append(QString::number(lPercent, 'f', colorDecimals).toDouble());
            break;
        }

        case ColorFormat::CMYK: {
            QColor color(r, g, b);
            int c, m, y, k;
            color.getCmyk(&c, &m, &y, &k);
            arr.append(static_cast<double>(c));
            arr.append(static_cast<double>(m));
            arr.append(static_cast<double>(y));
            arr.append(static_cast<double>(k));
            break;
        }
    }

    return arr;
}

// DetectionPoint::fromJson - 从任意格式解析回内部表示（像素坐标 + RGB）
DetectionPoint DetectionPoint::fromJson(const QJsonArray& arr,
                                       CoordinateFormat xyFormat, ColorFormat colorFormat,
                                       int imgWidth, int imgHeight)
{
    DetectionPoint point;

    // 根据坐标格式解析坐标
    if (xyFormat == CoordinateFormat::Normalized) {
        // 从JSON加载归一化坐标 → 作为主坐标
        double normX = arr[0].toDouble();
        double normY = arr[1].toDouble();

        // 验证并限制归一化坐标范围
        normX = qBound(0.0, normX, 1.0);
        normY = qBound(0.0, normY, 1.0);

        point.normX = normX;
        point.normY = normY;
        point.primaryCoord = PrimaryNormalized;  // 标记为主坐标

        // 缓存像素坐标（用于取色）
        point.x = qRound(point.normX * qMax(1, imgWidth - 1));
        point.y = qRound(point.normY * qMax(1, imgHeight - 1));
    } else {
        // 从JSON加载像素坐标 → 作为主坐标
        point.x = arr[0].toInt();
        point.y = arr[1].toInt();
        point.primaryCoord = PrimaryPixel;  // 标记为主坐标

        // 计算归一化坐标（缓存）
        point.normX = double(point.x) / qMax(1, imgWidth - 1);
        point.normY = double(point.y) / qMax(1, imgHeight - 1);
    }

    point.hasNormalized = true;

    // 根据颜色格式解析颜色
    switch (colorFormat) {
        case ColorFormat::RGB:
            point.r = arr[2].toInt();
            point.g = arr[3].toInt();
            point.b = arr[4].toInt();
            break;

        case ColorFormat::HEX: {
            QString hex = arr[2].toString();
            bool ok;
            point.r = hex.mid(1, 2).toInt(&ok, 16);
            point.g = hex.mid(3, 2).toInt(&ok, 16);
            point.b = hex.mid(5, 2).toInt(&ok, 16);
            break;
        }

        case ColorFormat::HSV: {
            double h = arr[2].toDouble();
            double s = arr[3].toDouble();
            double v = arr[4].toDouble();
            QColor color;
            color.setHsv(static_cast<int>(h),
                        static_cast<int>(s * 2.55),
                        static_cast<int>(v * 2.55));
            point.r = color.red();
            point.g = color.green();
            point.b = color.blue();
            break;
        }

        case ColorFormat::HSL: {
            double h = arr[2].toDouble();
            double s = arr[3].toDouble();
            double l = arr[4].toDouble();
            QColor color;
            color.setHsl(static_cast<int>(h),
                        static_cast<int>(s * 2.55),
                        static_cast<int>(l * 2.55));
            point.r = color.red();
            point.g = color.green();
            point.b = color.blue();
            break;
        }

        case ColorFormat::CMYK: {
            double c = arr[2].toDouble();
            double m = arr[3].toDouble();
            double y = arr[4].toDouble();
            double k = arr[5].toDouble();
            QColor color;
            color.setCmyk(static_cast<int>(c),
                         static_cast<int>(m),
                         static_cast<int>(y),
                         static_cast<int>(k));
            point.r = color.red();
            point.g = color.green();
            point.b = color.blue();
            break;
        }
    }

    return point;
}

// DetectionPoint::fromJsonLegacy - 向后兼容旧格式（没有格式信息的 JSON）
DetectionPoint DetectionPoint::fromJsonLegacy(const QJsonArray& arr)
{
    // 旧格式: [x, y, r, g, b, normX?, normY?]
    // 只返回像素坐标和 RGB
    return DetectionPoint(
        arr[0].toInt(),
        arr[1].toInt(),
        arr[2].toInt(),
        arr[3].toInt(),
        arr[4].toInt()
    );
}


QString MainWindow::formatFileSize(qint64 bytes)
{
    if (bytes < 1024) {
        return QString("%1 B").arg(bytes);
    } else if (bytes < 1024 * 1024) {
        return QString("%1 KB").arg(bytes / 1024.0, 0, 'f', fileSizeDecimals);
    } else if (bytes < 1024 * 1024 * 1024) {
        return QString("%1 MB").arg(bytes / (1024.0 * 1024.0), 0, 'f', fileSizeDecimals);
    } else {
        return QString("%1 GB").arg(bytes / (1024.0 * 1024.0 * 1024.0), 0, 'f', fileSizeDecimals);
    }
}

void MainWindow::updateImageInfoDisplay()
{
    if (currentImage.isNull()) {
        ui->imageWidthValue->setText("-");
        ui->imageHeightValue->setText("-");
        ui->imageSizeValue->setText("-");
        ui->imageFileBaseNameEdit->setText("未加载图片");
        ui->imageFileExtensionEdit->clear();
        ui->imageFileBaseNameEdit->setEnabled(false);
        ui->imageFileExtensionEdit->setEnabled(false);
        ui->imageFileBaseNameEdit->setToolTip("");
        ui->imageFileExtensionEdit->setToolTip("");
        ui->imageFileSizeValue->setText("-");
    } else {
        int width = currentImage.width();
        int height = currentImage.height();

        ui->imageWidthValue->setText(QString::number(width));
        ui->imageHeightValue->setText(QString::number(height));
        ui->imageSizeValue->setText(QString("%1 x %2").arg(width).arg(height));

        if (!currentImageFileName.isEmpty()) {
            QFileInfo fileInfo(currentImageFileName);
            ui->imageFileBaseNameEdit->setText(fileInfo.completeBaseName());
            ui->imageFileExtensionEdit->setText(fileInfo.suffix());
            ui->imageFileBaseNameEdit->setEnabled(true);
            ui->imageFileExtensionEdit->setEnabled(true);
            ui->imageFileBaseNameEdit->setToolTip(currentImageFileName);
            ui->imageFileExtensionEdit->setToolTip(currentImageFileName);

            if (currentImageFileSize > 0) {
                ui->imageFileSizeValue->setText(formatFileSize(currentImageFileSize));
            } else {
                ui->imageFileSizeValue->setText("-");
            }
        } else {
            ui->imageFileBaseNameEdit->setText("(拖放加载)");
            ui->imageFileExtensionEdit->clear();
            ui->imageFileBaseNameEdit->setEnabled(false);
            ui->imageFileExtensionEdit->setEnabled(false);
            ui->imageFileBaseNameEdit->setToolTip("");
            ui->imageFileExtensionEdit->setToolTip("");
            ui->imageFileSizeValue->setText("-");
        }
    }
}

void MainWindow::setupConnections()
{
    connect(ui->loadImageBtn, &QPushButton::clicked, this, &MainWindow::loadImage);
    connect(ui->loadConfigBtn, &QPushButton::clicked, this, &MainWindow::loadConfig);
    connect(ui->saveConfigBtn, &QPushButton::clicked, this, &MainWindow::saveConfig);
    connect(ui->saveAsConfigBtn, &QPushButton::clicked, this, &MainWindow::saveAsConfig);
    connect(ui->clearPointsBtn, &QPushButton::clicked, this, &MainWindow::clearAllPoints);
    connect(ui->colorFormatCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this]() {
        updatePointsList();
    });
    connect(ui->coordinateFormatCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int index) {
        CoordinateFormat newFormat = static_cast<CoordinateFormat>(index);
        convertCoordinateFormat(newFormat);
        updatePointsList();
    });
    connect(ui->pointsList, &QListWidget::itemDoubleClicked, this, [this](QListWidgetItem *item) {
        this->onPointsListItemDoubleClicked(item);
    });
    connect(ui->pointsList, &QListWidget::itemSelectionChanged, this, &MainWindow::onPointsListSelectionChanged);
    connect(ui->zoomInBtn, &QPushButton::clicked, this, &MainWindow::zoomIn);
    connect(ui->zoomOutBtn, &QPushButton::clicked, this, &MainWindow::zoomOut);
    connect(ui->zoomSlider, &QSlider::valueChanged, this, &MainWindow::zoomChanged);
    connect(ui->fitToScreenBtn, &QPushButton::clicked, this, &MainWindow::fitToScreen);
    connect(ui->actualSizeBtn, &QPushButton::clicked, this, &MainWindow::actualSize);

    // 高倍率时滚动需要重新渲染可见区域
    connect(ui->scrollArea->horizontalScrollBar(), &QScrollBar::valueChanged, this, [this]() {
        if (m_currentZoom > ZOOM_PIXEL_PERFECT_THRESHOLD) updateImageDisplay();
    });
    connect(ui->scrollArea->verticalScrollBar(), &QScrollBar::valueChanged, this, [this]() {
        if (m_currentZoom > ZOOM_PIXEL_PERFECT_THRESHOLD) updateImageDisplay();
    });
    connect(ui->addPointManuallyBtn, &QPushButton::clicked, this, &MainWindow::onAddPointManuallyClicked);
    connect(ui->deletePointBtn, &QPushButton::clicked, this, &MainWindow::onDeletePointClicked);
    connect(ui->editPointBtn, &QPushButton::clicked, this, &MainWindow::onEditPointClicked);
    connect(ui->copyCoordBtn, &QPushButton::clicked, this, &MainWindow::onCopyCoordClicked);
    connect(ui->copyColorBtn, &QPushButton::clicked, this, &MainWindow::onCopyColorClicked);
    connect(ui->copyPointBtn, &QPushButton::clicked, this, &MainWindow::onCopyPointClicked);
    connect(ui->loadFolderBtn, &QPushButton::clicked, this, &MainWindow::loadFolder);
    connect(ui->prevImageBtn, &QPushButton::clicked, this, &MainWindow::previousImage);
    connect(ui->nextImageBtn, &QPushButton::clicked, this, &MainWindow::nextImage);
    connect(ui->openInExplorerBtn, &QPushButton::clicked, this, &MainWindow::onOpenInExplorerClicked);
    connect(ui->selectInExplorerBtn, &QPushButton::clicked, this, &MainWindow::onSelectInExplorerClicked);
    connect(ui->copyImageBtn, &QPushButton::clicked, this, &MainWindow::onCopyImageClicked);
    connect(ui->copyImageFileBtn, &QPushButton::clicked, this, &MainWindow::onCopyImageFileClicked);
    connect(ui->imageFileBaseNameEdit, &QLineEdit::returnPressed, this, &MainWindow::commitImageFileRenameFromEditors);
    connect(ui->imageFileExtensionEdit, &QLineEdit::returnPressed, this, &MainWindow::commitImageFileRenameFromEditors);
}

void MainWindow::loadImage()
{
    QString fileName = QFileDialog::getOpenFileName(this, "选择图片", "",
        "Images (*.png *.jpg *.jpeg *.jfif *.bmp *.webp)");

    if (!fileName.isEmpty()) {
        if (!loadImageFile(fileName)) {
            ui->statusbar->showMessage("错误: 无法加载图片", 3000);
        }
    }
}

bool MainWindow::loadImageFile(const QString& filePath)
{
    QFileInfo fileInfo(filePath);
    if (!fileInfo.isFile() || !isSupportedImageFile(fileInfo)) {
        return false;
    }

    // Stop any running animation when switching images
    if (zoomAnimation && zoomAnimation->state() == QPropertyAnimation::Running) {
        zoomAnimation->stop();
    }

    // 清空文件夹模式，切换回单张图片模式
    if (!imageFileList.isEmpty()) {
        imageFileList.clear();
        currentImageIndex = -1;
        updateNavigationButtons();
        updateImageCounterDisplay();
    }

    // 保存当前的滚动条位置
    QScrollBar* hBar = ui->scrollArea->horizontalScrollBar();
    QScrollBar* vBar = ui->scrollArea->verticalScrollBar();
    int oldHScroll = hBar->value();
    int oldVScroll = vBar->value();

    currentImage.load(filePath);
    if (currentImage.isNull()) {
        return false;
    }

    // 保存图片文件信息
    currentImageFileName = filePath;
    currentImageFileSize = fileInfo.size();

    ui->imageLabel->setText("");
    // 保持当前缩放比例，不调用 fitToScreen()
    // 先更新所有检测点的RGB值和坐标
    updatePointsRGBFromImage();
    updateImageInfoDisplay();
    updateImageDisplay();

    // 启用资源管理器和复制图片按钮
    ui->openInExplorerBtn->setEnabled(true);
    ui->selectInExplorerBtn->setEnabled(true);
    ui->copyImageBtn->setEnabled(true);
    ui->copyImageFileBtn->setEnabled(true);
    renameImageShortcut->setEnabled(true);

    // 恢复滚动条位置（尽量恢复）
    hBar->setValue(qMin(oldHScroll, hBar->maximum()));
    vBar->setValue(qMin(oldVScroll, vBar->maximum()));
    return true;
}

void MainWindow::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton && !currentImage.isNull()) {
        if (ui->imageLabel->underMouse()) {
            // Start drag
            isDragging = true;
            dragStartPos = event->globalPos();
            scrollStartPos.setX(ui->scrollArea->horizontalScrollBar()->value());
            scrollStartPos.setY(ui->scrollArea->verticalScrollBar()->value());
            setCursor(Qt::ClosedHandCursor);
            grabMouse();  // Capture mouse to ensure move events are received
            return;
        }
    }
    QMainWindow::mousePressEvent(event);
}

void MainWindow::mouseMoveEvent(QMouseEvent *event)
{
    if (isDragging) {
        QPoint delta = event->globalPos() - dragStartPos;
        int newHValue = scrollStartPos.x() - delta.x();
        int newVValue = scrollStartPos.y() - delta.y();

        QScrollBar* hBar = ui->scrollArea->horizontalScrollBar();
        QScrollBar* vBar = ui->scrollArea->verticalScrollBar();

        hBar->setValue(qBound(hBar->minimum(), newHValue, hBar->maximum()));
        vBar->setValue(qBound(vBar->minimum(), newVValue, vBar->maximum()));
    }
    QMainWindow::mouseMoveEvent(event);
}

void MainWindow::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton && isDragging) {
        isDragging = false;
        setCursor(Qt::ArrowCursor);
        releaseMouse();  // Release mouse capture

        // Check if this was a click (small movement)
        QPoint delta = event->globalPos() - dragStartPos;
        if (delta.manhattanLength() < 5) {
            // This was a click, add detection point
            if (ui->imageLabel->underMouse()) {
                QPoint labelPos = ui->imageLabel->mapFromGlobal(event->globalPos());
                QSize imageSize = currentImage.size();

                int imageX, imageY;
                QPixmap pixmap = ui->imageLabel->pixmap();
                if (!pixmap.isNull()) {
                    QSize scaledSize = pixmap.size();
                    double scaleX = (double)imageSize.width() / scaledSize.width();
                    double scaleY = (double)imageSize.height() / scaledSize.height();
                    int offsetX = (ui->imageLabel->width() - scaledSize.width()) / 2;
                    int offsetY = (ui->imageLabel->height() - scaledSize.height()) / 2;
                    imageX = (labelPos.x() - offsetX) * scaleX;
                    imageY = (labelPos.y() - offsetY) * scaleY;
                } else {
                    imageX = static_cast<int>(labelPos.x() / m_currentZoom);
                    imageY = static_cast<int>(labelPos.y() / m_currentZoom);
                }

                if (imageX >= 0 && imageX < imageSize.width() &&
                    imageY >= 0 && imageY < imageSize.height()) {
                    addDetectionPoint(QPoint(imageX, imageY));
                }
            }
        }
    }
    QMainWindow::mouseReleaseEvent(event);
}

void MainWindow::wheelEvent(QWheelEvent *event)
{
    // 滚轮事件已在 eventFilter 中处理
    // 这里只处理不在 scrollArea 上的情况
    QMainWindow::wheelEvent(event);
}

void MainWindow::addDetectionPoint(const QPoint& pos)
{
    QColor color = getPixelColor(pos);
    DetectionPoint point;

    CoordinateFormat coordFormat = getCurrentCoordinateFormat();

    if (coordFormat == CoordinateFormat::Normalized) {
        // 归一化模式：归一化坐标为主
        point.normX = double(pos.x()) / qMax(1, currentImage.width() - 1);
        point.normY = double(pos.y()) / qMax(1, currentImage.height() - 1);
        point.primaryCoord = DetectionPoint::PrimaryNormalized;

        // 缓存像素坐标（用于取色）
        point.x = pos.x();
        point.y = pos.y();
    } else {
        // 像素模式：像素坐标为主
        point.x = pos.x();
        point.y = pos.y();
        point.primaryCoord = DetectionPoint::PrimaryPixel;

        // 计算归一化坐标（缓存）
        if (!currentImage.isNull()) {
            point.normX = double(point.x) / qMax(1, currentImage.width() - 1);
            point.normY = double(point.y) / qMax(1, currentImage.height() - 1);
        }
    }

    point.r = color.red();
    point.g = color.green();
    point.b = color.blue();
    point.hasNormalized = true;  // 总是标记为有归一化坐标

    detectionPoints.append(point);
    updatePointsList();
    drawDetectionPoints();
}

QColor MainWindow::getPixelColor(const QPoint& pos) const
{
    if (!currentImage.isNull() && pos.x() >= 0 && pos.x() < currentImage.width() &&
        pos.y() >= 0 && pos.y() < currentImage.height()) {
        return currentImage.pixel(pos);
    }
    return QColor();
}

void MainWindow::drawDetectionPoints()
{
    if (currentImage.isNull()) return;
    updateImageDisplay();
}

void MainWindow::updateImageDisplay()
{
    if (currentImage.isNull()) return;

    QSize scaledSize = currentImage.size() * m_currentZoom;

    // 高倍率时由 eventFilter 中的 Paint 事件处理可见区域渲染
    if (m_currentZoom > ZOOM_PIXEL_PERFECT_THRESHOLD) {
        ui->imageLabel->setContentsMargins(0, 0, 0, 0);
        ui->imageLabel->setFixedSize(scaledSize);
        ui->imageLabel->setPixmap(QPixmap());
        ui->imageLabel->update();
        updateMinimap();
        return;
    }

    // 低倍率：正常渲染整张图
    ui->imageLabel->setContentsMargins(0, 0, 0, 0);

    QImage displayImage = currentImage.copy();
    QPainter painter(&displayImage);

    for (int i = 0; i < detectionPoints.size(); ++i) {
        const auto& point = detectionPoints[i];
        if (i == selectedPointIndex) {
            painter.setPen(QPen(Qt::blue, 3));
        } else {
            painter.setPen(QPen(Qt::red, 2));
        }
        painter.drawEllipse(point.x - 2, point.y - 2, 4, 4);
    }

    QPixmap pixmap = QPixmap::fromImage(displayImage);
    Qt::TransformationMode mode = (m_currentZoom >= ZOOM_PIXEL_PERFECT_THRESHOLD)
        ? Qt::FastTransformation
        : Qt::SmoothTransformation;
    QPixmap scaledPixmap = pixmap.scaled(scaledSize, Qt::KeepAspectRatio, mode);
    ui->imageLabel->setPixmap(scaledPixmap);
    ui->imageLabel->setAlignment(Qt::AlignCenter);
    ui->imageLabel->setFixedSize(scaledPixmap.size());

    updateMinimap();
}

void MainWindow::updatePointsList()
{
    int savedSelection = selectedPointIndex;
    ui->pointsList->blockSignals(true);
    ui->pointsList->clear();

    ColorFormat colorFormat = getCurrentColorFormat();
    CoordinateFormat coordFormat = getCurrentCoordinateFormat();
    const int iconSize = 16;  // 颜色色块大小

    for (int i = 0; i < detectionPoints.size(); ++i) {
        const auto& point = detectionPoints[i];

        // 创建颜色图标
        QPixmap colorIcon(iconSize, iconSize);
        colorIcon.fill(QColor(point.r, point.g, point.b));

        // 添加边框以提高可见性（特别是浅色）
        QPainter painter(&colorIcon);
        painter.setPen(QPen(Qt::gray, 1));
        painter.drawRect(0, 0, iconSize - 1, iconSize - 1);

        // 创建列表项文本
        QString colorText = formatColorToString(point.r, point.g, point.b, colorFormat);
        QString coordText;
        if (coordFormat == CoordinateFormat::Normalized) {
            double normX, normY;

            if (point.isNormalizedPrimary()) {
                // 使用主坐标（归一化）
                normX = point.normX;
                normY = point.normY;
            } else if (!currentImage.isNull()) {
                // 像素坐标为主，动态计算
                normX = double(point.x) / qMax(1, currentImage.width() - 1);
                normY = double(point.y) / qMax(1, currentImage.height() - 1);
            } else {
                coordText = "(?, ?)";
                break;
            }

            coordText = QString("(%1, %2)")
                .arg(QString::number(normX, 'f', normalizedDecimals))
                .arg(QString::number(normY, 'f', normalizedDecimals));
        } else {
            // 像素坐标模式：检查是否超出图片范围
            if (!currentImage.isNull() &&
                (point.x >= currentImage.width() || point.y >= currentImage.height())) {
                // 像素坐标超出范围，颜色显示为 ???
                colorText = "???";
            }
            coordText = QString("(%1, %2)").arg(point.x).arg(point.y);
        }
        QString text = QString("%1 %2")
            .arg(coordText)
            .arg(colorText);

        // 创建列表项并设置图标
        QListWidgetItem* item = new QListWidgetItem(text);
        item->setIcon(QIcon(colorIcon));
        item->setData(Qt::UserRole, i);  // 存储点索引便于后续使用

        ui->pointsList->addItem(item);
    }

    // 设置统一的图标大小
    ui->pointsList->setIconSize(QSize(iconSize, iconSize));

    // 恢复之前的选中状态
    if (savedSelection >= 0 && savedSelection < detectionPoints.size()) {
        ui->pointsList->setCurrentRow(savedSelection);
    }
    ui->pointsList->blockSignals(false);

    // 更新列表高度
    updatePointsListHeight();
}

void MainWindow::onPointsListItemDoubleClicked(QListWidgetItem *item)
{
    int index = ui->pointsList->row(item);
    if (index >= 0 && index < detectionPoints.size()) {
        // 编辑检测点
        DetectionPoint& point = detectionPoints[index];
        CoordinateFormat coordFormat = getCurrentCoordinateFormat();

        QDialog dialog(this);
        dialog.setWindowTitle("编辑检测点");

        QFormLayout* layout = new QFormLayout(&dialog);

        QWidget* xInputWidget = nullptr;
        QWidget* yInputWidget = nullptr;

        if (coordFormat == CoordinateFormat::Normalized) {
            if (currentImage.isNull()) {
                ui->statusbar->showMessage("错误: 请先加载图片", 3000);
                return;
            }

            QDoubleSpinBox* xSpinBox = new QDoubleSpinBox(&dialog);
            xSpinBox->setRange(0.0, 1.0);
            xSpinBox->setDecimals(normalizedDecimals);
            xSpinBox->setSingleStep(qPow(10, -normalizedDecimals));
            // 优先使用主坐标
            double normX = point.isNormalizedPrimary() ? point.normX :
                          double(point.x) / qMax(1, currentImage.width() - 1);
            xSpinBox->setValue(normX);
            layout->addRow("X坐标 (归一化):", xSpinBox);
            xInputWidget = xSpinBox;

            QDoubleSpinBox* ySpinBox = new QDoubleSpinBox(&dialog);
            ySpinBox->setRange(0.0, 1.0);
            ySpinBox->setDecimals(normalizedDecimals);
            ySpinBox->setSingleStep(qPow(10, -normalizedDecimals));
            double normY = point.isNormalizedPrimary() ? point.normY :
                          double(point.y) / qMax(1, currentImage.height() - 1);
            ySpinBox->setValue(normY);
            layout->addRow("Y坐标 (归一化):", ySpinBox);
            yInputWidget = ySpinBox;

            // 显示对应的像素坐标（只读）
            int pixX = point.isNormalizedPrimary() ? point.x : qRound(normX * qMax(1, currentImage.width() - 1));
            int pixY = point.isNormalizedPrimary() ? point.y : qRound(normY * qMax(1, currentImage.height() - 1));
            QLabel* pixelCoordLabel = new QLabel(QString("像素坐标: (%1, %2)").arg(pixX).arg(pixY), &dialog);
            pixelCoordLabel->setStyleSheet("color: #666; font-size: 11px; padding: 3px; background-color: #f9f9f9; border: 1px solid #ddd;");
            layout->addRow("像素坐标:", pixelCoordLabel);
        } else {
            QSpinBox* xSpinBox = new QSpinBox(&dialog);
            xSpinBox->setRange(0, currentImage.isNull() ? 1920 : currentImage.width() - 1);
            xSpinBox->setValue(point.x);
            layout->addRow("X坐标:", xSpinBox);
            xInputWidget = xSpinBox;

            QSpinBox* ySpinBox = new QSpinBox(&dialog);
            ySpinBox->setRange(0, currentImage.isNull() ? 1080 : currentImage.height() - 1);
            ySpinBox->setValue(point.y);
            layout->addRow("Y坐标:", ySpinBox);
            yInputWidget = ySpinBox;

            // 显示对应的归一化坐标（只读）
            if (!currentImage.isNull()) {
                double nX = point.isNormalizedPrimary() ? point.normX :
                           double(point.x) / qMax(1, currentImage.width() - 1);
                double nY = point.isNormalizedPrimary() ? point.normY :
                           double(point.y) / qMax(1, currentImage.height() - 1);
                QLabel* normCoordLabel = new QLabel(QString("归一化坐标: (%1, %2)").arg(nX, 0, 'f', normalizedDecimals).arg(nY, 0, 'f', normalizedDecimals), &dialog);
                normCoordLabel->setStyleSheet("color: #666; font-size: 11px; padding: 3px; background-color: #f9f9f9; border: 1px solid #ddd;");
                layout->addRow("归一化坐标:", normCoordLabel);
            }
        }

        // 显示当前RGB颜色（只读）
        QLabel* rgbLabel = new QLabel(QString("RGB(%1, %2, %3)").arg(point.r).arg(point.g).arg(point.b), &dialog);
        rgbLabel->setStyleSheet(QString("color: gray; padding: 5px; background-color: rgb(%1, %2, %3);").arg(point.r).arg(point.g).arg(point.b));
        layout->addRow("当前颜色:", rgbLabel);

        // 显示多种颜色格式
        ColorFormat currentFormat = getCurrentColorFormat();
        QString formatInfo = QString("当前显示格式: %1\n\n其他格式:\n%2\n%3\n%4\n%5")
            .arg(ui->colorFormatCombo->currentText())
            .arg(rgbToHex(point.r, point.g, point.b))
            .arg(rgbToHsl(point.r, point.g, point.b))
            .arg(rgbToHsv(point.r, point.g, point.b))
            .arg(rgbToCmyk(point.r, point.g, point.b));

        QLabel* allFormatsLabel = new QLabel(formatInfo, &dialog);
        allFormatsLabel->setStyleSheet("color: #333; font-size: 11px; padding: 5px; background-color: #f5f5f5; border: 1px solid #ccc;");
        layout->addRow(allFormatsLabel);

        // 提示信息
        QLabel* hintLabel = new QLabel("RGB颜色将自动从图片获取", &dialog);
        hintLabel->setStyleSheet("color: blue; font-size: 10px;");
        layout->addRow(hintLabel);

        // 添加图片尺寸提示
        if (!currentImage.isNull()) {
            QLabel* sizeHint = new QLabel(QString("图片尺寸: %1 x %2").arg(currentImage.width()).arg(currentImage.height()), &dialog);
            sizeHint->setStyleSheet("color: gray; font-size: 10px;");
            layout->addRow(sizeHint);
        }

        // 颜色范围按钮
        QPushButton* colorRangeBtn = new QPushButton("颜色范围...", &dialog);
        layout->addRow(colorRangeBtn);

        connect(colorRangeBtn, &QPushButton::clicked, &dialog, [this, &point]() {
            // 获取归一化坐标用于跨图片采样
            double nx = point.normX;
            double ny = point.normY;
            if (!point.hasNormalized && !currentImage.isNull()) {
                nx = double(point.x) / qMax(1, currentImage.width() - 1);
                ny = double(point.y) / qMax(1, currentImage.height() - 1);
            }

            QStringList files = QFileDialog::getOpenFileNames(this,
                "选择图片文件进行颜色范围分析",
                currentImageFileName.isEmpty() ? QString() : QFileInfo(currentImageFileName).absolutePath(),
                "图片文件 (*.png *.jpg *.jpeg *.bmp *.tif *.tiff *.gif *.webp *.jfif *.pbm *.pgm *.ppm *.xpm);;所有文件 (*.*)");

            if (files.isEmpty()) return;

            // 采样每张图片的像素颜色
            QList<QColor> sampledColors;
            QStringList failedFiles;

            QProgressDialog progress("正在加载图片...", "取消", 0, files.size(), this);
            progress.setWindowModality(Qt::WindowModal);
            progress.setMinimumDuration(0);
            progress.setValue(0);

            for (int fi = 0; fi < files.size(); ++fi) {
                progress.setValue(fi);
                if (progress.wasCanceled()) break;
                progress.setLabelText(QString("正在加载: %1").arg(QFileInfo(files[fi]).fileName()));

                QImage img(files[fi]);
                if (img.isNull()) {
                    failedFiles << QFileInfo(files[fi]).fileName();
                    continue;
                }
                int px = qRound(nx * qMax(1, img.width() - 1));
                int py = qRound(ny * qMax(1, img.height() - 1));
                px = qBound(0, px, img.width() - 1);
                py = qBound(0, py, img.height() - 1);
                sampledColors.append(img.pixelColor(px, py));
            }
            progress.setValue(files.size());

            if (sampledColors.isEmpty()) {
                QMessageBox::warning(this, "颜色范围", "无法加载任何选中的图片文件。");
                return;
            }

            // 创建颜色范围对话框
            QDialog rangeDialog(this);
            rangeDialog.setWindowTitle("颜色范围");
            rangeDialog.setMinimumWidth(400);

            QVBoxLayout* rangeLayout = new QVBoxLayout(&rangeDialog);

            // 顶部：格式选择 + 文件数信息
            QHBoxLayout* topLayout = new QHBoxLayout();
            QComboBox* formatCombo = new QComboBox(&rangeDialog);
            formatCombo->addItem("RGB", static_cast<int>(ColorFormat::RGB));
            formatCombo->addItem("HSV", static_cast<int>(ColorFormat::HSV));
            formatCombo->addItem("HSL", static_cast<int>(ColorFormat::HSL));
            formatCombo->addItem("CMYK", static_cast<int>(ColorFormat::CMYK));
            int defaultIdx = formatCombo->findData(static_cast<int>(getCurrentColorFormat()));
            if (defaultIdx < 0) defaultIdx = 0;
            formatCombo->setCurrentIndex(defaultIdx);
            topLayout->addWidget(new QLabel("颜色格式:", &rangeDialog));
            topLayout->addWidget(formatCombo);
            topLayout->addStretch();
            QLabel* countLabel = new QLabel(QString("采样文件数: %1").arg(sampledColors.size()), &rangeDialog);
            countLabel->setStyleSheet("color: #666;");
            topLayout->addWidget(countLabel);
            if (!failedFiles.isEmpty()) {
                QLabel* failLabel = new QLabel(QString("(失败: %1)").arg(failedFiles.size()), &rangeDialog);
                failLabel->setStyleSheet("color: red; font-size: 10px;");
                failLabel->setToolTip(failedFiles.join("\n"));
                topLayout->addWidget(failLabel);
            }
            rangeLayout->addLayout(topLayout);

            // 表格
            QTableWidget* table = new QTableWidget(&rangeDialog);
            table->setColumnCount(4);
            table->setHorizontalHeaderLabels({"名称", "最大值", "最小值", "平均值"});
            table->horizontalHeader()->setStretchLastSection(true);
            table->setEditTriggers(QAbstractItemView::NoEditTriggers);
            table->setSelectionBehavior(QAbstractItemView::SelectRows);
            table->verticalHeader()->setVisible(false);
            rangeLayout->addWidget(table);

            // 构建表格数据的 lambda
            auto buildTable = [&](ColorFormat fmt) {
                // 计算各分量值列表
                QList<QStringList> components; // each: {name, values...}
                QList<double> allValues;
                QStringList compNames;

                if (fmt == ColorFormat::RGB || fmt == ColorFormat::HEX) {
                    compNames = {"R", "G", "B"};
                    for (const auto& c : sampledColors) {
                        allValues << c.red() << c.green() << c.blue();
                    }
                } else if (fmt == ColorFormat::HSV) {
                    compNames = {"H (°)", "S (%)", "V (%)"};
                    for (const auto& c : sampledColors) {
                        auto vals = rgbToHsvValues(c.red(), c.green(), c.blue());
                        allValues << vals[0] << vals[1] << vals[2];
                    }
                } else if (fmt == ColorFormat::HSL) {
                    compNames = {"H (°)", "S (%)", "L (%)"};
                    for (const auto& c : sampledColors) {
                        auto vals = rgbToHslValues(c.red(), c.green(), c.blue());
                        allValues << vals[0] << vals[1] << vals[2];
                    }
                } else if (fmt == ColorFormat::CMYK) {
                    compNames = {"C", "M", "Y", "K"};
                    for (const auto& c : sampledColors) {
                        auto vals = rgbToCmykValues(c.red(), c.green(), c.blue());
                        allValues << vals[0] << vals[1] << vals[2] << vals[3];
                    }
                }

                int numComps = compNames.size();
                table->setRowCount(numComps);

                for (int i = 0; i < numComps; ++i) {
                    double minV = std::numeric_limits<double>::max();
                    double maxV = std::numeric_limits<double>::lowest();
                    double sum = 0;
                    for (int j = 0; j < sampledColors.size(); ++j) {
                        double v = allValues[j * numComps + i];
                        if (v < minV) minV = v;
                        if (v > maxV) maxV = v;
                        sum += v;
                    }
                    double avg = sum / sampledColors.size();

                    auto formatValue = [&](double v) -> QString {
                        if (fmt == ColorFormat::RGB || fmt == ColorFormat::HEX || fmt == ColorFormat::CMYK) {
                            if (colorDecimals == 0)
                                return QString::number(qRound(v));
                            return QString::number(v, 'f', colorDecimals);
                        }
                        // HSV/HSL: H 是整数度数，S/V/L 是百分比
                        if (i == 0) { // H
                            if (colorDecimals == 0)
                                return QString::number(qRound(v));
                            return QString::number(v, 'f', colorDecimals);
                        }
                        return QString::number(v, 'f', qMax(colorDecimals, 1));
                    };

                    table->setItem(i, 0, new QTableWidgetItem(compNames[i]));
                    table->setItem(i, 1, new QTableWidgetItem(formatValue(maxV)));
                    table->setItem(i, 2, new QTableWidgetItem(formatValue(minV)));
                    table->setItem(i, 3, new QTableWidgetItem(formatValue(avg)));
                }

                table->resizeColumnsToContents();
            };

            buildTable(static_cast<ColorFormat>(formatCombo->currentData().toInt()));

            // 格式切换
            connect(formatCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), [&](int idx) {
                Q_UNUSED(idx);
                buildTable(static_cast<ColorFormat>(formatCombo->currentData().toInt()));
            });

            // 复制按钮
            QPushButton* copyBtn = new QPushButton("复制", &rangeDialog);
            rangeLayout->addWidget(copyBtn);

            connect(copyBtn, &QPushButton::clicked, &rangeDialog, [this, &sampledColors, formatCombo]() {
                ColorFormat fmt = static_cast<ColorFormat>(formatCombo->currentData().toInt());
                QStringList tuples;

                auto formatVal = [&](double v) -> QString {
                    if (fmt == ColorFormat::RGB || fmt == ColorFormat::HEX || fmt == ColorFormat::CMYK) {
                        if (colorDecimals == 0)
                            return QString::number(qRound(v));
                        return QString::number(v, 'f', colorDecimals);
                    }
                    return QString::number(v, 'f', qMax(colorDecimals, 1));
                };

                for (const auto& c : sampledColors) {
                    QStringList vals;
                    if (fmt == ColorFormat::RGB || fmt == ColorFormat::HEX) {
                        vals << formatVal(c.red()) << formatVal(c.green()) << formatVal(c.blue());
                    } else if (fmt == ColorFormat::HSV) {
                        auto v = rgbToHsvValues(c.red(), c.green(), c.blue());
                        for (auto d : v) vals << formatVal(d);
                    } else if (fmt == ColorFormat::HSL) {
                        auto v = rgbToHslValues(c.red(), c.green(), c.blue());
                        for (auto d : v) vals << formatVal(d);
                    } else if (fmt == ColorFormat::CMYK) {
                        auto v = rgbToCmykValues(c.red(), c.green(), c.blue());
                        for (auto d : v) vals << formatVal(d);
                    }
                    tuples << "(" + vals.join(",") + ")";
                }
                QGuiApplication::clipboard()->setText(tuples.join(","));
            });

            rangeDialog.exec();
        });

        // 添加按钮
        QDialogButtonBox* buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
        layout->addRow(buttonBox);

        connect(buttonBox->button(QDialogButtonBox::Ok), &QPushButton::clicked, &dialog, &QDialog::accept);
        connect(buttonBox->button(QDialogButtonBox::Cancel), &QPushButton::clicked, &dialog, &QDialog::reject);

        // 显示对话框
        if (dialog.exec() == QDialog::Accepted) {
            // 更新坐标
            int newX, newY;

            if (coordFormat == CoordinateFormat::Normalized) {
                QDoubleSpinBox* xSpin = static_cast<QDoubleSpinBox*>(xInputWidget);
                QDoubleSpinBox* ySpin = static_cast<QDoubleSpinBox*>(yInputWidget);

                double normX = xSpin->value();
                double normY = ySpin->value();

                newX = qRound(normX * qMax(1, currentImage.width() - 1));
                newY = qRound(normY * qMax(1, currentImage.height() - 1));

                // 更新归一化坐标并设为主坐标
                point.normX = normX;
                point.normY = normY;
                point.primaryCoord = DetectionPoint::PrimaryNormalized;
            } else {
                QSpinBox* xSpin = static_cast<QSpinBox*>(xInputWidget);
                QSpinBox* ySpin = static_cast<QSpinBox*>(yInputWidget);

                newX = xSpin->value();
                newY = ySpin->value();

                // 更新像素坐标并设为主坐标
                point.primaryCoord = DetectionPoint::PrimaryPixel;
            }

            // 从图片获取新坐标的RGB颜色
            QColor newColor = getPixelColor(QPoint(newX, newY));

            // 更新检测点
            point.x = newX;
            point.y = newY;
            point.r = newColor.red();
            point.g = newColor.green();
            point.b = newColor.blue();

            // 如果在像素坐标模式下编辑，重新计算归一化坐标缓存
            if (coordFormat != CoordinateFormat::Normalized && !currentImage.isNull()) {
                point.normX = double(point.x) / qMax(1, currentImage.width() - 1);
                point.normY = double(point.y) / qMax(1, currentImage.height() - 1);
            }

            point.hasNormalized = true;

            updatePointsList();
            drawDetectionPoints();
        }
    }
}

void MainWindow::clearAllPoints()
{
    if (!detectionPoints.isEmpty()) {
        auto confirm = QMessageBox::question(this, "确认清空",
            "确定要清空所有检测点吗?",
            QMessageBox::Yes | QMessageBox::No);

        if (confirm == QMessageBox::Yes) {
            detectionPoints.clear();
            updatePointsList();
            drawDetectionPoints();
        }
    }
}

void MainWindow::dragEnterEvent(QDragEnterEvent *event)
{
    if (event->mimeData()->hasUrls()) {
        const QList<QUrl> urls = event->mimeData()->urls();
        for (const QUrl& url : urls) {
            QFileInfo fileInfo(url.toLocalFile());
            if (fileInfo.isDir() ||
                fileInfo.suffix().compare("json", Qt::CaseInsensitive) == 0 ||
                isSupportedImageFile(fileInfo)) {
                event->acceptProposedAction();
                return;
            }
        }
    }
}

void MainWindow::dropEvent(QDropEvent *event)
{
    const QMimeData* mimeData = event->mimeData();
    if (mimeData->hasUrls()) {
        QList<QUrl> urls = mimeData->urls();
        for (const QUrl& url : urls) {
            const QString path = url.toLocalFile();
            QFileInfo fileInfo(path);

            if (fileInfo.isDir()) {
                if (loadImageFolder(path)) {
                    ui->statusbar->showMessage("成功: 文件夹已加载", 3000);
                }
                event->acceptProposedAction();
                return;
            }

            if (fileInfo.suffix().compare("json", Qt::CaseInsensitive) == 0) {
                if (loadJsonConfig(path)) {
                    ui->statusbar->showMessage("成功: 配置已加载", 3000);
                } else {
                    ui->statusbar->showMessage("错误: 加载配置失败，请检查文件格式", 3000);
                }
                event->acceptProposedAction();
                return;
            }

            if (loadImageFile(path)) {
                event->acceptProposedAction();
                return;
            }
        }
    }
    event->ignore();
}

bool MainWindow::saveJsonConfig(const QString& filePath)
{
    QJsonObject root;

    // 获取当前格式
    CoordinateFormat xyFormat = getCurrentCoordinateFormat();
    ColorFormat colorFormat = getCurrentColorFormat();

    // 保存格式设置
    root["xyFormat"] = (xyFormat == CoordinateFormat::Normalized) ? "normalized" : "pixel";
    root["colorFormat"] = colorFormatToString(colorFormat);

    // 保存图片尺寸
    if (!currentImage.isNull()) {
        root["imageWidth"] = currentImage.width();
        root["imageHeight"] = currentImage.height();
    }

    // 保存显示设置
    root["normalizedDecimals"] = normalizedDecimals;
    root["colorDecimals"] = colorDecimals;
    root["fileSizeDecimals"] = fileSizeDecimals;
    root["configName"] = ui->configNameEdit->text();
    root["pointsListVisibleRows"] = pointsListVisibleRows;

    // 根据当前格式保存点数据
    QJsonArray pointsArray;
    for (const auto& point : detectionPoints) {
        pointsArray.append(point.toJson(xyFormat, colorFormat,
                                        currentImage.width(), currentImage.height(),
                                        normalizedDecimals, colorDecimals));
    }
    root["points"] = pointsArray;

    // 写入文件
    QJsonDocument doc(root);
    QFile file(filePath);
    if (file.open(QIODevice::WriteOnly)) {
        file.write(doc.toJson());
        file.close();
        return true;
    }
    return false;
}

static bool readConfigImageSize(const QString& filePath, QSize& imageSize)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        return false;
    }

    QJsonParseError error;
    QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &error);
    if (error.error != QJsonParseError::NoError || !doc.isObject()) {
        return false;
    }

    QJsonObject root = doc.object();
    if (!root["imageWidth"].isDouble() || !root["imageHeight"].isDouble()) {
        return false;
    }

    int width = root["imageWidth"].toInt();
    int height = root["imageHeight"].toInt();
    if (width <= 0 || height <= 0) {
        return false;
    }

    imageSize = QSize(width, height);
    return true;
}

void MainWindow::saveConfig()
{
    if (currentConfigFilePath.isEmpty()) {
        // 未加载过配置文件，跳转到另存为
        saveAsConfig();
        return;
    }

    QSize savedImageSize;
    if (!currentImage.isNull() &&
        readConfigImageSize(currentConfigFilePath, savedImageSize) &&
        savedImageSize != currentImage.size()) {
        QString message = QString("当前图片分辨率为 %1 x %2，原配置保存时的分辨率为 %3 x %4。\n\n"
                                  "继续覆盖保存可能会改变像素坐标对应的归一化结果。是否继续覆盖？")
                              .arg(currentImage.width())
                              .arg(currentImage.height())
                              .arg(savedImageSize.width())
                              .arg(savedImageSize.height());

        QMessageBox::StandardButton result = QMessageBox::warning(
            this,
            "分辨率不一致",
            message,
            QMessageBox::Yes | QMessageBox::No,
            QMessageBox::No);

        if (result != QMessageBox::Yes) {
            ui->statusbar->showMessage("已取消保存", 3000);
            return;
        }
    }

    if (saveJsonConfig(currentConfigFilePath)) {
        ui->statusbar->showMessage("成功: 配置已保存到: " + currentConfigFilePath, 3000);
    } else {
        ui->statusbar->showMessage("错误: 保存配置失败", 3000);
    }
}

void MainWindow::saveAsConfig()
{
    QString configName = ui->configNameEdit->text();
    if (configName.isEmpty()) {
        ui->statusbar->showMessage("错误: 请先输入配置名称", 3000);
        return;
    }

    QString defaultName = currentConfigFilePath.isEmpty()
        ? configName + ".json"
        : currentConfigFilePath;

    QString fileName = QFileDialog::getSaveFileName(this, "另存为配置", defaultName, "JSON Files (*.json)");
    if (!fileName.isEmpty()) {
        if (saveJsonConfig(fileName)) {
            currentConfigFilePath = fileName;
            ui->statusbar->showMessage("成功: 配置已保存到: " + fileName, 3000);
        } else {
            ui->statusbar->showMessage("错误: 保存配置失败", 3000);
        }
    }
}

bool MainWindow::loadJsonConfig(const QString& filePath)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        return false;
    }

    QByteArray data = file.readAll();
    file.close();

    QJsonParseError error;
    QJsonDocument doc = QJsonDocument::fromJson(data, &error);
    if (error.error != QJsonParseError::NoError || !doc.isObject()) {
        return false;
    }

    QJsonObject root = doc.object();
    detectionPoints.clear();

    // 检测是否为新格式（包含 xyFormat, colorFormat 和 points 字段）
    bool isNewFormat = root.contains("xyFormat") && root.contains("colorFormat") && root.contains("points");

    if (isNewFormat) {
        // 新格式：读取格式设置
        CoordinateFormat xyFormat = CoordinateFormat::Pixel;  // 默认
        if (root["xyFormat"].isString()) {
            QString formatStr = root["xyFormat"].toString();
            xyFormat = (formatStr == "normalized") ?
                       CoordinateFormat::Normalized : CoordinateFormat::Pixel;
        }

        ColorFormat colorFormat = ColorFormat::RGB;  // 默认
        if (root["colorFormat"].isString()) {
            colorFormat = stringToColorFormat(root["colorFormat"].toString());
        }

        // 恢复格式设置到界面
        ui->coordinateFormatCombo->setCurrentIndex(static_cast<int>(xyFormat));
        ui->colorFormatCombo->setCurrentIndex(static_cast<int>(colorFormat));

        // 读取图片尺寸（仅用于确认信息，实际像素坐标使用当前图片尺寸计算）
        int savedImgWidth = 1920, savedImgHeight = 1080;  // 默认值
        if (root.contains("imageWidth") && root["imageWidth"].isDouble()) {
            savedImgWidth = root["imageWidth"].toInt();
        }
        if (root.contains("imageHeight") && root["imageHeight"].isDouble()) {
            savedImgHeight = root["imageHeight"].toInt();
        }

        // 使用当前图片尺寸计算像素坐标（如果没有加载图片，使用保存时的尺寸作为后备）
        int currentImgWidth = currentImage.isNull() ? savedImgWidth : currentImage.width();
        int currentImgHeight = currentImage.isNull() ? savedImgHeight : currentImage.height();

        // 读取点数据（根据存储的格式解析）
        if (root["points"].isArray()) {
            QJsonArray pointsArray = root["points"].toArray();

            // 检查归一化坐标是否超出范围
            bool hasInvalidCoords = false;
            if (xyFormat == CoordinateFormat::Normalized) {
                for (const QJsonValue& value : pointsArray) {
                    if (value.isArray()) {
                        QJsonArray ptArr = value.toArray();
                        if (ptArr.size() >= 2) {
                            double x = ptArr[0].toDouble();
                            double y = ptArr[1].toDouble();
                            if (x < 0.0 || x > 1.0 || y < 0.0 || y > 1.0) {
                                hasInvalidCoords = true;
                                break;
                            }
                        }
                    }
                }
            }

            // 加载检测点数据
            for (const QJsonValue& value : pointsArray) {
                if (value.isArray()) {
                    detectionPoints.append(DetectionPoint::fromJson(
                        value.toArray(), xyFormat, colorFormat, currentImgWidth, currentImgHeight));
                }
            }

            // 如果检测到无效坐标，显示警告
            if (hasInvalidCoords) {
                ui->statusbar->showMessage("警告: 配置文件中包含超出范围的归一化坐标，已自动修正到 [0, 1] 范围内", 4000);
            }
        }
    } else {
        // 旧格式：使用旧加载逻辑（向后兼容）
        if (root.contains("point") && root["point"].isArray()) {
            QJsonArray pointsArray = root["point"].toArray();
            for (const QJsonValue& value : pointsArray) {
                if (value.isArray()) {
                    detectionPoints.append(DetectionPoint::fromJsonLegacy(value.toArray()));
                }
            }
        }

        // Load color format preference (backward compatible - defaults to 0 if not present)
        if (root.contains("colorFormat") && root["colorFormat"].isDouble()) {
            int colorFormatIndex = root["colorFormat"].toInt();
            ui->colorFormatCombo->setCurrentIndex(qBound(0, colorFormatIndex, 4));
        } else {
            ui->colorFormatCombo->setCurrentIndex(0);  // Default to RGB
        }

        // Load coordinate format preference (向后兼容)
        if (root.contains("coordinateFormat") && root["coordinateFormat"].isDouble()) {
            int coordFormatIndex = root["coordinateFormat"].toInt();
            ui->coordinateFormatCombo->setCurrentIndex(qBound(0, coordFormatIndex, 1));
        } else {
            ui->coordinateFormatCombo->setCurrentIndex(0);  // Default to Pixel
        }
    }

    // Load config name (backward compatible - defaults to empty string if not present)
    if (root.contains("configName") && root["configName"].isString()) {
        ui->configNameEdit->setText(root["configName"].toString());
    } else {
        ui->configNameEdit->setText("");
    }

    // Load display settings - decimal places (backward compatible - defaults if not present)
    if (root.contains("normalizedDecimals") && root["normalizedDecimals"].isDouble()) {
        normalizedDecimals = qBound(0, root["normalizedDecimals"].toInt(), 8);
        ui->normalizedDecimalsSpin->setValue(normalizedDecimals);
    }
    if (root.contains("colorDecimals") && root["colorDecimals"].isDouble()) {
        colorDecimals = qBound(0, root["colorDecimals"].toInt(), 2);
        ui->colorDecimalsSpin->setValue(colorDecimals);
    }
    if (root.contains("fileSizeDecimals") && root["fileSizeDecimals"].isDouble()) {
        fileSizeDecimals = qBound(0, root["fileSizeDecimals"].toInt(), 3);
        ui->fileSizeDecimalsSpin->setValue(fileSizeDecimals);
    }
    if (root.contains("pointsListVisibleRows") && root["pointsListVisibleRows"].isDouble()) {
        pointsListVisibleRows = qBound(1, root["pointsListVisibleRows"].toInt(), 50);
        ui->pointsListVisibleRowsSpin->setValue(pointsListVisibleRows);
    }

    // Load image dimensions (backward compatible - ignore if not present)
    // If image dimensions are in config, display them even if no image is loaded
    bool hasImageDimensions = root.contains("imageWidth") && root.contains("imageHeight") &&
                             root["imageWidth"].isDouble() && root["imageHeight"].isDouble();

    // 如果配置中有图片尺寸信息，更新显示
    if (hasImageDimensions && currentImage.isNull()) {
        int imageWidth = root["imageWidth"].toInt();
        int imageHeight = root["imageHeight"].toInt();

        ui->imageWidthValue->setText(QString::number(imageWidth));
        ui->imageHeightValue->setText(QString::number(imageHeight));
        ui->imageSizeValue->setText(QString("%1 x %2").arg(imageWidth).arg(imageHeight));
        ui->imageFileBaseNameEdit->setText("(配置中的尺寸)");
        ui->imageFileExtensionEdit->clear();
        ui->imageFileBaseNameEdit->setEnabled(false);
        ui->imageFileExtensionEdit->setEnabled(false);
        ui->imageFileSizeValue->setText("-");
    }

    // 如果已加载图片，根据图片更新检测点的RGB值
    if (!currentImage.isNull()) {
        updatePointsRGBFromImage();
    }

    updatePointsList();
    drawDetectionPoints();
    currentConfigFilePath = filePath;
    return true;
}

void MainWindow::loadConfig()
{
    QString fileName = QFileDialog::getOpenFileName(this, "加载配置", "", "JSON Files (*.json)");
    if (!fileName.isEmpty()) {
        if (loadJsonConfig(fileName)) {
            ui->statusbar->showMessage("成功: 配置已加载", 3000);
        } else {
            ui->statusbar->showMessage("错误: 加载配置失败，请检查文件格式", 3000);
        }
    }
}

void MainWindow::setZoom(double zoom)
{
    animatedZoomTo(zoom);
}

void MainWindow::zoomIn()
{
    setZoom(m_currentZoom + calculateZoomStep(m_currentZoom));
}

void MainWindow::zoomOut()
{
    setZoom(m_currentZoom - calculateZoomStep(m_currentZoom));
}

void MainWindow::zoomChanged(int value)
{
    animatedZoomTo(value / 100.0);
}

void MainWindow::fitToScreen()
{
    if (currentImage.isNull()) return;

    QSize scrollSize = fitToScreenAvailableSize();
    QSize imageSize = currentImage.size();

    double scaleX = (double)scrollSize.width() / imageSize.width();
    double scaleY = (double)scrollSize.height() / imageSize.height();
    double fitZoom = qMin(scaleX, scaleY);

    animatedZoomTo(fitZoom);
}

QSize MainWindow::fitToScreenAvailableSize() const
{
    QSize availableSize = ui->scrollArea->viewport()->size();

    if (QLayout* contentsLayout = ui->scrollAreaContents->layout()) {
        QMargins margins = contentsLayout->contentsMargins();
        availableSize.rwidth() -= margins.left() + margins.right();
        availableSize.rheight() -= margins.top() + margins.bottom();
    }

    if (ui->statusbar->isVisible()) {
        const int viewportTop = ui->scrollArea->viewport()->mapTo(this, QPoint(0, 0)).y();
        const int statusbarTop = ui->statusbar->mapTo(this, QPoint(0, 0)).y();
        if (statusbarTop > viewportTop) {
            availableSize.setHeight(qMin(availableSize.height(), statusbarTop - viewportTop));
        }
    }

    availableSize.setWidth(qMax(1, availableSize.width()));
    availableSize.setHeight(qMax(1, availableSize.height()));
    return availableSize;
}

void MainWindow::actualSize()
{
    setZoom(1.0);
}

double MainWindow::calculateZoomStep(double currentZoom) const
{
    if (currentZoom < 2.0) return 0.1;
    if (currentZoom < 10.0) return 0.5;
    return 1.0;
}

QString MainWindow::formatZoomLabel(double zoom) const
{
    double pct = zoom * 100.0;
    if (pct >= 100.0 && std::fmod(pct, 1.0) < 0.01) {
        return QString("缩放: %1%").arg(static_cast<int>(pct));
    }
    return QString("缩放: %1%").arg(pct, 0, 'f', 1);
}

void MainWindow::onAddPointManuallyClicked()
{
    if (currentImage.isNull()) {
        ui->statusbar->showMessage("错误: 请先加载图片", 3000);
        return;
    }

    CoordinateFormat coordFormat = getCurrentCoordinateFormat();

    // 创建对话框
    QDialog dialog(this);
    dialog.setWindowTitle("手动添加检测点");

    QFormLayout* layout = new QFormLayout(&dialog);

    QWidget* xInputWidget = nullptr;
    QWidget* yInputWidget = nullptr;

    if (coordFormat == CoordinateFormat::Normalized) {
        QDoubleSpinBox* xSpinBox = new QDoubleSpinBox(&dialog);
        xSpinBox->setRange(0.0, 1.0);
        xSpinBox->setDecimals(normalizedDecimals);
        xSpinBox->setSingleStep(qPow(10, -normalizedDecimals));
        xSpinBox->setValue(0.5);
        layout->addRow("X坐标 (归一化):", xSpinBox);
        xInputWidget = xSpinBox;

        QDoubleSpinBox* ySpinBox = new QDoubleSpinBox(&dialog);
        ySpinBox->setRange(0.0, 1.0);
        ySpinBox->setDecimals(normalizedDecimals);
        ySpinBox->setSingleStep(qPow(10, -normalizedDecimals));
        ySpinBox->setValue(0.5);
        layout->addRow("Y坐标 (归一化):", ySpinBox);
        yInputWidget = ySpinBox;
    } else {
        QSpinBox* xSpinBox = new QSpinBox(&dialog);
        xSpinBox->setRange(0, currentImage.width() - 1);
        xSpinBox->setValue(currentImage.width() / 2);
        layout->addRow("X坐标:", xSpinBox);
        xInputWidget = xSpinBox;

        QSpinBox* ySpinBox = new QSpinBox(&dialog);
        ySpinBox->setRange(0, currentImage.height() - 1);
        ySpinBox->setValue(currentImage.height() / 2);
        layout->addRow("Y坐标:", ySpinBox);
        yInputWidget = ySpinBox;
    }

    // 添加图片尺寸提示
    QLabel* sizeHint = new QLabel(QString("图片尺寸: %1 x %2").arg(currentImage.width()).arg(currentImage.height()), &dialog);
    sizeHint->setStyleSheet("color: gray; font-size: 10px;");
    layout->addRow(sizeHint);

    // 添加按钮
    QDialogButtonBox* buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    layout->addRow(buttonBox);

    connect(buttonBox->button(QDialogButtonBox::Ok), &QPushButton::clicked, &dialog, &QDialog::accept);
    connect(buttonBox->button(QDialogButtonBox::Cancel), &QPushButton::clicked, &dialog, &QDialog::reject);

    // 显示对话框
    if (dialog.exec() == QDialog::Accepted) {
        int x, y;

        if (coordFormat == CoordinateFormat::Normalized) {
            QDoubleSpinBox* xSpin = static_cast<QDoubleSpinBox*>(xInputWidget);
            QDoubleSpinBox* ySpin = static_cast<QDoubleSpinBox*>(yInputWidget);

            double normX = xSpin->value();
            double normY = ySpin->value();

            x = qRound(normX * (currentImage.width() - 1));
            y = qRound(normY * (currentImage.height() - 1));
        } else {
            QSpinBox* xSpin = static_cast<QSpinBox*>(xInputWidget);
            QSpinBox* ySpin = static_cast<QSpinBox*>(yInputWidget);

            x = xSpin->value();
            y = ySpin->value();
        }

        // 验证坐标
        if (x < 0 || x >= currentImage.width() || y < 0 || y >= currentImage.height()) {
            ui->statusbar->showMessage("错误: 坐标超出图片范围", 3000);
            return;
        }

        // 添加检测点
        QColor color = getPixelColor(QPoint(x, y));
        DetectionPoint point;

        if (coordFormat == CoordinateFormat::Normalized) {
            // 归一化模式：归一化坐标为主
            point.normX = double(x) / qMax(1, currentImage.width() - 1);
            point.normY = double(y) / qMax(1, currentImage.height() - 1);
            point.primaryCoord = DetectionPoint::PrimaryNormalized;

            // 缓存像素坐标（用于取色）
            point.x = x;
            point.y = y;
        } else {
            // 像素模式：像素坐标为主
            point.x = x;
            point.y = y;
            point.primaryCoord = DetectionPoint::PrimaryPixel;

            // 计算归一化坐标（缓存）
            point.normX = double(point.x) / qMax(1, currentImage.width() - 1);
            point.normY = double(point.y) / qMax(1, currentImage.height() - 1);
        }

        point.r = color.red();
        point.g = color.green();
        point.b = color.blue();
        point.hasNormalized = true;

        detectionPoints.append(point);
        updatePointsList();
        drawDetectionPoints();
    }
}

void MainWindow::onDeletePointClicked()
{
    QListWidgetItem* item = ui->pointsList->currentItem();
    if (!item) {
        ui->statusbar->showMessage("提示: 请先选择要删除的检测点", 3000);
        return;
    }

    int index = ui->pointsList->row(item);
    if (index >= 0 && index < detectionPoints.size()) {
        auto confirm = QMessageBox::question(this, "确认删除",
            "确定要删除这个检测点吗？",
            QMessageBox::Yes | QMessageBox::No);

        if (confirm == QMessageBox::Yes) {
            detectionPoints.removeAt(index);
            updatePointsList();
            drawDetectionPoints();
        }
    }
}

void MainWindow::onEditPointClicked()
{
    QListWidgetItem* item = ui->pointsList->currentItem();
    if (!item) {
        ui->statusbar->showMessage("提示: 请先选择要编辑的检测点", 3000);
        return;
    }

    // 调用双击事件的处理函数
    onPointsListItemDoubleClicked(item);
}

void MainWindow::onPointsListSelectionChanged()
{
    QList<QListWidgetItem*> selectedItems = ui->pointsList->selectedItems();
    if (selectedItems.isEmpty()) {
        selectedPointIndex = -1;
    } else {
        selectedPointIndex = ui->pointsList->row(selectedItems.first());
    }
    drawDetectionPoints();
}

void MainWindow::updatePointsRGBFromImage()
{
    if (currentImage.isNull()) {
        return;
    }

    for (DetectionPoint& point : detectionPoints) {
        int pixelX, pixelY;

        // 使用归一化坐标计算当前图片上的像素位置
        if (point.hasNormalized) {
            pixelX = qRound(point.normX * qMax(1, currentImage.width() - 1));
            pixelY = qRound(point.normY * qMax(1, currentImage.height() - 1));
            // 更新存储的像素坐标
            point.x = pixelX;
            point.y = pixelY;
        } else {
            // 如果没有归一化坐标，使用存储的像素坐标
            pixelX = point.x;
            pixelY = point.y;
        }

        // 检查坐标是否在图片范围内
        if (pixelX >= 0 && pixelX < currentImage.width() &&
            pixelY >= 0 && pixelY < currentImage.height()) {
            // 从新图片获取该坐标的RGB值
            QColor color = currentImage.pixel(pixelX, pixelY);
            point.r = color.red();
            point.g = color.green();
            point.b = color.blue();
        }
    }

    // 更新显示
    updatePointsList();
}

void MainWindow::onCopyPointClicked()
{
    // 获取当前选中项
    QListWidgetItem* item = ui->pointsList->currentItem();
    if (!item) {
        ui->statusbar->showMessage("提示: 请先选择要复制的检测点", 3000);
        return;
    }

    // 获取选中点的索引
    int index = ui->pointsList->row(item);
    if (index < 0 || index >= detectionPoints.size()) {
        ui->statusbar->showMessage("错误: 检测点索引无效", 3000);
        return;
    }

    // 获取检测点数据
    const DetectionPoint& point = detectionPoints[index];

    // 获取当前颜色格式和坐标格式
    ColorFormat format = getCurrentColorFormat();
    CoordinateFormat coordFormat = getCurrentCoordinateFormat();

    // 准备坐标部分
    QString coordPart;
    if (coordFormat == CoordinateFormat::Normalized) {
        if (currentImage.isNull()) {
            ui->statusbar->showMessage("错误: 请先加载图片", 3000);
            return;
        }

        // 关键修改：优先使用主坐标
        double normX, normY;
        if (point.isNormalizedPrimary()) {
            // 使用存储的归一化坐标（主坐标）
            normX = point.normX;
            normY = point.normY;
        } else {
            // 像素坐标为主，动态计算归一化坐标
            normX = static_cast<double>(point.x) / qMax(1, currentImage.width() - 1);
            normY = static_cast<double>(point.y) / qMax(1, currentImage.height() - 1);
        }

        coordPart = QString("%1,%2").arg(normX, 0, 'f', normalizedDecimals).arg(normY, 0, 'f', normalizedDecimals);
    } else {
        coordPart = QString("%1,%2").arg(point.x).arg(point.y);
    }

    // 根据格式构建不同的复制字符串
    QString copyText;
    switch (format) {
        case ColorFormat::RGB:
            // RGB格式: [x,y,r,g,b]
            copyText = QString("[%1,%2,%3,%4]")
                .arg(coordPart)
                .arg(point.r)
                .arg(point.g)
                .arg(point.b);
            break;
        case ColorFormat::HEX:
            // HEX格式: [x,y,#RRGGBB]
            copyText = QString("[%1,%2]")
                .arg(coordPart)
                .arg(rgbToHex(point.r, point.g, point.b));
            break;
        case ColorFormat::HSL: {
            // HSL格式: [x,y,h,s,l]
            QColor color(point.r, point.g, point.b);
            int h, s, l;
            color.getHsl(&h, &s, &l);
            double sPercent = s / 2.55;
            double lPercent = l / 2.55;
            if (colorDecimals == 0) {
                copyText = QString("[%1,%2,%3,%4]")
                    .arg(coordPart)
                    .arg(h < 0 ? 0 : h)
                    .arg(qRound(sPercent))
                    .arg(qRound(lPercent));
            } else {
                copyText = QString("[%1,%2,%3,%4]")
                    .arg(coordPart)
                    .arg(h < 0 ? 0 : h)
                    .arg(sPercent, 0, 'f', colorDecimals)
                    .arg(lPercent, 0, 'f', colorDecimals);
            }
            break;
        }
        case ColorFormat::HSV: {
            // HSV格式: [x,y,h,s,v]
            QColor color(point.r, point.g, point.b);
            int h, s, v;
            color.getHsv(&h, &s, &v);
            double sPercent = s / 2.55;
            double vPercent = v / 2.55;
            if (colorDecimals == 0) {
                copyText = QString("[%1,%2,%3,%4]")
                    .arg(coordPart)
                    .arg(h < 0 ? 0 : h)
                    .arg(qRound(sPercent))
                    .arg(qRound(vPercent));
            } else {
                copyText = QString("[%1,%2,%3,%4]")
                    .arg(coordPart)
                    .arg(h < 0 ? 0 : h)
                    .arg(sPercent, 0, 'f', colorDecimals)
                    .arg(vPercent, 0, 'f', colorDecimals);
            }
            break;
        }
        case ColorFormat::CMYK: {
            // CMYK格式: [x,y,c,m,y,k]
            QColor color(point.r, point.g, point.b);
            int c, m, y, k;
            color.getCmyk(&c, &m, &y, &k);
            copyText = QString("[%1,%2,%3,%4,%5]")
                .arg(coordPart)
                .arg(c)
                .arg(m)
                .arg(y)
                .arg(k);
            break;
        }
        default:
            // 默认RGB格式
            copyText = QString("[%1,%2,%3,%4]")
                .arg(coordPart)
                .arg(point.r)
                .arg(point.g)
                .arg(point.b);
            break;
    }

    // 复制到剪贴板
    QClipboard* clipboard = QGuiApplication::clipboard();
    clipboard->setText(copyText);

    // 显示成功提示
    ui->statusbar->showMessage(QString("成功: 已复制到剪贴板: %1").arg(copyText), 3000);
}

void MainWindow::onCopyCoordClicked()
{
    // 1. 获取并验证选中项（复用现有逻辑）
    QListWidgetItem* item = ui->pointsList->currentItem();
    if (!item) {
        ui->statusbar->showMessage("提示: 请先选择要复制的检测点", 3000);
        return;
    }

    int index = ui->pointsList->row(item);
    if (index < 0 || index >= detectionPoints.size()) {
        ui->statusbar->showMessage("错误: 检测点索引无效", 3000);
        return;
    }

    const DetectionPoint& point = detectionPoints[index];

    // 2. 获取当前坐标格式
    CoordinateFormat coordFormat = getCurrentCoordinateFormat();

    // 3. 检查归一化坐标是否需要图片
    if (coordFormat == CoordinateFormat::Normalized && currentImage.isNull()) {
        ui->statusbar->showMessage("错误: 请先加载图片", 3000);
        return;
    }

    // 4. 生成格式化的坐标字符串
    QString coordText;
    if (coordFormat == CoordinateFormat::Normalized) {
        // 关键修改：优先使用主坐标
        double normX, normY;
        if (point.isNormalizedPrimary()) {
            // 使用存储的归一化坐标（主坐标）
            normX = point.normX;
            normY = point.normY;
        } else {
            // 像素坐标为主，动态计算归一化坐标
            normX = static_cast<double>(point.x) / qMax(1, currentImage.width() - 1);
            normY = static_cast<double>(point.y) / qMax(1, currentImage.height() - 1);
        }

        coordText = QString("%1, %2")
            .arg(normX, 0, 'f', normalizedDecimals)
            .arg(normY, 0, 'f', normalizedDecimals);
    } else {
        coordText = QString("%1, %2").arg(point.x).arg(point.y);
    }

    // 5. 复制到剪贴板
    QClipboard* clipboard = QGuiApplication::clipboard();
    clipboard->setText(coordText);

    // 6. 显示成功提示
    QString formatDesc = (coordFormat == CoordinateFormat::Normalized) ? "归一化坐标" : "像素坐标";
    ui->statusbar->showMessage(QString("成功: 已复制%1到剪贴板: %2").arg(formatDesc).arg(coordText), 3000);
}

void MainWindow::onCopyColorClicked()
{
    // 1. 获取并验证选中项（复用现有逻辑）
    QListWidgetItem* item = ui->pointsList->currentItem();
    if (!item) {
        ui->statusbar->showMessage("提示: 请先选择要复制的检测点", 3000);
        return;
    }

    int index = ui->pointsList->row(item);
    if (index < 0 || index >= detectionPoints.size()) {
        ui->statusbar->showMessage("错误: 检测点索引无效", 3000);
        return;
    }

    const DetectionPoint& point = detectionPoints[index];

    // 2. 获取当前颜色格式
    ColorFormat colorFormat = getCurrentColorFormat();

    // 3. 使用现有的颜色格式化函数
    QString colorText = formatColorToString(point.r, point.g, point.b, colorFormat);

    // 4. 复制到剪贴板
    QClipboard* clipboard = QGuiApplication::clipboard();
    clipboard->setText(colorText);

    // 5. 显示成功提示
    ui->statusbar->showMessage(QString("成功: 已复制颜色到剪贴板: %1").arg(colorText), 3000);
}

void MainWindow::loadFolder()
{
    QString folderPath = QFileDialog::getExistingDirectory(
        this,
        "选择包含图片的文件夹",
        currentFolderPath.isEmpty() ? "" : currentFolderPath
    );

    if (folderPath.isEmpty()) {
        return;  // 用户取消选择
    }

    if (!loadImageFolder(folderPath)) {
        ui->statusbar->showMessage("警告: 所选文件夹中没有支持的图片文件", 3000);
    }
}

bool MainWindow::loadImageFolder(const QString& folderPath)
{
    // 扫描文件夹中的所有图片文件
    imageFileList.clear();
    QDir folder(folderPath);
    if (!folder.exists()) {
        updateNavigationButtons();
        updateImageCounterDisplay();
        return false;
    }

    QStringList filters;
    filters << "*.png" << "*.jpg" << "*.jpeg" << "*.jfif" << "*.bmp" << "*.webp";
    folder.setNameFilters(filters);

    QFileInfoList fileList = folder.entryInfoList(QDir::Files | QDir::NoDotAndDotDot);

    // 按文件名排序
    std::sort(fileList.begin(), fileList.end(), [](const QFileInfo& a, const QFileInfo& b) {
        return a.fileName().compare(b.fileName(), Qt::CaseInsensitive) < 0;
    });

    for (const QFileInfo& fileInfo : fileList) {
        imageFileList.append(fileInfo.absoluteFilePath());
    }

    if (imageFileList.isEmpty()) {
        updateNavigationButtons();
        updateImageCounterDisplay();
        return false;
    }

    // 保存文件夹路径
    currentFolderPath = folderPath;

    // 加载第一张图片
    currentImageIndex = 0;
    loadImageAtIndex(currentImageIndex);
    return true;
}

void MainWindow::previousImage()
{
    if (imageFileList.isEmpty()) {
        return;
    }

    // 循环浏览：第一张的前一张是最后一张
    currentImageIndex = (currentImageIndex - 1 + imageFileList.size()) % imageFileList.size();
    loadImageAtIndex(currentImageIndex);
}

void MainWindow::nextImage()
{
    if (imageFileList.isEmpty()) {
        return;
    }

    // 循环浏览：最后一张的下一张是第一张
    currentImageIndex = (currentImageIndex + 1) % imageFileList.size();
    loadImageAtIndex(currentImageIndex);
}

void MainWindow::loadImageAtIndex(int index)
{
    if (index < 0 || index >= imageFileList.size()) {
        return;
    }

    // Stop any running animation when switching images
    if (zoomAnimation && zoomAnimation->state() == QPropertyAnimation::Running) {
        zoomAnimation->stop();
    }

    QString fileName = imageFileList[index];

    // 保存当前的滚动条位置和缩放比例
    QScrollBar* hBar = ui->scrollArea->horizontalScrollBar();
    QScrollBar* vBar = ui->scrollArea->verticalScrollBar();
    int oldHScroll = hBar->value();
    int oldVScroll = vBar->value();

    // 加载新图片
    currentImage.load(fileName);
    if (!currentImage.isNull()) {
        // 更新当前索引和文件信息
        currentImageIndex = index;
        currentImageFileName = fileName;
        QFileInfo fileInfo(fileName);
        currentImageFileSize = fileInfo.size();

        ui->imageLabel->setText("");
        updatePointsRGBFromImage();
        updateImageInfoDisplay();
        updateImageCounterDisplay();
        updateImageDisplay();

        // 恢复滚动条位置
        hBar->setValue(qMin(oldHScroll, hBar->maximum()));
        vBar->setValue(qMin(oldVScroll, vBar->maximum()));

        // 更新按钮状态
        updateNavigationButtons();

        // 启用资源管理器和复制图片按钮
        ui->openInExplorerBtn->setEnabled(true);
        ui->selectInExplorerBtn->setEnabled(true);
        ui->copyImageBtn->setEnabled(true);
        ui->copyImageFileBtn->setEnabled(true);
        renameImageShortcut->setEnabled(true);
    } else {
        ui->statusbar->showMessage(QString("错误: 无法加载图片: %1").arg(fileName), 3000);
    }
}

void MainWindow::updateNavigationButtons()
{
    bool hasImages = !imageFileList.isEmpty();
    ui->prevImageBtn->setEnabled(hasImages);
    ui->nextImageBtn->setEnabled(hasImages);
    prevImageShortcut->setEnabled(hasImages);
    nextImageShortcut->setEnabled(hasImages);
}

void MainWindow::updateImageCounterDisplay()
{
    if (imageFileList.isEmpty() || currentImageIndex < 0) {
        ui->imageCounterLabel->setText("-/-");
    } else {
        ui->imageCounterLabel->setText(
            QString("%1/%2").arg(currentImageIndex + 1).arg(imageFileList.size())
        );
    }
}

void MainWindow::setupZoomAnimation()
{
    zoomAnimation = new QPropertyAnimation(this, "currentZoom");
    zoomAnimation->setDuration(200);  // 200ms动画时长
    zoomAnimation->setEasingCurve(QEasingCurve::OutCubic);  // 自然减速效果
}

void MainWindow::setCurrentZoom(double zoom)
{
    if (qFuzzyCompare(m_currentZoom, zoom)) return;

    m_currentZoom = qBound(ZOOM_MIN, zoom, ZOOM_MAX);

    // 更新UI显示
    ui->zoomSlider->blockSignals(true);
    ui->zoomSlider->setValue(static_cast<int>(m_currentZoom * 100));
    ui->zoomSlider->blockSignals(false);
    ui->zoomLabel->setText(formatZoomLabel(m_currentZoom));

    updateImageDisplay();
    emit currentZoomChanged(m_currentZoom);

    // Update minimap zoom
    if (minimapWidget) {
        minimapWidget->setZoom(m_currentZoom);
    }
}

void MainWindow::animatedZoomTo(double targetZoom, const QPoint& centerPos)
{
    targetZoom = qBound(ZOOM_MIN, targetZoom, ZOOM_MAX);

    // 极小差值直接设置
    if (qAbs(targetZoom - m_currentZoom) < 0.01) {
        setCurrentZoom(targetZoom);
        return;
    }

    // 如果动画正在运行，停止旧动画
    if (zoomAnimation->state() == QPropertyAnimation::Running) {
        zoomAnimation->stop();
    }

    // 保存缩放中心点（用于动画过程中调整滚动条）
    QPoint center = centerPos.isNull() ?
        ui->scrollArea->viewport()->mapToGlobal(ui->scrollArea->viewport()->rect().center())
        : centerPos;

    QScrollBar* hBar = ui->scrollArea->horizontalScrollBar();
    QScrollBar* vBar = ui->scrollArea->verticalScrollBar();

    // 计算中心点在原图上的归一化坐标
    QPoint mousePos = ui->scrollArea->mapFromGlobal(center);
    QPixmap pixmap = ui->imageLabel->pixmap();
    int offsetX = 0;
    int offsetY = 0;
    if (!pixmap.isNull()) {
        QSize scaledSize = pixmap.size();
        offsetX = (ui->imageLabel->width() - scaledSize.width()) / 2;
        offsetY = (ui->imageLabel->height() - scaledSize.height()) / 2;
    }

    double imageX = (mousePos.x() + hBar->value() - offsetX) / m_currentZoom;
    double imageY = (mousePos.y() + vBar->value() - offsetY) / m_currentZoom;

    // 配置动画
    zoomAnimation->setStartValue(m_currentZoom);
    zoomAnimation->setEndValue(targetZoom);

    // 动画过程中调整滚动条保持中心点
    disconnect(zoomAnimation, &QPropertyAnimation::valueChanged, nullptr, nullptr);
    connect(zoomAnimation, &QPropertyAnimation::valueChanged, [=](const QVariant& value) {
        double newZoom = value.toDouble();

        // 计算新的滚动条位置
        QPixmap curPixmap = ui->imageLabel->pixmap();
        int newOffsetX = 0;
        int newOffsetY = 0;
        if (!curPixmap.isNull()) {
            QSize newScaledSize = curPixmap.size();
            newOffsetX = (ui->imageLabel->width() - newScaledSize.width()) / 2;
            newOffsetY = (ui->imageLabel->height() - newScaledSize.height()) / 2;
        }

        int newHScroll = static_cast<int>(imageX * newZoom + newOffsetX - mousePos.x());
        int newVScroll = static_cast<int>(imageY * newZoom + newOffsetY - mousePos.y());

        hBar->setValue(qBound(hBar->minimum(), newHScroll, hBar->maximum()));
        vBar->setValue(qBound(vBar->minimum(), newVScroll, vBar->maximum()));

        // Update minimap during zoom animation
        if (minimapWidget) {
            minimapWidget->setZoom(newZoom);
        }
    });

    zoomAnimation->start(QPropertyAnimation::KeepWhenStopped);
}

// DetectionPoint helper methods implementation

void DetectionPoint::ensurePixelCoords(const QSize& imgSize)
{
    if (x == 0 && y == 0 && !isPixelPrimary()) {
        x = qRound(normX * qMax(1, imgSize.width() - 1));
        y = qRound(normY * qMax(1, imgSize.height() - 1));
    }
}

void DetectionPoint::ensureNormalizedCoords(const QSize& imgSize)
{
    if (normX == 0.0 && normY == 0.0 && !isNormalizedPrimary()) {
        normX = double(x) / qMax(1, imgSize.width() - 1);
        normY = double(y) / qMax(1, imgSize.height() - 1);
    }
}

void MainWindow::onOpenInExplorerClicked()
{
    if (currentImageFileName.isEmpty()) {
        ui->statusbar->showMessage("提示: 未加载图片", 3000);
        return;
    }

    QFileInfo fileInfo(currentImageFileName);
    if (!fileInfo.exists()) {
        ui->statusbar->showMessage("错误: 图片文件不存在", 3000);
        return;
    }

#ifdef Q_OS_WIN
    // Windows: 使用 explorer 命令选中文件
    QString command = QString("explorer.exe /select,\"%1\"").arg(currentImageFileName);
    QProcess::startDetached(command);
#elif defined(Q_OS_MACOS)
    // macOS: 使用 open 命令选中文件
    QString command = QString("open -R \"%1\"").arg(currentImageFileName);
    QProcess::startDetached(command);
#else
    // Linux: 打开文件夹
    QDesktopServices::openUrl(QUrl::fromLocalFile(fileInfo.path()));
#endif
}

void MainWindow::onSelectInExplorerClicked()
{
    if (currentImageFileName.isEmpty()) {
        ui->statusbar->showMessage("提示: 未加载图片", 3000);
        return;
    }

    QFileInfo fileInfo(currentImageFileName);
    if (!fileInfo.exists()) {
        ui->statusbar->showMessage("错误: 图片文件不存在", 3000);
        return;
    }

#ifdef Q_OS_WIN
    QString nativePath = QDir::toNativeSeparators(fileInfo.absoluteFilePath());
    QStringList args;
    args << "/select," << nativePath;
    QProcess::startDetached("explorer.exe", args);
#elif defined(Q_OS_MACOS)
    QStringList args;
    args << "-R" << fileInfo.absoluteFilePath();
    QProcess::startDetached("open", args);
#else
    QDesktopServices::openUrl(QUrl::fromLocalFile(fileInfo.absolutePath()));
#endif
}

void MainWindow::onRenameImageFileClicked()
{
    focusImageFileNameEditor();
}

void MainWindow::focusImageFileNameEditor()
{
    if (currentImageFileName.isEmpty()) {
        ui->statusbar->showMessage("提示: 未加载图片", 3000);
        return;
    }

    ui->imageFileBaseNameEdit->setFocus();
    ui->imageFileBaseNameEdit->selectAll();
}

void MainWindow::commitImageFileRenameFromEditors()
{
    if (currentImageFileName.isEmpty()) {
        ui->statusbar->showMessage("提示: 未加载图片", 3000);
        return;
    }

    QFileInfo oldFileInfo(currentImageFileName);
    if (!oldFileInfo.exists()) {
        ui->statusbar->showMessage("错误: 图片文件不存在", 3000);
        return;
    }

    const QString newBaseName = ui->imageFileBaseNameEdit->text().trimmed();
    QString newExtension = ui->imageFileExtensionEdit->text().trimmed();
    while (newExtension.startsWith('.')) {
        newExtension.remove(0, 1);
    }

    if (newBaseName.isEmpty()) {
        QMessageBox::warning(this, "重命名失败", "请输入文件名。");
        return;
    }

    if (newBaseName.contains('/') || newBaseName.contains('\\') ||
        newExtension.contains('/') || newExtension.contains('\\') ||
        QDir::isAbsolutePath(newBaseName) || QDir::isAbsolutePath(newExtension)) {
        QMessageBox::warning(this, "重命名失败", "请输入文件名和扩展名，不要包含路径。");
        return;
    }

#ifdef Q_OS_WIN
    const QString invalidChars = "<>:\"|?*";
    for (const QChar& ch : invalidChars) {
        if (newBaseName.contains(ch) || newExtension.contains(ch)) {
            QMessageBox::warning(this, "重命名失败", "文件名包含 Windows 不允许的字符: < > : \" | ? *");
            return;
        }
    }
#endif

    const QString newFileName = newExtension.isEmpty()
        ? newBaseName
        : QString("%1.%2").arg(newBaseName, newExtension);

    QDir dir(oldFileInfo.absolutePath());
    QString newFilePath = dir.absoluteFilePath(newFileName);
    QFileInfo newFileInfo(newFilePath);

    if (QDir::cleanPath(newFileInfo.absoluteFilePath()) == QDir::cleanPath(oldFileInfo.absoluteFilePath())) {
        ui->imageFileBaseNameEdit->clearFocus();
        ui->imageFileExtensionEdit->clearFocus();
        return;
    }

    if (newFileInfo.exists()) {
        QMessageBox::warning(this, "重命名失败", "目标文件名已存在。");
        return;
    }

    if (!QFile::rename(oldFileInfo.absoluteFilePath(), newFilePath)) {
        QMessageBox::warning(this, "重命名失败", "无法重命名文件，请确认文件未被其他程序占用且当前目录可写。");
        return;
    }

    currentImageFileName = newFilePath;
    currentImageFileSize = QFileInfo(newFilePath).size();

    int renamedIndex = imageFileList.indexOf(oldFileInfo.absoluteFilePath());
    if (renamedIndex >= 0) {
        imageFileList[renamedIndex] = newFilePath;
        currentImageIndex = renamedIndex;
        updateImageCounterDisplay();
        updateNavigationButtons();
    }

    updateImageInfoDisplay();
    ui->imageFileBaseNameEdit->clearFocus();
    ui->imageFileExtensionEdit->clearFocus();
    ui->statusbar->showMessage(QString("成功: 已重命名为 %1").arg(QFileInfo(newFilePath).fileName()), 3000);
}

void MainWindow::onCopyImageClicked()
{
    if (currentImage.isNull()) {
        ui->statusbar->showMessage("提示: 未加载图片", 3000);
        return;
    }

    QClipboard* clipboard = QGuiApplication::clipboard();
    clipboard->setPixmap(QPixmap::fromImage(currentImage));

    ui->statusbar->showMessage("成功: 图片已复制到剪贴板", 3000);
}

void MainWindow::onCopyImageFileClicked()
{
    if (currentImageFileName.isEmpty()) {
        ui->statusbar->showMessage("提示: 未加载图片", 3000);
        return;
    }

    QFileInfo fileInfo(currentImageFileName);
    if (!fileInfo.exists()) {
        ui->statusbar->showMessage("错误: 图片文件不存在", 3000);
        return;
    }

    QList<QUrl> urls;
    urls << QUrl::fromLocalFile(fileInfo.absoluteFilePath());
    QMimeData* mimeData = new QMimeData;
    mimeData->setUrls(urls);
    QGuiApplication::clipboard()->setMimeData(mimeData);

    ui->statusbar->showMessage("成功: 图片文件已复制到剪贴板", 3000);
}

void MainWindow::updatePointsListHeight()
{
    // 根据每行的高度和可见行数设置列表高度
    int rowHeight = 30;  // 默认每行高度
    int totalHeight = pointsListVisibleRows * rowHeight;

    // 如果列表中有项目，动态获取第一项的高度
    if (ui->pointsList->count() > 0) {
        QListWidgetItem* firstItem = ui->pointsList->item(0);
        QRect itemRect = ui->pointsList->visualItemRect(firstItem);
        if (itemRect.height() > 0) {
            rowHeight = itemRect.height();
            totalHeight = pointsListVisibleRows * rowHeight;
        }
    }

    // 设置最小高度，确保至少能显示指定行数
    ui->pointsList->setMinimumHeight(totalHeight);
}

void MainWindow::updateMinimap()
{
    if (!minimapWidget) return;

    if (currentImage.isNull()) {
        minimapWidget->setImage(QImage());
    } else {
        minimapWidget->setImage(currentImage);
        minimapWidget->setZoom(m_currentZoom);
        minimapWidget->reposition();
        minimapWidget->setVisible(ui->showMinimapCheckBox->isChecked());
    }
}
