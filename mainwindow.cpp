#include "mainwindow.h"
#include "./ui_mainwindow.h"
#include <QFileDialog>
#include <QMessageBox>
#include <QMouseEvent>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QMimeData>
#include <QPainter>
#include <QPixmap>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    setAcceptDrops(true);

    currentSkill = "skill1";
    setupConnections();

    ui->imageLabel->setMouseTracking(true);
    ui->imageLabel->installEventFilter(this);
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::setupConnections()
{
    connect(ui->loadImageBtn, &QPushButton::clicked, this, &MainWindow::loadImage);
    connect(ui->loadConfigBtn, &QPushButton::clicked, this, &MainWindow::loadConfig);
    connect(ui->saveConfigBtn, &QPushButton::clicked, this, &MainWindow::saveConfig);
    connect(ui->clearPointsBtn, &QPushButton::clicked, this, &MainWindow::clearCurrentSkillPoints);
    connect(ui->skillCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &MainWindow::onSkillChanged);
    connect(ui->pointsList, &QListWidget::itemDoubleClicked, this, [this](QListWidgetItem *item) {
        this->onPointsListItemDoubleClicked(item);
    });
    connect(ui->zoomInBtn, &QPushButton::clicked, this, &MainWindow::zoomIn);
    connect(ui->zoomOutBtn, &QPushButton::clicked, this, &MainWindow::zoomOut);
    connect(ui->zoomSlider, &QSlider::valueChanged, this, &MainWindow::zoomChanged);
    connect(ui->fitToScreenBtn, &QPushButton::clicked, this, &MainWindow::fitToScreen);
    connect(ui->actualSizeBtn, &QPushButton::clicked, this, &MainWindow::actualSize);
}

QString MainWindow::getCurrentSkill() const
{
    switch (ui->skillCombo->currentIndex()) {
        case 0: return "skill1";
        case 1: return "skill2";
        case 2: return "skill3";
        case 3: return "skill4";
        default: return "skill1";
    }
}

void MainWindow::loadImage()
{
    QString fileName = QFileDialog::getOpenFileName(this, "选择图片", "",
        "Images (*.png *.jpg *.jpeg *.bmp *.webp)");

    if (!fileName.isEmpty()) {
        currentImage.load(fileName);
        if (!currentImage.isNull()) {
            fitToScreen();
            ui->imageLabel->setText("");
        } else {
            QMessageBox::warning(this, "错误", "无法加载图片");
        }
    }
}

void MainWindow::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton && !currentImage.isNull()) {
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
    QMainWindow::mousePressEvent(event);
}

void MainWindow::addDetectionPoint(const QPoint& pos)
{
    QColor color = getPixelColor(pos);
    DetectionPoint point(pos.x(), pos.y(), color.red(), color.green(), color.blue());

    QString skill = getCurrentSkill();
    skillPoints[skill].append(point);

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
    painter.setPen(QPen(Qt::red, 2));

    QString currentSkillStr = getCurrentSkill();
    if (skillPoints.contains(currentSkillStr)) {
        for (const auto& point : skillPoints[currentSkillStr]) {
            painter.drawEllipse(point.x, point.y, 5, 5);
            painter.drawEllipse(point.x, point.y, 10, 10);
        }
    }

    QPixmap pixmap = QPixmap::fromImage(displayImage);
    QSize scaledSize = pixmap.size() * currentZoom;
    QPixmap scaledPixmap = pixmap.scaled(scaledSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    ui->imageLabel->setPixmap(scaledPixmap);
    ui->imageLabel->setAlignment(Qt::AlignCenter);
}

void MainWindow::updatePointsList()
{
    ui->pointsList->clear();
    QString skill = getCurrentSkill();

    if (skillPoints.contains(skill)) {
        const auto& points = skillPoints[skill];
        for (int i = 0; i < points.size(); ++i) {
            const auto& point = points[i];
            QString text = QString("点 %1: (%2, %3) RGB(%4, %5, %6)")
                .arg(i + 1)
                .arg(point.x)
                .arg(point.y)
                .arg(point.r)
                .arg(point.g)
                .arg(point.b);
            ui->pointsList->addItem(text);
        }
    }
}

void MainWindow::onSkillChanged(int index)
{
    currentSkill = getCurrentSkill();
    updatePointsList();
    drawDetectionPoints();
}

void MainWindow::onPointsListItemDoubleClicked(QListWidgetItem *item)
{
    QString skill = getCurrentSkill();
    if (skillPoints.contains(skill)) {
        int index = ui->pointsList->row(item);
        if (index >= 0 && index < skillPoints[skill].size()) {
            auto confirm = QMessageBox::question(this, "确认删除",
                "确定要删除这个检测点吗？",
                QMessageBox::Yes | QMessageBox::No);

            if (confirm == QMessageBox::Yes) {
                skillPoints[skill].removeAt(index);
                updatePointsList();
                drawDetectionPoints();
            }
        }
    }
}

void MainWindow::clearCurrentSkillPoints()
{
    QString skill = getCurrentSkill();
    if (skillPoints.contains(skill) && !skillPoints[skill].isEmpty()) {
        auto confirm = QMessageBox::question(this, "确认清空",
            "确定要清空当前技能的所有检测点吗？",
            QMessageBox::Yes | QMessageBox::No);

        if (confirm == QMessageBox::Yes) {
            skillPoints[skill].clear();
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
                currentImage.load(fileName);
                if (!currentImage.isNull()) {
                    fitToScreen();
                    ui->imageLabel->setText("");
                }
            }
        }
    }
    event->acceptProposedAction();
}

bool MainWindow::saveJsonConfig(const QString& filePath)
{
    QJsonObject root;

    root["ConfigName"] = ui->configNameEdit->text();
    root["screenWidth"] = ui->screenWidthSpin->value();
    root["screenHeight"] = ui->screenHeightSpin->value();

    for (const QString& skill : {"skill1", "skill2", "skill3", "skill4"}) {
        if (skillPoints.contains(skill) && !skillPoints[skill].isEmpty()) {
            QJsonArray pointsArray;
            for (const auto& point : skillPoints[skill]) {
                pointsArray.append(point.toJson());
            }
            root[skill] = pointsArray;
        }
    }

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
    if (error.error != QJsonParseError::NoError) {
        return false;
    }

    if (!doc.isObject()) {
        return false;
    }

    QJsonObject root = doc.object();

    if (root.contains("ConfigName")) {
        ui->configNameEdit->setText(root["ConfigName"].toString());
    }

    if (root.contains("screenWidth")) {
        ui->screenWidthSpin->setValue(root["screenWidth"].toInt());
    }

    if (root.contains("screenHeight")) {
        ui->screenHeightSpin->setValue(root["screenHeight"].toInt());
    }

    skillPoints.clear();

    for (const QString& skill : {"skill1", "skill2", "skill3", "skill4"}) {
        if (root.contains(skill) && root[skill].isArray()) {
            QJsonArray pointsArray = root[skill].toArray();
            QList<DetectionPoint> points;
            for (const QJsonValue& value : pointsArray) {
                if (value.isObject()) {
                    points.append(DetectionPoint::fromJson(value.toObject()));
                }
            }
            skillPoints[skill] = points;
        }
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
    currentZoom = qBound(0.1, zoom, 2.0);
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
