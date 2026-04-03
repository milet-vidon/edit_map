#include <QApplication>
#include <QMainWindow>
#include <QWidget>
#include <QImage>
#include <QPainter>
#include <QMouseEvent>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QRadioButton>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QSlider>
#include <QCheckBox>
#include <QLabel>
#include <QScrollArea>
#include <QFileDialog>
#include <QMessageBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QComboBox>
#include <QFile>
#include <QTextStream>
#include <QFileInfo>
#include <QStringList>
#include <QStatusBar>   // 修复不完整的类型
#include <QAction>      // 修复不完整的类型
#include <QByteArray>
#include <deque>
#include <map>
#include <algorithm>
#include <cctype>

const int FREE_VAL = 254;
const int UNKNOWN_VAL = 205;
const int OCCUPIED_VAL = 0;
const int MAX_HISTORY = 20;

// 默认地图元数据
std::map<QString, QString> DEFAULT_MAP_METADATA = {
    {"resolution", "0.01"},
    {"origin", "[0.0, 0.0, 0.0]"},
    {"negate", "0"},
    {"occupied_thresh", "0.65"},
    {"free_thresh", "0.196"},
    {"mode", "trinary"}
};

// ================= 底层 PGM 解析器 (防 Qt 插件缺失) =================
QImage loadPgmFailsafe(const QString& path) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) return QImage();
    QByteArray data = file.readAll();
    int pos = 0;

    // 辅助函数：跳过空格和注释读取下一个 token
    auto nextToken = [&]() -> QByteArray {
        while (pos < data.size() && std::isspace(data[pos])) pos++;
        while (pos < data.size() && data[pos] == '#') {
            while (pos < data.size() && data[pos] != '\n') pos++;
            while (pos < data.size() && std::isspace(data[pos])) pos++;
        }
        int start = pos;
        while (pos < data.size() && !std::isspace(data[pos])) pos++;
        return data.mid(start, pos - start);
    };

    QByteArray magic = nextToken();
    if (magic != "P2" && magic != "P5") return QImage(); // 不是 PGM 格式

    int width = nextToken().toInt();
    int height = nextToken().toInt();
    int maxval = nextToken().toInt();

    if (width <= 0 || height <= 0 || maxval <= 0) return QImage();

    QImage img(width, height, QImage::Format_Grayscale8);

    if (magic == "P5") {
        // 二进制 PGM (ROS map_saver 默认格式)
        if (pos < data.size() && std::isspace(data[pos])) pos++; // 跳过 maxval 后的一个换行符
        int expected = width * height;
        if (data.size() - pos >= expected) {
            memcpy(img.bits(), data.constData() + pos, expected);
        }
    } else {
        // ASCII PGM (P2)
        for (int y = 0; y < height; ++y) {
            uchar* line = img.scanLine(y);
            for (int x = 0; x < width; ++x) {
                line[x] = nextToken().toInt();
            }
        }
    }
    return img;
}

// ================= 画布组件 =================
class MapCanvas : public QWidget {
    Q_OBJECT
public:
    MapCanvas(QWidget* parent = nullptr) : QWidget(parent) {
        setMouseTracking(true); 
    }

    void updateCanvasSize() {
        if (!mapImage.isNull()) {
            // 【关键修复】：强制设置画布固定大小，这样 QScrollArea 才会正确撑开并显示地图
            setFixedSize(mapImage.width() * zoom, mapImage.height() * zoom);
        }
    }

    void setImage(const QImage& img) {
        mapImage = img.convertToFormat(QImage::Format_Grayscale8);
        updateCanvasSize();
        update();
    }

    QImage getImage() const { return mapImage; }

    void setZoom(int z) {
        zoom = std::max(2, z);
        updateCanvasSize();
        update();
    }

    void setGrid(bool show) { showGrid = show; update(); }
    void setBrushSize(int size) { brushSize = size; }
    void setBrushValue(int val) { brushValue = val; }

signals:
    void statusUpdated(const QString& msg);
    void paintStarted();

protected:
    void paintEvent(QPaintEvent* event) override {
        if (mapImage.isNull()) return;

        QPainter painter(this);
        painter.setRenderHint(QPainter::SmoothPixmapTransform, false);
        QRect targetRect(0, 0, mapImage.width() * zoom, mapImage.height() * zoom);
        
        // 绘制图像
        painter.drawImage(targetRect, mapImage);

        // 绘制网格
        if (showGrid && zoom >= 6) {
            painter.setPen(QColor(120, 170, 255));
            for (int x = 0; x <= targetRect.width(); x += zoom) {
                painter.drawLine(x, 0, x, targetRect.height());
            }
            for (int y = 0; y <= targetRect.height(); y += zoom) {
                painter.drawLine(0, y, targetRect.width(), y);
            }
        }
    }

    void mousePressEvent(QMouseEvent* event) override {
        if (event->button() == Qt::LeftButton && !mapImage.isNull()) {
            isPainting = true;
            emit paintStarted(); 
            paintAt(event->pos());
        }
    }

    void mouseMoveEvent(QMouseEvent* event) override {
        if (mapImage.isNull()) return;

        int px = event->x() / zoom;
        int py = event->y() / zoom;

        if (px >= 0 && py >= 0 && px < mapImage.width() && py < mapImage.height()) {
            int val = qGray(mapImage.pixel(px, py));
            emit statusUpdated(QString("pixel=(%1, %2) value=%3 | 1:Obstacle, 2:Free, 3:Unknown, Ctrl+S:Save, Ctrl+Z:Undo")
                               .arg(px).arg(py).arg(val));
        }

        if (isPainting && (event->buttons() & Qt::LeftButton)) {
            paintAt(event->pos());
        }
    }

    void mouseReleaseEvent(QMouseEvent* event) override {
        if (event->button() == Qt::LeftButton) {
            isPainting = false;
        }
    }

private:
    void paintAt(const QPoint& pos) {
        int px = pos.x() / zoom;
        int py = pos.y() / zoom;
        int radius = std::max(0, brushSize - 1);

        QPainter imgPainter(&mapImage);
        imgPainter.fillRect(px - radius, py - radius, radius * 2 + 1, radius * 2 + 1, 
                            QColor(brushValue, brushValue, brushValue));
        
        update();
    }

    QImage mapImage;
    int zoom = 8;
    bool showGrid = true;
    int brushSize = 3;
    int brushValue = OCCUPIED_VAL;
    bool isPainting = false;
};

// ================= 主窗口 =================
class MapEditor : public QMainWindow {
    Q_OBJECT
public:
    MapEditor(QWidget* parent = nullptr) : QMainWindow(parent) {
        setWindowTitle("PGM Map Editor (C++ Extreme Edition)");
        resize(1280, 900);
        mapMetadata = DEFAULT_MAP_METADATA;

        buildUI();
        bindShortcuts();
        statusLabel->setText("Load a PGM map or create a new map to start editing.");
    }

    void loadMap(const QString& path) {
        // 先尝试用我们自己的暴力解码器读取，失败的话再退回到 Qt 自带的加载器
        QImage img = loadPgmFailsafe(path);
        if (img.isNull()) {
            if (!img.load(path)) {
                QMessageBox::critical(this, "Open failed", "Could not open map: " + path);
                return;
            }
        }
        
        mapPath = path;
        loadMetadata(path);
        canvas->setImage(img);
        history.clear();
        
        statusLabel->setText(QString("Loaded %1 | size=%2x%3px | Left drag to paint, 1/2/3 to switch brush.")
                             .arg(path).arg(img.width()).arg(img.height()));
    }

private slots:
    void createNewMap() {
        QDialog dialog(this);
        dialog.setWindowTitle("Create Map");

        QFormLayout* formLayout = new QFormLayout(&dialog);

        QSpinBox* widthSpin = new QSpinBox(&dialog);
        widthSpin->setRange(1, 20000);
        widthSpin->setValue(500);
        formLayout->addRow("Width (px):", widthSpin);

        QSpinBox* heightSpin = new QSpinBox(&dialog);
        heightSpin->setRange(1, 20000);
        heightSpin->setValue(500);
        formLayout->addRow("Height (px):", heightSpin);

        QDoubleSpinBox* resolutionSpin = new QDoubleSpinBox(&dialog);
        resolutionSpin->setRange(0.001, 10.0);
        resolutionSpin->setDecimals(3);
        resolutionSpin->setSingleStep(0.01);
        resolutionSpin->setValue(mapMetadata["resolution"].toDouble());
        formLayout->addRow("Resolution (m/px):", resolutionSpin);

        QComboBox* fillCombo = new QComboBox(&dialog);
        fillCombo->addItem("Free (254)", FREE_VAL);
        fillCombo->addItem("Unknown (205)", UNKNOWN_VAL);
        fillCombo->addItem("Obstacle (0)", OCCUPIED_VAL);
        formLayout->addRow("Initial value:", fillCombo);

        QDialogButtonBox* buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
        formLayout->addRow(buttonBox);

        connect(buttonBox, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
        connect(buttonBox, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);

        if (dialog.exec() != QDialog::Accepted) return;

        const int width = widthSpin->value();
        const int height = heightSpin->value();
        const int fillValue = fillCombo->currentData().toInt();

        QImage img(width, height, QImage::Format_Grayscale8);
        img.fill(fillValue);

        mapPath.clear();
        mapMetadata = DEFAULT_MAP_METADATA;
        mapMetadata["resolution"] = QString::number(resolutionSpin->value(), 'f', 3);
        history.clear();
        canvas->setImage(img);

        statusLabel->setText(QString("Created new map | size=%1x%2px | resolution=%3 m/px | Save As to export.")
                             .arg(width).arg(height).arg(mapMetadata["resolution"]));
    }

    void openMap() {
        QString path = QFileDialog::getOpenFileName(this, "Open PGM map", "", "PGM map (*.pgm);;All files (*.*)");
        if (!path.isEmpty()) loadMap(path);
    }

    void reloadMap() {
        if (!mapPath.isEmpty()) loadMap(mapPath);
    }

    void saveMapAs() {
        if (canvas->getImage().isNull()) return;

        QString suggested = mapPath.isEmpty() ? "edited_map.pgm" : QFileInfo(mapPath).fileName();
        QString path = QFileDialog::getSaveFileName(this, "Save PGM map", suggested, "PGM map (*.pgm)");
        
        if (path.isEmpty()) return;

        if (!path.endsWith(".pgm", Qt::CaseInsensitive)) path += ".pgm";
        
        savePgm(path, canvas->getImage());
        
        QString yamlPath = path;
        yamlPath.replace(".pgm", ".yaml");
        saveYaml(yamlPath, QFileInfo(path).fileName());

        mapPath = path;
        statusLabel->setText(QString("Saved %1 and %2.").arg(path).arg(yamlPath));
    }

    void undo() {
        if (history.empty()) return;
        canvas->setImage(history.back());
        history.pop_back();
        statusLabel->setText("Undo applied.");
    }

    void pushHistory() {
        if (history.size() >= MAX_HISTORY) {
            history.pop_front();
        }
        history.push_back(canvas->getImage().copy());
    }

    void setBrushModeOccupied() { rbOccupied->setChecked(true); updateBrush(); }
    void setBrushModeFree() { rbFree->setChecked(true); updateBrush(); }
    void setBrushModeUnknown() { rbUnknown->setChecked(true); updateBrush(); }
    void toggleGrid() { chkGrid->setChecked(!chkGrid->isChecked()); updateGrid(); }

private:
    void buildUI() {
        QWidget* centralWidget = new QWidget(this);
        setCentralWidget(centralWidget);
        QVBoxLayout* mainLayout = new QVBoxLayout(centralWidget);

        QHBoxLayout* controls = new QHBoxLayout();
        
        QPushButton* btnNew = new QPushButton("New Map");
        QPushButton* btnOpen = new QPushButton("Open PGM");
        QPushButton* btnSave = new QPushButton("Save PGM As");
        QPushButton* btnUndo = new QPushButton("Undo");
        QPushButton* btnReload = new QPushButton("Reload");
        
        controls->addWidget(btnNew);
        controls->addWidget(btnOpen);
        controls->addWidget(btnSave);
        controls->addWidget(btnUndo);
        controls->addWidget(btnReload);

        rbOccupied = new QRadioButton("Obstacle");
        rbFree = new QRadioButton("Free");
        rbUnknown = new QRadioButton("Unknown");
        rbOccupied->setChecked(true);
        controls->addWidget(rbOccupied);
        controls->addWidget(rbFree);
        controls->addWidget(rbUnknown);

        controls->addWidget(new QLabel("Brush:"));
        spinBrushSize = new QSpinBox();
        spinBrushSize->setRange(1, 25);
        spinBrushSize->setValue(3);
        controls->addWidget(spinBrushSize);

        controls->addWidget(new QLabel("Zoom:"));
        sliderZoom = new QSlider(Qt::Horizontal);
        sliderZoom->setRange(2, 20);
        sliderZoom->setValue(8);
        controls->addWidget(sliderZoom);

        chkGrid = new QCheckBox("Grid");
        chkGrid->setChecked(true);
        controls->addWidget(chkGrid);
        
        controls->addStretch();
        controls->addWidget(new QLabel("Values: obstacle=0  free=254  unknown=205"));

        mainLayout->addLayout(controls);

        QScrollArea* scrollArea = new QScrollArea();
        scrollArea->setBackgroundRole(QPalette::Dark);
        scrollArea->setAlignment(Qt::AlignCenter);
        canvas = new MapCanvas();
        scrollArea->setWidget(canvas);
        mainLayout->addWidget(scrollArea, 1);

        statusLabel = new QLabel();
        statusBar()->addWidget(statusLabel);

        connect(btnNew, &QPushButton::clicked, this, &MapEditor::createNewMap);
        connect(btnOpen, &QPushButton::clicked, this, &MapEditor::openMap);
        connect(btnSave, &QPushButton::clicked, this, &MapEditor::saveMapAs);
        connect(btnUndo, &QPushButton::clicked, this, &MapEditor::undo);
        connect(btnReload, &QPushButton::clicked, this, &MapEditor::reloadMap);

        connect(rbOccupied, &QRadioButton::toggled, this, &MapEditor::updateBrush);
        connect(rbFree, &QRadioButton::toggled, this, &MapEditor::updateBrush);
        connect(rbUnknown, &QRadioButton::toggled, this, &MapEditor::updateBrush);
        
        connect(spinBrushSize, QOverload<int>::of(&QSpinBox::valueChanged), 
                [this](int val) { canvas->setBrushSize(val); });
        
        connect(sliderZoom, &QSlider::valueChanged, 
                [this](int val) { canvas->setZoom(val); });
                
        connect(chkGrid, &QCheckBox::toggled, this, &MapEditor::updateGrid);

        connect(canvas, &MapCanvas::paintStarted, this, &MapEditor::pushHistory);
        connect(canvas, &MapCanvas::statusUpdated, [this](const QString& msg){ statusLabel->setText(msg); });
    }

    void bindShortcuts() {
        QAction* actNew = new QAction(this); actNew->setShortcut(QKeySequence("Ctrl+N"));
        connect(actNew, &QAction::triggered, this, &MapEditor::createNewMap);
        addAction(actNew);

        QAction* actOpen = new QAction(this); actOpen->setShortcut(QKeySequence("Ctrl+O"));
        connect(actOpen, &QAction::triggered, this, &MapEditor::openMap);
        addAction(actOpen);

        QAction* actSave = new QAction(this); actSave->setShortcut(QKeySequence("Ctrl+S"));
        connect(actSave, &QAction::triggered, this, &MapEditor::saveMapAs);
        addAction(actSave);

        QAction* actUndo = new QAction(this); actUndo->setShortcut(QKeySequence("Ctrl+Z"));
        connect(actUndo, &QAction::triggered, this, &MapEditor::undo);
        addAction(actUndo);

        QAction* actKey1 = new QAction(this); actKey1->setShortcut(QKeySequence("1"));
        connect(actKey1, &QAction::triggered, this, &MapEditor::setBrushModeOccupied);
        addAction(actKey1);

        QAction* actKey2 = new QAction(this); actKey2->setShortcut(QKeySequence("2"));
        connect(actKey2, &QAction::triggered, this, &MapEditor::setBrushModeFree);
        addAction(actKey2);

        QAction* actKey3 = new QAction(this); actKey3->setShortcut(QKeySequence("3"));
        connect(actKey3, &QAction::triggered, this, &MapEditor::setBrushModeUnknown);
        addAction(actKey3);

        QAction* actGrid = new QAction(this); actGrid->setShortcut(QKeySequence("G"));
        connect(actGrid, &QAction::triggered, this, &MapEditor::toggleGrid);
        addAction(actGrid);
    }

    void updateBrush() {
        if (rbOccupied->isChecked()) canvas->setBrushValue(OCCUPIED_VAL);
        else if (rbFree->isChecked()) canvas->setBrushValue(FREE_VAL);
        else canvas->setBrushValue(UNKNOWN_VAL);
    }

    void updateGrid() {
        canvas->setGrid(chkGrid->isChecked());
    }

    void savePgm(const QString& path, const QImage& image) {
        QFile file(path);
        if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) return;
        QTextStream out(&file);
        
        out << "P2\n# Saved by C++ MapEditor\n" << image.width() << " " << image.height() << "\n255\n";
        for (int y = 0; y < image.height(); ++y) {
            for (int x = 0; x < image.width(); ++x) {
                out << qGray(image.pixel(x, y)) << " ";
            }
            out << "\n";
        }
    }

    void loadMetadata(const QString& pgmPath) {
        mapMetadata = DEFAULT_MAP_METADATA;
        QString yamlPath = QString(pgmPath).replace(".pgm", ".yaml");
        QFile file(yamlPath);
        
        if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            QTextStream in(&file);
            while (!in.atEnd()) {
                QString line = in.readLine();
                int colonIdx = line.indexOf(':');
                if (colonIdx != -1) {
                    QString key = line.left(colonIdx).trimmed();
                    QString value = line.mid(colonIdx + 1).trimmed();
                    if (mapMetadata.find(key) != mapMetadata.end()) {
                        mapMetadata[key] = value;
                    }
                }
            }
        }
    }

    void saveYaml(const QString& yamlPath, const QString& imageName) {
        QFile file(yamlPath);
        if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) return;
        QTextStream out(&file);
        
        out << "image: " << imageName << "\n"
            << "resolution: " << mapMetadata["resolution"] << "\n"
            << "origin: " << mapMetadata["origin"] << "\n"
            << "negate: " << mapMetadata["negate"] << "\n"
            << "occupied_thresh: " << mapMetadata["occupied_thresh"] << "\n"
            << "free_thresh: " << mapMetadata["free_thresh"] << "\n"
            << "mode: " << mapMetadata["mode"] << "\n";
    }

    QString mapPath;
    std::map<QString, QString> mapMetadata;
    std::deque<QImage> history;
    
    MapCanvas* canvas;
    QLabel* statusLabel;
    QRadioButton *rbOccupied, *rbFree, *rbUnknown;
    QSpinBox* spinBrushSize;
    QSlider* sliderZoom;
    QCheckBox* chkGrid;
};

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);
    MapEditor editor;
    editor.show();
    return app.exec();
}

#include "main.moc"
