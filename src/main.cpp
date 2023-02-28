#include "chemlab.h"

int main(int argc, char** argv)
{
    SetTraceLogLevel(LOG_ERROR);

    Frames frames;
    try {
        if (argc > 1){
            frames = readXYZ(argv[1]);
        } else {
            frames = readXYZ("assets/test_xyz_file.xyz");
            //frames = readXYZ("assets/W20_stable_clathrate_cages_TTM21F_first_200.xyz");
        }
    } catch (std::filesystem::filesystem_error& e) {
        std::cout << e.what();
        return -1;
    }
    ActiveContext context = InitContext(frames, 1000, 800);

    int fps = 0;
    while (!WindowShouldClose())
    {
        if (!context.lockCamera)
            UpdateCamera3D(context);
        
        DoFrame(context);
        fps = 1.0 / GetFrameTime();
        if (fps < 30.0)
            std::cout << "Last Frame Time: " << 1.0 / fps * 1000 << " ms" << std::endl; 
    }

    CloseWindow();

    return 0;
}
