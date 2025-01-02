#include <SFML/Graphics.hpp>
#include <cmath>
#include <vector>
#include <iostream>
#include <string>

static const float PI = 3.14159265f;

// Convert degrees to radians
float degToRad(float deg) {
    return deg * PI / 180.0f;
}

// A function to measure distance between two points
float distance(sf::Vector2f a, sf::Vector2f b) {
    float dx = a.x - b.x;
    float dy = a.y - b.y;
    return std::sqrt(dx * dx + dy * dy);
}

int main() {
    // ---------------------------------------------------------
    // 1. Create window
    // ---------------------------------------------------------
    sf::RenderWindow window(sf::VideoMode(1000, 800), "2D Racing - Borders & Checkpoints");
    window.setFramerateLimit(60);

    // Center of the track
    sf::Vector2f center(window.getSize().x / 2.0f, window.getSize().y / 2.0f);

    // ---------------------------------------------------------
    // 2. Track geometry: ring
    //    We'll define outer radius & inner radius
    // ---------------------------------------------------------
    float outerRadius = 300.0f;
    float innerRadius = 200.0f;

    // Outer circle (visual)
    sf::CircleShape outerCircle(outerRadius);
    outerCircle.setFillColor(sf::Color(80, 80, 80)); // dark grey
    outerCircle.setOrigin(outerRadius, outerRadius);
    outerCircle.setPosition(center);

    // Inner circle (visual "infield")
    sf::CircleShape innerCircle(innerRadius);
    innerCircle.setFillColor(sf::Color(0, 150, 0)); // green infield
    innerCircle.setOrigin(innerRadius, innerRadius);
    innerCircle.setPosition(center);

    // ---------------------------------------------------------
    // 3. Start/Finish line
    //    We'll place a small rectangle near the top of the ring
    // ---------------------------------------------------------
    sf::RectangleShape finishLine(sf::Vector2f(10.0f, 60.0f));
    finishLine.setFillColor(sf::Color::White);
    finishLine.setOrigin(5.0f, 30.0f); // center the line
    // position it at an angle ~ 90 deg from x-axis (top of circle)
    // top = center + (outerRadius * [0, -1]) in local coords, but let's offset inward
    finishLine.setPosition(center.x, center.y - outerRadius + 20.0f);

    // We'll define an angle for the finish line (i.e. 270 degrees if 0 deg is to the right),
    // but for simplicity, let's keep it vertical:
    // 0 deg rotation in SFML = pointing right; 90 deg = pointing down, etc.
    finishLine.setRotation(0.0f); // keep it vertical

    // ---------------------------------------------------------
    // 4. Define checkpoints around the circle
    //    We'll pick angles around the track for each checkpoint
    // ---------------------------------------------------------
    std::vector<float> checkpointAngles = { 0.0f, 90.0f, 180.0f, 270.0f };
    // That means we have 4 checkpoints around the circle: 
    //   angle=0   (right side)
    //   angle=90  (down)
    //   angle=180 (left)
    //   angle=270 (up)
    // They must be visited in that order (0 -> 1 -> 2 -> 3).
    // After the last checkpoint, crossing the finish line means you finish.

    // We'll store checkpoint shapes in a vector so we can draw them
    std::vector<sf::RectangleShape> checkpointShapes;
    for (float ang : checkpointAngles) {
        float rad = degToRad(ang);
        // Calculate center position
        float rMid = (outerRadius + innerRadius) / 2.0f;
        sf::Vector2f pos;
        pos.x = center.x + rMid * std::cos(rad);
        pos.y = center.y + rMid * std::sin(rad);

        // Create rectangle for checkpoint
        sf::RectangleShape cp(sf::Vector2f(outerRadius - innerRadius, 10.0f));  // width spans the track, 10 pixels thick
        cp.setFillColor(sf::Color::Yellow);
        cp.setOrigin(cp.getSize().x / 2.0f, cp.getSize().y / 2.0f);
        cp.setPosition(pos);
        cp.setRotation(ang);  // Rotate to align with track radius
        checkpointShapes.push_back(cp);
    }

    // We'll track if a racer has 'activated' each checkpoint in order
    // For each racer, 'nextCheckpointIndex' is which checkpoint they must get next.
    // If nextCheckpointIndex == checkpointAngles.size(), then the next crossing 
    // of the finish line ends the race.

    // ---------------------------------------------------------
    // 5. Car shapes (Player & AI)
    // ---------------------------------------------------------
    sf::RectangleShape playerCar(sf::Vector2f(40.0f, 20.0f));
    playerCar.setFillColor(sf::Color::Green);
    playerCar.setOrigin(20.0f, 10.0f);

    // We'll start the player near the finish line, slightly behind it
    float startAngle = -90.0f;
    float startRadius = (outerRadius + innerRadius) / 2.0f;
    float sr = degToRad(startAngle);
    sf::Vector2f startPos(center.x + startRadius * std::cos(sr),
                         center.y + startRadius * std::sin(sr));
    playerCar.setPosition(startPos);
    playerCar.setRotation(90.0f);

    // Player movement variables
    float playerSpeed = 0.0f;
    const float PLAYER_MAX_SPEED = 3.0f;
    const float ACCEL = 0.15f;
    const float DECEL = 0.08f;
    const float TURN_SPEED = 3.5f;

    bool playerFinished = false;
    int playerNextCheckpoint = 0; // index into checkpointAngles

    // AI
    sf::RectangleShape aiCar(sf::Vector2f(40.0f, 20.0f));
    aiCar.setFillColor(sf::Color::Red);
    aiCar.setOrigin(20.0f, 10.0f);

    // Start AI near the player, but offset
    float aiStartAngle = -90.0f;
    float aiStartRadius = (outerRadius + innerRadius) / 2.0f + 40.0f;
    float aiRad = degToRad(aiStartAngle);
    sf::Vector2f aiPos(center.x + aiStartRadius * std::cos(aiRad),
                      center.y + aiStartRadius * std::sin(aiRad));
    aiCar.setPosition(aiPos);
    aiCar.setRotation(90.0f);

    float aiSpeed = 0.0f;
    const float AI_MAX_SPEED = 3.5f;
    const float AI_ACCEL = 0.12f;
    const float AI_DECEL = 0.08f;
    bool aiFinished = false;
    int aiNextCheckpoint = 0;

    // We'll define AI waypoints around the ring, much like the checkpointAngles, 
    // but with smaller angular increments so it can navigate smoothly.
    std::vector<sf::Vector2f> aiWaypoints;
    {
        // For instance, generate points every 10 degrees along the middle of the ring
        float rMid = (outerRadius + innerRadius) / 2.0f;
        for (float a = 0.0f; a < 360.0f; a += 10.0f) {
            float rr = degToRad(a);
            sf::Vector2f p(center.x + rMid * std::cos(rr),
                           center.y + rMid * std::sin(rr));
            aiWaypoints.push_back(p);
        }
    }
    int aiCurrentWaypoint = 0;

    // A small helper to keep track of whether the race is over
    bool raceOver = false;
    bool playerWon = false;

    // ---------------------------------------------------------
    // 6. Game Loop
    // ---------------------------------------------------------
    while (window.isOpen()) {
        // ---------------------------------
        // Handle Events
        // ---------------------------------
        sf::Event event;
        while (window.pollEvent(event)) {
            if (event.type == sf::Event::Closed) {
                window.close();
            }
        }

        if (!raceOver) {
            // ---------------------------------
            // Player Controls
            // ---------------------------------
            if (sf::Keyboard::isKeyPressed(sf::Keyboard::W)) {
                playerSpeed += ACCEL;
                if (playerSpeed > PLAYER_MAX_SPEED) {
                    playerSpeed = PLAYER_MAX_SPEED;
                }
            } 
            else if (sf::Keyboard::isKeyPressed(sf::Keyboard::S)) {
                playerSpeed -= ACCEL;
                if (playerSpeed < -PLAYER_MAX_SPEED/2.0f) {
                    playerSpeed = -PLAYER_MAX_SPEED/2.0f; 
                }
            } 
            else {
                // decelerate naturally
                if (playerSpeed > 0) {
                    playerSpeed -= DECEL;
                    if (playerSpeed < 0) playerSpeed = 0;
                } else {
                    playerSpeed += DECEL;
                    if (playerSpeed > 0) playerSpeed = 0;
                }
            }

            if (sf::Keyboard::isKeyPressed(sf::Keyboard::A)) {
                playerCar.rotate(-TURN_SPEED);
            } else if (sf::Keyboard::isKeyPressed(sf::Keyboard::D)) {
                playerCar.rotate(TURN_SPEED);
            }

            // Attempt to move the player
            float angleRad = degToRad(playerCar.getRotation());
            float vx = std::cos(angleRad) * playerSpeed;
            float vy = std::sin(angleRad) * playerSpeed;

            // We'll store the old position in case we need to revert
            sf::Vector2f oldPos = playerCar.getPosition();
            // Tentatively move
            playerCar.move(vx, vy);

            // Check ring boundary
            float distFromCenter = distance(playerCar.getPosition(), center);
            if (distFromCenter < innerRadius || distFromCenter > outerRadius) {
                // Revert the move
                playerCar.setPosition(oldPos);
            }

            // ---------------------------------
            // Check if player hits a checkpoint
            // ---------------------------------
            if (!playerFinished) {
                if (playerNextCheckpoint < (int)checkpointAngles.size()) {
                    // Check if we overlap next checkpoint using rectangle bounds
                    sf::FloatRect cpBounds = checkpointShapes[playerNextCheckpoint].getGlobalBounds();
                    sf::FloatRect playerBounds = playerCar.getGlobalBounds();
                    if (cpBounds.intersects(playerBounds)) {
                        playerNextCheckpoint++;
                    }
                } else {
                    // Check finish line after all checkpoints
                    sf::FloatRect lineRect = finishLine.getGlobalBounds();
                    sf::FloatRect playerRect = playerCar.getGlobalBounds();
                    if (lineRect.intersects(playerRect)) {
                        playerFinished = true;
                        raceOver = true;
                        playerWon = true;
                    }
                }
            }

            // ---------------------------------
            // AI Movement
            // ---------------------------------
            if (!aiFinished) {
                // AI heads toward aiWaypoints[aiCurrentWaypoint]
                sf::Vector2f aiP = aiCar.getPosition();
                sf::Vector2f waypoint = aiWaypoints[aiCurrentWaypoint];
                sf::Vector2f dir = waypoint - aiP;
                float distToWaypoint = distance(aiP, waypoint);

                // If close to current waypoint, go to next
                if (distToWaypoint < 15.0f) {
                    aiCurrentWaypoint = (aiCurrentWaypoint + 1) % aiWaypoints.size();
                }

                // Desired angle
                float desiredAngle = std::atan2(dir.y, dir.x) * 180.0f / PI;
                // Current angle
                float currentAngle = aiCar.getRotation();

                // Normalize angle difference to (-180, 180)
                float angleDiff = desiredAngle - currentAngle;
                while (angleDiff > 180.0f) angleDiff -= 360.0f;
                while (angleDiff < -180.0f) angleDiff += 360.0f;

                // Turn a bit
                float turnStep = 2.0f;
                if (angleDiff > 0) {
                    aiCar.rotate(std::min(turnStep, angleDiff));
                } else {
                    aiCar.rotate(std::max(-turnStep, angleDiff));
                }

                // Speed control: if not at waypoint, accelerate
                if (distToWaypoint > 20.0f) {
                    aiSpeed += AI_ACCEL;
                    if (aiSpeed > AI_MAX_SPEED) aiSpeed = AI_MAX_SPEED;
                } else {
                    aiSpeed -= AI_DECEL;
                    if (aiSpeed < 0.0f) aiSpeed = 0.0f;
                }

                // Move AI
                float aiAngleRad = degToRad(aiCar.getRotation());
                float avx = std::cos(aiAngleRad) * aiSpeed;
                float avy = std::sin(aiAngleRad) * aiSpeed;

                // Check ring boundary
                sf::Vector2f oldAIPos = aiCar.getPosition();
                aiCar.move(avx, avy);

                float aiDistCenter = distance(aiCar.getPosition(), center);
                if (aiDistCenter < innerRadius || aiDistCenter > outerRadius) {
                    // revert
                    aiCar.setPosition(oldAIPos);
                }

                // ---------------------------------
                // AI checkpoint logic
                // ---------------------------------
                if (aiNextCheckpoint < (int)checkpointAngles.size()) {
                    sf::FloatRect cpBounds = checkpointShapes[aiNextCheckpoint].getGlobalBounds();
                    sf::FloatRect aiBounds = aiCar.getGlobalBounds();
                    if (cpBounds.intersects(aiBounds)) {
                        aiNextCheckpoint++;
                    }
                }
                else {
                    // AI visited all checkpoints, check finish line
                    sf::FloatRect lineRect = finishLine.getGlobalBounds();
                    sf::FloatRect aiRect = aiCar.getGlobalBounds();
                    if (lineRect.intersects(aiRect)) {
                        aiFinished = true;
                        if (!playerFinished) {  // If player hasn't finished yet
                            raceOver = true;
                            playerWon = false;
                        }
                    }
                }
            }

            // If both finished, determine winner by who finished first
            if (playerFinished && aiFinished && !raceOver) {
                raceOver = true;
                // Winner already determined by who triggered raceOver first
            }
        } 
        else {
            // Race is over: let user press ESC to close
            if (sf::Keyboard::isKeyPressed(sf::Keyboard::Escape)) {
                window.close();
            }
        }

        // ---------------------------------
        // Rendering
        // ---------------------------------
        window.clear(sf::Color(0, 100, 0)); // background

        // Draw outer circle
        window.draw(outerCircle);
        // Draw inner circle
        window.draw(innerCircle);

        // Draw finish line
        window.draw(finishLine);

        // Draw checkpoints (only the ones not yet collected)
        for (size_t i = 0; i < checkpointShapes.size(); i++) {
            // For player's next checkpoint, make it yellow
            // For passed checkpoints, don't draw them
            // For future checkpoints, make them gray
            if (i == playerNextCheckpoint) {
                checkpointShapes[i].setFillColor(sf::Color::Yellow);
                window.draw(checkpointShapes[i]);
            } else if (i > playerNextCheckpoint) {
                checkpointShapes[i].setFillColor(sf::Color(128, 128, 128)); // Gray
                window.draw(checkpointShapes[i]);
            }
        }

        // Draw cars
        window.draw(playerCar);
        window.draw(aiCar);

        // Replace the entire font loading and text rendering section with simple shapes
        if (raceOver) {
            // Draw win/lose indicator
            sf::RectangleShape resultBox;
            resultBox.setSize(sf::Vector2f(200.f, 100.f));
            resultBox.setPosition(window.getSize().x / 2.f - 100.f, window.getSize().y / 2.f - 50.f);
            resultBox.setFillColor(playerWon ? sf::Color::Green : sf::Color::Red);
            window.draw(resultBox);
        } else {
            // Draw checkpoint progress indicators
            for (int i = 0; i < (int)checkpointAngles.size(); i++) {
                sf::CircleShape indicator(10.f);
                indicator.setPosition(10.f + i * 30.f, 10.f);
                if (i < playerNextCheckpoint) {
                    indicator.setFillColor(sf::Color::Green);  // Completed
                } else if (i == playerNextCheckpoint) {
                    indicator.setFillColor(sf::Color::Yellow); // Current
                } else {
                    indicator.setFillColor(sf::Color(128, 128, 128)); // Not yet reached
                }
                window.draw(indicator);
            }
        }

        window.display();
    }

    return 0;
}
