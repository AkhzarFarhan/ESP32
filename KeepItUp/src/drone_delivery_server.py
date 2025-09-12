from flask import Flask, jsonify, request
import random
import math

# --- Simulation Parameters ---
# World
WORLD_WIDTH = 400
WORLD_HEIGHT = 300
# Drone
THRUST_POWER = 0.15
FRICTION = 0.98
STARTING_BATTERY = 600.0
# Obstacles
NUM_BUILDINGS = 5
MAX_BUILDING_WIDTH = 40
MAX_BUILDING_HEIGHT = 200

class DroneSimulator:
    def __init__(self):
        self.reset()

    def reset(self):
        """Generates a new, random city layout and mission."""
        # Generate Buildings (Obstacles)
        self.buildings = []
        for _ in range(NUM_BUILDINGS):
            w = random.uniform(20, MAX_BUILDING_WIDTH)
            h = random.uniform(50, MAX_BUILDING_HEIGHT)
            x = random.uniform(0, WORLD_WIDTH - w)
            self.buildings.append({'x': x, 'y': 0, 'w': w, 'h': h})

        # Mission locations
        self.pickup_x = random.uniform(20, WORLD_WIDTH - 20)
        self.pickup_y = random.uniform(20, 50)
        self.dropoff_x = random.uniform(20, WORLD_WIDTH - 20)
        self.dropoff_y = random.uniform(20, 50)

        # Drone State
        self.x = WORLD_WIDTH / 2
        self.y = WORLD_HEIGHT - 20
        self.vx = 0
        self.vy = 0
        self.battery = STARTING_BATTERY
        self.has_package = False
        
        return self._get_state_dict()

    def _get_state_dict(self):
        """Packages the current state into a dictionary."""
        target_x = self.dropoff_x if self.has_package else self.pickup_x
        target_y = self.dropoff_y if self.has_package else self.pickup_y
        return {
            "x": self.x, "y": self.y, "vx": self.vx, "vy": self.vy,
            "battery": self.battery, "has_package": self.has_package,
            "target_x": target_x, "target_y": target_y
        }

    def step(self, action):
        """
        Performs one time-step of the simulation.
        - action (int): 0=Nothing, 1=Thrust Up, 2=Thrust Down, 3=Thrust Left, 4=Thrust Right
        """
        # --- Apply Actions ---
        if self.battery > 0:
            if action == 1: self.vy += THRUST_POWER
            elif action == 2: self.vy -= THRUST_POWER
            elif action == 3: self.vx -= THRUST_POWER
            elif action == 4: self.vx += THRUST_POWER
            self.battery -= 0.5 # Constant battery drain
            if action != 0: self.battery -= 0.5 # Extra drain for thrust

        # --- Apply Physics ---
        self.vy -= 0.02 # A little bit of gravity
        self.vx *= FRICTION
        self.vy *= FRICTION
        self.x += self.vx
        self.y += self.vy

        # --- Calculate Reward and Done state ---
        done = False
        reward = -0.1 # Small penalty for each step to encourage speed

        # Guidance reward: get closer to the current target
        current_target = (self.dropoff_x, self.dropoff_y) if self.has_package else (self.pickup_x, self.pickup_y)
        dist_before = math.hypot(self.x - current_target[0], self.y - current_target[1])
        dist_after = math.hypot((self.x + self.vx) - current_target[0], (self.y + self.vy) - current_target[1])
        reward += (dist_before - dist_after) * 0.1 # Reward for making progress

        # Check for collisions and mission completion
        if self.y <= 0 or self.y >= WORLD_HEIGHT or self.x <= 0 or self.x >= WORLD_WIDTH or self.battery <= 0:
            done = True
            reward = -100.0 # Crash penalty
        for b in self.buildings:
            if self.x > b['x'] and self.x < b['x'] + b['w'] and self.y < b['h']:
                done = True
                reward = -100.0 # Crash penalty

        # Check mission objectives
        if not self.has_package and math.hypot(self.x - self.pickup_x, self.y - self.pickup_y) < 15:
            self.has_package = True
            reward = 50.0 # Big reward for picking up package
        elif self.has_package and math.hypot(self.x - self.dropoff_x, self.y - self.dropoff_y) < 15:
            done = True
            reward = 200.0 # Huge reward for successful delivery

        return {**self._get_state_dict(), "reward": reward, "done": done}

# --- Flask API Setup ---
app = Flask(__name__)
sim = DroneSimulator()
@app.route('/reset')
def reset_env(): return jsonify(sim.reset())
@app.route('/step')
def step_env(): return jsonify(sim.step(int(request.args.get('action'))))

if __name__ == '__main__':
    app.run(host='0.0.0.0', port=5000)
