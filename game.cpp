#include "raylib.h"
#include <vector>
#include <random>       
#include <chrono>       
#include <algorithm>    
#include <string>
#include <stack>        
#include <queue>        
#include <map>          
#include <functional>   

using namespace std;

#define RAYGUI_IMPLEMENTATION
#include "raygui.h" 

// --- Game States & Settings ---
typedef enum GameScreen { HOME = 0, LEVEL_SELECT, GAMEPLAY, WIN, GAMEOVER } GameScreen;
GameScreen currentScreen = HOME; 

const int fixedScreenWidth = 1280; 
const int fixedScreenHeight = 720; 
const int HUD_HEIGHT = 70; // Height of the HUD

int mazeWidth = 51;   
int mazeHeight = 25; 
const int TILE_SIZE = 20; 

// The time limit for each level in seconds. You can adjust these values.
const float levelTimeLimits[] = {50.0f, 100.0f, 165.0f, 210.0f};

int mazeOffsetX = 0;
int mazeOffsetY = 0;

// Gameplay Variables
vector<vector<char>> maze; 
int playerX, playerY;
int exitX, exitY;
float totalTime = 0.0f; 
int totalMoves = 0;   
int idealMoves = 0;   
float accuracy = 0.0f;
float playerRenderX;
float playerRenderY;
const float playerMoveSpeed = 10.0f; // প্লেয়ারের স্মুথ মুভমেন্টের গতি (adjust as needed)
float pulseTimer = 0.0f;            // এগজিট সাইন পালসিং এর জন্য টাইমার

int currentLevelIndex = 0; 
const char* levelNames[] = {"Easy", "Medium", "Hard", "Very Hard"};

// --- New Enum for Entrance/Exit Strategies ---
typedef enum EntranceExitStrategy { 
    RANDOM_PLACEMENT = 0, 
    TOP_LEFT_BOTTOM_RIGHT, 
    LEFT_RIGHT_CENTER 
} EntranceExitStrategy;

// --- Text Animation Variables for Home Screen ---
struct TextAnimation {
    string fullText;
    string displayedText;
    float typingTimer;
    float typingSpeed; 
    int currentDisplayCharIndex;
    bool animationComplete;

    void Start(const string& text, float speed) {
        fullText = text;
        displayedText = "";
        typingTimer = 0.0f;
        typingSpeed = speed;
        currentDisplayCharIndex = 0;
        animationComplete = false;
    }

    void Update(float dt) {
        if (animationComplete) return;

        typingTimer += dt;
        int targetChars = static_cast<int>(typingTimer * typingSpeed);

        if (targetChars > currentDisplayCharIndex) {
            for (int i = currentDisplayCharIndex; i < fullText.length() && i < targetChars; ++i) { 
                displayedText += fullText[i];
            }
            currentDisplayCharIndex = targetChars;
        }

        if (currentDisplayCharIndex >= fullText.length()) {
            animationComplete = true;
        }
    }
};

struct ScrambleTextEffect {
    string originalText;
    string scrambledText;
    float effectTimer;
    float effectDuration;
    bool isActive;
    mt19937* rngPtr; 

    void Init(const string& text, mt19937* rng) {
        originalText = text;
        scrambledText = text;
        effectTimer = 0.0f;
        effectDuration = 0.1f; 
        isActive = false;
        rngPtr = rng;
    }

    void Start() {
        if (!isActive) {
            isActive = true;
            effectTimer = 0.0f;
        }
    }

    void Stop() {
        isActive = false;
        scrambledText = originalText; 
    }

    string GetText(float dt) {
        if (!isActive) return originalText;

        effectTimer += dt;
        if (effectTimer >= effectDuration) {
            scrambledText = originalText; 
            uniform_int_distribution<> dist(0, originalText.length() - 1);
            uniform_int_distribution<> char_dist(33, 126); 

            int numScrambleChars = max(1, (int)(originalText.length() * 0.3)); 
            for (int i = 0; i < numScrambleChars; ++i) {
                int indexToScramble = dist(*rngPtr);
                scrambledText[indexToScramble] = static_cast<char>(char_dist(*rngPtr));
            }
            effectTimer = 0.0f; 
        }
        return scrambledText;
    }
};

TextAnimation welcomeTextAnim;
ScrambleTextEffect pressEnterTextEffect;
ScrambleTextEffect levelButtonsTextEffects[4]; 

Texture2D logoTexture; 

mt19937 rng;

// --- Maze Generation Algorithms ---

// Iterative DFS (Depth-First Search) maze generation to avoid stack overflow
void generateMazeDFS(int startX, int startY) {
    maze.assign(mazeHeight, vector<char>(mazeWidth, '#'));
    stack<pair<int, int>> s;
    s.push({startX, startY});
    maze[startY][startX] = ' ';

    int dX[] = {0, 0, 1, -1}; 
    int dY[] = {-1, 1, 0, 0};
    
    while (!s.empty()) {
        int x = s.top().first;
        int y = s.top().second;
        s.pop();

        vector<int> dirs = {0, 1, 2, 3};
        shuffle(dirs.begin(), dirs.end(), rng);

        for (int i : dirs) {
            int nextX = x + dX[i] * 2;
            int nextY = y + dY[i] * 2;
            int wallX = x + dX[i];
            int wallY = y + dY[i];

            if (nextX >= 0 && nextX < mazeWidth && nextY >= 0 && nextY < mazeHeight && maze[nextY][nextX] == '#') {
                maze[wallY][wallX] = ' ';
                maze[nextY][nextX] = ' ';
                s.push({nextX, nextY});
            }
        }
    }
}

// BFS (Breadth-First Search) maze generation - Note: This is actually a randomized DFS with a queue.
void generateMazeBFS(int startX, int startY) {
    maze.assign(mazeHeight, vector<char>(mazeWidth, '#')); 
    queue<pair<int, int>> q; 
    q.push({startX, startY});
    maze[startY][startX] = ' ';

    int dX[] = {0, 0, 1, -1}; 
    int dY[] = {-1, 1, 0, 0};

    // This BFS is a bit unusual for maze generation as it carves paths immediately.
    // A more common BFS for maze gen would typically involve building a 'frontier' of walls
    // or cells to visit and making decisions based on reaching unvisited areas.
    // This current implementation more closely resembles a randomized DFS if it were recursive,
    // but using a queue makes it iterative.
    while (!q.empty()) {
        pair<int, int> current = q.front();
        q.pop();
        int x = current.first;
        int y = current.second;
        vector<int> indices = {0, 1, 2, 3};
        shuffle(indices.begin(), indices.end(), rng); 

        for (int i : indices) {
            int nextX = x + dX[i]*2; // Move 2 steps to ensure we are connecting cells, not just adjacent walls
            int nextY = y + dY[i]*2; // This makes it similar to DFS style of carving
            int wallX = x + dX[i];
            int wallY = y + dY[i];

            if (nextX > 0 && nextX < mazeWidth-1 && nextY > 0 && nextY < mazeHeight-1 && maze[nextY][nextX] == '#') {
                maze[wallY][wallX] = ' '; 
                maze[nextY][nextX] = ' '; 
                q.push({nextX, nextY});   
            }
        }
    }
}


// Kruskal's Algorithm maze generation
void generateMazeKruskal() {
    maze.assign(mazeHeight, vector<char>(mazeWidth, '#')); 
    for (int y = 1; y < mazeHeight - 1; y += 2) {
        for (int x = 1; x < mazeWidth - 1; x += 2) {
            maze[y][x] = ' ';
        }
    }
    struct Edge {
        int x1, y1, x2, y2;
    };
    vector<Edge> edges;
    for (int y = 1; y < mazeHeight - 1; y += 2) {
        for (int x = 1; x < mazeWidth - 2; x += 2) {
            edges.push_back({x, y, x + 2, y});
        }
    }
    for (int y = 1; y < mazeHeight - 2; y += 2) {
        for (int x = 1; x < mazeWidth - 1; x += 2) {
            edges.push_back({x, y, x, y + 2});
        }
    }
    shuffle(edges.begin(), edges.end(), rng);

    vector<int> parent;
    parent.resize((mazeWidth / 2) * (mazeHeight / 2));
    for (size_t i = 0; i < parent.size(); i++) parent[i] = i;

    function<int(int)> findSet = 
        [&](int i) {
            if (parent[i] == i) return i;
            return parent[i] = findSet(parent[i]);
        };
    
    function<void(int, int)> uniteSets = 
        [&](int a, int b) {
            a = findSet(a);
            b = findSet(b);
            if (a != b) parent[b] = a;
        };

    for (const auto& edge : edges) {
        int cell1 = (edge.y1 / 2) * (mazeWidth / 2) + (edge.x1 / 2);
        int cell2 = (edge.y2 / 2) * (mazeWidth / 2) + (edge.x2 / 2);
        if (findSet(cell1) != findSet(cell2)) {
            uniteSets(cell1, cell2);
            maze[(edge.y1 + edge.y2) / 2][(edge.x1 + edge.x2) / 2] = ' ';
        }
    }
}

// Prim's Algorithm maze generation
void generateMazePrim(int startX, int startY) {
    maze.assign(mazeHeight, vector<char>(mazeWidth, '#'));
    
    // Ensure startX and startY are odd, as Prim's operates on a grid of cells (odd coords)
    startX = (startX / 2) * 2 + 1;
    startY = (startY / 2) * 2 + 1;

    // A vector of walls to be considered
    vector<pair<int, int>> frontier;
    
    // Start with a single cell and add its surrounding walls to the frontier
    maze[startY][startX] = ' '; 
    int dX[] = {0, 0, 1, -1};
    int dY[] = {-1, 1, 0, 0};
    
    for (int i = 0; i < 4; ++i) {
        int wallX = startX + dX[i];
        int wallY = startY + dY[i];
        if (wallX > 0 && wallX < mazeWidth-1 && wallY > 0 && wallY < mazeHeight-1) {
            frontier.push_back({wallX, wallY});
        }
    }

    // Keep carving paths until there are no more walls in the frontier
    while (!frontier.empty()) {
        // Pick a random wall from the frontier
        uniform_int_distribution<> dist_frontier(0, frontier.size() - 1);
        int rand_idx = dist_frontier(rng); 
        
        pair<int, int> currentWall = frontier[rand_idx];
        frontier.erase(frontier.begin() + rand_idx);
        
        int wallX = currentWall.first; 
        int wallY = currentWall.second;
        
        // Find the cell on the opposite side of the wall
        // This is the cell to be connected to the maze
        int oppositeX, oppositeY;

        // Check horizontal walls
        if (wallX % 2 == 0) {
            // Wall is vertical, check left and right cells
            int leftCellX = wallX - 1;
            int rightCellX = wallX + 1;
            int cellY = wallY;
            
            if (leftCellX > 0 && maze[cellY][leftCellX] == ' ') {
                oppositeX = rightCellX;
                oppositeY = cellY;
            } else if (rightCellX < mazeWidth-1 && maze[cellY][rightCellX] == ' ') {
                oppositeX = leftCellX;
                oppositeY = cellY;
            } else {
                continue; // Wall is not between one visited and one unvisited cell
            }
        }
        // Check vertical walls
        else {
            // Wall is horizontal, check top and bottom cells
            int cellX = wallX;
            int topCellY = wallY - 1;
            int bottomCellY = wallY + 1;

            if (topCellY > 0 && maze[topCellY][cellX] == ' ') {
                oppositeX = cellX;
                oppositeY = bottomCellY;
            } else if (bottomCellY < mazeHeight-1 && maze[bottomCellY][cellX] == ' ') {
                oppositeX = cellX;
                oppositeY = topCellY;
            } else {
                continue; // Wall is not between one visited and one unvisited cell
            }
        }

        // Carve the path and add new walls to the frontier
        if (oppositeX > 0 && oppositeX < mazeWidth-1 && oppositeY > 0 && oppositeY < mazeHeight-1 && maze[oppositeY][oppositeX] == '#') {
            maze[wallY][wallX] = ' ';
            maze[oppositeY][oppositeX] = ' ';
            
            for (int i = 0; i < 4; ++i) {
                int newWallX = oppositeX + dX[i];
                int newWallY = oppositeY + dY[i];
                if (newWallX > 0 && newWallX < mazeWidth-1 && newWallY > 0 && newWallY < mazeHeight-1 && maze[newWallY][newWallX] == '#') {
                    frontier.push_back({newWallX, newWallY});
                }
            }
        }
    }
}

// --- Pathfinding Function (BFS for Shortest Path) ---
int calculateShortestPathLength(int startX, int startY, int targetX, int targetY) {
    if (startX < 0 || startX >= mazeWidth || startY < 0 || startY >= mazeHeight || maze[startY][startX] == '#') return -1;
    if (targetX < 0 || targetX >= mazeWidth || targetY < 0 || targetY >= mazeHeight || maze[targetY][targetX] == '#') return -1;
    
    queue<pair<int, int>> q;
    vector<vector<int>> dist(mazeHeight, vector<int>(mazeWidth, -1));

    q.push({startX, startY});
    dist[startY][startX] = 0;

    int dX[] = {0, 0, 1, -1};
    int dY[] = {-1, 1, 0, 0};

    while (!q.empty()) {
        pair<int, int> current = q.front();
        q.pop();
        int x = current.first;
        int y = current.second;

        if (x == targetX && y == targetY) {
            return dist[y][x];
        }

        for (int i = 0; i < 4; ++i) {
            int nx = x + dX[i];
            int ny = y + dY[i];

            if (nx >= 0 && nx < mazeWidth && ny >= 0 && ny < mazeHeight &&
                maze[ny][nx] != '#' && dist[ny][nx] == -1) {
                dist[ny][nx] = dist[y][x] + 1;
                q.push({nx, ny});
            }
        }
    }
    return -1;
}

// Helper function to find a valid ' ' cell for player/exit
pair<int, int> findValidEmptyCell() {
    uniform_int_distribution<> dist_x(1, mazeWidth - 2);
    uniform_int_distribution<> dist_y(1, mazeHeight - 2);
    int maxAttempts = 1000;
    for (int attempts = 0; attempts < maxAttempts; ++attempts) {
        int x = dist_x(rng);
        int y = dist_y(rng);
        if (maze[y][x] == ' ') {
            return {x, y};
        }
    }
    // Fallback if no random empty cell is found (shouldn't happen in a valid maze)
    return {1, 1}; 
}

// --- General Game Functions ---
void setupGame(int levelIdx, EntranceExitStrategy strategy) {
    currentLevelIndex = levelIdx;
    
    // Set maze dimensions based on level
    switch (levelIdx) {
        case 0: mazeWidth = 31; mazeHeight = 15; break;      // Easy
        case 1: mazeWidth = 41; mazeHeight = 21; break;      // Medium
        case 2: mazeWidth = 51; mazeHeight = 25; break;      // Hard
        case 3: mazeWidth = 61; mazeHeight = 31; break;      // Very Hard
        default: mazeWidth = 51; mazeHeight = 25; break; 
    }
    
    // Ensure maze dimensions are odd for proper generation with Prim's/DFS/BFS
    if (mazeWidth % 2 == 0) mazeWidth++;
    if (mazeHeight % 2 == 0) mazeHeight++;

    // Dynamically calculate offsets to center the maze
    mazeOffsetX = (fixedScreenWidth - mazeWidth * TILE_SIZE) / 2;
    mazeOffsetY = (fixedScreenHeight - mazeHeight * TILE_SIZE - HUD_HEIGHT) / 2 + HUD_HEIGHT;
    if (mazeOffsetX < 0) mazeOffsetX = 0;
    if (mazeOffsetY < 0) mazeOffsetY = 0;
    
    // ⭐⭐ Generate maze using the appropriate algorithm (EASY/MEDIUM SWAPPED) ⭐⭐
    switch (levelIdx) {
        case 0: generateMazeBFS(1, 1); break;   // Now EASY (using the less complex BFS structure)
        case 1: generateMazeDFS(1, 1); break;   // Now MEDIUM (using the more complex DFS structure)
        case 2: generateMazeKruskal(); break;
        case 3: generateMazePrim(1, 1); break;
        default: generateMazeDFS(1, 1); break;
    }
    // ⭐⭐ END SWAP ⭐⭐

    // --- Player and Exit Placement based on Strategy ---
    switch (strategy) {
        case RANDOM_PLACEMENT: {
            pair<int, int> playerPos = findValidEmptyCell();
            playerX = playerPos.first;
            playerY = playerPos.second;

            pair<int, int> exitPos;
            int maxAttempts = 1000;
            int attempts = 0;
            do {
                exitPos = findValidEmptyCell();
                attempts++;
            } while ((exitPos.first == playerX && exitPos.second == playerY) && attempts < maxAttempts);
            exitX = exitPos.first;
            exitY = exitPos.second;
        } break;

        case TOP_LEFT_BOTTOM_RIGHT: {
            playerX = 1; // Top-left
            playerY = 1;
            while(maze[playerY][playerX] == '#') { // Ensure player starts on a path
                playerX++; 
                if (playerX >= mazeWidth - 1) { playerX = 1; playerY++; }
                if (playerY >= mazeHeight - 1) { playerY = 1; break; } // Should not happen in valid maze
            }

            exitX = mazeWidth - 2; // Bottom-right
            exitY = mazeHeight - 2;
            while(maze[exitY][exitX] == '#') { // Ensure exit is on a path
                exitX--; 
                if (exitX <= 0) { exitX = mazeWidth - 2; exitY--; }
                if (exitY <= 0) { exitY = mazeHeight - 2; break; } // Should not happen
            }
            // Ensure player and exit are not the same (unlikely but good safeguard)
            if (playerX == exitX && playerY == exitY) {
                exitX = mazeWidth - 4; // Shift exit if it overlaps
                exitY = mazeHeight - 4;
            }

        } break;

        case LEFT_RIGHT_CENTER: {
            // Find a valid spot on the left edge (y-coordinate in the middle-ish)
            playerX = 1;
            playerY = mazeHeight / 2;
            while (maze[playerY][playerX] == '#') {
                playerY++;
                if (playerY >= mazeHeight - 1) playerY = 1; // Wrap around if needed
            }

            // Find a valid spot on the right edge (y-coordinate in the middle-ish)
            exitX = mazeWidth - 2;
            exitY = mazeHeight / 2;
            while (maze[exitY][exitX] == '#') {
                exitY++;
                if (exitY >= mazeHeight - 1) exitY = 1; // Wrap around if needed
            }
            // Ensure player and exit are not the same
            if (playerX == exitX && playerY == exitY) {
                exitX = mazeWidth - 4; // Shift exit if it overlaps
            }

        } break;
    }
    
    maze[exitY][exitX] = 'E';

    idealMoves = calculateShortestPathLength(playerX, playerY, exitX, exitY);
    if (idealMoves == -1) {
        TraceLog(LOG_WARNING, "No path found in generated maze!");
        idealMoves = 0;
    }

    // 1. Player Render Position Initialize for Smooth Movement
    playerRenderX = (float)playerX;
    playerRenderY = (float)playerY;

    // 2. Pulse Timer Initialize
    pulseTimer = 0.0f;

    // 3. Reset Game Stats
    totalTime = 0.0f;
    totalMoves = 0;

}

void drawMaze() {
    for (int i = 0; i < mazeHeight; ++i) {
        for (int j = 0; j < mazeWidth; ++j) {
            Rectangle tile = { (float)(j * TILE_SIZE + mazeOffsetX), (float)(i * TILE_SIZE + mazeOffsetY), (float)TILE_SIZE, (float)TILE_SIZE };
            
            if (maze[i][j] == '#') {
                // Draw a darker base for the brick
                DrawRectangleRec(tile, GetColor(0x4A4A4AFF)); // Darker gray for the main brick body
                // Draw a lighter highlight for a simple 3D effect
                DrawRectangle(tile.x, tile.y, TILE_SIZE, 2, GetColor(0x606060FF)); // Top edge highlight
                DrawRectangle(tile.x, tile.y, 2, TILE_SIZE, GetColor(0x606060FF)); // Left edge highlight
                } 
            //     else if (maze[i][j] == 'E') {
            //     // ⭐ Exit Sign Pulsing Logic
            //     float pulseFactor = sin(pulseTimer) * 0.5f + 0.5f; // Goes from 0.0 (dim) to 1.0 (bright)

            //     // Blend between LIME (base color) and WHITE (highlight color)
            //     Color baseColor = LIME;
                
            //     // ম্যানুয়ালি কালার ব্লেন্ড করা
            //     Color finalColor;
            //     finalColor.r = (unsigned char)(baseColor.r * (1.0f - pulseFactor) + WHITE.r * pulseFactor);
            //     finalColor.g = (unsigned char)(baseColor.g * (1.0f - pulseFactor) + WHITE.g * pulseFactor);
            //     finalColor.b = (unsigned char)(baseColor.b * (1.0f - pulseFactor) + WHITE.b * pulseFactor);
            //     finalColor.a = 255;
                
            //     Vector2 triA = { tile.x + TILE_SIZE / 2, tile.y + TILE_SIZE / 4 };
            //     Vector2 triB = { tile.x + TILE_SIZE / 4, tile.y + TILE_SIZE - TILE_SIZE / 4 };
            //     Vector2 triC = { tile.x + TILE_SIZE - TILE_SIZE / 4, tile.y + TILE_SIZE - TILE_SIZE / 4 };
            //     DrawTriangle(triA, triB, triC, finalColor); // ⭐ finalColor ব্যবহার করা হয়েছে
            // }

            else if (maze[i][j] == 'E') {
            // ⭐⭐ Color and Brightness Pulsing Logic (Using HSV) ⭐⭐
            
            float pulseFactor = sin(pulseTimer) * 0.5f + 0.5f; // Goes from 0.0 (dim) to 1.0 (bright)

            // 1. Hue Shifting: Time-based color change (from Green to Yellow/Cyan)
            // pulseTimer ব্যবহার করে Hue পরিবর্তন করা হচ্ছে
            float hue = 120.0f + sin(pulseTimer * 0.5f) * 60.0f; // 120 (Green) থেকে 60 (Yellow) পর্যন্ত
            
            // 2. Value Pulsing: Brightness change
            float value = 0.8f + pulseFactor * 0.2f; // 80% থেকে 100% উজ্জ্বলতা

            // HSV থেকে Color এ রূপান্তর
            Color finalColor = ColorFromHSV(hue, 1.0f, value);
            
            // Draw the Exit Triangle
            Vector2 tile = { (float)(j * TILE_SIZE + mazeOffsetX), (float)(i * TILE_SIZE + mazeOffsetY) };
            
            Vector2 triA = { tile.x + TILE_SIZE / 2, tile.y + TILE_SIZE / 4 };
            Vector2 triB = { tile.x + TILE_SIZE / 4, tile.y + TILE_SIZE - TILE_SIZE / 4 };
            Vector2 triC = { tile.x + TILE_SIZE - TILE_SIZE / 4, tile.y + TILE_SIZE - TILE_SIZE / 4 };
            
            DrawTriangle(triA, triB, triC, finalColor); 
        }

        }
    }
}

void handleGameplayInput() {
    int oldX = playerX;
    int oldY = playerY;

    if (IsKeyPressed(KEY_W) || IsKeyPressed(KEY_UP)) playerY--;
    else if (IsKeyPressed(KEY_S) || IsKeyPressed(KEY_DOWN)) playerY++;
    else if (IsKeyPressed(KEY_A) || IsKeyPressed(KEY_LEFT)) playerX--;
    else if (IsKeyPressed(KEY_D) || IsKeyPressed(KEY_RIGHT)) playerX++;

    if (oldX != playerX || oldY != playerY) {
        if (playerY >= 0 && playerY < mazeHeight && playerX >= 0 && playerX < mazeWidth && maze[playerY][playerX] == '#') {
            playerX = oldX;
            playerY = oldY;
        } else {
            totalMoves++;
        }
    }
}

bool checkWinCondition() {
    return (playerX == exitX && playerY == exitY);
}

// --- Main Game Loop ---
int main() {
    InitWindow(fixedScreenWidth, fixedScreenHeight, "Raylib Maze Game"); 
    SetTargetFPS(60);
    
    rng.seed(chrono::steady_clock::now().time_since_epoch().count());

    logoTexture = LoadTexture("assets/logo.jpg"); // Consider a placeholder if this fails
    if (logoTexture.id == 0) {
        TraceLog(LOG_WARNING, "LOGO: Failed to load assets/logo.jpg. Ensure the file exists and path is correct. Using a default background color for the logo area instead.");
    }

    welcomeTextAnim.Start("Welcome to MAZE Game!", 20.0f);
    pressEnterTextEffect.Init("Press ENTER to Start", &rng);

    for (int i = 0; i < 4; ++i) {
        levelButtonsTextEffects[i].Init(levelNames[i], &rng);
    }
    
    while (!WindowShouldClose()) {
        float deltaTime = GetFrameTime();

        switch (currentScreen) {
            case HOME: {
                welcomeTextAnim.Update(deltaTime);
                Rectangle pressEnterRect = {
                    (float)GetScreenWidth() / 2 - MeasureText(pressEnterTextEffect.originalText.c_str(), 30) / 2,
                    (float)GetScreenHeight() / 2 + 100,
                    (float)MeasureText(pressEnterTextEffect.originalText.c_str(), 30),
                    30.0f
                };
                if (CheckCollisionPointRec(GetMousePosition(), pressEnterRect)) {
                    pressEnterTextEffect.Start();
                } else {
                    pressEnterTextEffect.Stop();
                }
                pressEnterTextEffect.GetText(deltaTime);
                
                if (IsKeyPressed(KEY_ENTER)) {
                    currentScreen = LEVEL_SELECT;
                }
            } break;
            case LEVEL_SELECT: {
                int buttonTextSize = 30;
                for (int i = 0; i < 4; ++i) {
                    Rectangle buttonRect = {(float)GetScreenWidth()/2 - 150, (float)(150 + i * 80), 300, 60};
                    
                    GuiSetStyle(BUTTON, TEXT_ALIGNMENT, TEXT_ALIGN_CENTER);
                    bool buttonPressed = GuiButton(buttonRect, "");
                    
                    const char* buttonDisplayName = levelButtonsTextEffects[i].GetText(deltaTime).c_str();
                    
                    float textX = buttonRect.x + buttonRect.width / 2 - MeasureText(buttonDisplayName, buttonTextSize) / 2;
                    float textY = buttonRect.y + buttonRect.height / 2 - buttonTextSize / 2;
                    DrawText(buttonDisplayName, textX, textY, buttonTextSize, BLACK);

                    if (CheckCollisionPointRec(GetMousePosition(), buttonRect)) {
                        levelButtonsTextEffects[i].Start();
                    } else {
                        levelButtonsTextEffects[i].Stop();
                    }
                    levelButtonsTextEffects[i].GetText(deltaTime); // Update effect even if not pressed

                    if (buttonPressed) {
                        // Select strategy based on level for now
                        EntranceExitStrategy strategy;
                        if (i == 0 || i == 1) { // Easy and Medium are random
                            strategy = RANDOM_PLACEMENT;
                        } else if (i == 2) { // Hard is top-left to bottom-right
                            strategy = TOP_LEFT_BOTTOM_RIGHT;
                        } else { // Very Hard is left-center to right-center
                            strategy = LEFT_RIGHT_CENTER;
                        }
                        setupGame(i, strategy);
                        currentScreen = GAMEPLAY;
                    }
                }
            } break;
            // main() ফাংশনের ভেতরে, case GAMEPLAY: এর লজিক আপডেট করুন
            case GAMEPLAY: {
                totalTime += GetFrameTime();
                handleGameplayInput();
                
                float dt = GetFrameTime();

                // 🚀 1. Player Smooth Movement Update (EASING)
                // playerRenderX/Y কে টার্গেট পজিশন playerX/Y এর দিকে মসৃণভাবে মুভ করাবে
                playerRenderX = playerRenderX + ((float)playerX - playerRenderX) * playerMoveSpeed * dt;
                playerRenderY = playerRenderY + ((float)playerY - playerRenderY) * playerMoveSpeed * dt;

                // // ✨ 2. Exit Sign Pulse Update
                // pulseTimer += dt * 4.0f; // 4.0f হচ্ছে পালসের গতি
                // if (pulseTimer > PI * 2) pulseTimer -= PI * 2; // Keep it within 0 to 2*PI

                // ⭐⭐ Exit Sign Pulse Update - Speed Increased to 8.0f ⭐⭐
                pulseTimer += dt * 8.0f; // 8.0f মানে এখন দ্বিগুণ দ্রুত পালস হবে
                if (pulseTimer > PI * 2) pulseTimer -= PI * 2;
                
                if (totalTime > levelTimeLimits[currentLevelIndex]) {
                    currentScreen = GAMEOVER;
                }
                        
                // --- INSIDE case GAMEPLAY: ---
                if (checkWinCondition()) {
                    
                    // ⭐ Step 1: Default to 0.0f in case of no path or no moves.
                    accuracy = 0.0f; 

                    if (idealMoves > 0 && totalMoves > 0) {
                        // ⭐ Case A: Standard Calculation (Both ideal and actual moves > 0)
                        accuracy = (static_cast<float>(idealMoves) / totalMoves) * 100.0f;
                        
                    } else if (idealMoves == 0) {
                        // ⭐ Case B: No path found (idealMoves was -1 or 0)
                        // If idealMoves is 0, the maze probably had an issue, so accuracy is low/zero.
                        accuracy = 0.0f; 
                        
                    } else if (totalMoves == 0 && idealMoves > 0) {
                        // ⭐ Case C: Player won in 0 moves (highly unlikely unless start=end)
                        accuracy = 100.0f; 
                    }
                    
                    // Cap accuracy at 100% (in case player somehow takes fewer than ideal moves)
                    if (accuracy > 100.0f) {
                        accuracy = 100.0f;
                    }

                    currentScreen = WIN;
                }
            } break;
            case WIN: {
                if (IsKeyPressed(KEY_ENTER)) {
                    currentScreen = LEVEL_SELECT;
                }
            } break;
            case GAMEOVER: {
                if (IsKeyPressed(KEY_ENTER)) {
                    currentScreen = LEVEL_SELECT;
                }
            } break;
        }

        BeginDrawing();
    
    // // Dot Background Pattern
    // // 1. ব্যাকগ্রাউন্ডের জন্য অত্যন্ত হালকা রং
    // ClearBackground(GetColor(0xFFFFFFFF)); // প্রায় সাদা ব্যাকগ্রাউন্ড
    
    // // 2. প্যাটার্নের সেটিংস:
    // int dotSpacing = 60;
    // int dotRadius = 3;
    // Color dotColor = GetColor(0xE0E0E0FF); // হালকা ধূসর রং।

    // for (int y = 0; y < fixedScreenHeight; y += dotSpacing) {
    //     for (int x = 0; x < fixedScreenWidth; x += dotSpacing) {
    //         DrawCircle(x, y, dotRadius, dotColor); 
    //     }   
    // }


    // Subtle Grid Background Pattern
    // 1. Very Light Background Color
    ClearBackground(GetColor(0xF5F5F5FF)); 
    // 2. Draw the Subtle Grid Pattern
    int gridSize = 100; // Distance between grid lines (Good for projectors)
    int lineThickness = 1; // Thin lines look better
    Color lineColor = GetColor(0xD0D0D0AA); // A very light gray with transparency (D0D0D0)

    // Vertical Lines
    for (int x = 0; x < fixedScreenWidth; x += gridSize) {
        DrawLineEx({(float)x, 0.0f}, {(float)x, (float)fixedScreenHeight}, (float)lineThickness, lineColor);
    }

    // Horizontal Lines
    for (int y = 0; y < fixedScreenHeight; y += gridSize) {
        DrawLineEx({0.0f, (float)y}, {(float)fixedScreenWidth, (float)y}, (float)lineThickness, lineColor);
    }


        switch (currentScreen) {
            case HOME: {
                if (logoTexture.id != 0) {
                    float scale = 0.4f;
                    float logoX = GetScreenWidth() / 2 - (logoTexture.width * scale) / 2;
                    float logoY = GetScreenHeight() / 2 - (logoTexture.height * scale) / 2 - 150;
                    DrawTextureEx(logoTexture, {logoX, logoY}, 0.0f, scale, WHITE);
                } else {
                    // Placeholder for logo if it fails to load
                    Rectangle logoPlaceholder = { (float)GetScreenWidth() / 2 - 100, (float)GetScreenHeight() / 2 - 250, 200, 100 };
                    DrawRectangleRec(logoPlaceholder, LIGHTGRAY);
                    DrawText("LOGO", logoPlaceholder.x + logoPlaceholder.width/2 - MeasureText("LOGO", 30)/2, logoPlaceholder.y + logoPlaceholder.height/2 - 15, 30, GRAY);
                }

                int welcomeTextSize = 50;
                float welcomeTextX = GetScreenWidth() / 2 - MeasureText(welcomeTextAnim.displayedText.c_str(), welcomeTextSize) / 2;
                float welcomeTextY = GetScreenHeight() / 2 - welcomeTextSize / 2;
                DrawText(welcomeTextAnim.displayedText.c_str(), welcomeTextX, welcomeTextY, welcomeTextSize, DARKBLUE);
                
                int pressEnterTextSize = 30;
                const char* pressEnterDisplay = pressEnterTextEffect.GetText(deltaTime).c_str();
                float pressEnterTextX = GetScreenWidth() / 2 - MeasureText(pressEnterDisplay, pressEnterTextSize) / 2;
                float pressEnterTextY = GetScreenHeight() / 2 + 100;
                DrawText(pressEnterDisplay, pressEnterTextX, pressEnterTextY, pressEnterTextSize, DARKGRAY);

            } break;
            case LEVEL_SELECT: {
                int selectLevelTextSize = 40;
                DrawText("Select Level:", GetScreenWidth() / 2 - MeasureText("Select Level:", selectLevelTextSize) / 2, 50, selectLevelTextSize, BLACK);
                
                int buttonYStart = 150;
                int buttonSpacing = 80;
                int buttonWidth = 300;
                int buttonHeight = 60;
                int buttonTextSize = 30;

                for(int i = 0; i < 4; ++i) {
                    Rectangle buttonRect = {(float)GetScreenWidth()/2 - buttonWidth/2, (float)(buttonYStart + i * buttonSpacing), (float)buttonWidth, (float)buttonHeight};
                    
                    GuiSetStyle(BUTTON, TEXT_ALIGNMENT, TEXT_ALIGN_CENTER);
                    bool buttonPressed = GuiButton(buttonRect, "");
                    
                    const char* buttonDisplayName = levelButtonsTextEffects[i].GetText(deltaTime).c_str();
                    
                    float textX = buttonRect.x + buttonRect.width / 2 - MeasureText(buttonDisplayName, buttonTextSize) / 2;
                    float textY = buttonRect.y + buttonRect.height / 2 - buttonTextSize / 2;
                    DrawText(buttonDisplayName, textX, textY, buttonTextSize, BLACK);

                    if (CheckCollisionPointRec(GetMousePosition(), buttonRect)) {
                        levelButtonsTextEffects[i].Start();
                    } else {
                        levelButtonsTextEffects[i].Stop();
                    }
                    levelButtonsTextEffects[i].GetText(deltaTime); // Update effect even if not pressed

                    if (buttonPressed) {
                        // Select strategy based on level for now
                        EntranceExitStrategy strategy;
                        if (i == 0 || i == 1) { // Easy and Medium are random
                            strategy = RANDOM_PLACEMENT;
                        } else if (i == 2) { // Hard is top-left to bottom-right
                            strategy = TOP_LEFT_BOTTOM_RIGHT;
                        } else { // Very Hard is left-center to right-center
                            strategy = LEFT_RIGHT_CENTER;
                        }
                        setupGame(i, strategy);
                        currentScreen = GAMEPLAY;
                    }
                }
            } break;
            case GAMEPLAY: {
                drawMaze();
                
                // Draw player as a circle
                DrawCircle((float)(playerRenderX * TILE_SIZE + mazeOffsetX + TILE_SIZE / 2), // ⭐ এখানে পরিবর্তন
                        (float)(playerRenderY * TILE_SIZE + mazeOffsetY + TILE_SIZE / 2), // ⭐ এখানে পরিবর্তন
                        TILE_SIZE / 2 - 2, BLUE);

                Rectangle hudBackground = { 0, 0, (float)GetScreenWidth(), 70 };
                DrawRectangleRec(hudBackground, GetColor(0xE0E0E0FF)); // Lighter gray HUD background
                
                float remainingTime = levelTimeLimits[currentLevelIndex] - totalTime;
                Color timeColor = (remainingTime < 10) ? RED : BLACK;
                
                DrawText(TextFormat("Time: %.2f", remainingTime), 10, 10, 20, timeColor);
                DrawText(TextFormat("Moves: %i", totalMoves), 10, 40, 20, BLACK);
                DrawText(TextFormat("Ideal: %i", idealMoves), GetScreenWidth() - MeasureText(TextFormat("Ideal: %i", idealMoves), 20) - 10, 10, 20, DARKBLUE);
            } break;
            case WIN: {
                DrawText("You Won!", GetScreenWidth() / 2 - MeasureText("You Won!", 40) / 2, GetScreenHeight() / 2 - 80, 40, DARKGREEN);
                DrawText(TextFormat("Time Taken: %.2f seconds", totalTime), GetScreenWidth() / 2 - MeasureText("Time Taken: 00.00 seconds", 20) / 2, GetScreenHeight() / 2 - 30, 20, BLACK);
                DrawText(TextFormat("Total Moves: %i", totalMoves), GetScreenWidth() / 2 - MeasureText("Total Moves: 0000", 20) / 2, GetScreenHeight() / 2 + 0, 20, BLACK);
                DrawText(TextFormat("Ideal Moves: %i", idealMoves), GetScreenWidth() / 2 - MeasureText(TextFormat("Ideal Moves: %i", idealMoves), 20) / 2, GetScreenHeight() / 2 + 30, 20, BLACK);
                DrawText(TextFormat("Accuracy: %.2f%%", accuracy), GetScreenWidth() / 2 - MeasureText(TextFormat("Accuracy: %.2f%%", accuracy), 30) / 2, GetScreenHeight() / 2 + 70, 30, (accuracy >= 80.0f) ? BLUE : RED);
                DrawText("Press ENTER to go to Level Select", GetScreenWidth() / 2 - 200, GetScreenHeight() / 2 + 120, 20, GRAY);
            } break;
            case GAMEOVER: {
                DrawText("GAME OVER!", GetScreenWidth() / 2 - MeasureText("GAME OVER!", 40) / 2, GetScreenHeight() / 2 - 80, 40, RED);
                DrawText(TextFormat("Time's up! You couldn't solve the maze."), GetScreenWidth()/2 - MeasureText("Time's up! You couldn't solve the maze.", 20)/2, GetScreenHeight()/2, 20, BLACK);
                DrawText("Press ENTER to go to Level Select", GetScreenWidth() / 2 - 200, GetScreenHeight() / 2 + 30, 20, GRAY);
            } break;
        }
        EndDrawing();
    }

    if (logoTexture.id != 0) {
        UnloadTexture(logoTexture);
    }
    CloseWindow();
    return 0;
}


// .\game.exe