/******************************************************
 *  2D Racing with Particle Swarm Optimization + SFML
 ******************************************************/

#include <SFML/Graphics.hpp>
#include <cmath>
#include <vector>
#include <iostream>
#include <string>
#include <fstream>
#include <algorithm>
#include <random>
#include <tuple>
#include <limits>
#include <chrono>

// -------------------- Constants --------------------
static const float PI                     = 3.14159265f;
static const int   NUM_ANGLE_STATES      = 36;   // 10-degree bins (0–350)
static const int   NUM_SPEED_STATES      = 6;    // Speeds 0–5
static const int   NUM_ACTIONS           = 5;    // STEER_LEFT, STEER_RIGHT, ACCELERATE, BRAKE, NOOP
static const int   SWARM_SIZE            = 200;  // Swarm size for PSO
static const float INERTIA_WEIGHT        = 0.7f; // Inertia weight (w)
static const float COGNITIVE_COEFF       = 1.5f; // Cognitive coefficient (c1)
static const float SOCIAL_COEFF          = 1.5f; // Social coefficient (c2)
static const float MAX_VELOCITY          = 0.5f; // Maximum velocity per dimension
static const int   MAX_STEPS             = 2000; // Maximum steps per episode
static const int   MAX_GENERATIONS       = 7000; // Maximum number of generations
const float MIN_SPEED_THRESHOLD = 0.1f;  // Minimum speed threshold
const int MAX_STEPS_PER_EPISODE = 1000;  // Limit steps per episode
const int MAX_ZERO_SPEED_STEPS = 50;      // Maximum steps allowed at zero speed
const float POSITION_MIN = -5.f;          // Minimum policy weight
const float POSITION_MAX = 5.f;           // Maximum policy weight

// -------------------- Actions --------------------
enum Action { STEER_LEFT, STEER_RIGHT, ACCELERATE, BRAKE, NOOP };

// -------------------- Utility Functions --------------------
float degToRad(float deg) { 
    return deg * PI / 180.0f; 
}

float distance(const sf::Vector2f& a, const sf::Vector2f& b) {
    float dx = a.x - b.x;
    float dy = a.y - b.y;
    return std::sqrt(dx * dx + dy * dy);
}

int angleToDiscrete(float angle) {
    while (angle < 0.f)   angle += 360.f;
    while (angle >= 360.f) angle -= 360.f;
    return static_cast<int>(angle / 10.f) % NUM_ANGLE_STATES;
}

int speedToDiscrete(float speed) {
    int speedState = static_cast<int>(std::floor(speed));
    // clamp to [0, NUM_SPEED_STATES - 1]
    return std::clamp(speedState, 0, NUM_SPEED_STATES - 1);
}

// -------------------- Car Class --------------------
class Car {
public:
    Car(const std::vector<sf::Vector2f>& wps, const std::vector<sf::Vector2f>& cps, std::mt19937& rng)
        : trainingTrackPoints(wps), checkpointPositions(cps) {
        reset();
        initializeRandomPolicy(rng);
    }

    // Copy constructor
    Car(const Car& other)
        : trainingTrackPoints(other.trainingTrackPoints),
          checkpointPositions(other.checkpointPositions),
          policyWeights(other.policyWeights) {
        // We'll call reset() so that each new clone starts fresh
        reset();
    }

    void reset() {
        position           = trainingTrackPoints[0];
        orientation        = 0.f;   // facing "east"
        speed              = 0.f;
        done               = false;
        steps              = 0;
        currentCheckpoint  = 0;    // Start with the first checkpoint
        path.clear();             // Clear previous path
        path.emplace_back(position); // Add starting position
    }

    // Initialize policy weights randomly in [-1, +1]
    void initializeRandomPolicy(std::mt19937& rng) {
        std::uniform_real_distribution<float> dist(-1.f, 1.f);
        policyWeights.resize(NUM_ANGLE_STATES * NUM_SPEED_STATES * NUM_ACTIONS);
        for (auto& w : policyWeights) {
            w = dist(rng);
        }
    }

    // Choose action based on current state + policy
    Action chooseAction() const {
        int angleState = angleToDiscrete(orientation);
        int speedState = speedToDiscrete(speed);

        // Compute scores for each action
        float scores[NUM_ACTIONS] = {0.f};
        for (int a = 0; a < NUM_ACTIONS; a++) {
            int index = angleState * NUM_SPEED_STATES * NUM_ACTIONS 
                      + speedState * NUM_ACTIONS 
                      + a;
            scores[a] = policyWeights[index];
        }

        // Select action with highest score
        int   bestAction = 0;
        float maxScore   = scores[0];
        for (int a = 1; a < NUM_ACTIONS; a++) {
            if (scores[a] > maxScore) {
                maxScore   = scores[a];
                bestAction = a;
            }
        }
        return static_cast<Action>(bestAction);
    }

    // Apply action to update the Car's state
    void applyAction(Action act) {
        const float TURN_SPEED = 5.f;   // degrees per action
        const float ACCEL = 0.2f;       // acceleration per action
        const float DECEL = 0.2f;       // deceleration per action
        const float MAX_SPEED = 5.f;    // maximum speed

        switch (act) {
            case STEER_LEFT:
                orientation -= TURN_SPEED;
                // Allow slight acceleration while turning
                speed += ACCEL * 0.5f;
                break;
            case STEER_RIGHT:
                orientation += TURN_SPEED;
                // Allow slight acceleration while turning
                speed += ACCEL * 0.5f;
                break;
            case ACCELERATE:
                speed += ACCEL;
                break;
            case BRAKE:
                speed -= DECEL;
                break;
            case NOOP:
                // Small speed decay for NOOP to discourage inaction
                speed *= 0.99f;
                break;
        }

        // Clamp orientation
        while (orientation < 0.f) orientation += 360.f;
        while (orientation >= 360.f) orientation -= 360.f;

        // Clamp speed
        speed = std::clamp(speed, 0.f, MAX_SPEED);

        // Move car
        float rad = degToRad(orientation);
        float vx = std::cos(rad) * speed;
        float vy = std::sin(rad) * speed;
        position += sf::Vector2f(vx, vy);

        // Record path
        path.emplace_back(position);

        steps++;
    }

    // Evaluate performance of this car's policy on the training track
    // Returns {totalReward, successStatus}
    std::pair<float, bool> evaluate() {
        float totalReward = 0.f;
        reset();
        int zeroSpeedSteps = 0;
        float lastDistToCheckpoint = distance(position, checkpointPositions[currentCheckpoint]);

        while (!done && steps < MAX_STEPS_PER_EPISODE) {
            steps++;

            // Current target checkpoint
            if (currentCheckpoint >= static_cast<int>(checkpointPositions.size())) {
                // All checkpoints hit
                done = true;
                break;
            }

            const sf::Vector2f& targetPoint = checkpointPositions[currentCheckpoint];
            float distToTarget = distance(position, targetPoint);

            // Calculate angle to target
            float angleDiff = getAngleToTarget(targetPoint);

            // Base reward structure
            float reward = 0.f;

            // Distance-based reward
            if (distToTarget < lastDistToCheckpoint) {
                reward += 1.0f; // Encourages moving closer to the target
            } else {
                reward -= 0.5f; // Penalty for moving away
            }

            // Speed reward when generally aligned with target
            if (std::abs(angleDiff) < 45.f) {
                reward += speed * 0.2f;
            }

            // Checkpoint completion reward
            if (distToTarget < 30.f) { // Threshold for hitting a checkpoint
                reward += 100.f; // Significant reward for hitting a checkpoint
                currentCheckpoint++;

                if (currentCheckpoint >= static_cast<int>(checkpointPositions.size())) {
                    reward += 1000.f; // Additional reward for completing all checkpoints
                    done = true;
                    break;
                }

                // Update for next checkpoint
                lastDistToCheckpoint = distance(position, checkpointPositions[currentCheckpoint]);
            }

            // Severe penalty for very slow movement
            if (speed < MIN_SPEED_THRESHOLD) {
                zeroSpeedSteps++;
                reward -= 1.0f;
                if (zeroSpeedSteps > MAX_ZERO_SPEED_STEPS) {
                    done = true;
                    break;
                }
            } else {
                zeroSpeedSteps = 0;
            }

            // Apply chosen action
            Action act = chooseAction();
            applyAction(act);

            lastDistToCheckpoint = distToTarget;
            totalReward += reward;
        }

        // Scale final reward based on progress through checkpoints
        float progressMultiplier = static_cast<float>(currentCheckpoint) / checkpointPositions.size();
        totalReward *= (0.5f + progressMultiplier); // Base 0.5 + progress ratio

        bool success = (currentCheckpoint >= static_cast<int>(checkpointPositions.size()));
        return {totalReward, success};
    }

    // Accessors
    sf::Vector2f getPosition()   const { return position; }
    float        getOrientation() const { return orientation; }
    bool         isSuccess()     const { 
        return currentCheckpoint >= static_cast<int>(checkpointPositions.size()); 
    }

    // Policy weights (public for PSO manipulation)
    std::vector<float> policyWeights;

    // Path taken during evaluation
    std::vector<sf::Vector2f> path;

    // Current checkpoint index
    int currentCheckpoint;

    void setPosition(const sf::Vector2f& pos) {
        position = pos;
    }

    void setOrientation(float orient) {
        orientation = orient;
        while (orientation < 0.f) orientation += 360.f;
        while (orientation >= 360.f) orientation -= 360.f;
    }

    void reduceSpeed() {
        speed *= 0.5f; // Reduce speed by half on collision
    }

private:
    float getAngleToTarget(const sf::Vector2f& target) const {
        sf::Vector2f dirToTarget = target - position;
        float targetAngle = std::atan2(dirToTarget.y, dirToTarget.x) * 180.0f / PI;
        float angleDiff = targetAngle - orientation;
        while (angleDiff > 180.f) angleDiff -= 360.f;
        while (angleDiff < -180.f) angleDiff += 360.f;
        return angleDiff;
    }

    // Calculate optimal racing line point
    sf::Vector2f calculateRacingLinePoint() const {
        const sf::Vector2f& currentCP = checkpointPositions[currentCheckpoint];
        sf::Vector2f nextCP;

        // Look ahead to next checkpoint
        if (currentCheckpoint + 1 < static_cast<int>(checkpointPositions.size())) {
            nextCP = checkpointPositions[currentCheckpoint + 1];
        } else {
            nextCP = checkpointPositions[0]; // Loop back to start if needed
        }

        // Calculate middle point between checkpoints
        sf::Vector2f midPoint = (currentCP + nextCP) * 0.5f;

        // Adjust racing line towards inside of corners
        float cornerAngle = std::abs(getAngleToTarget(nextCP) - getAngleToTarget(currentCP));
        if (cornerAngle > 45.f) {
            // Approaching a corner, adjust line towards inside
            float cornerBias = 0.3f; // How much to cut the corner
            return currentCP + (midPoint - currentCP) * cornerBias;
        }

        return currentCP; // Default to checkpoint if not cornering
    }

    // Member variables
    sf::Vector2f              position;
    float                     orientation; // in degrees
    float                     speed;
    bool                      done;
    int                       steps;
    std::vector<sf::Vector2f> trainingTrackPoints;
    std::vector<sf::Vector2f> checkpointPositions;

    // Car movement constants
    static const float TURN_SPEED;
    static const float ACCEL;
    static const float DECEL;
    static const float MAX_SPEED;
};

// Initialize Car static constants
const float Car::TURN_SPEED = 5.f;   // degrees per action
const float Car::ACCEL      = 0.2f;  // acceleration per action
const float Car::DECEL      = 0.2f;  // deceleration per action
const float Car::MAX_SPEED  = 5.f;   // max speed ~ 5 px/frame

// -------------------- Particle Class --------------------
struct Particle {
    std::vector<float> position;           // Current policyWeights
    std::vector<float> velocity;           // Velocity in policyWeights space
    std::vector<float> personalBestPosition; // Best position ever achieved by this particle
    float personalBestFitness;             // Fitness at the personal best position

    Particle(int numWeights, std::mt19937& rng) {
        std::uniform_real_distribution<float> posDist(POSITION_MIN, POSITION_MAX);
        std::uniform_real_distribution<float> velDist(-0.1f, 0.1f); // Initial velocity range

        position.resize(numWeights);
        velocity.resize(numWeights);
        personalBestPosition.resize(numWeights);
        personalBestFitness = -std::numeric_limits<float>::max();

        for(int i = 0; i < numWeights; ++i) {
            position[i] = posDist(rng);
            velocity[i] = velDist(rng);
            personalBestPosition[i] = position[i];
        }
    }

    void updatePersonalBest(float fitness) {
        if(fitness > personalBestFitness) {
            personalBestFitness = fitness;
            personalBestPosition = position;
        }
    }
};

// -------------------- Best Performance Struct --------------------
struct BestPerformance {
    float reward;
    int generation;
    int checkpoints;
    std::vector<float> weights;
    std::vector<sf::Vector2f> bestPath;
    BestPerformance() : reward(-std::numeric_limits<float>::max()), generation(0), checkpoints(0) {}
};

// -------------------- Main Function --------------------
int main() {
    // Create a simple rectangular track with rounded corners
    std::vector<sf::Vector2f> trainingWaypoints = {
        // Start/Finish on the left side
        {200, 400},  // Start
        {400, 400},  // Right side of bottom straight
        {600, 400},
        {800, 400},  // Approaching first turn

        // First turn (right)
        {900, 400},
        {900, 300},  // Going up
        {900, 200},

        // Top straight
        {800, 200},
        {600, 200},
        {400, 200},
        {200, 200},

        // Final turn (right)
        {200, 300},  // Going down
        {200, 400}   // Back to start
    };

    // Define checkpoints for evaluation and visualization
    std::vector<sf::Vector2f> checkpointPositions = {
        {500, 400},  // Bottom straight
        {900, 300},  // First turn
        {500, 200},  // Top straight
        {200, 300}   // Final turn
    };

    // RNG
    std::random_device rd;
    std::mt19937 rng(rd());

    // Initialize swarm
    std::cout << "Initializing swarm with " << SWARM_SIZE << " particles..." << std::endl;
    int numWeights = NUM_ANGLE_STATES * NUM_SPEED_STATES * NUM_ACTIONS;
    std::vector<Particle> swarm;
    swarm.reserve(SWARM_SIZE);
    for(int i = 0; i < SWARM_SIZE; i++) {
        swarm.emplace_back(numWeights, rng);
    }

    // Initialize global best
    std::vector<float> globalBestPosition(numWeights);
    float globalBestFitness = -std::numeric_limits<float>::max();

    // We will store the best car seen so far
    BestPerformance bestEver;
    int successfulEpisodes = 0;

    // Training loop
    std::cout << "\nStarting training with PSO...\n" << std::endl;
    int generation = 0;
    auto startTime = std::chrono::high_resolution_clock::now();
    int lastProgress = 0;
    bool raceCompleted = false;

    while (generation < MAX_GENERATIONS && !raceCompleted) {
        generation++;

        // Calculate progress percentage
        int progress = (generation * 100) / MAX_GENERATIONS;
        if (progress != lastProgress) {
            auto currentTime = std::chrono::high_resolution_clock::now();
            auto elapsedSeconds = std::chrono::duration_cast<std::chrono::seconds>(
                currentTime - startTime).count();

            // Clear line and show progress
            std::cout << "\rProgress: " << progress << "% | "
                      << "Generation: " << generation << " | "
                      << "Time: " << elapsedSeconds << "s | "
                      << "Best Checkpoints: " << bestEver.checkpoints << "/" 
                      << checkpointPositions.size()
                      << std::flush;

            lastProgress = progress;
        }

        // Evaluate each particle
        for(auto& particle : swarm) {
            // Create a Car instance with particle's position as policyWeights
            Car car(trainingWaypoints, checkpointPositions, rng);
            car.policyWeights = particle.position;

            // Evaluate car's performance
            auto [reward, success] = car.evaluate();

            // Update personal best
            particle.updatePersonalBest(reward);

            // Update global best
            if(reward > globalBestFitness) {
                globalBestFitness = reward;
                globalBestPosition = particle.position;

                bestEver.reward = reward;
                bestEver.generation = generation;
                bestEver.checkpoints = car.currentCheckpoint;
                bestEver.weights = particle.position;
                bestEver.bestPath = car.path;

                if(success) {
                    raceCompleted = true;
                }
            }
        }

        // PSO velocity and position update
        for(auto& particle : swarm) {
            for(int i = 0; i < numWeights; ++i) {
                // Random coefficients
                std::uniform_real_distribution<float> dist(0.f, 1.f);
                float r1 = dist(rng);
                float r2 = dist(rng);

                // Update velocity
                particle.velocity[i] = INERTIA_WEIGHT * particle.velocity[i]
                                       + COGNITIVE_COEFF * r1 * (particle.personalBestPosition[i] - particle.position[i])
                                       + SOCIAL_COEFF * r2 * (globalBestPosition[i] - particle.position[i]);

                // Clamp velocity
                if(particle.velocity[i] > MAX_VELOCITY)
                    particle.velocity[i] = MAX_VELOCITY;
                if(particle.velocity[i] < -MAX_VELOCITY)
                    particle.velocity[i] = -MAX_VELOCITY;

                // Update position
                particle.position[i] += particle.velocity[i];

                // Clamp position
                if(particle.position[i] > POSITION_MAX)
                    particle.position[i] = POSITION_MAX;
                if(particle.position[i] < POSITION_MIN)
                    particle.position[i] = POSITION_MIN;
            }
        }
    }

    // Final report
    auto endTime = std::chrono::high_resolution_clock::now();
    auto totalSeconds = std::chrono::duration_cast<std::chrono::seconds>(
        endTime - startTime).count();

    std::cout << "\n\n=== Training Complete ===" << std::endl;
    std::cout << "Total Time: " << totalSeconds << " seconds" << std::endl;
    std::cout << "Generations Run: " << generation << std::endl;
    std::cout << "\nBest Performance:" << std::endl;
    std::cout << "  Generation: " << bestEver.generation << std::endl;
    std::cout << "  Reward: " << bestEver.reward << std::endl;
    std::cout << "  Checkpoints Hit: " << bestEver.checkpoints << "/" 
              << checkpointPositions.size() << std::endl;

    if (raceCompleted) {
        std::cout << "\nSUCCESS: All checkpoints hit!" << std::endl;
    } else {
        std::cout << "\nFailed to hit all checkpoints. Best progress: " 
                  << bestEver.checkpoints << " checkpoints" << std::endl;
    }

    // --------------- Visualization Phase ---------------
    // Load textures
    sf::Texture player1Texture, player2Texture;
    if (!player1Texture.loadFromFile("player1.png") ||
        !player2Texture.loadFromFile("player2.png")) {
        std::cout << "Error loading car textures!\nMake sure player1.png & player2.png exist.\n";
        return -1;
    }

    // Create window
    sf::RenderWindow window(sf::VideoMode(1000, 800), "2D Racing with PSO");
    window.setFramerateLimit(60);

    // Optional player car sprite (not driven by AI)
    sf::Sprite playerCar(player1Texture);
    playerCar.setScale(
        40.0f / player1Texture.getSize().x,
        20.0f / player1Texture.getSize().y
    );
    playerCar.setOrigin(
        player1Texture.getSize().x / 2.0f, 
        player1Texture.getSize().y / 2.0f
    );

    // AI car sprite
    sf::Sprite aiCar(player2Texture);
    aiCar.setScale(
        40.0f / player2Texture.getSize().x,
        20.0f / player2Texture.getSize().y
    );
    aiCar.setOrigin(
        player2Texture.getSize().x / 2.0f, 
        player2Texture.getSize().y / 2.0f
    );

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
        {150, 450},  // Bottom left
        {950, 450},  // Bottom right
        {950, 150},  // Top right
        {150, 150},  // Top left
        {150, 450}   // Back to start
    };

    // Inner border coordinates (clockwise)
    std::vector<sf::Vector2f> innerBorder = {
        {250, 350},  // Bottom left
        {850, 350},  // Bottom right
        {850, 250},  // Top right
        {250, 250},  // Top left
        {250, 350}   // Back to start
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

    // Set up the "bestCar" for visualization
    Car bestCar(trainingWaypoints, checkpointPositions, rng);
    bestCar.policyWeights = globalBestPosition;
    bestCar.reset();
    // Apply the policy weights to the bestCar
    // Normally, the bestCar would be evaluated again to set its path correctly
    bestCar.evaluate();

    // Load font for displaying results
    sf::Font font;
    if (!font.loadFromFile("arial.ttf")) {
        std::cerr << "Failed to load font!\nMake sure arial.ttf exists." << std::endl;
        return -1;
    }

    bool raceOver = false;
    bool aiWon    = false;

    // Create a VertexArray to store the best path
    sf::VertexArray bestPathLine(sf::LineStrip, bestEver.bestPath.size());
    for (size_t i = 0; i < bestEver.bestPath.size(); ++i) {
        bestPathLine[i].position = bestEver.bestPath[i];
        bestPathLine[i].color = sf::Color::Blue;
    }

    // --------------- Main Rendering Loop ---------------
    while (window.isOpen()) {
        sf::Event event;
        while (window.pollEvent(event)) {
            if (event.type == sf::Event::Closed) {
                window.close();
            }
        }

        // Player 1 Controls
        float playerSpeed = 0.0f;
        float playerRotation = 0.0f;
        
        if (sf::Keyboard::isKeyPressed(sf::Keyboard::Up))
            playerSpeed = 5.0f;
        if (sf::Keyboard::isKeyPressed(sf::Keyboard::Down))
            playerSpeed = -3.0f;
        if (sf::Keyboard::isKeyPressed(sf::Keyboard::Left))
            playerRotation = -3.0f;
        if (sf::Keyboard::isKeyPressed(sf::Keyboard::Right))
            playerRotation = 3.0f;

        // Update player position
        sf::Vector2f oldPlayerPos = playerCar.getPosition();
        float oldPlayerRot = playerCar.getRotation();
        
        playerCar.rotate(playerRotation);
        float angle = playerCar.getRotation() * PI / 180.f;
        playerCar.move(std::cos(angle) * playerSpeed, std::sin(angle) * playerSpeed);

        // Check player collision with borders
        bool playerCollision = false;
        for (const auto& border : trackBorders) {
            if (playerCar.getGlobalBounds().intersects(border.getGlobalBounds())) {
                playerCollision = true;
                break;
            }
        }
        
        if (playerCollision) {
            // Bounce effect: reverse position and add a small opposite impulse
            playerCar.setPosition(oldPlayerPos);
            playerCar.setRotation(oldPlayerRot);
            // Optional: Add a small bounce-back effect
            float bounceAngle = oldPlayerRot * PI / 180.f;
            playerCar.move(-std::cos(bounceAngle) * 2.0f, -std::sin(bounceAngle) * 2.0f);
        }

        // AI Car Update
        if (!raceOver) {
            // Reset and evaluate the bestCar to get its updated path
            bestCar.reset();
            auto [reward, success] = bestCar.evaluate();

            // Update AI car sprite
            aiCar.setPosition(bestCar.getPosition());
            aiCar.setRotation(bestCar.getOrientation());

            // Update the best path line
            bestPathLine.clear();
            for (const auto& pos : bestCar.path) {
                bestPathLine.append(sf::Vertex(pos, sf::Color::Blue));
            }

            // Check if AI has hit all checkpoints
            if (success) {
                raceOver = true;
                aiWon = true;
            }
        }

        // ---- Draw ----
        window.clear(sf::Color(0, 100, 0)); // green background

        // 1. Track
        for (auto& seg : trackSegments) {
            window.draw(seg);
        }

        // 2. Checkpoints
        for (size_t i = 0; i < checkpointShapes.size(); i++) {
            if (i < (size_t)bestCar.currentCheckpoint) {
                checkpointShapes[i].setFillColor(sf::Color::Green);
            } else if (i == (size_t)bestCar.currentCheckpoint) {
                checkpointShapes[i].setFillColor(sf::Color::Yellow);
            } else {
                checkpointShapes[i].setFillColor(sf::Color(128,128,128));
            }
            window.draw(checkpointShapes[i]);
        }

        // 3. Best Path
        if (!bestCar.path.empty()) {
            window.draw(bestPathLine);
        }

        // 4. AI car
        window.draw(aiCar);

        // 5. Player car
        window.draw(playerCar);

        // 6. Borders
        for (auto& border : trackBorders) {
            window.draw(border);
        }

        // 7. Race result overlay if finished
        if (raceOver) {
            sf::RectangleShape resultBox(sf::Vector2f(300.f, 100.f));
            resultBox.setPosition(
                window.getSize().x / 2.f - 150.f, 
                window.getSize().y / 2.f - 50.f
            );
            resultBox.setFillColor(aiWon ? sf::Color::Green : sf::Color::Red);
            window.draw(resultBox);

            // Show text
            sf::Text resultText;
            resultText.setFont(font);
            resultText.setString(aiWon ? "AI Completed All Checkpoints!" : "AI Failed to Complete All Checkpoints.");
            resultText.setCharacterSize(24);
            resultText.setFillColor(sf::Color::White);
            resultText.setPosition(
                resultBox.getPosition().x + 20.f, 
                resultBox.getPosition().y + 30.f
            );
            window.draw(resultText);
        }

        window.display();
    }

    return 0;
}
