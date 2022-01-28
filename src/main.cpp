#include "chemlab.h"

int main(void)
{
    Frames frames = readXYZ("assets/test_xyz_file.xyz");

    // initialize context
    ActiveContext context = InitContext(frames, 800, 450);
    context.mode = ANIMATION;

    //Light lights[MAX_LIGHTS] = { 0 };
    //lights[0] = CreateLight(LIGHT_DIRECTIONAL, context.camera.position, context.camera.target, WHITE, context.model->materials[0].shader);

    SetTargetFPS(60);
    //--------------------------------------------------------------------------------------
    // Main game loop
    while (!WindowShouldClose())
    {
        // Update
        updateCamera3D(context);

        BeginDrawing();

            DoFrame(context);

            DrawFPS(10, 10);

        EndDrawing();
    }

    // De-Initialization
    //--------------------------------------------------------------------------------------
    CloseWindow();        // Close window and OpenGL context
    //--------------------------------------------------------------------------------------

    return 0;
}
