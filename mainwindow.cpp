#include "mainwindow.h"
#include "./ui_mainwindow.h"
#include "minimapwidget.h"
#include <QFileDialog>
#include <QFileInfo>
#include <QMessageBox>
#include <QInputDialog>
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

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    setAcceptDrops(true);

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

            // 计算缩放步长（每次滚动缩放 0.1 倍）
            double zoomStep = 0.1;

            // 根据滚动方向确定是放大还是缩小
            double newZoom;
            if (delta > 0) {
                newZoom = m_currentZoom + zoomStep;
            } else {
                newZoom = m_currentZoom - zoomStep;
            }

            // 限制缩放范围：0.1 到 6.0
            newZoom = qBound(0.1, newZoom, 6.0);

            // 如果缩放比例没有变化，直接返回
            if (qFuzzyCompare(newZoom, m_currentZoom)) {
                return true;
            }

            // ========== 以鼠标位置为中心缩放 ==========

            // 1. 保存旧的缩放比例
            double oldZoom = m_currentZoom;

            // 2. 获取鼠标在 scrollArea 中的相对位置
            QPoint mousePos = ui->scrollArea->mapFromGlobal(wheelEvent->globalPosition().toPoint());

            // 3. 获取当前的滚动条位置
            QScrollBar* hBar = ui->scrollArea->horizontalScrollBar();
            QScrollBar* vBar = ui->scrollArea->verticalScrollBar();
            int oldHScroll = hBar->value();
            int oldVScroll = vBar->value();

            // 4. 获取当前显示的图片（缩放后的）
            QPixmap pixmap = ui->imageLabel->pixmap();
            if (pixmap.isNull()) {
                return false;
            }

            // 5. 计算鼠标在原图上的位置（归一化坐标，不随缩放改变）
            //    这样可以在缩放后保持鼠标指向的图片内容不变
            QSize scaledSize = pixmap.size();
            int offsetX = (ui->imageLabel->width() - scaledSize.width()) / 2;
            int offsetY = (ui->imageLabel->height() - scaledSize.height()) / 2;

            // 计算鼠标在原图上的坐标（相对于图片左上角，单位：原图像素）
            double imageX = (mousePos.x() + oldHScroll - offsetX) / oldZoom;
            double imageY = (mousePos.y() + oldVScroll - offsetY) / oldZoom;

            // 6. 使用平滑动画应用新的缩放比例
            animatedZoomTo(newZoom, wheelEvent->globalPosition().toPoint());
            return true;
        }

        // 没有按 Ctrl 键，让默认处理（滚动条工作）
        return false;
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
        ui->imageFileNameValue->setText("未加载图片");
        ui->imageFileSizeValue->setText("-");
    } else {
        int width = currentImage.width();
        int height = currentImage.height();

        ui->imageWidthValue->setText(QString::number(width));
        ui->imageHeightValue->setText(QString::number(height));
        ui->imageSizeValue->setText(QString("%1 x %2").arg(width).arg(height));

        if (!currentImageFileName.isEmpty()) {
            QFileInfo fileInfo(currentImageFileName);
            QString fileName = fileInfo.fileName();

            // Elide long filenames (max 40 characters)
            if (fileName.length() > 40) {
                fileName = fileName.left(18) + "..." + fileName.right(19);
            }

            ui->imageFileNameValue->setText(fileName);
            ui->imageFileNameValue->setToolTip(currentImageFileName);

            if (currentImageFileSize > 0) {
                ui->imageFileSizeValue->setText(formatFileSize(currentImageFileSize));
            } else {
                ui->imageFileSizeValue->setText("-");
            }
        } else {
            ui->imageFileNameValue->setText("(拖放加载)");
            ui->imageFileSizeValue->setText("-");
        }
    }
}

void MainWindow::setupConnections()
{
    connect(ui->loadImageBtn, &QPushButton::clicked, this, &MainWindow::loadImage);
    connect(ui->loadConfigBtn, &QPushButton::clicked, this, &MainWindow::loadConfig);
    connect(ui->saveConfigBtn, &QPushButton::clicked, this, &MainWindow::saveConfig);
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
    connect(ui->copyImageBtn, &QPushButton::clicked, this, &MainWindow::onCopyImageClicked);
}

void MainWindow::loadImage()
{
    // Stop any running animation when switching images
    if (zoomAnimation && zoomAnimation->state() == QPropertyAnimation::Running) {
        zoomAnimation->stop();
    }

    QString fileName = QFileDialog::getOpenFileName(this, "选择图片", "",
        "Images (*.png *.jpg *.jpeg *.bmp *.webp)");

    if (!fileName.isEmpty()) {
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

        currentImage.load(fileName);
        if (!currentImage.isNull()) {
            // 保存图片文件信息
            currentImageFileName = fileName;
            QFileInfo fileInfo(fileName);
            currentImageFileSize = fileInfo.size();

            ui->imageLabel->setText("");
            // 保持当前缩放比例，不调用 fitToScreen()
            // 先更新所有检测点的RGB值和坐标
            updatePointsRGBFromImage();
            updateImageInfoDisplay();
            updateImageDisplay();

            // 启用资源管理器和复制图片按钮
            ui->openInExplorerBtn->setEnabled(true);
            ui->copyImageBtn->setEnabled(true);

            // 恢复滚动条位置（尽量恢复）
            hBar->setValue(qMin(oldHScroll, hBar->maximum()));
            vBar->setValue(qMin(oldVScroll, vBar->maximum()));
        } else {
            QMessageBox::warning(this, "错误", "无法加载图片");
        }
    }
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
                QPixmap pixmap = ui->imageLabel->pixmap();
                if (!pixmap.isNull()) {
                    QSize scaledSize = pixmap.size();
                    QSize imageSize = currentImage.size();

                    double scaleX = (double)imageSize.width() / scaledSize.width();
                    double scaleY = (double)imageSize.height() / scaledSize.height();

                    int offsetX = (ui->imageLabel->width() - scaledSize.width()) / 2;
                    int offsetY = (ui->imageLabel->height() - scaledSize.height()) / 2;

                    int imageX = (labelPos.x() - offsetX) * scaleX;
                    int imageY = (labelPos.y() - offsetY) * scaleY;

                    if (imageX >= 0 && imageX < imageSize.width() &&
                        imageY >= 0 && imageY < imageSize.height()) {
                        addDetectionPoint(QPoint(imageX, imageY));
                    }
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

    QImage displayImage = currentImage.copy();
    QPainter painter(&displayImage);

    for (int i = 0; i < detectionPoints.size(); ++i) {
        const auto& point = detectionPoints[i];
        // 选中的点用蓝色，其他点用红色
        if (i == selectedPointIndex) {
            painter.setPen(QPen(Qt::blue, 3));
        } else {
            painter.setPen(QPen(Qt::red, 2));
        }
        painter.drawEllipse(point.x - 2, point.y - 2, 4, 4);
    }

    QPixmap pixmap = QPixmap::fromImage(displayImage);
    QSize scaledSize = pixmap.size() * m_currentZoom;
    QPixmap scaledPixmap = pixmap.scaled(scaledSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    ui->imageLabel->setPixmap(scaledPixmap);
    ui->imageLabel->setAlignment(Qt::AlignCenter);
    // Set the label size to match the pixmap size so scrollbars appear when zoomed
    ui->imageLabel->setFixedSize(scaledPixmap.size());

    // Update minimap
    updateMinimap();
}

void MainWindow::updatePointsList()
{
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
                QMessageBox::warning(this, "错误", "请先加载图片");
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
        event->acceptProposedAction();
    }
}

void MainWindow::dropEvent(QDropEvent *event)
{
    // Stop any running animation when switching images
    if (zoomAnimation && zoomAnimation->state() == QPropertyAnimation::Running) {
        zoomAnimation->stop();
    }

    const QMimeData* mimeData = event->mimeData();
    if (mimeData->hasUrls()) {
        QList<QUrl> urls = mimeData->urls();
        if (!urls.isEmpty()) {
            QString fileName = urls.first().toLocalFile();
            if (fileName.endsWith(".png") || fileName.endsWith(".jpg") ||
                fileName.endsWith(".jpeg") || fileName.endsWith(".bmp") ||
                fileName.endsWith(".webp")) {
                // 保存当前的滚动条位置
                QScrollBar* hBar = ui->scrollArea->horizontalScrollBar();
                QScrollBar* vBar = ui->scrollArea->verticalScrollBar();
                int oldHScroll = hBar->value();
                int oldVScroll = vBar->value();

                currentImage.load(fileName);
                if (!currentImage.isNull()) {
                    // 保存图片文件信息
                    currentImageFileName = fileName;
                    QFileInfo fileInfo(fileName);
                    currentImageFileSize = fileInfo.size();

                    ui->imageLabel->setText("");
                    // 保持当前缩放比例，不调用 fitToScreen()
                    updateImageDisplay();
                    updateImageInfoDisplay();

                    // 更新所有检测点的RGB值
                    updatePointsRGBFromImage();

                    // 启用资源管理器和复制图片按钮
                    ui->openInExplorerBtn->setEnabled(true);
                    ui->copyImageBtn->setEnabled(true);

                    // 恢复滚动条位置（尽量恢复）
                    hBar->setValue(qMin(oldHScroll, hBar->maximum()));
                    vBar->setValue(qMin(oldVScroll, vBar->maximum()));
                }
            }
        }
    }
    event->acceptProposedAction();
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

void MainWindow::saveConfig()
{
    QString configName = ui->configNameEdit->text();
    if (configName.isEmpty()) {
        QMessageBox::warning(this, "错误", "请先输入配置名称");
        return;
    }

    QString fileName = QFileDialog::getSaveFileName(this, "保存配置", configName + ".json", "JSON Files (*.json)");
    if (!fileName.isEmpty()) {
        if (saveJsonConfig(fileName)) {
            QMessageBox::information(this, "成功", "配置已保存");
        } else {
            QMessageBox::warning(this, "错误", "保存配置失败");
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
                QMessageBox::warning(this, "警告",
                    "配置文件中包含超出范围的归一化坐标，已自动修正到 [0, 1] 范围内");
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
        ui->imageFileNameValue->setText("(配置中的尺寸)");
        ui->imageFileSizeValue->setText("-");
    }

    // 如果已加载图片，根据图片更新检测点的RGB值
    if (!currentImage.isNull()) {
        updatePointsRGBFromImage();
    }

    updatePointsList();
    drawDetectionPoints();
    return true;
}

void MainWindow::loadConfig()
{
    QString fileName = QFileDialog::getOpenFileName(this, "加载配置", "", "JSON Files (*.json)");
    if (!fileName.isEmpty()) {
        if (loadJsonConfig(fileName)) {
            QMessageBox::information(this, "成功", "配置已加载");
        } else {
            QMessageBox::warning(this, "错误", "加载配置失败，请检查文件格式");
        }
    }
}

void MainWindow::setZoom(double zoom)
{
    animatedZoomTo(zoom);
}

void MainWindow::zoomIn()
{
    setZoom(m_currentZoom + 0.1);
}

void MainWindow::zoomOut()
{
    setZoom(m_currentZoom - 0.1);
}

void MainWindow::zoomChanged(int value)
{
    m_currentZoom = value / 100.0;
    ui->zoomLabel->setText(QString("缩放: %1%").arg(value));
    updateImageDisplay();
}

void MainWindow::fitToScreen()
{
    if (currentImage.isNull()) return;

    QSize scrollSize = ui->scrollArea->viewport()->size();
    QSize imageSize = currentImage.size();

    double scaleX = (double)scrollSize.width() / imageSize.width();
    double scaleY = (double)scrollSize.height() / imageSize.height();
    double fitZoom = qMin(scaleX, scaleY);

    animatedZoomTo(fitZoom);
}

void MainWindow::actualSize()
{
    setZoom(1.0);
}

void MainWindow::onAddPointManuallyClicked()
{
    if (currentImage.isNull()) {
        QMessageBox::warning(this, "错误", "请先加载图片");
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
            QMessageBox::warning(this, "错误", "坐标超出图片范围");
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
        QMessageBox::warning(this, "提示", "请先选择要删除的检测点");
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
        QMessageBox::warning(this, "提示", "请先选择要编辑的检测点");
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
        QMessageBox::warning(this, "提示", "请先选择要复制的检测点");
        return;
    }

    // 获取选中点的索引
    int index = ui->pointsList->row(item);
    if (index < 0 || index >= detectionPoints.size()) {
        QMessageBox::warning(this, "错误", "检测点索引无效");
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
            QMessageBox::warning(this, "错误", "请先加载图片");
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
    QMessageBox::information(this, "成功", QString("已复制到剪贴板:\n%1").arg(copyText));
}

void MainWindow::onCopyCoordClicked()
{
    // 1. 获取并验证选中项（复用现有逻辑）
    QListWidgetItem* item = ui->pointsList->currentItem();
    if (!item) {
        QMessageBox::warning(this, "提示", "请先选择要复制的检测点");
        return;
    }

    int index = ui->pointsList->row(item);
    if (index < 0 || index >= detectionPoints.size()) {
        QMessageBox::warning(this, "错误", "检测点索引无效");
        return;
    }

    const DetectionPoint& point = detectionPoints[index];

    // 2. 获取当前坐标格式
    CoordinateFormat coordFormat = getCurrentCoordinateFormat();

    // 3. 检查归一化坐标是否需要图片
    if (coordFormat == CoordinateFormat::Normalized && currentImage.isNull()) {
        QMessageBox::warning(this, "错误", "请先加载图片");
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
    QMessageBox::information(this, "成功",
        QString("已复制%1到剪贴板:\n%2").arg(formatDesc).arg(coordText));
}

void MainWindow::onCopyColorClicked()
{
    // 1. 获取并验证选中项（复用现有逻辑）
    QListWidgetItem* item = ui->pointsList->currentItem();
    if (!item) {
        QMessageBox::warning(this, "提示", "请先选择要复制的检测点");
        return;
    }

    int index = ui->pointsList->row(item);
    if (index < 0 || index >= detectionPoints.size()) {
        QMessageBox::warning(this, "错误", "检测点索引无效");
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
    QMessageBox::information(this, "成功",
        QString("已复制颜色到剪贴板:\n%1").arg(colorText));
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

    // 扫描文件夹中的所有图片文件
    imageFileList.clear();
    QDir folder(folderPath);
    QStringList filters;
    filters << "*.png" << "*.jpg" << "*.jpeg" << "*.bmp" << "*.webp";
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
        QMessageBox::warning(this, "警告", "所选文件夹中没有支持的图片文件");
        return;
    }

    // 保存文件夹路径
    currentFolderPath = folderPath;

    // 加载第一张图片
    currentImageIndex = 0;
    loadImageAtIndex(currentImageIndex);
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
        ui->copyImageBtn->setEnabled(true);
    } else {
        QMessageBox::warning(this, "错误", QString("无法加载图片: %1").arg(fileName));
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

    m_currentZoom = qBound(0.1, zoom, 6.0);

    // 更新UI显示
    ui->zoomSlider->blockSignals(true);
    ui->zoomSlider->setValue(static_cast<int>(m_currentZoom * 100));
    ui->zoomSlider->blockSignals(false);
    ui->zoomLabel->setText(QString("缩放: %1%").arg(static_cast<int>(m_currentZoom * 100)));

    updateImageDisplay();
    emit currentZoomChanged(m_currentZoom);

    // Update minimap zoom
    if (minimapWidget) {
        minimapWidget->setZoom(m_currentZoom);
    }
}

void MainWindow::animatedZoomTo(double targetZoom, const QPoint& centerPos)
{
    targetZoom = qBound(0.1, targetZoom, 6.0);

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
    QSize scaledSize = pixmap.size();
    int offsetX = (ui->imageLabel->width() - scaledSize.width()) / 2;
    int offsetY = (ui->imageLabel->height() - scaledSize.height()) / 2;

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
        QSize newScaledSize = ui->imageLabel->pixmap().size();
        int newOffsetX = (ui->imageLabel->width() - newScaledSize.width()) / 2;
        int newOffsetY = (ui->imageLabel->height() - newScaledSize.height()) / 2;

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
        QMessageBox::warning(this, "提示", "未加载图片");
        return;
    }

    QFileInfo fileInfo(currentImageFileName);
    if (!fileInfo.exists()) {
        QMessageBox::warning(this, "错误", "图片文件不存在");
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

void MainWindow::onCopyImageClicked()
{
    if (currentImage.isNull()) {
        QMessageBox::warning(this, "提示", "未加载图片");
        return;
    }

    QClipboard* clipboard = QGuiApplication::clipboard();
    clipboard->setPixmap(QPixmap::fromImage(currentImage));

    QMessageBox::information(this, "成功", "图片已复制到剪贴板");
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
