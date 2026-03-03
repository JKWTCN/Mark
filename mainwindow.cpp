#include "mainwindow.h"
#include "./ui_mainwindow.h"
#include <QFileDialog>
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
#include <QLabel>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    setAcceptDrops(true);

    currentSkill = "skill1";
    currentHero = "Phoenix";
    setupConnections();
    loadHeroList();

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
    connect(ui->heroCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &MainWindow::onHeroChanged);
    connect(ui->addHeroBtn, &QPushButton::clicked, this, &MainWindow::onAddHeroClicked);
    connect(ui->deleteHeroBtn, &QPushButton::clicked, this, &MainWindow::onDeleteHeroClicked);
    connect(ui->applyToAllBtn, &QPushButton::clicked, this, &MainWindow::onApplyToAllClicked);
    connect(ui->pointsList, &QListWidget::itemDoubleClicked, this, [this](QListWidgetItem *item) {
        this->onPointsListItemDoubleClicked(item);
    });
    connect(ui->zoomInBtn, &QPushButton::clicked, this, &MainWindow::zoomIn);
    connect(ui->zoomOutBtn, &QPushButton::clicked, this, &MainWindow::zoomOut);
    connect(ui->zoomSlider, &QSlider::valueChanged, this, &MainWindow::zoomChanged);
    connect(ui->fitToScreenBtn, &QPushButton::clicked, this, &MainWindow::fitToScreen);
    connect(ui->actualSizeBtn, &QPushButton::clicked, this, &MainWindow::actualSize);
    connect(ui->addPointManuallyBtn, &QPushButton::clicked, this, &MainWindow::onAddPointManuallyClicked);
    connect(ui->deletePointBtn, &QPushButton::clicked, this, &MainWindow::onDeletePointClicked);
    connect(ui->editPointBtn, &QPushButton::clicked, this, &MainWindow::onEditPointClicked);
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

QString MainWindow::getCurrentHero() const
{
    return ui->heroCombo->currentText();
}

void MainWindow::loadHeroList()
{
    QFile heroFile("D:/Temporary/2602/05/valorant_hero.json");
    if (heroFile.open(QIODevice::ReadOnly)) {
        QByteArray data = heroFile.readAll();
        heroFile.close();

        QJsonParseError error;
        QJsonDocument doc = QJsonDocument::fromJson(data, &error);
        if (error.error == QJsonParseError::NoError && doc.isObject()) {
            QJsonObject root = doc.object();
            if (root.contains("viewMode") && root["viewMode"].isArray()) {
                QJsonArray viewModeArray = root["viewMode"].toArray();
                if (!viewModeArray.isEmpty() && viewModeArray[0].isObject()) {
                    QJsonObject viewModeObj = viewModeArray[0].toObject();
                    if (viewModeObj.contains("hero") && viewModeObj["hero"].isObject()) {
                        QJsonObject heroesObj = viewModeObj["hero"].toObject();
                        QStringList heroes = heroesObj.keys();
                        ui->heroCombo->addItems(heroes);
                        if (!heroes.isEmpty()) {
                            currentHero = heroes.first();
                        }
                    }
                }
            }
        }
    }

    if (ui->heroCombo->count() == 0) {
        ui->heroCombo->addItem("Phoenix");
        currentHero = "Phoenix";
    }

    updateHeroCombo();
}

void MainWindow::updateHeroCombo()
{
    if (heroSkillPoints.isEmpty()) {
        return;
    }

    QString currentText = ui->heroCombo->currentText();
    ui->heroCombo->clear();

    QSet<QString> allHeroes;
    for (int i = 0; i < ui->heroCombo->count(); ++i) {
        allHeroes.insert(ui->heroCombo->itemText(i));
    }

    for (const QString& hero : heroSkillPoints.keys()) {
        if (!allHeroes.contains(hero)) {
            allHeroes.insert(hero);
        }
    }

    QStringList heroList = allHeroes.values();
    std::sort(heroList.begin(), heroList.end());
    ui->heroCombo->addItems(heroList);

    int index = ui->heroCombo->findText(currentText);
    if (index >= 0) {
        ui->heroCombo->setCurrentIndex(index);
    }
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
            ui->imageLabel->setText("");
            // 保持当前缩放比例，不调用 fitToScreen()
            updateImageDisplay();

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

    QString hero = getCurrentHero();
    QString skill = getCurrentSkill();

    if (!heroSkillPoints.contains(hero)) {
        heroSkillPoints[hero] = QMap<QString, QList<DetectionPoint>>();
    }
    heroSkillPoints[hero][skill].append(point);

    skillPoints[skill] = heroSkillPoints[hero][skill];

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

    QString hero = getCurrentHero();
    QString skill = getCurrentSkill();

    if (heroSkillPoints.contains(hero) && heroSkillPoints[hero].contains(skill)) {
        for (const auto& point : heroSkillPoints[hero][skill]) {
            painter.drawEllipse(point.x - 2, point.y - 2, 4, 4);
        }
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
    QString hero = getCurrentHero();
    QString skill = getCurrentSkill();

    if (heroSkillPoints.contains(hero) && heroSkillPoints[hero].contains(skill)) {
        const auto& points = heroSkillPoints[hero][skill];
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

void MainWindow::onHeroChanged(int index)
{
    currentHero = getCurrentHero();

    if (heroSkillPoints.contains(currentHero)) {
        skillPoints = heroSkillPoints[currentHero];
    } else {
        skillPoints.clear();
    }

    updatePointsList();
    drawDetectionPoints();
}

void MainWindow::onPointsListItemDoubleClicked(QListWidgetItem *item)
{
    QString hero = getCurrentHero();
    QString skill = getCurrentSkill();

    if (heroSkillPoints.contains(hero) && heroSkillPoints[hero].contains(skill)) {
        int index = ui->pointsList->row(item);
        if (index >= 0 && index < heroSkillPoints[hero][skill].size()) {
            // 编辑检测点
            DetectionPoint& point = heroSkillPoints[hero][skill][index];

            QDialog dialog(this);
            dialog.setWindowTitle("编辑检测点");

            QFormLayout* layout = new QFormLayout(&dialog);

            // X坐标输入
            QSpinBox* xSpinBox = new QSpinBox(&dialog);
            xSpinBox->setRange(0, currentImage.isNull() ? 1920 : currentImage.width() - 1);
            xSpinBox->setValue(point.x);
            layout->addRow("X坐标:", xSpinBox);

            // Y坐标输入
            QSpinBox* ySpinBox = new QSpinBox(&dialog);
            ySpinBox->setRange(0, currentImage.isNull() ? 1080 : currentImage.height() - 1);
            ySpinBox->setValue(point.y);
            layout->addRow("Y坐标:", ySpinBox);

            // 显示当前RGB颜色（只读）
            QLabel* rgbLabel = new QLabel(QString("RGB(%1, %2, %3)").arg(point.r).arg(point.g).arg(point.b), &dialog);
            rgbLabel->setStyleSheet("color: gray; padding: 5px; background-color: rgb(%1, %2, %3);");
            layout->addRow("当前颜色:", rgbLabel);

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
                int newX = xSpinBox->value();
                int newY = ySpinBox->value();

                // 从图片获取新坐标的RGB颜色
                QColor newColor = getPixelColor(QPoint(newX, newY));

                // 更新检测点
                point.x = newX;
                point.y = newY;
                point.r = newColor.red();
                point.g = newColor.green();
                point.b = newColor.blue();

                skillPoints[skill] = heroSkillPoints[hero][skill];
                updatePointsList();
                drawDetectionPoints();
            }
        }
    }
}

void MainWindow::clearCurrentSkillPoints()
{
    QString hero = getCurrentHero();
    QString skill = getCurrentSkill();

    if (heroSkillPoints.contains(hero) && heroSkillPoints[hero].contains(skill) &&
        !heroSkillPoints[hero][skill].isEmpty()) {
        auto confirm = QMessageBox::question(this, "确认清空",
            "确定要清空当前技能的所有检测点吗？",
            QMessageBox::Yes | QMessageBox::No);

        if (confirm == QMessageBox::Yes) {
            heroSkillPoints[hero][skill].clear();
            skillPoints[skill] = heroSkillPoints[hero][skill];
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
                    ui->imageLabel->setText("");
                    // 保持当前缩放比例，不调用 fitToScreen()
                    updateImageDisplay();

                    // 恢复滚动条位置（尽量恢复）
                    hBar->setValue(qMin(oldHScroll, hBar->maximum()));
                    vBar->setValue(qMin(oldVScroll, vBar->maximum()));
                }
            }
        }
    }
    event->acceptProposedAction();
}

void MainWindow::addHero(const QString& heroName)
{
    if (heroName.isEmpty()) {
        return;
    }

    if (!heroSkillPoints.contains(heroName)) {
        heroSkillPoints[heroName] = QMap<QString, QList<DetectionPoint>>();
    }

    if (ui->heroCombo->findText(heroName) == -1) {
        ui->heroCombo->addItem(heroName);
    }

    ui->heroCombo->setCurrentText(heroName);
}

void MainWindow::deleteHero(const QString& heroName)
{
    if (heroName.isEmpty()) {
        return;
    }

    if (heroSkillPoints.size() <= 1) {
        QMessageBox::warning(this, "错误", "至少需要保留一个英雄配置");
        return;
    }

    auto confirm = QMessageBox::question(this, "确认删除",
        QString("确定要删除英雄 '%1' 的所有配置吗？").arg(heroName),
        QMessageBox::Yes | QMessageBox::No);

    if (confirm == QMessageBox::Yes) {
        heroSkillPoints.remove(heroName);
        ui->heroCombo->removeItem(ui->heroCombo->findText(heroName));

        if (getCurrentHero() == heroName && ui->heroCombo->count() > 0) {
            ui->heroCombo->setCurrentIndex(0);
            onHeroChanged(0);
        }
    }
}

void MainWindow::onAddHeroClicked()
{
    bool ok;
    QString heroName = QInputDialog::getText(this, "添加英雄",
        "请输入英雄名称:", QLineEdit::Normal, "", &ok);

    if (ok && !heroName.isEmpty()) {
        if (ui->heroCombo->findText(heroName) != -1) {
            QMessageBox::warning(this, "错误", "该英雄已存在");
            return;
        }
        addHero(heroName);
    }
}

void MainWindow::onDeleteHeroClicked()
{
    QString heroName = getCurrentHero();
    deleteHero(heroName);
}

void MainWindow::applyToAllHeroes()
{
    QString sourceHero = getCurrentHero();

    if (!heroSkillPoints.contains(sourceHero)) {
        QMessageBox::warning(this, "错误", "当前英雄没有配置数据");
        return;
    }

    const auto& sourceSkills = heroSkillPoints[sourceHero];
    if (sourceSkills.isEmpty()) {
        QMessageBox::warning(this, "错误", "当前英雄没有配置数据");
        return;
    }

    auto confirm = QMessageBox::question(this, "确认应用",
        QString("确定要将当前英雄 '%1' 的配置应用到所有英雄吗？\n这将覆盖其他英雄的现有配置。").arg(sourceHero),
        QMessageBox::Yes | QMessageBox::No);

    if (confirm == QMessageBox::Yes) {
        QStringList allHeroes;
        if (ui->heroCombo->count() > 0) {
            for (int i = 0; i < ui->heroCombo->count(); ++i) {
                allHeroes.append(ui->heroCombo->itemText(i));
            }
        } else {
            allHeroes = heroSkillPoints.keys();
        }

        for (const QString& hero : allHeroes) {
            if (hero != sourceHero) {
                heroSkillPoints[hero] = sourceSkills;
            }
        }

        QMessageBox::information(this, "成功", QString("已将 '%1' 的配置应用到 %2 个英雄").arg(sourceHero).arg(allHeroes.size() - 1));
    }
}

void MainWindow::onApplyToAllClicked()
{
    applyToAllHeroes();
}

bool MainWindow::saveJsonConfig(const QString& filePath)
{
    QJsonObject root;

    root["ConfigName"] = ui->configNameEdit->text();
    root["screenWidth"] = ui->screenWidthSpin->value();
    root["screenHeight"] = ui->screenHeightSpin->value();

    if (!heroSkillPoints.isEmpty()) {
        QJsonObject heroObject;
        for (const QString& hero : heroSkillPoints.keys()) {
            QJsonObject skillObject;
            const auto& skills = heroSkillPoints[hero];

            for (const QString& skill : {"skill1", "skill2", "skill3", "skill4"}) {
                if (skills.contains(skill) && !skills[skill].isEmpty()) {
                    QJsonArray pointsArray;
                    for (const auto& point : skills[skill]) {
                        pointsArray.append(point.toJson());
                    }
                    skillObject[skill] = pointsArray;
                }
            }

            if (!skillObject.isEmpty()) {
                heroObject[hero] = skillObject;
            }
        }

        if (!heroObject.isEmpty()) {
            QJsonObject finalObject;
            finalObject["hero"] = heroObject;
            root = finalObject;
            root["ConfigName"] = ui->configNameEdit->text();
            root["screenWidth"] = ui->screenWidthSpin->value();
            root["screenHeight"] = ui->screenHeightSpin->value();
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

    heroSkillPoints.clear();
    skillPoints.clear();

    if (root.contains("hero") && root["hero"].isObject()) {
        QJsonObject heroObject = root["hero"].toObject();
        for (const QString& hero : heroObject.keys()) {
            if (heroObject[hero].isObject()) {
                QJsonObject skillObject = heroObject[hero].toObject();
                QMap<QString, QList<DetectionPoint>> skillMap;

                for (const QString& skill : {"skill1", "skill2", "skill3", "skill4"}) {
                    if (skillObject.contains(skill) && skillObject[skill].isArray()) {
                        QJsonArray pointsArray = skillObject[skill].toArray();
                        QList<DetectionPoint> points;
                        for (const QJsonValue& value : pointsArray) {
                            if (value.isObject()) {
                                points.append(DetectionPoint::fromJson(value.toObject()));
                            }
                        }
                        skillMap[skill] = points;
                    }
                }

                if (!skillMap.isEmpty()) {
                    heroSkillPoints[hero] = skillMap;
                }
            }
        }
    } else {
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

        if (!skillPoints.isEmpty()) {
            heroSkillPoints[currentHero] = skillPoints;
        }
    }

    if (!heroSkillPoints.isEmpty()) {
        updateHeroCombo();
        QString firstHero = heroSkillPoints.keys().first();
        ui->heroCombo->setCurrentText(firstHero);
        onHeroChanged(ui->heroCombo->currentIndex());
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

    // 创建对话框
    QDialog dialog(this);
    dialog.setWindowTitle("手动添加检测点");

    QFormLayout* layout = new QFormLayout(&dialog);

    // X坐标输入
    QSpinBox* xSpinBox = new QSpinBox(&dialog);
    xSpinBox->setRange(0, currentImage.width() - 1);
    xSpinBox->setValue(currentImage.width() / 2);
    xSpinBox->setPrefix("X: ");
    layout->addRow("X坐标:", xSpinBox);

    // Y坐标输入
    QSpinBox* ySpinBox = new QSpinBox(&dialog);
    ySpinBox->setRange(0, currentImage.height() - 1);
    ySpinBox->setValue(currentImage.height() / 2);
    ySpinBox->setPrefix("Y: ");
    layout->addRow("Y坐标:", ySpinBox);

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
        int x = xSpinBox->value();
        int y = ySpinBox->value();

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

    QString hero = getCurrentHero();
    QString skill = getCurrentSkill();

    if (heroSkillPoints.contains(hero) && heroSkillPoints[hero].contains(skill)) {
        int index = ui->pointsList->row(item);
        if (index >= 0 && index < heroSkillPoints[hero][skill].size()) {
            auto confirm = QMessageBox::question(this, "确认删除",
                "确定要删除这个检测点吗？",
                QMessageBox::Yes | QMessageBox::No);

            if (confirm == QMessageBox::Yes) {
                heroSkillPoints[hero][skill].removeAt(index);
                skillPoints[skill] = heroSkillPoints[hero][skill];
                updatePointsList();
                drawDetectionPoints();
            }
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
