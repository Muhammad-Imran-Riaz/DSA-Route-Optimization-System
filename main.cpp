#include <QApplication>
#include <QMainWindow>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFileDialog>
#include <QGraphicsView>
#include <QGraphicsScene>
#include <QGraphicsEllipseItem>
#include <QGraphicsPathItem>
#include <QProgressBar>
#include <QPropertyAnimation>
#include <QGraphicsOpacityEffect>
#include <QParallelAnimationGroup>
#include <QEasingCurve>
#include <QScreen>
#include <QLabel>
#include <QStackedWidget>
#include <QKeyEvent>
#include <QTableWidget>
#include <QHeaderView>
#include <QTimer>
#include <QPainter>
#include <QPainterPath>
#include <QGraphicsBlurEffect>
#include <fstream>
#include <cmath>
#include <vector>
#include <algorithm>
#include <chrono>
#include <random>
#include <sstream>

using namespace std;
using namespace std::chrono;

// Class to manage the core logic, data structures, and pathfinding algorithms
class RouteOptimizer {
public:
    // 2D vector acting as a dynamic adjacency matrix to store distances between cities
    vector<vector<int>> adjMatrix;
    // Total number of cities loaded from the file
    int numCities = 0;
    // Vector serving as an array to keep track of the final calculated sequence of cities
    vector<int> lastPath;
    // Variable to store the total travel cost of the last calculated path
    int lastCost = 0;
    // Variable to store the execution time of the algorithm
    double lastTime = 0.0;
    // Qt list to store the names of the cities
    QStringList cityNames;

    // Fallback list of city names in case the input file only contains numbers
    const QStringList fallbackCities = {
        "FATEH JANG", "ISLAMABAD", "LAHORE", "KARACHI", "PESHAWAR",
        "QUETTA", "MULTAN", "FAISALABAD", "NAROWAL", "GUJRAT",
        "SARGODHA", "BAHAWALPUR", "SUKKUR", "JHANG", "SHEIKHUPURA",
        "RAHIM YAR KHAN", "GUJRANWALA", "MARDAN", "KASUR", "D.G. KHAN"
    };

    RouteOptimizer() {}

    // Function to read the adjacency matrix and city names from a text file
    void load(string filename) {
        ifstream file(filename);
        if (!file) return;

        // Clear existing data before loading new data
        adjMatrix.clear();
        cityNames.clear();

        string line;
        bool hasStringHeader = false;

        // Read the first line to check if it contains city headers (text) or just matrix numbers
        if (getline(file, line)) {
            stringstream ss(line);
            string token;
            if (ss >> token) {
                // Check if the first word contains letters
                for (char c : token) {
                    if (isalpha(c)) {
                        hasStringHeader = true;
                        break;
                    }
                }
            }

            stringstream ssNames(line);
            if (hasStringHeader) {
                // If text is found, extract city names and replace underscores with spaces
                string cityName;
                while (ssNames >> cityName) {
                    QString formattedName = QString::fromStdString(cityName).replace("_", " ");
                    cityNames.append(formattedName.toUpper());
                }
            } else {
                // If no text, treat the first line as the first row of distances
                vector<int> row;
                int val;
                stringstream ssMatrix(line);
                while (ssMatrix >> val) {
                    row.push_back(val);
                }
                if (!row.empty()) adjMatrix.push_back(row);
            }
        }

        // Loop through the rest of the lines to build the 2D matrix
        while (getline(file, line)) {
            if (line.empty()) continue;
            vector<int> row;
            int val;
            stringstream ssMatrix(line);
            while (ssMatrix >> val) {
                row.push_back(val);
            }
            if (!row.empty()) adjMatrix.push_back(row);
        }

        // Set total cities based on the number of rows in the matrix
        numCities = adjMatrix.size();

        // Assign fallback generic names if the file lacked a string header
        if (!hasStringHeader || cityNames.isEmpty()) {
            for (int i = 0; i < numCities; i++) {
                if (i < fallbackCities.size()) {
                    cityNames.append(fallbackCities[i]);
                } else {
                    cityNames.append(QString("CITY %1").arg(i + 1));
                }
            }
        }

        // Pad the list with generic names if there are more cities than names provided
        while (cityNames.size() < numCities) {
            cityNames.append(QString("CITY %1").arg(cityNames.size() + 1));
        }
        file.close();
    }

    // Function to calculate the total distance (cost) of a specific route array
    int calculateCost(const vector<int>& path) {
        int cost = 0;
        if (path.empty()) return 1e7; // Return extremely high cost if path is empty

        // Sum up the values from the adjacency matrix for consecutive cities
        for (size_t i = 0; i < path.size() - 1; i++) {
            if (adjMatrix[path[i]][path[i+1]] == 0) return 1e7; // Penalize invalid connections
            cost += adjMatrix[path[i]][path[i+1]];
        }
        return cost;
    }

    // Helper function to count the total number of connected edges in the graph
    int countEdges() {
        int count = 0;
        for (int i = 0; i < numCities; i++)
            for (int j = 0; j < numCities; j++)
                if (adjMatrix[i][j] > 0) count++;
        return count;
    }

    // Nearest Neighbor (Greedy) Algorithm - Fast but not always perfectly optimal
    void runGreedy() {
        auto start = high_resolution_clock::now();
        if (numCities < 2) return;

        lastPath.clear();
        // Array to track visited cities to avoid infinite loops
        vector<bool> visited(numCities, false);
        int current = 0; lastCost = 0;

        // Start from city 0
        visited[0] = true; lastPath.push_back(0);

        // Loop to find the closest unvisited city at every step
        for (int i = 1; i < numCities; i++) {
            int nearest = -1, minDist = 1e7;
            for (int next = 0; next < numCities; next++) {
                if (!visited[next] && adjMatrix[current][next] > 0 && adjMatrix[current][next] < minDist) {
                    minDist = adjMatrix[current][next];
                    nearest = next;
                }
            }
            // Move to the nearest city and mark it visited
            if (nearest != -1) { current = nearest; visited[current] = true; lastPath.push_back(current); lastCost += minDist; }
        }

        // Complete the loop by returning to the starting point
        lastPath.push_back(0);
        lastCost += adjMatrix[current][0];

        auto stop = high_resolution_clock::now();
        lastTime = duration<double, milli>(stop - start).count();
    }

    // Exact approach testing every possible sequence - Guarantees best path but takes a lot of time
    void runBruteForce() {
        auto start = high_resolution_clock::now();
        if (numCities < 2) return;
        // Limit Brute force to 10 cities to prevent the application from hanging (O(N!) complexity)
        if (numCities > 10) { runGreedy(); return; }

        // Create an array of intermediate cities to shuffle
        vector<int> cities;
        for(int i=1; i<numCities; i++) cities.push_back(i);

        int minCost = 1e7; vector<int> bestP;

        // Iterate through all permutations of the array
        do {
            vector<int> currentPath = {0};
            currentPath.insert(currentPath.end(), cities.begin(), cities.end());
            currentPath.push_back(0);

            int currentCost = calculateCost(currentPath);
            if(currentCost < minCost) { minCost = currentCost; bestP = currentPath; }
        } while(next_permutation(cities.begin(), cities.end()));

        lastPath = bestP; lastCost = minCost;

        auto stop = high_resolution_clock::now();
        lastTime = duration<double, milli>(stop - start).count();
    }

    // Hybrid Algorithm utilizing Random Selection and 2-Opt Optimization
    void runCustom() {
        auto start = high_resolution_clock::now();
        if (numCities < 2) return;

        // Start with a baseline greedy path
        runGreedy();
        vector<int> bestPath = lastPath;
        int bestCost = lastCost;

        // Random number generator setup for probabilistic jumps
        mt19937 rng(chrono::steady_clock::now().time_since_epoch().count());
        int iterations = 80;

        // Phase 1: Try finding better routes using slight randomness
        for (int it = 0; it < iterations; it++) {
            vector<int> path; vector<bool> visited(numCities, false);
            int current = 0; path.push_back(0); visited[0] = true;
            bool valid = true;

            for (int step = 1; step < numCities; step++) {
                vector<pair<int,int>> candidates;
                for (int j = 0; j < numCities; j++)
                    if (!visited[j] && adjMatrix[current][j] > 0) candidates.push_back({adjMatrix[current][j], j});

                if (candidates.empty()) { valid = false; break; }

                sort(candidates.begin(), candidates.end());
                // 80% chance to pick the absolute closest, 20% to pick a random neighbor (exploration)
                int nextCity = (uniform_int_distribution<int>(0, 100)(rng) < 80) ? candidates[0].second : candidates[rng() % candidates.size()].second;
                visited[nextCity] = true; path.push_back(nextCity); current = nextCity;
            }
            if (valid && adjMatrix[current][0] > 0) {
                path.push_back(0);
                int cost = calculateCost(path);
                if (cost < bestCost) { bestCost = cost; bestPath = path; }
            }
        }

        // Phase 2: 2-Opt algorithm to uncross intersecting paths by reversing sub-sections
        for (int i = 1; i < (int)bestPath.size() - 2; i++) {
            for (int j = i + 1; j < (int)bestPath.size() - 1; j++) {
                vector<int> temp = bestPath;
                reverse(temp.begin() + i, temp.begin() + j + 1);
                int newCost = calculateCost(temp);
                if (newCost < bestCost) { bestCost = newCost; bestPath = temp; }
            }
        }

        lastPath = bestPath; lastCost = bestCost;
        auto stop = high_resolution_clock::now();
        lastTime = duration<double, milli>(stop - start).count();
    }

    // Converts the resulting path array into a readable string of city names
    QString getPathString() {
        QString p = "";
        for (size_t i = 0; i < lastPath.size(); i++) {
            if (lastPath[i] < cityNames.size()) {
                p += cityNames[lastPath[i]];
            }
            if (i < lastPath.size() - 1) p += " -> ";
        }
        return p;
    }
};

// Widget handling the animated splash screen and background drawing
class WorldMapWidget : public QWidget {
    Q_OBJECT
private:
    double rotationAngle = 0.0;
    double shineOffset = -1.0;
    QTimer *refreshTimer;

public:
    WorldMapWidget(QWidget *parent = nullptr) : QWidget(parent) {
        refreshTimer = new QTimer(this);
        // Timer updates rotation and shine effect variables repeatedly
        connect(refreshTimer, &QTimer::timeout, this, [this]() {
            rotationAngle += 1.5;
            if (rotationAngle >= 360.0) rotationAngle -= 360.0;

            shineOffset += 0.02;
            if (shineOffset > 2.0) shineOffset = -1.0;

            update(); // Trigger the paintEvent
        });
        refreshTimer->start(30); // Run at ~30 FPS
    }

protected:
    // Core function to draw the customized background and bike vector graphics
    void paintEvent(QPaintEvent *) override {
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing); // Smooth edges

        int w = width();
        int h = height();

        // Map land background styling
        QLinearGradient landGrad(0, 0, w, h);
        landGrad.setColorAt(0, QColor(222, 230, 236));
        landGrad.setColorAt(1, QColor(204, 216, 224));
        painter.fillRect(rect(), landGrad);

        // Drawing abstract map layout (blocks and ellipses for regions)
        painter.setPen(Qt::NoPen);
        painter.setBrush(QColor(188, 208, 218));
        painter.drawRect(w * 0.05, h * 0.1, w * 0.2, h * 0.25);
        painter.drawRect(w * 0.65, h * 0.05, w * 0.25, h * 0.3);
        painter.drawRect(w * 0.7, h * 0.55, w * 0.22, h * 0.2);

        painter.setBrush(QColor(194, 214, 202));
        painter.drawEllipse(QPointF(w * 0.55, h * 0.2), 120, 80);
        painter.drawEllipse(QPointF(w * 0.15, h * 0.65), 160, 110);

        // Drawing road boundaries
        QPen blockRoadPen(QColor(238, 242, 246), 2);
        painter.setPen(blockRoadPen);
        painter.drawLine(0, h * 0.22, w, h * 0.18);
        painter.drawLine(0, h * 0.45, w, h * 0.52);
        painter.drawLine(0, h * 0.75, w, h * 0.72);
        painter.drawLine(w * 0.25, 0, w * 0.18, h);
        painter.drawLine(w * 0.55, 0, w * 0.62, h);
        painter.drawLine(w * 0.82, 0, w * 0.78, h);

        // Main animated highway structure logic
        QPainterPath mainHighway;
        mainHighway.moveTo(w, h * 0.05);
        mainHighway.cubicTo(w * 0.65, h * 0.12, w * 0.60, h * 0.28, w * 0.45, h * 0.36);
        mainHighway.cubicTo(w * 0.25, h * 0.46, w * 0.75, h * 0.62, w * 0.82, h * 0.78);
        mainHighway.cubicTo(w * 0.88, h * 0.90, w * 0.55, h * 0.98, w * 0.35, h);
        mainHighway.lineTo(w * 0.48, h);
        mainHighway.cubicTo(w * 0.68, h * 0.94, w * 0.98, h * 0.86, w * 0.92, h * 0.74);
        mainHighway.cubicTo(w * 0.84, h * 0.56, w * 0.36, h * 0.40, w * 0.55, h * 0.30);
        mainHighway.cubicTo(w * 0.68, h * 0.22, w * 0.74, h * 0.08, w, h * 0.0);
        mainHighway.closeSubpath();

        painter.setBrush(QColor(64, 78, 92));
        painter.setPen(Qt::NoPen);
        painter.drawPath(mainHighway);

        QPainterPath trackSpine;
        trackSpine.moveTo(w, h * 0.025);
        trackSpine.cubicTo(w * 0.665, h * 0.10, w * 0.575, h * 0.26, w * 0.50, h * 0.33);
        trackSpine.cubicTo(w * 0.305, h * 0.43, w * 0.795, h * 0.59, w * 0.87, h * 0.76);
        trackSpine.cubicTo(w * 0.93, h * 0.88, w * 0.615, h * 0.96, w * 0.415, h);

        QPen dashboardLine(QColor(241, 245, 249), 4);
        dashboardLine.setStyle(Qt::DashLine);
        painter.setPen(dashboardLine);
        painter.setBrush(Qt::NoBrush);
        painter.drawPath(trackSpine);

        // Vector shapes constructed specifically to draw the Delivery Bike and Rider
        painter.save();
        painter.translate(w * 0.56, h * 0.50);
        painter.rotate(-15.0);

        // 1. Draw Bike Shadow
        painter.setBrush(QColor(0, 0, 0, 60));
        painter.setPen(Qt::NoPen);
        painter.drawEllipse(QPointF(0, 22), 45, 12);

        // 2. Draw Bike Tires
        painter.setBrush(QColor(30, 41, 59));
        painter.setPen(QPen(QColor(71, 85, 105), 2));
        painter.drawEllipse(QPointF(-32, 12), 16, 16);
        painter.drawEllipse(QPointF(28, 12), 16, 16);

        // Inner Hubs of tires
        painter.setBrush(QColor(241, 245, 249));
        painter.drawEllipse(QPointF(-32, 12), 4, 4);
        painter.drawEllipse(QPointF(28, 12), 4, 4);

        // 3. Draw Bike Frame Layout
        QPainterPath bikeBody;
        bikeBody.moveTo(-32, 12);
        bikeBody.lineTo(-12, -2);
        bikeBody.lineTo(8, -8);
        bikeBody.lineTo(24, -14);
        bikeBody.lineTo(28, 12);
        bikeBody.lineTo(4, 10);
        bikeBody.closeSubpath();

        painter.setPen(QPen(QColor(15, 23, 42), 3));
        painter.setBrush(Qt::NoBrush);
        painter.drawPath(bikeBody);

        // Drawing the Fuel Tank of the bike
        QPainterPath fuelTank;
        fuelTank.moveTo(-8, -4);
        fuelTank.cubicTo(-4, -14, 6, -14, 12, -6);
        fuelTank.lineTo(4, 4);
        fuelTank.closeSubpath();
        painter.setPen(Qt::NoPen);
        painter.setBrush(QColor(220, 38, 38));
        painter.drawPath(fuelTank);

        // Handlebars configuration
        painter.setPen(QPen(QColor(15, 23, 42), 3, Qt::SolidLine, Qt::RoundCap));
        painter.drawLine(24, -14, 18, -24);
        painter.drawLine(18, -24, 12, -22);

        // 4. Drawing Rider Silhouette
        QPainterPath riderBody;
        riderBody.moveTo(-16, -2);
        riderBody.cubicTo(-22, -18, -12, -28, 2, -26);
        riderBody.lineTo(12, -20);
        riderBody.lineTo(4, 4);
        riderBody.closeSubpath();
        painter.setBrush(QColor(30, 41, 59));
        painter.drawPath(riderBody);

        // Helmet styling and glow effect
        painter.setBrush(QColor(15, 23, 42));
        painter.drawEllipse(QRectF(-4, -42, 18, 18));
        painter.setBrush(QColor(34, 211, 238));
        painter.drawPie(QRectF(-2, -40, 16, 16), 0 * 16, 90 * 16);

        // Rider's Arm geometry
        painter.setPen(QPen(QColor(30, 41, 59), 4, Qt::SolidLine, Qt::RoundCap));
        painter.drawLine(-4, -22, 14, -22);

        // Traveling Salesman Delivery Bag
        painter.setPen(QPen(QColor(15, 23, 42), 2));
        painter.setBrush(QColor(234, 179, 8));
        painter.drawRect(QRectF(-34, -24, 16, 20));

        painter.setPen(QPen(QColor(15, 23, 42), 1));
        painter.drawLine(-26, -24, -26, -4);

        painter.restore();

        // Calculate dynamic pin scaling to give 3D spinning effect based on timer
        double radians = (rotationAngle * M_PI) / 180.0;
        double horizontalStretch = std::cos(radians);

        // Lambda function to draw floating location pins easily at different coordinates
        auto drawLocationPin = [&](double x, double y, double scale) {
            painter.save();
            painter.translate(x, y);

            double dynamicScale = scale * (1.0 + 0.08 * std::sin(radians));
            painter.scale(dynamicScale, dynamicScale);

            // Draw shadow beneath pin
            painter.setBrush(QColor(0, 0, 0, 45));
            painter.setPen(Qt::NoPen);
            painter.drawEllipse(QPointF(0, -2), 16 * std::abs(horizontalStretch), 5);

            // Drawing the pin head using cubic curves
            QPainterPath pinHead;
            pinHead.moveTo(0, 0);
            pinHead.cubicTo(-15 * horizontalStretch, -26, -15 * horizontalStretch, -44, 0, -44);
            pinHead.cubicTo(15 * horizontalStretch, -44, 15 * horizontalStretch, -26, 0, 0);
            pinHead.closeSubpath();

            painter.setBrush(QColor(220, 38, 38));
            painter.drawPath(pinHead);

            painter.setBrush(QColor(222, 230, 236));
            double coreWidth = 5.0 * std::abs(horizontalStretch);
            painter.drawEllipse(QRectF(-coreWidth / 2.0, -33.5, coreWidth, 5.0));

            painter.restore();
        };

        // Adding background decorative pins
        drawLocationPin(w * 0.12, h * 0.20, 1.25);
        drawLocationPin(w * 0.32, h * 0.26, 1.45);
        drawLocationPin(w * 0.52, h * 0.18, 1.15);
        drawLocationPin(w * 0.22, h * 0.48, 1.55);
        drawLocationPin(w * 0.45, h * 0.40, 1.85);
        drawLocationPin(w * 0.74, h * 0.52, 2.50);
        drawLocationPin(w * 0.65, h * 0.15, 1.30);
        drawLocationPin(w * 0.90, h * 0.62, 1.20);
        drawLocationPin(w * 0.58, h * 0.85, 2.75);

        // Rendering credits at the bottom of splash screen
        QString creditsText = "POWERED BY AIZAZ NISAR & IMRAN RIAZ";
        QFont creditFont("Segoe UI", 13, QFont::Bold);
        creditFont.setLetterSpacing(QFont::AbsoluteSpacing, 8);
        painter.setFont(creditFont);

        int textY = h - 65;

        // Shadow for text
        painter.setPen(QColor(15, 23, 42, 220));
        painter.drawText(QRect(0, textY + 3, w, 50), Qt::AlignCenter, creditsText);

        // Outline for text
        painter.setPen(QColor(255, 255, 255, 255));
        painter.drawText(QRect(-1, textY - 1, w, 50), Qt::AlignCenter, creditsText);
        painter.drawText(QRect(1, textY - 1, w, 50), Qt::AlignCenter, creditsText);
        painter.drawText(QRect(-1, textY + 1, w, 50), Qt::AlignCenter, creditsText);
        painter.drawText(QRect(1, textY + 1, w, 50), Qt::AlignCenter, creditsText);

        painter.setPen(QColor(220, 38, 38));
        painter.drawText(QRect(0, textY, w, 50), Qt::AlignCenter, creditsText);

        // Animated laser/beam wipe effect across the credits text
        QLinearGradient highlightBeam(w * shineOffset, textY, w * (shineOffset + 0.15), textY);
        highlightBeam.setColorAt(0, QColor(220, 38, 38, 0));
        highlightBeam.setColorAt(0.5, QColor(255, 255, 255, 230));
        highlightBeam.setColorAt(1, QColor(220, 38, 38, 0));

        painter.save();
        painter.setCompositionMode(QPainter::CompositionMode_SourceAtop);
        painter.setPen(QPen(highlightBeam, 2));
        painter.setBrush(highlightBeam);
        painter.drawRect(0, textY - 15, w, 65);
        painter.restore();
    }
};

// Main Window Class to handle all User Interface interactions and displays
class RouteUI : public QMainWindow {
    Q_OBJECT
    RouteOptimizer optimizer; // Instance of the backend logic class
    QStackedWidget *stack; // Controls switching between Welcome Screen and Main Dashboard
    QStackedWidget *contentStack; // Controls switching between Graph View and Tables
    QGraphicsScene *scene;
    QLabel *statusLabel;
    QLabel *pathSummaryLabel;
    QTableWidget *compareTable;
    QTableWidget *repTable;
    QTableWidget *matrixDisplayTable;
    QTableWidget *listDisplayTable;
    QTimer *animTimer; // Timer for visualizing algorithms path by path
    QTimer *graphRotationTimer;
    double dynamicGraphAngle = 0.0;
    int animStep = 0;
    QString currentAlgoName;
    QColor currentPathColor;

    // Vectors (Dynamic arrays) to track UI elements so they can be modified or rotated later
    vector<QGraphicsPathItem*> canvasPinItems;
    vector<QGraphicsEllipseItem*> canvasCoreItems;
    bool isGraphGenerated = false;

public:
    RouteUI() {
        // App basic setups and main styles
        this->setWindowTitle("Pakistan Route Optimizer Pro");
        this->resize(1200, 900);

        this->setStyleSheet(
            "QMainWindow { background-color: #dae2ec; }"
            "QWidget#dashboard { background-color: #dae2ec; }"
            "QPushButton { "
            "   background-color: #b0bec5; "
            "   color: #1c3f75; "
            "   border: 1px solid #90a4ae; "
            "   border-radius: 4px; "
            "   padding: 10px; "
            "   font-family: 'Segoe UI'; "
            "   font-weight: bold; "
            "}"
            "QPushButton:hover { background-color: #1c3f75; color: #dae2ec; border: 1px solid #1c3f75; }"
            "QLabel { color: #1c3f75; }"
            );

        // Timer for path drawing animations
        animTimer = new QTimer(this);
        connect(animTimer, &QTimer::timeout, this, &RouteUI::animatePath);

        // Timer that continuously updates coordinates for graph pins to spin
        graphRotationTimer = new QTimer(this);
        connect(graphRotationTimer, &QTimer::timeout, this, [this]() {
            dynamicGraphAngle += 3.0;
            if (dynamicGraphAngle >= 360.0) dynamicGraphAngle -= 360.0;

            if (contentStack->currentIndex() == 0 && isGraphGenerated) {
                double radians = (dynamicGraphAngle * M_PI) / 180.0;
                double horizontalStretch = std::cos(radians);

                // Update geometry values of all map pins inside our tracker arrays
                for (size_t i = 0; i < canvasPinItems.size(); i++) {
                    if (canvasPinItems[i] && i < canvasCoreItems.size() && canvasCoreItems[i]) {
                        QPainterPath rotatedPath;
                        rotatedPath.moveTo(0, 0);
                        rotatedPath.cubicTo(-14 * horizontalStretch, -24, -14 * horizontalStretch, -40, 0, -40);
                        rotatedPath.cubicTo(14 * horizontalStretch, -40, 14 * horizontalStretch, -24, 0, 0);
                        rotatedPath.closeSubpath();

                        canvasPinItems[i]->setPath(rotatedPath);

                        QRectF coreRect = canvasCoreItems[i]->rect();
                        double cx = coreRect.center().x();
                        double cy = coreRect.center().y();
                        double newWidth = 6.0 * std::abs(horizontalStretch);
                        canvasCoreItems[i]->setRect(cx - (newWidth / 2.0), cy - 3.0, newWidth, 6.0);
                    }
                }
            }
        });
        graphRotationTimer->start(30);

        // Build the loading Splash Screen UI
        WorldMapWidget *welcomePage = new WorldMapWidget();
        QVBoxLayout *vbox = new QVBoxLayout(welcomePage);
        vbox->setContentsMargins(0, 45, 0, 45);

        QLabel *title = new QLabel("ROUTE OPTIMIZATION\n SYSTEM");
        title->setAlignment(Qt::AlignCenter);
        title->setStyleSheet("color: #1e293b; font-size: 72px; font-weight: 900; font-family: 'Impact', sans-serif; background: transparent; letter-spacing: 1px;");

        QVBoxLayout *loadingLayout = new QVBoxLayout();
        QLabel *loadingText = new QLabel("Loading...");
        loadingText->setAlignment(Qt::AlignCenter);
        loadingText->setStyleSheet("color: #334155; font-size: 18px; font-family: 'Segoe UI'; font-weight: bold; background: transparent;");

        QProgressBar *progressBar = new QProgressBar();
        progressBar->setRange(0, 100);
        progressBar->setValue(0);
        progressBar->setTextVisible(false);
        progressBar->setFixedWidth(400);
        progressBar->setStyleSheet(
            "QProgressBar { border: 1px solid #90a4ae; border-radius: 5px; background-color: #cfd8dc; height: 10px; }"
            "QProgressBar::chunk { background-color: #1c3f75; }"
            );

        loadingLayout->addWidget(loadingText);
        loadingLayout->addWidget(progressBar, 0, Qt::AlignCenter);

        vbox->addWidget(title);
        vbox->addStretch(1);
        vbox->addSpacing(320);
        vbox->addStretch(1);
        vbox->addLayout(loadingLayout);
        vbox->addSpacing(45);

        // Fake loading timer to visually impress the user before launching dashboard
        QTimer *loadingTimer = new QTimer(this);
        connect(loadingTimer, &QTimer::timeout, [=]() mutable {
            int val = progressBar->value();
            if (val < 100) {
                progressBar->setValue(val + 1);
            } else {
                loadingTimer->stop();
                progressBar->setStyleSheet(
                    "QProgressBar { border: 1px solid #2ecc71; border-radius: 5px; background-color: #cfd8dc; height: 10px; }"
                    "QProgressBar::chunk { background-color: #2ecc71; }"
                    );
                loadingText->setText("SYSTEM INITIALIZED!");
                loadingText->setStyleSheet("color: #2ecc71; font-weight: bold; font-size: 18px;");
                // Launch dashboard with transition effect
                QTimer::singleShot(500, this, [=](){
                    fadeToDashboard();
                });
            }
        });
        loadingTimer->start(100);

        // Build the Main Dashboard UI
        QWidget *dashboard = new QWidget();
        dashboard->setStyleSheet("background-color: #dae2ec;");
        QHBoxLayout *mainLayout = new QHBoxLayout(dashboard);
        QVBoxLayout *sidebar = new QVBoxLayout();
        QString btnStyle = "QPushButton { background-color: #b0bec5; color: #1c3f75; border-radius: 8px; padding: 12px; font-weight: bold; border: 1px solid #90a4ae; font-size: 12px; }"
                           "QPushButton:hover { background-color: #1c3f75; color: #dae2ec; border: 1px solid #1c3f75; }";

        // Dashboard Buttons initialization
        QPushButton *btnLoad = new QPushButton("📂 Load Data File");
        QPushButton *btnShowGraph = new QPushButton("🗺 Show Original Graph");
        QPushButton *btnGreedy = new QPushButton("⚡ Run Greedy");
        QPushButton *btnBrute = new QPushButton("🔍 Run Brute Force");
        QPushButton *btnCustom = new QPushButton("🛠 Run Hybrid Optimized");
        QPushButton *btnCompare = new QPushButton("📊 Performance Report");
        QPushButton *btnRep = new QPushButton("💾 Memory Analysis");

        btnLoad->setStyleSheet(btnStyle);
        btnShowGraph->setStyleSheet(btnStyle);
        btnGreedy->setStyleSheet(btnStyle);
        btnBrute->setStyleSheet(btnStyle);
        btnCustom->setStyleSheet(btnStyle);
        btnCompare->setStyleSheet(btnStyle);
        btnRep->setStyleSheet("QPushButton { background-color: #cfd8dc; color: #1c3f75; border-radius: 8px; padding: 12px; font-weight: bold; border: 2px solid #1c3f75; } QPushButton:hover { background-color: #1c3f75; color: #dae2ec; }");

        // UI Widget to show output/messages
        statusLabel = new QLabel("System Ready...");
        statusLabel->setStyleSheet("color: #1c3f75; font-size: 14px; font-weight: bold; background: #b0bec5; padding: 10px; border-radius: 5px; border: 1px solid #90a4ae;");
        statusLabel->setWordWrap(true);

        sidebar->addWidget(btnLoad);
        sidebar->addWidget(btnShowGraph);
        sidebar->addWidget(btnGreedy);
        sidebar->addWidget(btnBrute);
        sidebar->addWidget(btnCustom);
        sidebar->addWidget(btnCompare);
        sidebar->addWidget(btnRep);
        sidebar->addStretch();
        sidebar->addWidget(statusLabel);

        // Core Graphics Viewer to render graph network
        contentStack = new QStackedWidget();
        scene = new QGraphicsScene();
        QGraphicsView *view = new QGraphicsView(scene);
        view->setRenderHint(QPainter::Antialiasing);
        view->setStyleSheet("background-color: #e6ecf2; border-radius: 15px; border: 1px solid #90a4ae;");
        contentStack->addWidget(view);

        // Layout setup for Performance comparison table
        QWidget *compareWidget = new QWidget();
        QVBoxLayout *compLayout = new QVBoxLayout(compareWidget);
        QLabel *compTitle = new QLabel("ALGORITHM PERFORMANCE REPORT");
        compTitle->setStyleSheet("font-size: 24px; font-weight: bold; color: #1c3f75; margin-bottom: 10px;");
        compareTable = new QTableWidget(3, 3);
        compareTable->setHorizontalHeaderLabels({"Algorithm Name", "Total Cost", "Execution Time"});
        compareTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
        compareTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
        compareTable->setFixedHeight(180);
        compareTable->setStyleSheet("QTableWidget { background-color: #b0bec5; gridline-color: #cfd8dc; font-size: 16px; color: #1e293b; border: 1px solid #90a4ae; }"
                                    "QHeaderView::section { background-color: #cfd8dc; color: #1c3f75; padding: 10px; font-weight: bold; border-bottom: 1px solid #90a4ae; }");
        QLabel *compPathTitle = new QLabel("SELECTED PATHS COMPARISON:");
        compPathTitle->setStyleSheet("font-size: 22px; font-weight: bold; color: #1c3f75; margin-top: 20px; margin-bottom: 10px;");
        pathSummaryLabel = new QLabel("Run algorithms to compare paths...");
        pathSummaryLabel->setStyleSheet("background-color: #b0bec5; border: 1px solid #90a4ae; border-radius: 10px; padding: 15px; color: #1e293b; font-size: 16px; font-family: 'Segoe UI', sans-serif;");
        pathSummaryLabel->setAlignment(Qt::AlignTop | Qt::AlignLeft);
        pathSummaryLabel->setWordWrap(true);
        compLayout->addWidget(compTitle);
        compLayout->addWidget(compareTable);
        compLayout->addWidget(compPathTitle);
        compLayout->addWidget(pathSummaryLabel);
        compLayout->addStretch();
        contentStack->addWidget(compareWidget);

        // Layout setup for Memory details and Data Structures view
        QWidget *repWidget = new QWidget();
        QVBoxLayout *repLayout = new QVBoxLayout(repWidget);
        QLabel *repTitle = new QLabel("MEMORY & SPEED ANALYSIS");
        repTitle->setStyleSheet("font-size: 24px; font-weight: bold; color: #1c3f75; margin-bottom: 10px;");
        repTable = new QTableWidget(2, 3);
        repTable->setHorizontalHeaderLabels({"Structure", "Memory Used", "Traversal Speed"});
        repTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
        repTable->setFixedHeight(120);
        repTable->setStyleSheet("QTableWidget { background-color: #b0bec5; gridline-color: #cfd8dc; font-size: 15px; color: #1e293b; border: 1px solid #90a4ae; }"
                                "QHeaderView::section { background-color: #cfd8dc; color: #1c3f75; padding: 10px; font-weight: bold; border-bottom: 1px solid #90a4ae; }");

        QLabel *matTitle = new QLabel("ADJACENCY MATRIX");
        matTitle->setStyleSheet("font-size: 18px; font-weight: bold; color: #1c3f75; margin-top: 15px;");
        matrixDisplayTable = new QTableWidget();
        matrixDisplayTable->setStyleSheet("QTableWidget { background-color: #b0bec5; gridline-color: #cfd8dc; font-size: 12px; font-weight: bold; color: #1e293b; border: 1px solid #90a4ae; }"
                                          "QHeaderView::section { background-color: #cfd8dc; color: #1c3f75; padding: 5px; font-weight: bold; border-bottom: 1px solid #90a4ae; }");

        QLabel *listTitle = new QLabel("ADJACENCY LIST");
        listTitle->setStyleSheet("font-size: 18px; font-weight: bold; color: #1c3f75; margin-top: 15px;");
        listDisplayTable = new QTableWidget();
        listDisplayTable->setStyleSheet("QTableWidget { background-color: #b0bec5; gridline-color: #cfd8dc; font-size: 12px; color: #1e293b; border: 1px solid #90a4ae; }"
                                        "QHeaderView::section { background-color: #cfd8dc; color: #1c3f75; padding: 5px; font-weight: bold; border-bottom: 1px solid #90a4ae; }");

        repLayout->addWidget(repTitle);
        repLayout->addWidget(repTable);
        repLayout->addWidget(matTitle);
        repLayout->addWidget(matrixDisplayTable, 1);
        repLayout->addWidget(listTitle);
        repLayout->addWidget(listDisplayTable, 1);
        contentStack->addWidget(repWidget);

        // Put sidebar and content layouts together
        mainLayout->addLayout(sidebar, 1);
        mainLayout->addWidget(contentStack, 4);

        stack = new QStackedWidget();
        stack->addWidget(welcomePage);
        stack->addWidget(dashboard);
        setCentralWidget(stack);

        // Linking UI interaction elements to functions (Events)
        connect(btnLoad, &QPushButton::clicked, this, &RouteUI::loadFile);

        connect(btnShowGraph, &QPushButton::clicked, this, [this]() {
            contentStack->setCurrentIndex(0);
            isGraphGenerated = false;
            drawBaseGraph(true);
            statusLabel->setText("Displaying original graph.");
        });

        connect(btnGreedy, &QPushButton::clicked, this, &RouteUI::onGreedy);
        connect(btnBrute, &QPushButton::clicked, this, &RouteUI::onBrute);
        connect(btnCustom, &QPushButton::clicked, this, &RouteUI::onCustom);
        connect(btnCompare, &QPushButton::clicked, this, &RouteUI::showComparison);
        connect(btnRep, &QPushButton::clicked, this, &RouteUI::showRepComparison);
    }

    // Qt Animation properties for zooming and fading into dashboard from splash screen
    void fadeToDashboard() {
        stack->setCurrentIndex(1);
        QWidget* dashboard = stack->widget(1);
        dashboard->show();

        QGraphicsOpacityEffect *inEffect = new QGraphicsOpacityEffect(dashboard);
        dashboard->setGraphicsEffect(inEffect);

        QPropertyAnimation *scaleAnim = new QPropertyAnimation(dashboard, "geometry");
        scaleAnim->setDuration(1200);
        QRect finalGeom = dashboard->geometry();
        QRect startGeom = QRect(finalGeom.center().x(), finalGeom.center().y(), 0, 0);

        scaleAnim->setStartValue(startGeom);
        scaleAnim->setEndValue(finalGeom);
        scaleAnim->setEasingCurve(QEasingCurve::OutExpo);

        QPropertyAnimation *fadeIn = new QPropertyAnimation(inEffect, "opacity");
        fadeIn->setDuration(1000);
        fadeIn->setStartValue(0.0);
        fadeIn->setEndValue(1.0);

        QParallelAnimationGroup *group = new QParallelAnimationGroup(this);
        group->addAnimation(scaleAnim);
        group->addAnimation(fadeIn);
        group->start(QAbstractAnimation::DeleteWhenStopped);

        // Shake effect
        QTimer::singleShot(200, [this]() {
            QPropertyAnimation *shake = new QPropertyAnimation(this, "pos");
            shake->setDuration(500);
            QPoint p = this->pos();
            shake->setKeyValueAt(0, p + QPoint(-5, 0));
            shake->setKeyValueAt(0.2, p + QPoint(5, 5));
            shake->setKeyValueAt(0.4, p + QPoint(-5, -5));
            shake->setKeyValueAt(0.6, p + QPoint(5, 0));
            shake->setKeyValueAt(0.8, p + QPoint(-5, 5));
            shake->setKeyValueAt(1, p);
            shake->start(QAbstractAnimation::DeleteWhenStopped);
        });
    }

    // Prompts File Dialog to select file and triggers RouteOptimizer load function
    void loadFile() {
        QString fileName = QFileDialog::getOpenFileName(this, "Select Data File", "", "Text Files (*.txt)");
        if (!fileName.isEmpty()) {
            isGraphGenerated = false;
            optimizer.load(fileName.toStdString());
            statusLabel->setText(QString("✅ %1 Cities Dynamic Labels Loaded Successfully.").arg(optimizer.numCities));
            contentStack->setCurrentIndex(0);
            drawBaseGraph();
        }
    }

    // Force-Directed Layout Algorithm (Physics-based) to organize city coordinates naturally without overlapping
    vector<QPointF> getWindingHighwayPositions(int numCities, int w, int h) {
        // We use a vector array (dynamically sized based on cities) to hold physical point coordinates
        vector<QPointF> positions(numCities);
        if (numCities == 0) return positions;

        // Set origin city placement
        positions[0] = QPointF(w * 0.48, h * 0.70);

        // Start cities in a circular orbit setup around the origin before physics take over
        for (int i = 1; i < numCities; i++) {
            double theta = i * 2.39996 + (i * 0.15);
            double r = 180.0 + (numCities * 4.0) + (i * 8.0);
            positions[i] = QPointF(w * 0.5 + r * std::cos(theta), h * 0.45 + r * std::sin(theta));
        }

        int iterations = 350; // Number of physics loops
        double kRepulsion = 3000000.0;
        double kAttraction = 0.010;
        double damping = 0.85;

        // Simulate attraction (connected cities) and repulsion (prevent overlap) forces
        for (int iter = 0; iter < iterations; iter++) {
            // Arrays to store movement directions applied this frame
            vector<QPointF> displacement(numCities, QPointF(0, 0));

            for (int i = 0; i < numCities; i++) {
                for (int j = i + 1; j < numCities; j++) {
                    double dx = positions[i].x() - positions[j].x();
                    double dy = positions[i].y() - positions[j].y();
                    double distSq = dx * dx + dy * dy;

                    // Manual push logic if perfectly stacked
                    if (std::abs(dx) < 2.0 && std::abs(dy) < 5.0) {
                        dx += (i % 2 == 0 ? 12.0 : -12.0);
                    }

                    if (distSq < 10000.0) distSq = 10000.0;
                    double dist = std::sqrt(distSq);

                    // Apply Repulsion Force formula
                    double force = kRepulsion / distSq;
                    displacement[i] += QPointF((dx / dist) * force, (dy / dist) * force);
                    displacement[j] -= QPointF((dx / dist) * force, (dy / dist) * force);
                }
            }

            // Apply Attraction Force for connected nodes via matrix lookup
            for (int i = 0; i < numCities; i++) {
                for (int j = 0; j < numCities; j++) {
                    if (optimizer.adjMatrix[i][j] > 0) {
                        double dx = positions[j].x() - positions[i].x();
                        double dy = positions[j].y() - positions[i].y();
                        double dist = std::sqrt(dx * dx + dy * dy);
                        if (dist < 1.0) dist = 1.0;

                        double force = kAttraction * dist;
                        displacement[i] += QPointF((dx / dist) * force, (dy / dist) * force);
                        displacement[j] -= QPointF((dx / dist) * force, (dy / dist) * force);
                    }
                }
            }

            // Move the actual items using calculated displacements and dampening factor
            for (int i = 1; i < numCities; i++) {
                positions[i] += displacement[i] * damping;
            }
        }

        // Normalize coordinates to make sure the graph fits exactly inside the Screen boundaries
        double minX = 1e9, maxX = -1e9, minY = 1e9, maxY = -1e9;
        for (int i = 0; i < numCities; i++) {
            if (positions[i].x() < minX) minX = positions[i].x();
            if (positions[i].x() > maxX) maxX = positions[i].x();
            if (positions[i].y() < minY) minY = positions[i].y();
            if (positions[i].y() > maxY) maxY = positions[i].y();
        }

        double spanX = maxX - minX;
        double spanY = maxY - minY;
        if (spanX < 1.0) spanX = 1.0;
        if (spanY < 1.0) spanY = 1.0;

        double targetMinX = w * 0.10;
        double targetMaxX = w * 0.90;
        double targetMinY = h * 0.10;
        double targetMaxY = h * 0.82;

        for (int i = 0; i < numCities; i++) {
            double normX = (positions[i].x() - minX) / spanX;
            double normY = (positions[i].y() - minY) / spanY;
            positions[i] = QPointF(targetMinX + normX * (targetMaxX - targetMinX),
                                   targetMinY + normY * (targetMaxY - targetMinY));
        }

        return positions;
    }

    // Master function to render environment context and underlying city nodes on QGraphicsScene
    void drawBaseGraph(bool showAll = true) {
        if (isGraphGenerated && showAll) return;

        scene->clear();
        canvasPinItems.clear();
        canvasCoreItems.clear();
        if (optimizer.numCities == 0) return;

        int w = 800;
        int h = 670;
        scene->setSceneRect(0, 0, w, h);
        scene->addRect(0, 0, w, h, QPen(Qt::NoPen), QBrush(QColor(238, 242, 246)));

        // Drawing Grid background
        QPen gridPen(QColor(203, 213, 225, 90), 1.0, Qt::DashLine);
        int gridSize = 50;
        for (int x = gridSize; x < w; x += gridSize) {
            scene->addLine(x, 0, x, h, gridPen);
        }
        for (int y = gridSize; y < h; y += gridSize) {
            scene->addLine(0, y, w, y, gridPen);
        }

        // Decorating geography (e.g. Rings, Rivers, Borders)
        QPen contourPen(QColor(148, 163, 184, 60), 1.5);
        auto addContourRing = [&](double cx, double cy, double rx, double ry, int layers) {
            for (int k = 0; k < layers; ++k) {
                auto ring = scene->addEllipse(cx - (rx - k*15), cy - (ry - k*12), (rx - k*15)*2, (ry - k*12)*2, contourPen, Qt::NoBrush);
                ring->setZValue(1);
            }
        };
        addContourRing(w * 0.55, h * 0.20, 140, 75, 4);
        addContourRing(w * 0.20, h * 0.35, 110, 85, 3);
        addContourRing(w * 0.78, h * 0.15, 90, 60, 5);

        // Stylized Rivers
        QPen riverPen(QColor(125, 211, 252, 140), 2.5);
        QPainterPath indusRiver;
        indusRiver.moveTo(w * 0.65, 0);
        indusRiver.cubicTo(w * 0.60, h * 0.20, w * 0.45, h * 0.35, w * 0.50, h * 0.55);
        indusRiver.cubicTo(w * 0.55, h * 0.68, w * 0.30, h * 0.75, w * 0.22, h * 0.78);
        auto river1 = scene->addPath(indusRiver, riverPen, Qt::NoBrush);
        river1->setZValue(2);

        QPen branchPen(QColor(125, 211, 252, 90), 1.5);
        QPainterPath jhelumChenab;
        jhelumChenab.moveTo(w * 0.80, h * 0.10);
        jhelumChenab.cubicTo(w * 0.70, h * 0.25, w * 0.55, h * 0.38, w * 0.50, h * 0.55);
        auto river2 = scene->addPath(jhelumChenab, branchPen, Qt::NoBrush);
        river2->setZValue(2);

        // Drawing boundaries
        QPen borderPen(QColor(148, 163, 184, 110), 1.5, Qt::DashDotLine);
        scene->addLine(w * 0.38, h * 0.40, w * 0.38, h * 0.75, borderPen)->setZValue(1);
        scene->addLine(w * 0.38, h * 0.40, w * 0.75, h * 0.40, borderPen)->setZValue(1);

        // Draw Ocean/Sea Path at bottom
        QPainterPath seaPath;
        seaPath.moveTo(0, h);
        seaPath.lineTo(0, h * 0.78);
        seaPath.cubicTo(w * 0.25, h * 0.74, w * 0.45, h * 0.88, w * 0.70, h * 0.81);
        seaPath.cubicTo(w * 0.85, h * 0.77, w * 0.92, h * 0.83, w, h * 0.79);
        seaPath.lineTo(w, h);
        seaPath.closeSubpath();

        QLinearGradient seaGrad(0, h * 0.74, 0, h);
        seaGrad.setColorAt(0, QColor(14, 116, 144, 220));
        seaGrad.setColorAt(0.3, QColor(21, 94, 117));
        seaGrad.setColorAt(1, QColor(15, 23, 42));

        auto seaItem = scene->addPath(seaPath, QPen(Qt::NoPen), QBrush(seaGrad));
        seaItem->setZValue(3);

        QPen foamPen(QColor(255, 255, 255, 85), 2.0);
        auto foamCrest = scene->addPath(seaPath, foamPen, Qt::NoBrush);
        foamCrest->setZValue(4);

        QPainterPath coastalShelf = seaPath;
        coastalShelf.translate(0, -6);
        scene->addPath(coastalShelf, QPen(QColor(103, 232, 249, 40), 4.0), Qt::NoBrush)->setZValue(3);

        // Map City Indicators
        auto drawTechUrbanSector = [&](double cx, double cy, double sw, double sh) {
            auto baseBox = scene->addRect(cx, cy, sw, sh, QPen(QColor(203, 213, 225, 180), 1), QBrush(QColor(226, 232, 240, 150)));
            baseBox->setZValue(1);
            int subBlocks = 4;
            double bw = sw / subBlocks;
            for (int k = 0; k < subBlocks; ++k) {
                if ((k + 1) % 2 == 0) {
                    auto inner = scene->addRect(cx + (k * bw) + 2, cy + 4, bw - 4, sh - 8, QPen(Qt::NoPen), QBrush(QColor(148, 163, 184, 80)));
                    inner->setZValue(1);
                }
            }
        };
        drawTechUrbanSector(w * 0.68, h * 0.32, 80, 50);
        drawTechUrbanSector(w * 0.40, h * 0.66, 95, 35);

        // Visualizing the Compass UI elements
        double compX = w - 65;
        double compY = 65;
        auto outRing = scene->addEllipse(compX - 35, compY - 35, 70, 70, QPen(QColor(71, 85, 105, 180), 1.5), Qt::NoBrush);
        auto inRing = scene->addEllipse(compX - 31, compY - 31, 62, 62, QPen(QColor(148, 163, 184, 120), 1, Qt::DotLine));
        outRing->setZValue(5); inRing->setZValue(5);

        QPolygonF nWBlade, nEBlade, sWBlade, sEBlade;
        nWBlade << QPointF(compX, compY) << QPointF(compX - 7, compY) << QPointF(compX, compY - 28);
        nEBlade << QPointF(compX, compY) << QPointF(compX + 7, compY) << QPointF(compX, compY - 28);
        sWBlade << QPointF(compX, compY) << QPointF(compX - 7, compY) << QPointF(compX, compY + 28);
        sEBlade << QPointF(compX, compY) << QPointF(compX + 7, compY) << QPointF(compX, compY + 28);

        scene->addPolygon(nWBlade, QPen(Qt::NoPen), QBrush(QColor(220, 38, 38)))->setZValue(5);
        scene->addPolygon(nEBlade, QPen(Qt::NoPen), QBrush(QColor(185, 28, 28)))->setZValue(5);
        scene->addPolygon(sWBlade, QPen(Qt::NoPen), QBrush(QColor(71, 85, 105)))->setZValue(5);
        scene->addPolygon(sEBlade, QPen(Qt::NoPen), QBrush(QColor(51, 65, 85)))->setZValue(5);

        auto nText = scene->addText("N");
        nText->setFont(QFont("Segoe UI", 9, QFont::Bold));
        nText->setDefaultTextColor(QColor(30, 41, 59));
        nText->setPos(compX - 7, compY - 47);
        nText->setZValue(5);

        // Visualizing the distance scale bar
        double scaleX = 35;
        double scaleY = h - 35;
        scene->addRect(scaleX, scaleY, 60, 5, QPen(Qt::NoPen), QBrush(QColor(30, 41, 59)))->setZValue(5);
        scene->addRect(scaleX + 60, scaleY, 60, 5, QPen(Qt::NoPen), QBrush(QColor(255, 255, 255)))->setZValue(5);
        auto borderLine = scene->addRect(scaleX, scaleY, 120, 5, QPen(QColor(30, 41, 59), 1), Qt::NoBrush);
        borderLine->setZValue(5);

        auto scaleText = scene->addText("0    100    200 km");
        scaleText->setFont(QFont("Consolas", 7, QFont::Bold));
        scaleText->setDefaultTextColor(QColor(30, 41, 59));
        scaleText->setPos(scaleX - 4, scaleY - 18);
        scaleText->setZValue(5);

        // Fetch city locations dynamically from physics algorithm
        vector<QPointF> positions = getWindingHighwayPositions(optimizer.numCities, w, h);

        // Draw connections (edges) between city pins if matrix value is greater than 0
        if (showAll) {
            QPen backPen(QColor(71, 85, 105, 120), 1.5);
            for (int i = 0; i < optimizer.numCities; i++) {
                for (int j = i + 1; j < optimizer.numCities; j++) {
                    if (optimizer.adjMatrix[i][j] > 0) {
                        scene->addLine(positions[i].x(), positions[i].y(),
                                       positions[j].x(), positions[j].y(), backPen);

                        // Attach matrix weight/distance values on the lines
                        int weight = optimizer.adjMatrix[i][j];
                        QPointF midPoint = (positions[i] + positions[j]) / 2.0;
                        auto wText = scene->addText(QString::number(weight));
                        wText->setDefaultTextColor(QColor(47, 61, 80));
                        wText->setFont(QFont("Consolas", 8, QFont::Bold));
                        wText->setPos(midPoint.x() - 8, midPoint.y() - 8);
                        wText->setZValue(6);
                    }
                }
            }
        }

        // Draw physical map pins for each coordinate
        for (int i = 0; i < optimizer.numCities; i++) {
            double px = positions[i].x();
            double py = positions[i].y();

            auto shadow = scene->addEllipse(px - 15, py - 6, 30, 12, QPen(Qt::NoPen), QBrush(QColor(0, 0, 0, 35)));
            shadow->setZValue(11);

            QPainterPath pinPath;
            pinPath.moveTo(0, 0);
            pinPath.cubicTo(-14, -24, -14, -40, 0, -40);
            pinPath.cubicTo(14, -40, 14, -24, 0, 0);
            pinPath.closeSubpath();

            QColor customPinColor = (i == 0) ? QColor(2, 132, 199) : QColor(220, 38, 38);
            auto pinItem = scene->addPath(pinPath, QPen(Qt::NoPen), QBrush(customPinColor));
            pinItem->setPos(px, py);
            pinItem->setZValue(12);
            canvasPinItems.push_back(pinItem); // Store pointer into list for rotation loop

            auto innerCore = scene->addEllipse(px - 3, py - 28, 6, 6, QPen(Qt::NoPen), QBrush(QColor(236, 240, 241)));
            innerCore->setZValue(13);
            canvasCoreItems.push_back(innerCore); // Store pointer into list for rotation loop

            // Adding labels over map pins
            QString nameStr = (i < optimizer.cityNames.size()) ? optimizer.cityNames[i] : QString("CITY %1").arg(i + 1);
            auto t = scene->addText(nameStr);
            t->setDefaultTextColor(QColor(15, 23, 42));
            t->setFont(QFont("Segoe UI", 9, QFont::Bold));

            if (py < h * 0.5) t->setPos(px - 30, py - 55);
            else t->setPos(px - 30, py + 12);
            t->setZValue(14);
        }
        isGraphGenerated = true;
    }

    // Prepare UI elements and timer for starting a path trace simulation
    void startHighlightAnimation(QString algoName, QColor color) {
        currentAlgoName = algoName;
        currentPathColor = color;
        contentStack->setCurrentIndex(0);

        isGraphGenerated = false;
        drawBaseGraph(false);

        statusLabel->setText("<b>" + algoName + " Running...</b><br>Optimizing route, please wait.");

        animStep = 0;
        animTimer->start(500); // 500ms delay between consecutive city trace lines
    }

    // Function called by timer per tick to draw route links connecting array indices
    void animatePath() {
        if (animStep >= (int)optimizer.lastPath.size() - 1) {
            animTimer->stop();
            // Trace complete, show summary data extracted from logic class
            QString pathStr = optimizer.getPathString();
            QString result = QString("<b>%1 Completed!</b><br>"
                                     "<b>Total Cost:</b> %2 | <b>Execution Time:</b> %3 ms<br>"
                                     "<b>Optimized Path:</b> %4")
                                 .arg(currentAlgoName)
                                 .arg(optimizer.lastCost)
                                 .arg(optimizer.lastTime, 0, 'f', 4)
                                 .arg(pathStr);
            statusLabel->setText(result);
            return;
        }

        int w = 800;
        int h = 670;
        // Retrieve dynamic coordinates
        vector<QPointF> positions = getWindingHighwayPositions(optimizer.numCities, w, h);

        // Fetch two connecting nodes using their indexes stored inside our route vector array
        int u = optimizer.lastPath[animStep];
        int v = optimizer.lastPath[animStep + 1];
        QPointF startPt = positions[u];
        QPointF endPt = positions[v];

        // Drawing styling for the traced route line
        QPen neonPen(currentPathColor.darker(110), 3);
        auto pathLine = scene->addLine(QLineF(startPt, endPt), neonPen);
        pathLine->setZValue(10);

        // Appending glowing visual effects specifically built for path traces
        QGraphicsBlurEffect *glow = new QGraphicsBlurEffect();
        glow->setBlurRadius(8);
        glow->setBlurHints(QGraphicsBlurEffect::AnimationHint);

        auto glowLine = scene->addLine(QLineF(startPt, endPt), QPen(currentPathColor, 6));
        glowLine->setGraphicsEffect(glow);
        glowLine->setZValue(9);
        glowLine->setOpacity(0.35);

        QPen highlightedPen(currentPathColor, 4);
        scene->addLine(QLineF(startPt, endPt), highlightedPen)->setZValue(5);

        // Add directional arrowhead math
        double lineAngle = std::atan2(endPt.y() - startPt.y(), endPt.x() - startPt.x());
        double arrowSize = 12.0;
        double offset = 8.0;
        QPointF tip(endPt.x() - offset * cos(lineAngle), endPt.y() - offset * sin(lineAngle));
        QPointF p1 = tip - QPointF(arrowSize * cos(lineAngle - M_PI / 6), arrowSize * sin(lineAngle - M_PI / 6));
        QPointF p2 = tip - QPointF(arrowSize * cos(lineAngle + M_PI / 6), arrowSize * sin(lineAngle + M_PI / 6));
        QPolygonF arrowHead;
        arrowHead << tip << p1 << p2;
        scene->addPolygon(arrowHead, QPen(currentPathColor), QBrush(currentPathColor))->setZValue(6);

        // Draw active weight values embedded in background box for visibility
        int weight = optimizer.adjMatrix[u][v];
        QPointF midPoint = (startPt + endPt) / 2.0;
        auto wText = scene->addText(QString::number(weight));
        wText->setDefaultTextColor(QColor(245, 247, 250));
        wText->setFont(QFont("Arial", 9, QFont::Bold));
        wText->setPos(midPoint.x() - 10, midPoint.y() - 12);
        wText->setZValue(20);
        QRectF textRect = wText->boundingRect();
        auto bgRect = scene->addRect(textRect.translated(wText->pos()), QPen(Qt::NoPen), QBrush(currentPathColor));
        bgRect->setZValue(15);
        bgRect->setOpacity(0.95);

        if (v != 0) {
            int nodeRadius = 8;
            auto glowCircle = scene->addEllipse(positions[v].x() - (nodeRadius + 4),
                                                positions[v].y() - (nodeRadius + 4),
                                                (nodeRadius + 4) * 2, (nodeRadius + 4) * 2,
                                                QPen(Qt::NoPen), QBrush(currentPathColor));
            glowCircle->setZValue(8);
            glowCircle->setOpacity(0.25);
            QGraphicsBlurEffect *blur = new QGraphicsBlurEffect();
            blur->setBlurRadius(8);
            glowCircle->setGraphicsEffect(blur);
        }
        animStep++;
    }

    // Signal slot bindings to trigger algorithm runs manually by pushing UI Buttons
    void onGreedy() {
        optimizer.runGreedy();
        startHighlightAnimation("Greedy", QColor(5, 150, 105));
    }
    void onBrute() { optimizer.runBruteForce(); startHighlightAnimation("Brute Force", QColor(29, 78, 216)); }
    void onCustom() { optimizer.runCustom(); startHighlightAnimation("Hybrid (Greedy + ACO  + 2-OPT)", QColor(15, 118, 110)); }

    // Generates a statistical comparison grid by silently running all internal algos
    void showComparison() {
        if (optimizer.numCities < 2) return;
        statusLabel->setText("Performance Report Loaded.");
        contentStack->setCurrentIndex(1); // Swap view page to tables

        optimizer.runGreedy();
        int gCost = optimizer.lastCost; double gTime = optimizer.lastTime; QString gPath = optimizer.getPathString();

        optimizer.runBruteForce();
        int bCost = optimizer.lastCost; double bTime = optimizer.lastTime; QString bPath = optimizer.getPathString();

        optimizer.runCustom();
        int cCost = optimizer.lastCost; double cTime = optimizer.lastTime; QString cPath = optimizer.getPathString();

        // Feed logic output data directly into Table cells
        compareTable->setItem(0, 0, new QTableWidgetItem("Greedy Search"));
        compareTable->setItem(0, 1, new QTableWidgetItem(QString::number(gCost)));
        compareTable->setItem(0, 2, new QTableWidgetItem(QString::number(gTime, 'f', 4) + " ms"));
        compareTable->setItem(1, 0, new QTableWidgetItem("Brute Force (Exact)"));
        compareTable->setItem(1, 1, new QTableWidgetItem(QString::number(bCost)));
        compareTable->setItem(1, 2, new QTableWidgetItem(QString::number(bTime, 'f', 4) + " ms"));
        compareTable->setItem(2, 0, new QTableWidgetItem("Hybrid (Custom Engine)"));
        compareTable->setItem(2, 1, new QTableWidgetItem(QString::number(cCost)));
        compareTable->setItem(2, 2, new QTableWidgetItem(QString::number(cTime, 'f', 4) + " ms"));

        for(int i=0; i<3; i++) {
            for(int j=0; j<3; j++) {
                compareTable->item(i,j)->setTextAlignment(Qt::AlignCenter);
                compareTable->item(i,j)->setForeground(QBrush(QColor(30, 41, 59)));
            }
        }

        QString summary = QString("<b style='color:#1d71d6;'>Greedy Path:</b> %1<br><br>"
                                  "<b style='color:#1d71d6;'>Brute Force Path:</b> %2<br><br>"
                                  "<b style='color:#1d71d6;'>Hybrid Path:</b> %3").arg(gPath).arg(bPath).arg(cPath);
        pathSummaryLabel->setText(summary);
    }

    // Function specifically configured to construct educational tabular representations (Adjacency Matrix vs Adjacency List)
    void showRepComparison() {
        if (optimizer.numCities == 0) return;
        contentStack->setCurrentIndex(2);

        // Theoretical memory calculations simulating structures
        int matrixMem = sizeof(int) * optimizer.numCities * optimizer.numCities;
        int edgeCount = optimizer.countEdges();
        int listMem = (optimizer.numCities * sizeof(void*)) + (edgeCount * 12);

        repTable->setItem(0, 0, new QTableWidgetItem("Adjacency Matrix (Static)"));
        repTable->setItem(0, 1, new QTableWidgetItem(QString::number(matrixMem) + " Bytes"));
        repTable->setItem(0, 2, new QTableWidgetItem("Fast O(1) Access"));
        repTable->setItem(1, 0, new QTableWidgetItem("Adjacency List (Dynamic)"));
        repTable->setItem(1, 1, new QTableWidgetItem(QString::number(listMem) + " Bytes"));
        repTable->setItem(1, 2, new QTableWidgetItem("O(E) Edge Traversal"));

        for(int i=0; i<2; i++) {
            for(int j=0; j<3; j++) {
                repTable->item(i,j)->setTextAlignment(Qt::AlignCenter);
                repTable->item(i,j)->setForeground(QBrush(QColor(30, 41, 59)));
            }
        }

        // Initialize and configure visual table matching matrix array dimensions
        matrixDisplayTable->setRowCount(optimizer.numCities);
        matrixDisplayTable->setColumnCount(optimizer.numCities);

        QStringList headers;
        for(int i=0; i<optimizer.numCities; i++) {
            headers << ((i < optimizer.cityNames.size()) ? optimizer.cityNames[i] : QString("CITY %1").arg(i + 1));
        }
        matrixDisplayTable->setHorizontalHeaderLabels(headers);
        matrixDisplayTable->setVerticalHeaderLabels(headers);
        matrixDisplayTable->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);

        // Loop array indexes to visually embed matrix entries directly onto UI rows/cols
        for(int i=0; i<optimizer.numCities; i++) {
            for(int j=0; j<optimizer.numCities; j++) {
                QTableWidgetItem *item = new QTableWidgetItem(QString::number(optimizer.adjMatrix[i][j]));
                item->setTextAlignment(Qt::AlignCenter);
                if(optimizer.adjMatrix[i][j] == 0) item->setForeground(QBrush(QColor(120, 144, 156)));
                else item->setForeground(QBrush(QColor(28, 63, 117)));
                matrixDisplayTable->setItem(i, j, item);
            }
        }

        listDisplayTable->setRowCount(optimizer.numCities);
        listDisplayTable->setColumnCount(1);
        listDisplayTable->setHorizontalHeaderLabels({"Linked Node Representation [ Data | ➔ ]"});
        listDisplayTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);

        // Dynamically build a stylized linked-list layout parsing active paths
        for(int i=0; i<optimizer.numCities; i++) {
            QString currentCityName = (i < optimizer.cityNames.size()) ? optimizer.cityNames[i] : QString("CITY %1").arg(i + 1);
            QString listLine = QString("[ %1 | ➔ ]").arg(currentCityName);
            for(int j=0; j<optimizer.numCities; j++) {
                if(optimizer.adjMatrix[i][j] > 0) {
                    QString neighborCityName = (j < optimizer.cityNames.size()) ? optimizer.cityNames[j] : QString("CITY %1").arg(j + 1);
                    listLine += QString(" ➔ [ %1 | %2 | ➔ ]")
                                    .arg(neighborCityName)
                                    .arg(optimizer.adjMatrix[i][j]);
                }
            }
            listLine += " ➔ [ NULL ]";
            QTableWidgetItem *listItem = new QTableWidgetItem(listLine);
            listItem->setFont(QFont("Courier New", 10, QFont::Bold));
            listItem->setForeground(QColor(15, 118, 110));
            listDisplayTable->setItem(i, 0, listItem);
        }
        statusLabel->setText("💾 Memory Analysis Updated.");
    }
};

// Entry point mapping application startup execution
int main(int argc, char *argv[]) {
    QApplication a(argc, argv);
    RouteUI w;
    w.show();
    return a.exec();
}

#include "main.moc"