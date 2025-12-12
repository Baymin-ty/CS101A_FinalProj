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

| Key | Action |
|-----|--------|
| W/A/S/D | Move tank |
| Mouse | Aim turret |
| Left Click (Hold) | Continuous fire |
| E | Recruit nearby NPC (Multiplayer, costs 5 coins) |
| ESC | Pause / Return to menu |
| P | Pause game |
| R | Restart |

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
- Spend 5 coins to recruit a nearby NPC (press E when prompted)
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

| Module | Description |
|--------|-------------|
| `Game` | Main game loop, state machine, scene rendering, event handling |
| `Tank` | Tank entity - input handling, movement, rotation, shooting |
| `Enemy` | NPC AI - patrol, chase, and attack behaviors |
| `Bullet` | Bullet physics, collision detection, damage calculation |
| `Maze` | Maze data structure, tile rendering, wall collision |
| `MazeGenerator` | DFS-based random maze generation |
| `NetworkManager` | Singleton network manager for TCP communication |
| `AudioManager` | Singleton audio manager with BGM and distance-based SFX |
| `CollisionSystem` | Centralized collision detection and effect handling |

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
  - Maximum hearing range: 800 pixels

## License

MIT License
