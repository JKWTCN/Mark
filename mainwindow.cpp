#include "mainwindow.h"
#include "./ui_mainwindow.h"
#include <QFileDialog>
#include <QFileInfo>
#include <QMessageBox>
#include <QInputDialog>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QMimeData>
#include <QPainter>
#include <QPixmap>
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

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    setAcceptDrops(true);

    setupConnections();

    ui->imageLabel->setMouseTracking(true);
    ui->imageLabel->installEventFilter(this);

    // Remove the maximum height constraint from pointsGroup
    ui->pointsGroup->setMaximumSize(16777215, 16777215);

    // Create splitter to make control panel resizable
    splitter = new QSplitter(Qt::Horizontal, this);

    // Get the existing layout and remove widgets from it
    QHBoxLayout* mainLayout = ui->horizontalLayout;
    QWidget* controlPanel = ui->controlPanel;
    QWidget* imageContainer = ui->imageContainer;

    // Remove widgets from the layout
    mainLayout->removeWidget(controlPanel);
    mainLayout->removeWidget(imageContainer);

    // Add widgets to splitter
    splitter->addWidget(controlPanel);
    splitter->addWidget(imageContainer);

    // Set initial sizes - control panel gets 300px, rest goes to image
    splitter->setSizes({300, 900});

    // Set stretch factor so image container gets more space when resizing
    splitter->setStretchFactor(1, 1);

    // Add splitter to the main layout
    mainLayout->addWidget(splitter);
    // Initialize image info display
    updateImageInfoDisplay();
}

MainWindow::~MainWindow()
{
    delete ui;
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

    return QString("HSL(%1°, %2%, %3%)")
        .arg(h == 0 && r == 0 && g == 0 && b == 0 ? 0 : h)
        .arg(qRound(s / 2.55))
        .arg(qRound(l / 2.55));
}

QString MainWindow::rgbToHsv(int r, int g, int b)
{
    QColor color(r, g, b);
    int h, s, v;
    color.getHsv(&h, &s, &v);

    return QString("HSV(%1°, %2%, %3%)")
        .arg(h == 0 && r == 0 && g == 0 && b == 0 ? 0 : h)
        .arg(qRound(s / 2.55))
        .arg(qRound(v / 2.55));
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
    if (maxValue <= 0) return "0.000";
    double normalized = static_cast<double>(pixel) / maxValue;
    return QString::number(normalized, 'f', 3);
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

QString MainWindow::formatFileSize(qint64 bytes)
{
    if (bytes < 1024) {
        return QString("%1 B").arg(bytes);
    } else if (bytes < 1024 * 1024) {
        return QString("%1 KB").arg(bytes / 1024.0, 0, 'f', 2);
    } else if (bytes < 1024 * 1024 * 1024) {
        return QString("%1 MB").arg(bytes / (1024.0 * 1024.0), 0, 'f', 2);
    } else {
        return QString("%1 GB").arg(bytes / (1024.0 * 1024.0 * 1024.0), 0, 'f', 2);
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
            ui->imageFileNameValue->setText(fileInfo.fileName());
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
    connect(ui->coordinateFormatCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this]() {
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
    connect(ui->copyPointBtn, &QPushButton::clicked, this, &MainWindow::onCopyPointClicked);
}

void MainWindow::loadImage()
{
    QString fileName = QFileDialog::getOpenFileName(this, "选择图片", "",
        "Images (*.png *.jpg *.jpeg *.bmp *.webp)");

    if (!fileName.isEmpty()) {
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
                const QPixmap* pixmapPtr = ui->imageLabel->pixmap();
                if (pixmapPtr && !pixmapPtr->isNull()) {
                    QSize scaledSize = pixmapPtr->size();
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
    // 检查是否有图片加载
    if (currentImage.isNull()) {
        QMainWindow::wheelEvent(event);
        return;
    }

    // 检查鼠标是否在 scrollArea 上
    if (!ui->scrollArea->underMouse()) {
        QMainWindow::wheelEvent(event);
        return;
    }

    // 获取滚轮滚动的角度
    QPoint angleDelta = event->angleDelta();
    int delta = angleDelta.y();

    // 如果没有滚动，使用默认行为
    if (delta == 0) {
        QMainWindow::wheelEvent(event);
        return;
    }

    // 计算缩放步长（每次滚动缩放 0.1 倍）
    double zoomStep = 0.1;

    // 根据滚动方向确定是放大还是缩小
    double newZoom;
    if (delta > 0) {
        newZoom = currentZoom + zoomStep;
    } else {
        newZoom = currentZoom - zoomStep;
    }

    // 限制缩放范围：0.1 到 6.0
    newZoom = qBound(0.1, newZoom, 6.0);

    // 如果缩放比例没有变化，直接返回
    if (qFuzzyCompare(newZoom, currentZoom)) {
        return;
    }

    // ========== 以鼠标位置为中心缩放 ==========

    // 1. 获取鼠标在 scrollArea 中的相对位置
    QPoint mousePos = ui->scrollArea->mapFromGlobal(event->globalPosition().toPoint());

    // 2. 获取当前的滚动条位置
    QScrollBar* hBar = ui->scrollArea->horizontalScrollBar();
    QScrollBar* vBar = ui->scrollArea->verticalScrollBar();
    int oldHScroll = hBar->value();
    int oldVScroll = vBar->value();

    // 3. 计算鼠标在图片上的相对位置
    const QPixmap* pixmap = ui->imageLabel->pixmap();
    if (!pixmap || pixmap->isNull()) {
        QMainWindow::wheelEvent(event);
        return;
    }

    // 获取缩放后的图片尺寸
    QSize scaledSize = pixmap->size();

    // 计算 imageLabel 的边距（因为图片居中显示）
    int offsetX = (ui->imageLabel->width() - scaledSize.width()) / 2;
    int offsetY = (ui->imageLabel->height() - scaledSize.height()) / 2;

    // 计算鼠标在图片上的位置（相对于图片左上角）
    double mouseX = mousePos.x() + oldHScroll - offsetX;
    double mouseY = mousePos.y() + oldVScroll - offsetY;

    // 4. 计算新的缩放比例下的滚动条位置
    double zoomRatio = newZoom / currentZoom;
    int newHScroll = static_cast<int>(mouseX * (zoomRatio - 1) + oldHScroll);
    int newVScroll = static_cast<int>(mouseY * (zoomRatio - 1) + oldVScroll);

    // 5. 应用新的缩放比例
    currentZoom = newZoom;

    // 更新滑块和标签显示
    ui->zoomSlider->blockSignals(true);
    ui->zoomSlider->setValue(static_cast<int>(currentZoom * 100));
    ui->zoomSlider->blockSignals(false);
    ui->zoomLabel->setText(QString("缩放: %1%").arg(static_cast<int>(currentZoom * 100)));

    // 6. 更新图片显示
    updateImageDisplay();

    // 7. 设置新的滚动条位置（必须在 updateImageDisplay 之后）
    hBar->setValue(qBound(hBar->minimum(), newHScroll, hBar->maximum()));
    vBar->setValue(qBound(vBar->minimum(), newVScroll, vBar->maximum()));

    // 事件已处理
    event->accept();
}

void MainWindow::addDetectionPoint(const QPoint& pos)
{
    QColor color = getPixelColor(pos);
    DetectionPoint point(pos.x(), pos.y(), color.red(), color.green(), color.blue());
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
    QSize scaledSize = pixmap.size() * currentZoom;
    QPixmap scaledPixmap = pixmap.scaled(scaledSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    ui->imageLabel->setPixmap(scaledPixmap);
    ui->imageLabel->setAlignment(Qt::AlignCenter);
    // Set the label size to match the pixmap size so scrollbars appear when zoomed
    ui->imageLabel->setFixedSize(scaledPixmap.size());
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
        QString coordText = formatCoordinates(point.x, point.y, coordFormat);
        QString text = QString("点 %1: %2 %3")
            .arg(i + 1)
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
            xSpinBox->setDecimals(3);
            xSpinBox->setSingleStep(0.001);
            double normX = static_cast<double>(point.x) / qMax(1, currentImage.width() - 1);
            xSpinBox->setValue(normX);
            layout->addRow("X坐标 (归一化):", xSpinBox);
            xInputWidget = xSpinBox;

            QDoubleSpinBox* ySpinBox = new QDoubleSpinBox(&dialog);
            ySpinBox->setRange(0.0, 1.0);
            ySpinBox->setDecimals(3);
            ySpinBox->setSingleStep(0.001);
            double normY = static_cast<double>(point.y) / qMax(1, currentImage.height() - 1);
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

                newX = qRound(normX * (currentImage.width() - 1));
                newY = qRound(normY * (currentImage.height() - 1));
            } else {
                QSpinBox* xSpin = static_cast<QSpinBox*>(xInputWidget);
                QSpinBox* ySpin = static_cast<QSpinBox*>(yInputWidget);

                newX = xSpin->value();
                newY = ySpin->value();
            }

            // 从图片获取新坐标的RGB颜色
            QColor newColor = getPixelColor(QPoint(newX, newY));

            // 更新检测点
            point.x = newX;
            point.y = newY;
            point.r = newColor.red();
            point.g = newColor.green();
            point.b = newColor.blue();

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
    QJsonArray pointsArray;

    for (const auto& point : detectionPoints) {
        pointsArray.append(point.toJson());
    }
    root["point"] = pointsArray;

    // Save color format preference
    root["colorFormat"] = ui->colorFormatCombo->currentIndex();

    // Save coordinate format preference
    root["coordinateFormat"] = ui->coordinateFormatCombo->currentIndex();

    // Save image dimensions
    if (!currentImage.isNull()) {
        root["imageWidth"] = currentImage.width();
        root["imageHeight"] = currentImage.height();
    }

    // Save screen dimensions
    root["screenWidth"] = ui->screenWidthSpin->value();
    root["screenHeight"] = ui->screenHeightSpin->value();

    // Save config name
    root["configName"] = ui->configNameEdit->text();

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

    if (root.contains("point") && root["point"].isArray()) {
        QJsonArray pointsArray = root["point"].toArray();
        for (const QJsonValue& value : pointsArray) {
            if (value.isArray()) {
                detectionPoints.append(DetectionPoint::fromJson(value.toArray()));
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

    // Load config name (backward compatible - defaults to empty string if not present)
    if (root.contains("configName") && root["configName"].isString()) {
        ui->configNameEdit->setText(root["configName"].toString());
    } else {
        ui->configNameEdit->setText("");
    }

    // Load screen dimensions (backward compatible - defaults if not present)
    if (root.contains("screenWidth") && root["screenWidth"].isDouble()) {
        ui->screenWidthSpin->setValue(root["screenWidth"].toInt());
    }
    if (root.contains("screenHeight") && root["screenHeight"].isDouble()) {
        ui->screenHeightSpin->setValue(root["screenHeight"].toInt());
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
    currentZoom = qBound(0.1, zoom, 6.0);
    ui->zoomSlider->blockSignals(true);
    ui->zoomSlider->setValue(static_cast<int>(currentZoom * 100));
    ui->zoomSlider->blockSignals(false);
    ui->zoomLabel->setText(QString("缩放: %1%").arg(static_cast<int>(currentZoom * 100)));
    updateImageDisplay();
}

void MainWindow::zoomIn()
{
    setZoom(currentZoom + 0.1);
}

void MainWindow::zoomOut()
{
    setZoom(currentZoom - 0.1);
}

void MainWindow::zoomChanged(int value)
{
    currentZoom = value / 100.0;
    ui->zoomLabel->setText(QString("缩放: %1%").arg(value));
    updateImageDisplay();
}

void MainWindow::fitToScreen()
{
    if (currentImage.isNull()) return;

    QSize scrollSize = ui->scrollArea->size();
    QSize imageSize = currentImage.size();

    double scaleX = (double)scrollSize.width() / imageSize.width();
    double scaleY = (double)scrollSize.height() / imageSize.height();
    double fitZoom = qMin(scaleX, scaleY) * 0.9;

    setZoom(fitZoom);
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
        xSpinBox->setDecimals(3);
        xSpinBox->setSingleStep(0.001);
        xSpinBox->setValue(0.5);
        layout->addRow("X坐标 (归一化):", xSpinBox);
        xInputWidget = xSpinBox;

        QDoubleSpinBox* ySpinBox = new QDoubleSpinBox(&dialog);
        ySpinBox->setRange(0.0, 1.0);
        ySpinBox->setDecimals(3);
        ySpinBox->setSingleStep(0.001);
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
        addDetectionPoint(QPoint(x, y));
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
        // 检查坐标是否在图片范围内
        if (point.x >= 0 && point.x < currentImage.width() &&
            point.y >= 0 && point.y < currentImage.height()) {
            // 从新图片获取该坐标的RGB值
            QColor color = currentImage.pixel(point.x, point.y);
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
        double normX = static_cast<double>(point.x) / qMax(1, currentImage.width() - 1);
        double normY = static_cast<double>(point.y) / qMax(1, currentImage.height() - 1);
        coordPart = QString("%1,%2").arg(normX, 0, 'f', 3).arg(normY, 0, 'f', 3);
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
            copyText = QString("[%1,%2,%3,%4]")
                .arg(coordPart)
                .arg(h == 0 && point.r == 0 && point.g == 0 && point.b == 0 ? 0 : h)
                .arg(qRound(s / 2.55))
                .arg(qRound(l / 2.55));
            break;
        }
        case ColorFormat::HSV: {
            // HSV格式: [x,y,h,s,v]
            QColor color(point.r, point.g, point.b);
            int h, s, v;
            color.getHsv(&h, &s, &v);
            copyText = QString("[%1,%2,%3,%4]")
                .arg(coordPart)
                .arg(h == 0 && point.r == 0 && point.g == 0 && point.b == 0 ? 0 : h)
                .arg(qRound(s / 2.55))
                .arg(qRound(v / 2.55));
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
