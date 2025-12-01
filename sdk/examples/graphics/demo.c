/*
 * Graphics Demo for NBOS
 * Demonstrates BGI-style graphics functions
 */

#include <nbos.h>
#include <graphics.h>

int main(void) {
    int gd = DETECT, gm;
    
    // Initialize graphics
    initgraph(&gd, &gm, "");
    
    // Clear screen with dark blue background
    setbkcolor(BLUE);
    cleardevice();
    
    // Draw a title
    setcolor(WHITE);
    outtextxy(10, 10, "NBOS Graphics Demo");
    
    // Draw some shapes
    setcolor(YELLOW);
    rectangle(50, 50, 200, 150);
    
    setcolor(RED);
    circle(320, 240, 80);
    
    setcolor(GREEN);
    line(0, 0, getmaxx(), getmaxy());
    line(getmaxx(), 0, 0, getmaxy());
    
    // Filled shapes
    setfillstyle(SOLID_FILL, CYAN);
    bar(400, 100, 550, 200);
    
    setfillstyle(SOLID_FILL, MAGENTA);
    fillcircle(500, 350, 60);
    
    // Draw an ellipse
    setcolor(LIGHTGREEN);
    ellipse(150, 350, 0, 360, 100, 50);
    
    // Status message
    setcolor(WHITE);
    outtextxy(10, getmaxy() - 30, "Press any key to exit...");
    
    // Wait for key
    getch();
    
    // Cleanup
    closegraph();
    
    return 0;
}
