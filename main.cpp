/******************************************************
 *  2D Racing with Simple Genetic Algorithm + SFML
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

// -------------------- Constants --------------------
static const float PI                = 3.14159265f;
static const int   NUM_ANGLE_STATES = 36;   // 10-degree bins (0–350)
static const int   NUM_SPEED_STATES = 6;    // Speeds 0–5
static const int   NUM_ACTIONS      = 5;    // STEER_LEFT, STEER_RIGHT, ACCELERATE, BRAKE, NOOP
static const int   POPULATION_SIZE  = 100;  // Number of AI cars per generation
static const int   TOP_PERFORMERS   = 20;   // Number of top cars to select
static const float MUTATION_RATE    = 0.1f; // Mutation rate for policy weights
static const int   MAX_STEPS        = 10000; // Max steps per evaluation
static const int   MAX_GENERATIONS  = 1000000; // Stop after these many generations if no success
const int MAX_STEPS_PER_EPISODE = 100000;  // Limit steps per episode
const float MIN_SPEED_THRESHOLD = 0.1f;  // Minimum speed threshold
const int MAX_ZERO_SPEED_STEPS = 50;     // Maximum steps allowed at zero speed

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
    Car(const std::vector<sf::Vector2f>& wps, std::mt19937& rng)
        : trainingTrackPoints(wps) {
        reset();
        initializeRandomPolicy(rng);
    }

    // Copy constructor (important for cloning in GA)
    Car(const Car& other)
        : trainingTrackPoints(other.trainingTrackPoints),
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
        targetWaypointIndex = 0;    // Start with the first waypoint
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
                speed += ACCEL;
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

        steps++;
    }

    // Evaluate performance of this car's policy on the training track
    // Returns {totalReward, successStatus}
    std::pair<float, bool> evaluate() {
        float totalReward = 0.f;
        steps = 0;
        int zeroSpeedSteps = 0;
        float lastAngleDiff = 360.f;  // Track previous angle difference

        while (!done && steps < MAX_STEPS_PER_EPISODE) {
            steps++;
            
            if (steps % 1000 == 0) {
                std::cout << "Step: " << steps 
                          << " | Waypoint: " << targetWaypointIndex 
                          << "/" << trainingTrackPoints.size()
                          << " | Speed: " << speed << std::endl;
            }

            Action act = chooseAction();
            applyAction(act);

            sf::Vector2f targetPos = trainingTrackPoints[targetWaypointIndex];
            float distToTarget = distance(position, targetPos);
            
            sf::Vector2f dirToTarget = targetPos - position;
            float targetAngle = std::atan2(dirToTarget.y, dirToTarget.x) * 180.0f / PI;
            float angleDiff = std::abs(orientation - targetAngle);
            while (angleDiff > 180.f) angleDiff = std::abs(angleDiff - 360.f);

            // Base penalty to encourage quick completion
            float reward = -0.1f;

            // Turning improvement reward
            if (angleDiff < lastAngleDiff) {
                reward += 0.5f;  // Reward for turning towards target
            }
            lastAngleDiff = angleDiff;

            // Speed and direction rewards
            if (angleDiff < 45.f) {
                reward += (45.f - angleDiff) / 45.f * 2.0f;  // Double alignment reward
                if (speed > 2.f) {
                    reward += 1.0f;  // Bonus for speed while aligned
                }
            } else {
                // Strong penalty for not turning towards target
                reward -= angleDiff / 45.f;
            }

            // Zero speed penalty
            if (speed < 0.1f) {
                zeroSpeedSteps++;
                reward -= 1.0f;
                if (zeroSpeedSteps > MAX_ZERO_SPEED_STEPS) {
                    done = true;
                    reward -= 100.f;
                    break;
                }
            } else {
                zeroSpeedSteps = 0;
            }

            // Off-track penalty
            if (distToTarget > 200.f) {
                done = true;
                reward -= 200.f;
                break;
            }

            // Waypoint rewards
            if (distToTarget < 20.f) {
                reward += 100.f;  // Big reward for reaching waypoints
                targetWaypointIndex++;
                
                if (targetWaypointIndex >= static_cast<int>(trainingTrackPoints.size())) {
                    reward += 1000.f;  // Huge completion bonus
                    done = true;
                    break;
                }
            }

            totalReward += reward;
        }

        if (steps >= MAX_STEPS_PER_EPISODE) {
            done = true;
        }

        return {totalReward, (targetWaypointIndex >= static_cast<int>(trainingTrackPoints.size()))};
    }

    // Accessors
    sf::Vector2f getPosition()   const { return position; }
    float        getOrientation() const { return orientation; }
    bool         isSuccess()     const { 
        return targetWaypointIndex >= static_cast<int>(trainingTrackPoints.size()); 
    }

    // Policy weights (public for GA manipulation)
    std::vector<float> policyWeights;

    // We expose the current waypoint index for external checks
    int targetWaypointIndex;

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

public:
    sf::Vector2f              position;
    float                     orientation; // in degrees
    float                     speed;
    bool                      done;
    int                       steps;
    std::vector<sf::Vector2f> trainingTrackPoints;

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

// -------------------- Genetic Algorithm Helpers --------------------
std::vector<Car> selectTopPerformers(const std::vector<Car>& population,
                                     const std::vector<std::pair<float, bool>>& performances,
                                     int topN) 
{
    // Create index array [0..population.size()-1]
    std::vector<size_t> indices(population.size());
    for (size_t i = 0; i < population.size(); i++) 
        indices[i] = i;

    // Sort indices by descending reward
    std::sort(indices.begin(), indices.end(), [&](size_t a, size_t b) {
        return performances[a].first > performances[b].first;
    });

    // Gather top N
    std::vector<Car> topPerformers;
    for (int i = 0; i < topN && i < static_cast<int>(indices.size()); i++) {
        topPerformers.push_back(population[indices[i]]);
    }
    return topPerformers;
}

// Create next generation by cloning and mutating top performers
std::vector<Car> createNextGeneration(const std::vector<Car>& topPerformers,
                                      int populationSize,
                                      std::mt19937& rng) 
{
    std::vector<Car> nextGeneration;
    nextGeneration.reserve(populationSize);

    std::uniform_int_distribution<int> selectionDist(0, (int)topPerformers.size() - 1);
    std::normal_distribution<float>    mutationDist(0.f, 0.1f);

    while ((int)nextGeneration.size() < populationSize) {
        // Pick a parent from the top
        int parentIdx = selectionDist(rng);
        const Car& parent = topPerformers[parentIdx];

        // Clone
        Car child(parent); // uses copy constructor

        // Mutate
        for (auto& weight : child.policyWeights) {
            // random chance to mutate each weight
            std::uniform_real_distribution<float> chanceDist(0.f, 1.f);
            if (chanceDist(rng) < MUTATION_RATE) {
                weight += mutationDist(rng);
                // clamp weights for sanity
                weight = std::clamp(weight, -5.f, 5.f);
            }
        }

        nextGeneration.push_back(child);
    }
    return nextGeneration;
}

// Add these global variables at the top with other constants
struct BestPerformance {
    float reward;
    int generation;
    int waypoints;
    std::vector<float> weights;
    BestPerformance() : reward(-std::numeric_limits<float>::max()), generation(0), waypoints(0) {}
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

    // Optional: Add checkpoints for visualization
    std::vector<sf::Vector2f> checkpointPositions = {
        {500, 400},  // Bottom straight
        {900, 300},  // First turn
        {500, 200},  // Top straight
        {200, 300}   // Final turn
    };

    // RNG
    std::random_device rd;
    std::mt19937 rng(rd());

    // Initialize population
    std::cout << "Initializing population with " << POPULATION_SIZE << " cars..." << std::endl;
    std::vector<Car> population;
    population.reserve(POPULATION_SIZE);
    for (int i = 0; i < POPULATION_SIZE; i++) {
        population.emplace_back(trainingWaypoints, rng);
    }

    // We will store the best car seen so far
    Car bestCar(trainingWaypoints, rng);
    float bestCarReward = -999999.f;
    bool raceCompleted  = false;

    // Add these global variables at the top with other constants
    BestPerformance bestEver;
    int successfulEpisodes = 0;

    // Training loop
    std::cout << "\nStarting training...\n" << std::endl;
    int generation = 0;
    while (generation < MAX_GENERATIONS && !raceCompleted) {
        generation++;
        
        // Only show every 10th generation
        if (generation % 10 == 0) {
            std::cout << "\n=== Generation " << generation << " ===" << std::endl;
        }

        std::vector<std::pair<float, bool>> performances;
        performances.reserve(POPULATION_SIZE);

        float generationBestReward = -std::numeric_limits<float>::max();
        int generationBestWaypoints = 0;

        // Evaluate each car
        for (int i = 0; i < POPULATION_SIZE; i++) {
            auto& car = population[i];
            auto [reward, success] = car.evaluate();
            performances.push_back({reward, success});

            // Track best performance this generation
            if (reward > generationBestReward) {
                generationBestReward = reward;
                generationBestWaypoints = car.targetWaypointIndex;
            }

            // Track all-time best
            if (reward > bestEver.reward) {
                bestEver.reward = reward;
                bestEver.generation = generation;
                bestEver.waypoints = car.targetWaypointIndex;
                bestEver.weights = car.policyWeights;
                std::cout << "\n*** NEW BEST ***" 
                          << "\nGeneration: " << generation 
                          << "\nReward: " << reward 
                          << "\nWaypoints: " << car.targetWaypointIndex << std::endl;
            }

            if (success) {
                raceCompleted = true;
                std::cout << "\n!!! TRACK COMPLETED !!!" << std::endl;
                std::cout << "Generation: " << generation << std::endl;
                std::cout << "Reward: " << reward << std::endl;
            }
        }

        // Generation stats
        float avgReward = 0.f;
        int successCount = 0;
        for (auto& perf : performances) {
            avgReward += perf.first;
            if (perf.second) successCount++;
        }
        avgReward /= performances.size();

        std::cout << "Best This Gen: " << generationBestReward
                  << " (" << generationBestWaypoints << " waypoints)"
                  << "\nAll-time Best: " << bestEver.reward 
                  << " (Gen " << bestEver.generation 
                  << ", " << bestEver.waypoints << " waypoints)" << std::endl;

        // Create next generation
        std::vector<Car> topPerformers = selectTopPerformers(population, performances, TOP_PERFORMERS);
        population = createNextGeneration(topPerformers, POPULATION_SIZE, rng);
    }

    // At the end of training:
    std::cout << "\n=== Training Complete ===" << std::endl;
    std::cout << "Generations Run: " << generation << std::endl;
    std::cout << "Best Performance:" << std::endl;
    std::cout << "  Generation: " << bestEver.generation << std::endl;
    std::cout << "  Reward: " << bestEver.reward << std::endl;
    std::cout << "  Waypoints: " << bestEver.waypoints << std::endl;
    if (raceCompleted) {
        std::cout << "SUCCESS: Track completed!" << std::endl;
    } else {
        std::cout << "Track not completed. Best progress: " 
                  << bestEver.waypoints << " waypoints" << std::endl;
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
    sf::RenderWindow window(sf::VideoMode(1000, 800), "2D Racing with GA");
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

    // Optional "checkpoints"
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
    Car aiController = bestCar;
    aiController.reset();

    bool raceOver = false;
    bool aiWon    = false;

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
            // Store old position for collision check
            sf::Vector2f oldAIPos = aiCar.getPosition();
            float oldAIRot = aiCar.getRotation();

            Action act = aiController.chooseAction();
            aiController.applyAction(act);

            // Update AI car sprite
            aiCar.setPosition(aiController.getPosition());
            aiCar.setRotation(aiController.getOrientation());

            // Check AI collision with borders
            bool aiCollision = false;
            for (const auto& border : trackBorders) {
                if (aiCar.getGlobalBounds().intersects(border.getGlobalBounds())) {
                    aiCollision = true;
                    break;
                }
            }

            if (aiCollision) {
                // Bounce effect for AI
                aiController.setPosition(oldAIPos);
                aiController.setOrientation(oldAIRot);
                // Add a small bounce-back effect
                float bounceAngle = oldAIRot * PI / 180.f;
                sf::Vector2f bounceVec(-std::cos(bounceAngle) * 2.0f, -std::sin(bounceAngle) * 2.0f);
                aiController.setPosition(oldAIPos + bounceVec);
                
                // Optional: Reduce AI speed on collision
                aiController.reduceSpeed();
            }

            // Check if AI reached next waypoint
            float distToTarget = distance(aiController.getPosition(), 
                                       trainingWaypoints[aiController.targetWaypointIndex]);
            if (distToTarget < 10.f) {
                aiController.targetWaypointIndex++;
                if (aiController.targetWaypointIndex >= static_cast<int>(trainingWaypoints.size())) {
                    raceOver = true;
                    aiWon = true;
                }
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
            if (i < (size_t)aiController.targetWaypointIndex) {
                checkpointShapes[i].setFillColor(sf::Color::Green);
            } else if (i == (size_t)aiController.targetWaypointIndex) {
                checkpointShapes[i].setFillColor(sf::Color::Yellow);
            } else {
                checkpointShapes[i].setFillColor(sf::Color(128,128,128));
            }
            window.draw(checkpointShapes[i]);
        }

        // 3. AI car
        window.draw(aiCar);

        // 4. Borders
        for (auto& border : trackBorders) {
            window.draw(border);
        }

        // 5. Race result overlay if finished
        if (raceOver) {
            sf::RectangleShape resultBox(sf::Vector2f(200.f, 100.f));
            resultBox.setPosition(
                window.getSize().x / 2.f - 100.f, 
                window.getSize().y / 2.f - 50.f
            );
            resultBox.setFillColor(aiWon ? sf::Color::Green : sf::Color::Red);
            window.draw(resultBox);

            // Show text
            sf::Font font;
            if (!font.loadFromFile("arial.ttf")) {
                std::cerr << "Failed to load font!\n";
            }
            sf::Text resultText;
            resultText.setFont(font);
            resultText.setString(aiWon ? "AI Wins!" : "AI Failed!");
            resultText.setCharacterSize(24);
            resultText.setFillColor(sf::Color::White);
            resultText.setPosition(
                resultBox.getPosition().x + 20.f, 
                resultBox.getPosition().y + 35.f
            );
            window.draw(resultText);
        }

        window.display();
    }

    return 0;
}
