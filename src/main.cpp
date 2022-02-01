#include "chemlab.h"

int main()
{
    Frames frames = readXYZ("assets/test_xyz_file.xyz");
    ActiveContext context = InitContext(frames, 800, 450);

    SetTargetFPS(60);
    while (!WindowShouldClose())
    {
        UpdateCamera3D(context.renderContext);

        BeginDrawing();
            DoFrame(context);
        EndDrawing();
    }

    CloseWindow();

    return 0;
}
