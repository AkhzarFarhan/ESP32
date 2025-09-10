from flask import Flask, jsonify, request
import random
import math

# --- Simulation Parameters ---
# Physics
GRAVITY = -0.05
MAIN_THRUST_POWER = 0.12
ROTATION_THRUST_POWER = 0.015
FRICTION = 0.99 # A little air resistance to stabilize things

# World
WORLD_WIDTH = 400
WORLD_HEIGHT = 300
PAD_WIDTH = 40

# Lander
STARTING_FUEL = 500.0

class LanderSimulator:
    def __init__(self):
        """Initializes the simulation environment."""
        self.reset()

    def reset(self):
        """
        Resets the lander to a new random starting position and the pad to a new location.
        This forces the agent to learn a general solution, not just memorize one trajectory.
        """
        # Pad location
        self.pad_x = random.uniform(PAD_WIDTH, WORLD_WIDTH - PAD_WIDTH)
        
        # Lander initial state
        self.x = random.uniform(0, WORLD_WIDTH)
        self.y = random.uniform(WORLD_HEIGHT * 0.8, WORLD_HEIGHT * 0.9)
        self.vx = random.uniform(-2, 2)
        self.vy = random.uniform(-1, 1)
        self.angle = random.uniform(-math.pi / 4, math.pi / 4) # Angle in radians
        self.angular_velocity = 0
        self.fuel = STARTING_FUEL
        
        return self._get_state_dict()

    def _get_state_dict(self):
        """Helper to package the current state into a dictionary."""
        return {
            "x": self.x, "y": self.y,
            "vx": self.vx, "vy": self.vy,
            "angle": self.angle,
            "fuel": self.fuel,
            "pad_x": self.pad_x
        }

    def step(self, action):
        """
        Performs one time-step of the simulation.
        - action (int): 0=Nothing, 1=Main Thruster, 2=Rotate Left, 3=Rotate Right
        """
        # --- Apply Actions if there's fuel ---
        if self.fuel > 0:
            if action == 1: # Main Thruster
                # Convert angle to a force vector
                force_x = -math.sin(self.angle) * MAIN_THRUST_POWER
                force_y = math.cos(self.angle) * MAIN_THRUST_POWER
                self.vx += force_x
                self.vy += force_y
                self.fuel -= 1.0
            elif action == 2: # Rotate Left
                self.angular_velocity -= ROTATION_THRUST_POWER
                self.fuel -= 0.1
            elif action == 3: # Rotate Right
                self.angular_velocity += ROTATION_THRUST_POWER
                self.fuel -= 0.1

        # --- Apply Physics ---
        self.vy += GRAVITY
        
        self.vx *= FRICTION # Apply friction
        self.vy *= FRICTION
        
        self.x += self.vx
        self.y += self.vy
        
        self.angle += self.angular_velocity
        self.angular_velocity *= FRICTION

        # --- Calculate Reward and Done state ---
        done = False
        reward = 0.0

        # Small rewards/penalties to guide the agent
        # Penalize distance from pad, reward for being above it
        dist_to_pad = abs(self.x - self.pad_x) / WORLD_WIDTH
        reward -= dist_to_pad * 0.5 

        # Penalize high velocity and being tilted
        reward -= (abs(self.vx) + abs(self.vy)) * 0.1
        reward -= abs(self.angle) * 0.2

        # Check for terminal conditions
        if self.y <= 0 or self.x <= 0 or self.x >= WORLD_WIDTH or self.fuel <= 0:
            done = True
            is_on_pad = abs(self.x - self.pad_x) < PAD_WIDTH / 2
            is_slow_enough = math.sqrt(self.vx**2 + self.vy**2) < 2.0
            is_upright = abs(self.angle) < 0.2 # about 11 degrees

            if is_on_pad and is_slow_enough and is_upright:
                reward = 100.0  # Big reward for successful landing!
                print("SUCCESSFUL LANDING!")
            else:
                reward = -100.0 # Big penalty for crashing
                print("CRASHED!")

        return {**self._get_state_dict(), "reward": reward, "done": done}

# --- Flask API Setup ---
app = Flask(__name__)
sim = LanderSimulator()

@app.route('/reset', methods=['GET'])
def reset_environment():
    initial_state = sim.reset()
    print(f"--- EPISODE RESET --- New Pad at: {initial_state['pad_x']:.2f}")
    return jsonify(initial_state)

@app.route('/step', methods=['GET'])
def step_environment():
    try:
        action = int(request.args.get('action'))
        if action not in [0, 1, 2, 3]:
            raise ValueError("Action must be 0, 1, 2, or 3")
        state = sim.step(action)
        return jsonify(state)
    except Exception as e:
        return jsonify({"error": str(e)}), 400

if __name__ == '__main__':
    app.run(host='0.0.0.0', port=5000)
