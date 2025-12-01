/*
 * Simple Pong Game for NBOS
 * Arrow keys to move paddle, Q to quit
 */

#include <nbos.h>
#include <graphics.h>

#define PADDLE_WIDTH  80
#define PADDLE_HEIGHT 10
#define BALL_SIZE     8
#define PADDLE_SPEED  15
#define BALL_SPEED    5

int main(void) {
    int gd = DETECT, gm;
    initgraph(&gd, &gm, "");
    
    int max_x = getmaxx();
    int max_y = getmaxy();
    
    // Paddle position
    int paddle_x = (max_x - PADDLE_WIDTH) / 2;
    int paddle_y = max_y - 30;
    
    // Ball position and velocity
    int ball_x = max_x / 2;
    int ball_y = max_y / 2;
    int ball_dx = BALL_SPEED;
    int ball_dy = BALL_SPEED;
    
    // Score
    int score = 0;
    int running = 1;
    
    setbkcolor(BLACK);
    
    while (running) {
        // Clear screen
        cleardevice();
        
        // Draw border
        setcolor(WHITE);
        rectangle(0, 0, max_x, max_y);
        
        // Draw paddle
        setfillstyle(SOLID_FILL, CYAN);
        bar(paddle_x, paddle_y, paddle_x + PADDLE_WIDTH, paddle_y + PADDLE_HEIGHT);
        
        // Draw ball
        setfillstyle(SOLID_FILL, YELLOW);
        fillcircle(ball_x, ball_y, BALL_SIZE);
        
        // Draw score
        setcolor(WHITE);
        outtextxy(10, 10, "PONG - Use arrow keys, Q to quit");
        
        // Update ball position
        ball_x += ball_dx;
        ball_y += ball_dy;
        
        // Ball collision with walls
        if (ball_x <= BALL_SIZE || ball_x >= max_x - BALL_SIZE) {
            ball_dx = -ball_dx;
        }
        if (ball_y <= BALL_SIZE) {
            ball_dy = -ball_dy;
        }
        
        // Ball collision with paddle
        if (ball_y >= paddle_y - BALL_SIZE &&
            ball_x >= paddle_x && 
            ball_x <= paddle_x + PADDLE_WIDTH) {
            ball_dy = -ball_dy;
            ball_y = paddle_y - BALL_SIZE - 1;
            score++;
        }
        
        // Ball missed
        if (ball_y > max_y) {
            // Reset ball
            ball_x = max_x / 2;
            ball_y = max_y / 2;
            ball_dy = -BALL_SPEED;
        }
        
        // Check for input
        if (kbhit()) {
            char key = getch();
            switch (key) {
                case 'a':
                case 'A':
                    paddle_x -= PADDLE_SPEED;
                    if (paddle_x < 0) paddle_x = 0;
                    break;
                case 'd':
                case 'D':
                    paddle_x += PADDLE_SPEED;
                    if (paddle_x > max_x - PADDLE_WIDTH) 
                        paddle_x = max_x - PADDLE_WIDTH;
                    break;
                case 'q':
                case 'Q':
                    running = 0;
                    break;
            }
        }
        
        // Simple delay
        delay(16);  // ~60 FPS
    }
    
    closegraph();
    return 0;
}
