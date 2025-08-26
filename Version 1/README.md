## 🎯 Key Points

- **Language & Standard**: Written in **C++17** with cross‑platform support (Windows & POSIX systems).
- **Algorithm**: Uses **Recursive Depth‑First Search (DFS)** for procedural maze generation.
- **Maze Size**: Default **41×21** grid (odd dimensions for proper generation).
- **Cross‑Platform Input**:  
  - **Windows** → `_getch()` from `<conio.h>`  
  - **Linux/macOS** → Custom `getch_posix()` using `<termios.h>` and `<unistd.h>`
- **Real‑Time Rendering**:  
  - ANSI escape codes for **smooth movement** without redrawing the whole maze.  
  - **No flicker** due to cursor hiding/showing control.
- **Player & Exit**:  
  - Player represented by **`O`**  
  - Exit represented by **`E`**
- **Controls**:  
  - **W** → Up  
  - **A** → Left  
  - **S** → Down  
  - **D** → Right
- **Win Condition**: Reach the exit tile (`E`) to finish the game.
- **Single‑Draw Optimization**: Maze is drawn **once**; only player position updates afterward.
- **Randomness**: Uses `std::mt19937` seeded with `std::chrono` for unique mazes every run.

---

## ✨ Features

- 🌀 **Procedural Maze Generation** – Every playthrough is unique.
- ⚡ **Optimized Rendering** – Updates only changed positions for smooth gameplay.
- 🖥 **Cross‑Platform Compatibility** – Works on Windows, Linux, and macOS terminals.
- 🎮 **Simple Controls** – Easy to pick up, no extra dependencies.
- 🏁 **Clear Objective** – Find your way to the exit (`E`) to win.
- 🔄 **Replayable** – New maze layout each time you run the program.
- 📜 **Minimal Dependencies** – Standard C++ libraries only.