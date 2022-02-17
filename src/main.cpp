#include "chemlab.h"

int main(int argc, char** argv)
{
    Frames frames;
    if (argc > 1){
        frames = readXYZ(argv[1]);
    } else {
        frames = readXYZ("assets/test_xyz_file.xyz");
    }
    //frames = readXYZ("assets/W20_stable_clathrate_cages_TTM21F_first_200.xyz");
    frames = readXYZ("assets/test_xyz_file.xyz");
    ActiveContext context = InitContext(frames, 1440, 810);

    rlEnableBackfaceCulling();
    while (!WindowShouldClose())
    {
        if (!context.lockCamera)
            UpdateCamera3D(context);

        BeginDrawing();
            DoFrame(context);
        EndDrawing();
    }

    CloseWindow();

    return 0;
}
