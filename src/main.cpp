#include "chemlab.h"

int main()
{
    Frames frames = readXYZ("assets/test_xyz_file.xyz");

    // initialize context
    ActiveContext context = InitContext(frames, 800, 450);
    context.mode = VIEW;

    SetTargetFPS(60);
    while (!WindowShouldClose())
    {
        updateCamera3D(context);

        BeginDrawing();
            DoFrame(context);
        EndDrawing();
    }

    CloseWindow();        // Close window and OpenGL context

    return 0;
}
