/******************************************************
 *  2D Racing with Two-Player Mode + Simple AI Optimization + SFML
 ******************************************************/

#include <SFML/Graphics.hpp>
#include <cmath>
#include <vector>
#include <iostream>
#include <string>
#include <algorithm>
#include <chrono>
#include <random>
#include <iomanip>

// -------------------- Constants --------------------
static const float PI = 3.14159265f;
static const float CHECKPOINT_RADIUS = 30.0f;
static const size_t POPULATION_SIZE = 20;
static const int GENERATIONS = 3; // Number of pre-races for optimization
static const float MUTATION_RATE = 0.05f; // Mutation rate for waypoint adjustments

// -------------------- Utility Functions --------------------
float degToRad(float deg) {
    return deg * PI / 180.0f;
}

float radToDeg(float rad) {
    return rad * 180.0f / PI;
}

float distance(const sf::Vector2f& a, const sf::Vector2f& b) {
    float dx = a.x - b.x;
    float dy = a.y - b.y;
    return std::sqrt(dx * dx + dy * dy);
}

// Checks if the car is within track borders and handles collision
bool isWithinBorders(sf::Sprite& car, float& speed, const std::vector<sf::RectangleShape>& borders) {
    for (const auto& border : borders) {
        if (car.getGlobalBounds().intersects(border.getGlobalBounds())) {
            // Stop the car
            speed = 0.0f;

            // Move car slightly back in the opposite direction
            float currentAngle = car.getRotation();
            sf::Vector2f direction(-std::cos(degToRad(currentAngle)), -std::sin(degToRad(currentAngle)));
            car.move(direction * 5.f);

            return false;
        }
    }
    return true;
}

// Checks if the car has hit a checkpoint
bool hasHitCheckpoint(const sf::Vector2f& carPosition, const sf::Vector2f& checkpointPosition) {
    return distance(carPosition, checkpointPosition) < CHECKPOINT_RADIUS;
}

// -------------------- AI Optimization Structures --------------------
struct AIIndividual {
    std::vector<sf::Vector2f> waypoints;
    float fitness; // Lower is better
};

// -------------------- Simulation Function --------------------
// Simulates the AI car running through the waypoints and calculates fitness
float simulateRun(const std::vector<sf::Vector2f>& waypoints, const std::vector<sf::RectangleShape>& borders, float aiSpeed) {
    // Create a temporary AI car sprite for simulation
    sf::Texture dummyTexture;
    dummyTexture.create(40, 20); // Create a dummy texture
    sf::Sprite tempAiCar(dummyTexture);
    tempAiCar.setOrigin(dummyTexture.getSize().x / 2.0f, dummyTexture.getSize().y / 2.0f);
    tempAiCar.setPosition(waypoints[0]);
    tempAiCar.setRotation(0.f);

    size_t currentWaypoint = 0;
    float totalTime = 0.0f;
    float speed = aiSpeed;
    const float TIME_STEP = 1.0f / 60.0f; // Simulate at 60 FPS
    int collisionCount = 0;

    while (currentWaypoint < waypoints.size()) {
        sf::Vector2f target = waypoints[currentWaypoint];
        sf::Vector2f direction = target - tempAiCar.getPosition();
        float distanceToTarget = distance(tempAiCar.getPosition(), target);

        if (distanceToTarget < 10.0f) {
            currentWaypoint++;
            continue;
        }

        // Normalize direction
        if (distanceToTarget != 0) {
            direction /= distanceToTarget;
        }

        // Move AI car
        tempAiCar.move(direction * speed);

        // Set rotation towards movement direction
        float targetAngle = radToDeg(std::atan2(direction.y, direction.x));
        tempAiCar.setRotation(targetAngle);

        // Check for collision
        if (!isWithinBorders(tempAiCar, speed, borders)) {
            collisionCount++;
            totalTime += TIME_STEP * 2; // Penalize time for collision
        }

        totalTime += TIME_STEP;
    }

    // Fitness calculation: lower time and fewer collisions are better
    float fitness = totalTime + (collisionCount * 5.0f); // Each collision adds a penalty
    return fitness;
}

// -------------------- Optimization Function --------------------
// Optimizes the AI waypoints by running pre-races and adjusting waypoints based on performance
std::vector<sf::Vector2f> optimizeWaypoints(std::vector<sf::Vector2f> waypoints, const std::vector<sf::RectangleShape>& borders, float aiSpeed, int generations) {
    std::mt19937 rng(std::random_device{}());
    std::uniform_real_distribution<float> mutationDist(-20.0f, 20.0f); // Mutation range

    float bestFitness = simulateRun(waypoints, borders, aiSpeed);
    std::vector<sf::Vector2f> bestWaypoints = waypoints;

    std::cout << "Starting AI Optimization...\n";

    for (int gen = 1; gen <= generations; ++gen) {
        // Create mutated waypoints
        std::vector<sf::Vector2f> mutatedWaypoints = waypoints;
        for (auto& wp : mutatedWaypoints) {
            wp.x += mutationDist(rng);
            wp.y += mutationDist(rng);
        }

        // Simulate the mutated waypoints
        float fitness = simulateRun(mutatedWaypoints, borders, aiSpeed);
        std::cout << "Pre-Race " << gen << " - Fitness: " << fitness << " (Best: " << bestFitness << ")\n";

        // If mutated waypoints are better, keep them
        if (fitness < bestFitness) {
            bestFitness = fitness;
            bestWaypoints = mutatedWaypoints;
            std::cout << "Improved waypoints in Pre-Race " << gen << "!\n";
        } else {
            std::cout << "No improvement in Pre-Race " << gen << ".\n";
        }
    }

    std::cout << "AI Optimization Complete! Best Fitness: " << bestFitness << "\n\n";
    return bestWaypoints;
}

// -------------------- Main Function --------------------
int main() {
    // Create a simple rectangular track with rounded corners
    std::vector<sf::Vector2f> trainingWaypoints = {
        {200, 400}, {400, 400}, {600, 400}, {800, 400},
        {900, 400}, {900, 300}, {900, 200}, {800, 200},
        {600, 200}, {400, 200}, {200, 200}, {200, 300}, {200, 400}
    };

    // Define checkpoints for evaluation and visualization
    std::vector<sf::Vector2f> checkpointPositions = {
        {500, 400}, {900, 300}, {500, 200}, {200, 300}
    };

    // Load textures
    sf::Texture player1Texture, player2Texture;
    if (!player1Texture.loadFromFile("player1.png") ||
        !player2Texture.loadFromFile("player2.png")) {
        std::cerr << "Error loading car textures! Make sure player1.png & player2.png exist.\n";
        return -1;
    }

    // Load shader
    sf::Shader blueShader;
    if (!blueShader.loadFromFile("blue_shader.frag", sf::Shader::Fragment)) {
        std::cerr << "Error loading shader!\n";
        return -1;
    }
    blueShader.setUniform("texture", sf::Shader::CurrentTexture);
    blueShader.setUniform("color", sf::Glsl::Vec4(0.0f, 0.0f, 1.0f, 1.0f)); // Blue color

    // Create window
    sf::RenderWindow window(sf::VideoMode(1000, 800), "2D Racing - Two Player Mode");
    window.setFramerateLimit(60);

    // Player car sprite
    sf::Sprite playerCar(player1Texture);
    playerCar.setScale(40.0f / player1Texture.getSize().x, 20.0f / player1Texture.getSize().y);
    playerCar.setOrigin(player1Texture.getSize().x / 2.0f, player1Texture.getSize().y / 2.0f);
    playerCar.setPosition(trainingWaypoints[0]);

    // AI car sprite
    sf::Sprite aiCar(player2Texture);
    aiCar.setScale(40.0f / player2Texture.getSize().x, 20.0f / player2Texture.getSize().y);
    aiCar.setOrigin(player2Texture.getSize().x / 2.0f, player2Texture.getSize().y / 2.0f);
    aiCar.setPosition(trainingWaypoints[0]);

    // Define AI waypoints (these should be more detailed than checkpoints)
    std::vector<sf::Vector2f> aiWaypoints = {
        {200, 400}, {300, 400}, {400, 400}, {500, 400}, {600, 400}, {700, 400}, {800, 400},
        {900, 400}, {900, 350}, {900, 300}, {900, 250}, {900, 200}, {800, 200}, {700, 200},
        {600, 200}, {500, 200}, {400, 200}, {300, 200}, {200, 200}, {200, 250}, {200, 300},
        {200, 350}, {200, 400}
    };

    // AI car variables
    size_t aiCurrentWaypoint = 0;
    float aiSpeed = 3.0f;

    // Checkpoint tracking
    size_t playerCurrentCheckpoint = 0;
    size_t playerCheckpointsHit = 0;
    size_t aiCurrentCheckpoint = 0;
    size_t aiCheckpointsHit = 0;

    // Prepare track rendering
    const float TRACK_WIDTH = 80.f;
    std::vector<sf::ConvexShape> trackSegments;
    for (size_t i = 0; i < trainingWaypoints.size() - 1; i++) {
        sf::Vector2f current = trainingWaypoints[i];
        sf::Vector2f next    = trainingWaypoints[i + 1];
        sf::Vector2f dir     = next - current;
        float length         = std::sqrt(dir.x * dir.x + dir.y * dir.y);
        if (length > 0) {
            dir /= length;
            sf::Vector2f normal(-dir.y, dir.x);

            // Make a quad for the track segment
            sf::ConvexShape seg;
            seg.setPointCount(4);
            seg.setPoint(0, current + normal * (TRACK_WIDTH / 2.f));
            seg.setPoint(1, next    + normal * (TRACK_WIDTH / 2.f));
            seg.setPoint(2, next    - normal * (TRACK_WIDTH / 2.f));
            seg.setPoint(3, current - normal * (TRACK_WIDTH / 2.f));
            seg.setFillColor(sf::Color(80, 80, 80));
            trackSegments.push_back(seg);
        }
    }

    // Build track borders
    std::vector<sf::RectangleShape> trackBorders;

    // Function to add a border segment
    auto addBorderSegment = [&](const sf::Vector2f& start, const sf::Vector2f& end) {
        sf::Vector2f diff = end - start;
        float length = std::sqrt(diff.x * diff.x + diff.y * diff.y);

        sf::RectangleShape border(sf::Vector2f(length, 5.f));
        border.setPosition(start);
        border.setFillColor(sf::Color::Red);

        // Calculate rotation
        float rotation = std::atan2(diff.y, diff.x) * 180.f / PI;
        border.setRotation(rotation);

        trackBorders.push_back(border);
    };

    // Outer border coordinates (clockwise)
    std::vector<sf::Vector2f> outerBorder = {
        {150, 450}, {950, 450}, {950, 150}, {150, 150}, {150, 450}
    };

    // Inner border coordinates (clockwise)
    std::vector<sf::Vector2f> innerBorder = {
        {250, 350}, {850, 350}, {850, 250}, {250, 250}, {250, 350}
    };

    // Create border segments
    for (size_t i = 0; i < outerBorder.size() - 1; i++) {
        addBorderSegment(outerBorder[i], outerBorder[i + 1]);
        addBorderSegment(innerBorder[i], innerBorder[i + 1]);
    }

    // Optional "checkpoints" for visualization
    std::vector<sf::RectangleShape> checkpointShapes;
    for (size_t i = 0; i < checkpointPositions.size(); i++) {
        sf::RectangleShape cp(sf::Vector2f(TRACK_WIDTH, 10.f));
        cp.setOrigin(TRACK_WIDTH / 2.f, 5.f);
        cp.setPosition(checkpointPositions[i]);
        cp.setFillColor(sf::Color::Yellow);
        // Quick orientation hack
        if (i == 0 || i == 2)
            cp.setRotation(90.f);
        checkpointShapes.push_back(cp);
    }

    // Player controls
    float playerSpeed = 0.0f;
    float playerRotation = 0.0f;

    // -------------------- AI Optimization Phase --------------------
    // Optimize AI waypoints using simple pre-races
    aiWaypoints = optimizeWaypoints(aiWaypoints, trackBorders, aiSpeed, GENERATIONS);

    // Reset AI car position after optimization
    aiCar.setPosition(trainingWaypoints[0]);
    aiCurrentWaypoint = 0;

    // -------------------- Main Game Loop --------------------
    bool raceOver = false;
    std::string winner;

    // Load font for in-game text
    sf::Font font;
    if (!font.loadFromFile("arial.ttf")) {
        std::cerr << "Failed to load font!\n";
        // You can set SHOW_DEBUG_TEXT to false to avoid displaying text
    }

    while (window.isOpen()) {
        sf::Event event;
        while (window.pollEvent(event)) {
            if (event.type == sf::Event::Closed)
                window.close();
        }

        if (!raceOver) {
            // Player Controls (WASD)
            playerSpeed = 0.0f;
            playerRotation = 0.0f;

            if (sf::Keyboard::isKeyPressed(sf::Keyboard::W))
                playerSpeed = 5.0f;
            if (sf::Keyboard::isKeyPressed(sf::Keyboard::S))
                playerSpeed = -3.0f;
            if (sf::Keyboard::isKeyPressed(sf::Keyboard::A))
                playerRotation = -3.0f;
            if (sf::Keyboard::isKeyPressed(sf::Keyboard::D))
                playerRotation = 3.0f;

            // Update player car position
            playerCar.rotate(playerRotation);
            float angle = degToRad(playerCar.getRotation());
            playerCar.move(std::cos(angle) * playerSpeed, std::sin(angle) * playerSpeed);

            // Check for collision and adjust position if necessary
            if (!isWithinBorders(playerCar, playerSpeed, trackBorders)) {
                // Collision handled in isWithinBorders
            }

            // Check if player hits checkpoint
            if (playerCurrentCheckpoint < checkpointPositions.size()) {
                if (hasHitCheckpoint(playerCar.getPosition(), checkpointPositions[playerCurrentCheckpoint])) {
                    playerCheckpointsHit++;
                    playerCurrentCheckpoint++;
                    std::cout << "Player hit checkpoint " << playerCheckpointsHit << "\n";
                    if (playerCurrentCheckpoint >= checkpointPositions.size()) {
                        playerCurrentCheckpoint = 0; // Loop back to first checkpoint
                    }
                }
            }

            // AI car logic: move towards the next waypoint
            if (aiCurrentWaypoint < aiWaypoints.size()) {
                sf::Vector2f target = aiWaypoints[aiCurrentWaypoint];
                sf::Vector2f direction = target - aiCar.getPosition();
                float distanceToTarget = distance(aiCar.getPosition(), target);

                if (distanceToTarget < 10.0f) { // If close to the waypoint, move to the next
                    aiCurrentWaypoint++;
                    if (aiCurrentWaypoint >= aiWaypoints.size()) {
                        aiCurrentWaypoint = 0; // Loop back to the first waypoint
                    }
                } else {
                    direction /= distanceToTarget; // Normalize direction
                    aiCar.move(direction * aiSpeed);

                    if (!isWithinBorders(aiCar, aiSpeed, trackBorders)) {
                        // Collision handled in isWithinBorders
                    }

                    float targetAngle = std::atan2(direction.y, direction.x) * 180.f / PI;
                    aiCar.setRotation(targetAngle);
                }
            }

            // Check if AI hits checkpoint
            if (aiCurrentCheckpoint < checkpointPositions.size()) {
                if (hasHitCheckpoint(aiCar.getPosition(), checkpointPositions[aiCurrentCheckpoint])) {
                    aiCheckpointsHit++;
                    aiCurrentCheckpoint++;
                    std::cout << "AI hit checkpoint " << aiCheckpointsHit << "\n";
                    if (aiCurrentCheckpoint >= checkpointPositions.size()) {
                        aiCurrentCheckpoint = 0; // Loop back to first checkpoint
                    }
                }
            }

            // Check if the race is over
            if (playerCheckpointsHit >= checkpointPositions.size()) {
                raceOver = true;
                winner = "Player";
                std::cout << "Player Wins!\n";
            } else if (aiCheckpointsHit >= checkpointPositions.size()) {
                raceOver = true;
                winner = "AI";
                std::cout << "AI Wins!\n";
            }
        }

        // Draw everything
        window.clear(sf::Color(0, 100, 0)); // Green background

        // Track
        for (auto& seg : trackSegments) {
            window.draw(seg);
        }

        // Borders
        for (auto& border : trackBorders) {
            window.draw(border);
        }

        // Checkpoints
        for (size_t i = 0; i < checkpointShapes.size(); i++) {
            window.draw(checkpointShapes[i]);
        }

        // Player car
        window.draw(playerCar);

        // AI car with blue filter
        window.draw(aiCar, &blueShader);

        // Display race results if finished
        if (raceOver && font.getInfo().family != "") {
            sf::Text resultText;
            resultText.setFont(font);
            resultText.setString(winner + " Wins!");
            resultText.setCharacterSize(48);
            resultText.setFillColor(sf::Color::White);
            resultText.setPosition(400.f, 350.f);
            window.draw(resultText);
        }

        // Display checkpoint status
        if (font.getInfo().family != "") {
            sf::Text checkpointStatus;
            checkpointStatus.setFont(font);
            checkpointStatus.setCharacterSize(24);
            checkpointStatus.setFillColor(sf::Color::White);
            checkpointStatus.setPosition(10.f, 10.f);

            std::string status = "Player: " + std::to_string(playerCheckpointsHit) + "/" + std::to_string(checkpointPositions.size()) + "\n";
            status += "AI: " + std::to_string(aiCheckpointsHit) + "/" + std::to_string(checkpointPositions.size());

            checkpointStatus.setString(status);
            window.draw(checkpointStatus);
        }

        window.display();
    }

    return 0;
}
