/******************************************************
 *  2D Racing with Two-Player Mode + SFML
 ******************************************************/

#include <SFML/Graphics.hpp>
#include <cmath>
#include <vector>
#include <iostream>
#include <string>
#include <algorithm>
#include <chrono>

// -------------------- Constants --------------------
static const float PI = 3.14159265f;
static const float CHECKPOINT_RADIUS = 30.0f;

// -------------------- Utility Functions --------------------
float degToRad(float deg) {
    return deg * PI / 180.0f;
}

float distance(const sf::Vector2f& a, const sf::Vector2f& b) {
    float dx = a.x - b.x;
    float dy = a.y - b.y;
    return std::sqrt(dx * dx + dy * dy);
}

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

bool hasHitCheckpoint(const sf::Vector2f& carPosition, const sf::Vector2f& checkpointPosition) {
    return distance(carPosition, checkpointPosition) < CHECKPOINT_RADIUS;
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

    // Main rendering loop
    bool raceOver = false;
    std::string winner;

    while (window.isOpen()) {
        sf::Event event;
        while (window.pollEvent(event)) {
            if (event.type == sf::Event::Closed) {
                window.close();
            }
        }

        if (!raceOver) {
            // Player 1 Controls (WASD)
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
            sf::Vector2f oldPlayerPos = playerCar.getPosition();
            playerCar.rotate(playerRotation);
            float angle = degToRad(playerCar.getRotation());
            playerCar.move(std::cos(angle) * playerSpeed, std::sin(angle) * playerSpeed);

            // Check for collision and adjust position if necessary
            if (!isWithinBorders(playerCar, playerSpeed, trackBorders)) {
                // No need to reset position, as bounce is handled in isWithinBorders
            }

            // Check if player hits checkpoint
            if (hasHitCheckpoint(playerCar.getPosition(), checkpointPositions[playerCurrentCheckpoint])) {
                playerCurrentCheckpoint++;
                playerCheckpointsHit++;
                if (playerCurrentCheckpoint >= checkpointPositions.size()) {
                    playerCurrentCheckpoint = 0; // Loop back to first checkpoint
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
                    direction /= distanceToTarget;
                    aiCar.move(direction * aiSpeed);

                    if (!isWithinBorders(aiCar, aiSpeed, trackBorders)) {
                        // No need to reset position, as bounce is handled in isWithinBorders
                    }

                    float targetAngle = std::atan2(direction.y, direction.x) * 180.f / PI;
                    aiCar.setRotation(targetAngle);
                }
            }

            // Check if AI hits checkpoint
            if (hasHitCheckpoint(aiCar.getPosition(), checkpointPositions[aiCurrentCheckpoint])) {
                aiCurrentCheckpoint++;
                aiCheckpointsHit++;
                if (aiCurrentCheckpoint >= checkpointPositions.size()) {
                    aiCurrentCheckpoint = 0; // Loop back to first checkpoint
                }
            }

            // Check if the race is over
            if (playerCheckpointsHit >= 4) {
                raceOver = true;
                winner = "Player 1";
            } else if (aiCheckpointsHit >= 4) {
                raceOver = true;
                winner = "AI";
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
        if (raceOver) {
            sf::Font font;
            if (!font.loadFromFile("arial.ttf")) {
                std::cerr << "Failed to load font!\n";
                return -1;
            }

            sf::Text resultText;
            resultText.setFont(font);
            resultText.setString(winner + " Wins!");
            resultText.setCharacterSize(48);
            resultText.setFillColor(sf::Color::White);
            resultText.setPosition(400.f, 350.f);
            window.draw(resultText);
        }

        // Display checkpoint status
        sf::Font font;
        if (font.loadFromFile("arial.ttf")) {
            sf::Text checkpointStatus;
            checkpointStatus.setFont(font);
            checkpointStatus.setCharacterSize(24);
            checkpointStatus.setFillColor(sf::Color::White);
            checkpointStatus.setPosition(10.f, 10.f);

            std::string status = "Player 1: " + std::to_string(playerCheckpointsHit) + "/4\n";
            status += "AI: " + std::to_string(aiCheckpointsHit) + "/4\n";

            checkpointStatus.setString(status);
            window.draw(checkpointStatus);
        }

        window.display();
    }

    return 0;
}
