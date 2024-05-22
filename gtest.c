#include "types.h"
#include "user.h"

int main(int argc, char* argv[]) {
    setvideomode(0x13);

    // testing code start

    int pid = fork();

    if (pid == 0) {
        int hdc = beginpaint(0);

        for (int i = 20; i <= 160; i+=20) {
            selectpen(hdc, 10);
            moveto(hdc, i, i+20);
            lineto(hdc, i, i+40);
            lineto(hdc, i+20, i+40);
            lineto(hdc, i+20, i+20);
            lineto(hdc, i, i+20);
        }

        selectpen(hdc, 11);
        moveto(hdc, 0, 0);
        lineto(hdc, 50, 0);
        lineto(hdc, 50, 50);
        lineto(hdc, 0, 50);
        lineto(hdc, 0, 0);

        selectpen(hdc, 12);
        moveto(hdc, 270, 150);
        lineto(hdc, 320, 150);
        lineto(hdc, 320, 200);
        lineto(hdc, 270, 200);
        lineto(hdc, 270, 150);

        endpaint(hdc);
        exit();
    } else if (pid > 0) {
        int hdc = beginpaint(0);

        struct rect r;
        r.top = 50;
        r.left = 100;
        r.bottom = 150;
        r.right = 200;

        setpencolour(16, 63, 31, 31);
        selectpen(hdc, 16);
        fillrect(hdc, &r);

        r.top = 20;
        r.left = 10;
        r.bottom = 40;
        r.right = 200;
        
        fillrect(hdc, &r);

        selectpen(hdc, 9);
        moveto(hdc, 100, 50);
        lineto(hdc, 200, 50);
        lineto(hdc, 200, 150);
        lineto(hdc, 100, 150);
        lineto(hdc, 100, 50);

        endpaint(hdc);
        wait();
    }

    pid = fork();

    if (pid == 0) {
        int hdc = beginpaint(0);

        for (int i = 20; i <= 160; i+=20) {
            selectpen(hdc, 5);
            moveto(hdc, i, i);
            lineto(hdc, i, i+20);
            lineto(hdc, i+20, i+20);
            lineto(hdc, i+20, i);
            lineto(hdc, i, i);
        }

        endpaint(hdc);
        exit();
    } else if (pid > 0) {
        int hdc = beginpaint(0);

        struct rect r;
        r.top = 70;
        r.left = 120;
        r.bottom = 130;
        r.right = 180;

        setpencolour(17, 31, 31, 63);
        selectpen(hdc, 17);
        fillrect(hdc, &r);

        endpaint(hdc);
        wait();
    }

    // testing code end

    getch();
    setvideomode(0x03);
    exit();
}