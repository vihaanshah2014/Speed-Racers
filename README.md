
# Speed Racers

A 2D racing game where you compete against an AI that learns and optimizes its racing line.

![AI Winning Performance](ai_winning.PNG)

## Description

Speed Racers is a SFML-based racing game where players race against an AI opponent on a track. The AI first undergoes a training phase to optimize its racing line, then challenges the player in a checkpoint-based race.

## Features

- Real-time 2D racing gameplay
- AI opponent that learns optimal racing paths through a genetic algorithm
- Checkpoint-based racing system
- Collision detection with track borders
- Visual progress tracking during the race
- WASD controls for player movement

## Prerequisites

- C++ compiler with C++17 support
- SFML library (version 2.5 or higher)
- Make (for building)

## Required Files

- `player1.png` - Texture for player car
- `player2.png` - Texture for AI car
- `arial.ttf` - Font file for UI text (optional)

## AI Training

The AI undergoes a training phase where it refines its waypoints through generations of optimization. The following parameters govern its training:

- `GENERATIONS`: Number of optimization iterations (default: 100)
- `MUTATION_RATE`: Probability of mutation in waypoint adjustments (default: 0.05)

During training, the AI evaluates its fitness based on the time taken to complete the course and the number of collisions, progressively improving its performance.

## Building and Running

### Compile

Ensure `make` is installed and run:

```bash
make
```

### Run

After compilation, run the game with:

```bash
./run
```

## Controls

Press Enter to Start After Training (It may take a few clicks but you have time to get ready before the game starts)

- `W`: Accelerate
- `S`: Brake/Reverse
- `A`: Turn Left
- `D`: Turn Right

## Gameplay

1. On launch, the AI undergoes a training phase to optimize its racing line.
2. After training, the race begins.
3. Both player and AI must pass through all checkpoints in sequence.
4. The first to complete all checkpoints wins.
5. Collision with track borders stops the car temporarily.

## Technical Details

- Uses SFML for graphics, input handling, and collision detection.
- AI path optimization implemented with a genetic algorithm.
- Real-time physics-based car movement and checkpoint tracking.
- Visual indicators for progress and checkpoints.

## Contribution

Feel Free to contribute and improve this!!
