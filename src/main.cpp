#include "chemlab.h"

int main()
{
    Frames frames = readXYZ("assets/test_xyz_file.xyz");
    //Frames frames = readXYZ("assets/W20_stable_clathrate_cages_TTM21F_first_200.xyz");
    ActiveContext context = InitContext(frames, 1440, 810);
    while (!WindowShouldClose())
    {
        UpdateCamera3D(context);

        BeginDrawing();
            DoFrame(context);
        EndDrawing();
    }

    CloseWindow();

    return 0;
}
