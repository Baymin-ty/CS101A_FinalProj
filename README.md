# Tank Maze 🎮

A tank maze game built with SFML 3.0, featuring single-player and multiplayer modes.

## Game Overview

Navigate your tank through randomly generated mazes! The game offers two exciting modes:

- **Single Player**: Navigate through the maze, avoid or defeat NPCs, and reach the exit
- **Multiplayer**: Real-time online battle - race to the exit or defeat your opponent

## Features

- 🎲 **Random Maze Generation**: Every game features a unique maze layout
- 🎯 **Customizable Settings**: Adjust map size and NPC count
- 💥 **Destructible Walls**: Some walls can be destroyed by bullets to create new paths
- 🎨 **Special Walls** (Multiplayer only):
  - 🟡 **Gold Walls**: Earn 2 coins when destroyed
  - 🔵 **Heal Walls**: Restore 25% health when destroyed
  - 🔴 **Explosive Walls**: Explode and destroy surrounding walls
- 🌐 **Real-time Multiplayer**: TCP-based two-player online system
- 🎵 **Dynamic Audio**: Background music and sound effects with distance-based volume
- 🤖 **NPC System**: In multiplayer, spend coins to recruit NPCs to fight for you

## Controls

| Key               | Action                                                    |
| ----------------- | --------------------------------------------------------- |
| W/A/S/D           | Move tank                                                 |
| Mouse             | Aim turret                                                |
| Left Click (Hold) | Continuous fire                                           |
| R                 | Recruit nearby NPC (Multiplayer, costs 3 coins) / Restart |
| ESC               | Pause / Return to menu                                    |
| P                 | Pause game                                                |

## Game Modes

### Single Player

1. Select **Start Game** from the main menu
2. Customize your game:
   - **Map Width**: 21-151
   - **Map Height**: 15-101
   - **NPCs**: 3-100
3. Navigate through the maze and reach the green exit to win

**Note**: You don't need to defeat all NPCs - just reach the exit!

### Multiplayer

1. Start the server: `node server/server.js`
2. Select **Multiplayer** from the main menu
3. Enter the server IP address (use 127.0.0.1 for local testing)
4. **Create Room**: Press `C` to create a room and get a room code
5. **Join Room**: Enter a room code to join an existing room
6. Game starts automatically when both players are ready

**Victory Conditions**:
- First to reach the green exit wins
- Or defeat your opponent

**Special Mechanics**:
- Destroy gold walls to earn coins
- Spend 3 coins to recruit a nearby NPC (press R when prompted)
- Recruited NPCs will fight for you
- When either player sees the exit, epic battle music begins for both!

**Restart**:
- Host presses R: Regenerates maze, waits for other player
- Guest presses R: Automatically rejoins the room

## Building & Running

### Prerequisites

- CMake 3.16+
- C++20 compiler
- SFML 3.0 (automatically downloaded via FetchContent)
- Node.js (required for multiplayer server only)

### Build

```bash
mkdir Build && cd Build
cmake ..
make -j
```

### Run

```bash
# Single Player (from project root)
./Build/CS101AFinalProj.app/Contents/MacOS/CS101AFinalProj

# Multiplayer (start server first)
node server/server.js &
./Build/CS101AFinalProj.app/Contents/MacOS/CS101AFinalProj
```

## Project Structure

```
CS101A_FinalProj/
├── CMakeLists.txt          # CMake build configuration
├── README.md               # Project documentation
│
├── src/                    # Source code
│   ├── main.cpp            # Entry point
│   ├── Game.cpp            # Main game logic (state management, rendering)
│   ├── Tank.cpp            # Tank class (player control, movement, shooting)
│   ├── Enemy.cpp           # NPC AI (patrol, chase, attack)
│   ├── Bullet.cpp          # Bullet physics and collision
│   ├── Maze.cpp            # Maze data, rendering, collision detection
│   ├── MazeGenerator.cpp   # Random maze generation (DFS algorithm)
│   ├── HealthBar.cpp       # Health bar UI component
│   ├── NetworkManager.cpp  # Network management (TCP connection)
│   ├── AudioManager.cpp    # Audio system (BGM, SFX with distance attenuation)
│   ├── CollisionSystem.cpp # Collision detection and handling
│   ├── MultiplayerHandler.cpp # Multiplayer game logic
│   │
│   └── include/            # Header files
│       ├── Game.hpp
│       ├── Tank.hpp
│       ├── Enemy.hpp
│       ├── Bullet.hpp
│       ├── Maze.hpp
│       ├── MazeGenerator.hpp
│       ├── HealthBar.hpp
│       ├── NetworkManager.hpp
│       ├── AudioManager.hpp
│       ├── CollisionSystem.hpp
│       └── Utils.hpp
│
├── server/                 # Multiplayer server
│   └── server.js           # Node.js TCP server
│
├── tank_assets/            # Tank sprites
│   └── PNG/
│       ├── Hulls_Color_A/  # Player tank hull
│       ├── Hulls_Color_B/  # Opponent/NPC tank hull
│       ├── Weapon_Color_A/ # Player turret
│       └── Weapon_Color_B/ # Opponent/NPC turret
│
├── music_assets/           # Audio files
│   ├── menu.mp3            # Menu background music
│   ├── start.mp3           # Game start background music
│   ├── climax.mp3          # Battle climax music (when exit is visible)
│   ├── shoot.mp3           # Shooting sound effect
│   ├── explode.mp3         # Explosion sound effect
│   ├── Bingo.mp3           # Heal wall destroyed sound
│   └── collectCoins.mp3    # Coin collection sound
│
└── Build/                  # Build output directory
```

## Core Modules

| Module            | Description                                                    |
| ----------------- | -------------------------------------------------------------- |
| `Game`            | Main game loop, state machine, scene rendering, event handling |
| `Tank`            | Tank entity - input handling, movement, rotation, shooting     |
| `Enemy`           | NPC AI - patrol, chase, and attack behaviors                   |
| `Bullet`          | Bullet physics, collision detection, damage calculation        |
| `Maze`            | Maze data structure, tile rendering, wall collision            |
| `MazeGenerator`   | DFS-based random maze generation                               |
| `NetworkManager`  | Singleton network manager for TCP communication                |
| `AudioManager`    | Singleton audio manager with BGM and distance-based SFX        |
| `CollisionSystem` | Centralized collision detection and effect handling            |

## Network Protocol

Multiplayer mode uses a custom binary protocol:

```
[2 bytes: length][1 byte: type][payload...]
```

Message Types:
- `CreateRoom` / `JoinRoom`: Room operations
- `MazeData`: Maze data synchronization
- `PlayerUpdate`: Player state sync (position, angle, health)
- `PlayerShoot`: Shooting events
- `NpcActivate` / `NpcUpdate`: NPC recruitment and state sync
- `ClimaxStart`: Music synchronization
- `GameResult`: Game outcome

## Audio System

- **Background Music**: 
  - Menu → Start → Climax (when exit becomes visible)
  - Synchronized across players in multiplayer
- **Sound Effects**: Distance-based volume attenuation
  - Closer sounds are louder
  - Hearing range dynamically calculated: ~864 pixels (LOGICAL_WIDTH × VIEW_ZOOM × 0.6)

## Technical Highlights & Innovations

### 🖥️ Resolution-Independent Rendering

The game uses a **logical resolution system** (1920×1080) that is independent of the actual window size. This ensures:
- Consistent gameplay experience across different screen sizes
- Proper aspect ratio preservation when resizing windows
- UI elements remain correctly positioned and scaled
- No visual distortion on ultra-wide or non-standard displays

```cpp
// Logical resolution constants
const unsigned int LOGICAL_WIDTH = 1920;
const unsigned int LOGICAL_HEIGHT = 1080;

// View automatically scales to fit any window size
m_window.setView(sf::View({0, 0, LOGICAL_WIDTH, LOGICAL_HEIGHT}));
```

### 🎨 Smooth Color Interpolation for Destructible Walls

Destructible walls feature **real-time color interpolation** based on their remaining health:
- Walls smoothly transition from bright to dark as they take damage
- Special walls (Gold/Heal/Explosive) maintain their distinctive colors while showing damage
- Uses linear interpolation (lerp) for seamless visual feedback

```cpp
// Health-based color interpolation
float healthRatio = wall.health / wall.maxHealth;
color.r = dark.r + (original.r - dark.r) * healthRatio;
color.g = dark.g + (original.g - dark.g) * healthRatio;
color.b = dark.b + (original.b - dark.b) * healthRatio;
```

### 🔊 Distance-Based 3D Audio System

A sophisticated audio system that creates spatial awareness:
- **Volume attenuation**: Sounds decrease in volume based on distance from the player
- **Hearing range**: Configurable maximum range (default 800 pixels)
- **Linear falloff**: Volume = (1 - distance/maxRange) × baseVolume
- Sounds beyond the hearing range are not played (performance optimization)

```cpp
float AudioManager::calculateVolume(sf::Vector2f soundPos, sf::Vector2f listenerPos) {
  float distance = std::sqrt(dx*dx + dy*dy);
  if (distance >= m_listeningRange) return 0.f;
  float volumeRatio = 1.f - (distance / m_listeningRange);
  return m_sfxVolume * volumeRatio;
}
```

### 🎮 Smooth Tank Movement with Angle Interpolation

Tank rotation uses **angular interpolation (lerp)** for smooth turning:
- Hull gradually rotates toward movement direction
- Turret independently tracks mouse position
- Prevents jarring instant rotations
- Handles angle wrapping correctly (e.g., 350° to 10°)

```cpp
// Smooth angle interpolation with wrap-around handling
float lerpAngle(float from, float to, float t) {
  float diff = fmod(to - from + 540.f, 360.f) - 180.f;
  return from + diff * t;
}
```

### 🌐 Custom Binary Network Protocol

Efficient multiplayer synchronization using a custom binary protocol:
- **Minimal overhead**: 2-byte length header + 1-byte message type
- **State synchronization**: Player position, rotation, health, and actions
- **Event-driven**: Shooting, NPC recruitment, and game results
- **Cross-platform**: Works on macOS, Windows, and Linux

### 🧠 NPC AI with State Machine

Enemy NPCs feature intelligent behavior:
- **Patrol state**: Random wandering with obstacle avoidance
- **Chase state**: Pathfinding toward detected players
- **Attack state**: Aiming and shooting at targets
- **Activation system**: NPCs can be recruited by players in multiplayer

### 🎯 Wall Sliding Collision System

Advanced collision response that allows smooth movement along walls:
- Attempts X-axis movement if Y is blocked (and vice versa)
- Prevents getting stuck in corners
- Maintains momentum in valid directions

```cpp
if (maze.checkCollision(newPos, radius)) {
  bool canMoveX = !maze.checkCollision({oldPos.x + movement.x, oldPos.y}, radius);
  bool canMoveY = !maze.checkCollision({oldPos.x, oldPos.y + movement.y}, radius);
  // Apply valid movement component
}
```

### 🎵 Synchronized Multiplayer BGM

Background music is synchronized between players:
- When either player sees the exit, **both players** hear the climax music
- Creates shared dramatic moments in competitive gameplay
- Uses network messages to trigger audio state changes

### 🏗️ DFS-Based Maze Generation

Mazes are generated using **Depth-First Search** with randomization:
- Guaranteed solvable path from start to exit
- Configurable destructible wall ratio
- Strategic placement of special walls and NPC spawn points
- Multiplayer-aware generation (special walls only in multiplayer mode)

## License

MIT License
