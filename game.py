import pygame
import math
import numpy as np
import torch
import torch.nn as nn
import torch.optim as optim
from torch.distributions import Categorical

# Constants
PI = math.pi
NUM_ANGLE_STATES = 36  # 10-degree bins (0-350)
NUM_SPEED_STATES = 6   # Speeds 0-5
NUM_ACTIONS = 5        # STEER_LEFT, STEER_RIGHT, ACCELERATE, BRAKE, NOOP
SWARM_SIZE = 200       # Swarm size for PSO
INERTIA_WEIGHT = 0.7   # Inertia weight (w)
COGNITIVE_COEFF = 1.5  # Cognitive coefficient (c1)
SOCIAL_COEFF = 1.5     # Social coefficient (c2)
MAX_VELOCITY = 0.5     # Maximum velocity per dimension
MAX_STEPS = 2000       # Maximum steps per episode
MAX_GENERATIONS = 100       # Reduced from 1000
MIN_SPEED_THRESHOLD = 0.1
MAX_STEPS_PER_EPISODE = 200 # Reduced from 1000
MAX_ZERO_SPEED_STEPS = 50
POSITION_MIN = -5.0
POSITION_MAX = 5.0
LEARNING_RATE = 0.001      # Increased from 0.0003 for faster learning
GAMMA = 0.99
EPSILON = 0.2  # PPO clip parameter
EPOCHS = 2                  # Reduced from 4
HIDDEN_DIM = 32            # Reduced from 64 for lighter networks

# Actions
class Action:
    STEER_LEFT = 0
    STEER_RIGHT = 1
    ACCELERATE = 2
    BRAKE = 3
    NOOP = 4

# Utility Functions
def deg_to_rad(deg):
    return deg * PI / 180.0

def distance(a, b):
    return math.sqrt((a[0] - b[0])**2 + (a[1] - b[1])**2)

def angle_to_discrete(angle):
    while angle < 0:
        angle += 360.0
    while angle >= 360.0:
        angle -= 360.0
    return int(angle / 10.0) % NUM_ANGLE_STATES

def speed_to_discrete(speed):
    speed_state = int(math.floor(speed))
    return max(0, min(speed_state, NUM_SPEED_STATES - 1))

# Car Class
class Car:
    TURN_SPEED = 5.0   # degrees per action
    ACCEL = 0.2        # acceleration per action
    DECEL = 0.2        # deceleration per action
    MAX_SPEED = 5.0    # max speed

    def __init__(self, waypoints, checkpoints):
        self.waypoints = waypoints
        self.checkpoints = checkpoints
        
        # Define state dimensions (angle to target, speed, distance to target)
        state_dim = 3
        hidden_dim = HIDDEN_DIM  # Using smaller network
        
        # Initialize networks
        self.policy = PolicyNetwork(state_dim, hidden_dim, NUM_ACTIONS)
        self.value = ValueNetwork(state_dim, hidden_dim)
        
        # Initialize optimizers
        self.policy_optimizer = optim.Adam(self.policy.parameters(), lr=LEARNING_RATE)
        self.value_optimizer = optim.Adam(self.value.parameters(), lr=LEARNING_RATE)
        
        self.reset()

    def reset(self):
        self.position = np.array(self.waypoints[0], dtype=np.float64)
        self.orientation = 0.0
        self.speed = 0.0
        self.done = False
        self.steps = 0
        self.current_checkpoint = 0
        self.path = [self.position.copy()]
        self.last_dist_to_checkpoint = distance(self.position, self.checkpoints[0])

    def get_state(self):
        if self.current_checkpoint >= len(self.checkpoints):
            return torch.zeros(3)
            
        target = self.checkpoints[self.current_checkpoint]
        angle_diff = self.get_angle_to_target(np.array(target))
        dist = distance(self.position, target)
        
        # Normalize state values
        normalized_angle = angle_diff / 180.0
        normalized_speed = self.speed / self.MAX_SPEED
        normalized_dist = min(dist / 500.0, 1.0)  # 500 is max distance consideration
        
        return torch.FloatTensor([normalized_angle, normalized_speed, normalized_dist])

    def choose_action(self):
        state = self.get_state()
        with torch.no_grad():
            probs = self.policy(state)
            dist = Categorical(probs)
            action = dist.sample()
        return action.item()

    def update(self, states, actions, rewards, old_probs):
        # Convert to tensors
        states = torch.stack(states)
        actions = torch.tensor(actions)
        old_probs = torch.stack(old_probs)
        
        # Calculate returns
        returns = []
        R = 0
        for r in reversed(rewards):
            R = r + GAMMA * R
            returns.insert(0, R)
        returns = torch.tensor(returns)
        returns = (returns - returns.mean()) / (returns.std() + 1e-8)
        
        for _ in range(EPOCHS):
            # Get current action probabilities and state values
            probs = self.policy(states)
            dist = Categorical(probs)
            new_probs = dist.log_prob(actions)
            values = self.value(states)
            
            # Calculate ratios and surrogate losses
            ratios = torch.exp(new_probs - old_probs)
            advantages = returns - values.detach()
            surr1 = ratios * advantages
            surr2 = torch.clamp(ratios, 1-EPSILON, 1+EPSILON) * advantages
            
            # Calculate losses
            policy_loss = -torch.min(surr1, surr2).mean()
            value_loss = nn.MSELoss()(values.squeeze(), returns)
            
            # Update networks
            self.policy_optimizer.zero_grad()
            policy_loss.backward()
            self.policy_optimizer.step()
            
            self.value_optimizer.zero_grad()
            value_loss.backward()
            self.value_optimizer.step()

    def apply_action(self, action):
        if action == Action.STEER_LEFT:
            self.orientation -= self.TURN_SPEED
            self.speed += self.ACCEL * 0.5
        elif action == Action.STEER_RIGHT:
            self.orientation += self.TURN_SPEED
            self.speed += self.ACCEL * 0.5
        elif action == Action.ACCELERATE:
            self.speed += self.ACCEL
        elif action == Action.BRAKE:
            self.speed -= self.DECEL
        else:  # NOOP
            self.speed *= 0.99

        # Clamp orientation
        while self.orientation < 0:
            self.orientation += 360.0
        while self.orientation >= 360.0:
            self.orientation -= 360.0

        # Clamp speed
        self.speed = max(0.0, min(self.speed, self.MAX_SPEED))

        # Move car
        rad = deg_to_rad(self.orientation)
        velocity = np.array([math.cos(rad) * self.speed, math.sin(rad) * self.speed])
        self.position += velocity
        self.path.append(self.position.copy())
        self.steps += 1

    def get_angle_to_target(self, target):
        dir_to_target = target - self.position
        target_angle = math.atan2(dir_to_target[1], dir_to_target[0]) * 180.0 / PI
        angle_diff = target_angle - self.orientation
        while angle_diff > 180.0:
            angle_diff -= 360.0
        while angle_diff < -180.0:
            angle_diff += 360.0
        return angle_diff

    def evaluate(self):
        total_reward = 0.0
        self.reset()
        zero_speed_steps = 0
        last_dist_to_checkpoint = distance(self.position, self.checkpoints[self.current_checkpoint])

        while not self.done and self.steps < MAX_STEPS_PER_EPISODE:
            if self.current_checkpoint >= len(self.checkpoints):
                self.done = True
                break

            target_point = self.checkpoints[self.current_checkpoint]
            dist_to_target = distance(self.position, target_point)
            angle_diff = self.get_angle_to_target(target_point)

            # Reward calculation
            reward = 0.0
            if dist_to_target < last_dist_to_checkpoint:
                reward += 1.0
            else:
                reward -= 0.5

            if abs(angle_diff) < 45.0:
                reward += self.speed * 0.2

            if dist_to_target < 30.0:
                reward += 100.0
                self.current_checkpoint += 1
                
                if self.current_checkpoint >= len(self.checkpoints):
                    reward += 1000.0
                    self.done = True
                    break
                
                last_dist_to_checkpoint = distance(self.position, self.checkpoints[self.current_checkpoint])

            if self.speed < MIN_SPEED_THRESHOLD:
                zero_speed_steps += 1
                reward -= 1.0
                if zero_speed_steps > MAX_ZERO_SPEED_STEPS:
                    self.done = True
                    break
            else:
                zero_speed_steps = 0

            action = self.choose_action()
            self.apply_action(action)
            
            last_dist_to_checkpoint = dist_to_target
            total_reward += reward

        progress_multiplier = float(self.current_checkpoint) / len(self.checkpoints)
        total_reward *= (0.5 + progress_multiplier)
        
        success = self.current_checkpoint >= len(self.checkpoints)
        return total_reward, success

    def get_reward(self):
        if self.current_checkpoint >= len(self.checkpoints):
            return 0.0

        target_point = self.checkpoints[self.current_checkpoint]
        dist_to_target = distance(self.position, target_point)
        angle_diff = self.get_angle_to_target(target_point)
        
        # Base reward calculation
        reward = 0.0
        
        # Reward for getting closer to target
        if hasattr(self, 'last_dist_to_checkpoint'):
            if dist_to_target < self.last_dist_to_checkpoint:
                reward += 1.0
            else:
                reward -= 0.5
        self.last_dist_to_checkpoint = dist_to_target
        
        # Reward for good orientation
        if abs(angle_diff) < 45.0:
            reward += self.speed * 0.2
        
        # Reward for reaching checkpoint
        if dist_to_target < 30.0:
            reward += 100.0
            self.current_checkpoint += 1
            
            if self.current_checkpoint >= len(self.checkpoints):
                reward += 1000.0
                self.done = True
                return reward
        
        # Penalty for being stationary
        if self.speed < MIN_SPEED_THRESHOLD:
            reward -= 1.0
        
        return reward

# Particle class for PSO
class Particle:
    def __init__(self, num_weights):
        self.position = np.random.uniform(POSITION_MIN, POSITION_MAX, num_weights)
        self.velocity = np.random.uniform(-0.1, 0.1, num_weights)
        self.personal_best_position = self.position.copy()
        self.personal_best_fitness = float('-inf')

    def update_personal_best(self, fitness):
        if fitness > self.personal_best_fitness:
            self.personal_best_fitness = fitness
            self.personal_best_position = self.position.copy()

class PolicyNetwork(nn.Module):
    def __init__(self, state_dim, hidden_dim, action_dim):
        super(PolicyNetwork, self).__init__()
        self.network = nn.Sequential(
            nn.Linear(state_dim, hidden_dim),
            nn.ReLU(),
            nn.Linear(hidden_dim, hidden_dim),
            nn.ReLU(),
            nn.Linear(hidden_dim, action_dim),
            nn.Softmax(dim=-1)
        )
    
    def forward(self, x):
        return self.network(x)

class ValueNetwork(nn.Module):
    def __init__(self, state_dim, hidden_dim):
        super(ValueNetwork, self).__init__()
        self.network = nn.Sequential(
            nn.Linear(state_dim, hidden_dim),
            nn.ReLU(),
            nn.Linear(hidden_dim, hidden_dim),
            nn.ReLU(),
            nn.Linear(hidden_dim, 1)
        )
    
    def forward(self, x):
        return self.network(x)

# Game visualization
def main():
    # Define track waypoints and checkpoints
    waypoints = [
        (200, 400), (400, 400), (600, 400), (800, 400),  # Bottom straight
        (900, 400), (900, 300), (900, 200),              # First turn
        (800, 200), (600, 200), (400, 200), (200, 200),  # Top straight
        (200, 300), (200, 400)                           # Final turn
    ]

    checkpoints = [
        (500, 400),  # Bottom straight
        (900, 300),  # First turn
        (500, 200),  # Top straight
        (200, 300)   # Final turn
    ]

    # Training phase
    print("Starting training with PPO...")
    car = Car(waypoints, checkpoints)
    episode = 0
    best_reward = float('-inf')
    
    while episode < MAX_GENERATIONS:
        states, actions, rewards, old_probs = [], [], [], []
        total_reward = 0
        car.reset()
        
        while not car.done and car.steps < MAX_STEPS_PER_EPISODE:
            state = car.get_state()
            
            # Get action and its probability
            with torch.no_grad():
                probs = car.policy(state)
                dist = Categorical(probs)
                action = dist.sample()
                old_prob = dist.log_prob(action)
            
            # Store old action probability
            old_probs.append(old_prob)
            
            # Execute action and get reward
            car.apply_action(action.item())
            reward = car.get_reward()
            
            # Store transition
            states.append(state)
            actions.append(action)
            rewards.append(reward)
            total_reward += reward
            
        # Update policy after episode
        car.update(states, actions, rewards, old_probs)
        
        if total_reward > best_reward:
            best_reward = total_reward
            print(f"New best reward: {best_reward:.2f} at episode {episode}")
            # Save the best model
            torch.save({
                'policy_state_dict': car.policy.state_dict(),
                'value_state_dict': car.value.state_dict(),
            }, 'best_model.pth')
        
        if episode % 10 == 0:
            print(f"Episode {episode}, Reward: {total_reward:.2f}")
        
        episode += 1

    # Visualization phase
    print("\nTraining completed. Starting visualization...")
    pygame.init()
    screen = pygame.display.set_mode((1000, 800))
    pygame.display.set_caption("2D Racing with PPO")
    clock = pygame.time.Clock()

    # Load the best model
    checkpoint = torch.load('best_model.pth')
    car.policy.load_state_dict(checkpoint['policy_state_dict'])
    car.value.load_state_dict(checkpoint['value_state_dict'])
    
    # Visualization loop
    running = True
    car.reset()
    
    while running:
        for event in pygame.event.get():
            if event.type == pygame.QUIT:
                running = False

        # Clear screen
        screen.fill((255, 255, 255))
        
        # Draw track
        for i in range(len(waypoints) - 1):
            pygame.draw.line(screen, (200, 200, 200), waypoints[i], waypoints[i + 1], 2)
        
        # Draw checkpoints
        for checkpoint in checkpoints:
            pygame.draw.circle(screen, (0, 255, 0), checkpoint, 10)
        
        # Update car
        if not car.done:
            action = car.choose_action()
            car.apply_action(action)
            
            # Draw car
            pygame.draw.circle(screen, (255, 0, 0), car.position.astype(int), 5)
            
            # Draw car's path
            if len(car.path) > 1:
                pygame.draw.lines(screen, (0, 0, 255), False, 
                                [tuple(map(int, pos)) for pos in car.path], 1)
        
        pygame.display.flip()
        clock.tick(60)
        
        # Reset car if done
        if car.done:
            car.reset()

    pygame.quit()

if __name__ == "__main__":
    main()