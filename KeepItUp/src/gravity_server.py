from flask import Flask, jsonify, request
import random

# --- Simulation Parameters ---
GRAVITY = -0.5       # A constant downward acceleration
THRUST_POWER = 1.2   # How powerful the upward thrust is
MAX_HEIGHT = 100     # The top boundary
GROUND_LEVEL = 0     # The bottom boundary
STARTING_HEIGHT = 50 # Where the agent starts each time

class GravitySimulator:
    def __init__(self):
        """
        Initializes the simulation environment.
        """
        self.position = STARTING_HEIGHT
        self.velocity = 0.0
        self.max_steps = 500 # Prevent episodes from running forever
        self.current_step = 0

    def reset(self):
        """
        Resets the simulation to the starting state for a new episode.
        """
        self.position = STARTING_HEIGHT
        self.velocity = random.uniform(-1.0, 1.0) # Start with a little randomness
        self.current_step = 0
        return {"position": self.position, "velocity": self.velocity}

    def step(self, action):
        """
        Performs one time-step of the simulation.
        - action (int): 0 for 'do nothing', 1 for 'thrust'.
        """
        self.current_step += 1
        
        # 1. Apply action from the agent (the ESP32)
        if action == 1:
            self.velocity += THRUST_POWER

        # 2. Apply the laws of physics (gravity)
        self.velocity += GRAVITY
        self.position += self.velocity

        # 3. Check for terminal conditions (a "fall" or "crash")
        done = False
        if self.position <= GROUND_LEVEL or self.position >= MAX_HEIGHT or self.current_step >= self.max_steps:
            done = True

        # 4. Calculate the reward
        if done:
            reward = -100  # Big punishment for crashing
        else:
            reward = 1     # +1 reward for every step it survives

        return {
            "position": self.position,
            "velocity": self.velocity,
            "reward": reward,
            "done": done
        }

# --- Flask API Setup ---
app = Flask(__name__)
sim = GravitySimulator() # Create a single instance of our simulator

@app.route('/reset', methods=['GET'])
def reset_environment():
    """API endpoint to start a new episode."""
    initial_state = sim.reset()
    print("--- EPISODE RESET ---")
    return jsonify(initial_state)

@app.route('/step', methods=['GET'])
def step_environment():
    """
    API endpoint to take a step.
    Expects an action like: /step?action=1
    """
    try:
        action = int(request.args.get('action'))
        if action not in [0, 1]:
            raise ValueError("Action must be 0 or 1")
            
        state = sim.step(action)
        # Optional: print server-side state for debugging
        # print(f"Action: {action}, Pos: {state['position']:.2f}, Vel: {state['velocity']:.2f}, Done: {state['done']}")
        return jsonify(state)

    except (TypeError, ValueError) as e:
        return jsonify({"error": str(e)}), 400


if __name__ == '__main__':
    # Run the server on your local network IP.
    # On Windows, use 'ipconfig'. On Mac/Linux, use 'ifconfig' or 'ip a'.
    # This allows the ESP32 to connect to it.
    app.run(host='0.0.0.0', port=5000, debug=True)
