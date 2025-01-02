#include <iostream>
#include <cstdlib>  // For rand() and srand()
#include <ctime>    // For time()

int main() {
    // Seed the random number generator
    std::srand(static_cast<unsigned int>(std::time(nullptr)));

    // Game configuration
    const int FINISH_LINE = 50;  // The distance needed to win
    int playerPosition = 0;
    int aiPosition = 0;

    // Intro
    std::cout << "====================================\n";
    std::cout << "      Simple 2-Player Car Race      \n";
    std::cout << "====================================\n\n";
    std::cout << "Instructions:\n";
    std::cout << " - Press [W] + Enter to accelerate\n";
    std::cout << " - The AI moves automatically\n";
    std::cout << " - First to reach distance " << FINISH_LINE << " wins!\n\n";

    // Main game loop
    while (playerPosition < FINISH_LINE && aiPosition < FINISH_LINE) {
        // Display current positions
        std::cout << "-------------------------------------------------\n";
        std::cout << "Your car is at distance: " << playerPosition << "\n";
        std::cout << "AI car is at distance:   " << aiPosition << "\n";
        std::cout << "-------------------------------------------------\n";
        std::cout << "Press [W] then Enter to move forward: ";

        // Get user input
        char input;
        std::cin >> input;

        // Check user input
        if (input == 'W' || input == 'w') {
            // Increase player position (simulating acceleration)
            playerPosition += 1 + std::rand() % 3; 
            // The +1 ensures at least 1 step forward; %3 gives up to 2 additional steps
        } else {
            std::cout << "Invalid input! You lose a turn.\n";
        }

        // AI move
        // Let's say AI can move 1 to 4 steps randomly
        int aiMove = 1 + std::rand() % 4;
        aiPosition += aiMove;

        // Check if anyone reached the finish line
        if (playerPosition >= FINISH_LINE || aiPosition >= FINISH_LINE) {
            break; // break out of the loop to announce winner
        }

        // Just to visually separate each round
        std::cout << "\n";
    }

    // Determine winner
    if (playerPosition >= FINISH_LINE && aiPosition >= FINISH_LINE) {
        std::cout << "\nIt's a tie! Both reached " << FINISH_LINE << "!\n";
    } else if (playerPosition >= FINISH_LINE) {
        std::cout << "\nYou win! You reached " << FINISH_LINE << " first!\n";
    } else {
        std::cout << "\nAI wins! Better luck next time.\n";
    }

    return 0;
}
