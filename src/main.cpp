#include "raylib.h"
#include "camera.h"
#include "utils/molecule_tools.h"
#include "utils/atomic_data.h"
#include "utils/debug.h"

#define FLT_MAX     340282346638528859811704183484516925440.0f

int main(void)
{
    Frames frames = readXYZ("assets/test_xyz_file.xyz");

    // Initialization
    //--------------------------------------------------------------------------------------
    const int screenWidth = 800;
    const int screenHeight = 450;

    InitWindow(screenWidth, screenHeight, "raylib [core] example - 3d camera mode");

    // Define the camera to look into our 3d world
    Camera3D camera = { 0 };
    camera.position = (Vector3){ -10.0f, 15.0f, -10.0f };   // Camera position
    camera.target = (Vector3){ 0.0f, 0.0f, 0.0f };          // Camera looking at point
    camera.up = (Vector3){ 0.0f, 1.0f, 0.0f };              // Camera up vector (rotation towards target)
    camera.fovy = 45.0f;                                    // Camera field-of-view Y
    camera.projection = CAMERA_PERSPECTIVE;                 // Camera mode type

    Ray ray = { 0 };        // Picking ray

    SetTargetFPS(60);
    //--------------------------------------------------------------------------------------

    // Main game loop
    while (!WindowShouldClose())
    {
        // Update
        //----------------------------------------------------------------------------------
        // TODO: Update your variables here
        //----------------------------------------------------------------------------------
        updateCamera3D(camera);          // Update camera

        // Below is all the code for testing for collisions with a sphere
        RayCollision collision = { 0 };
        std::string hitObjectName = "None";
        collision.distance = FLT_MAX;
        collision.hit = false;

        // Get ray and test against objects
        ray = GetMouseRay(GetMousePosition(), camera);

        // Check ray collision against test sphere
        RayCollision sphereHitInfo = GetRayCollisionSphere(ray, Vector3((float)frames.atoms[0].xyz.row(0)(0), (float)frames.atoms[0].xyz.row(0)(1), (float)frames.atoms[0].xyz.row(0)(2)), 1.0f);

        // Draw
        //----------------------------------------------------------------------------------
        BeginDrawing();

            ClearBackground(Color(30, 30, 30, 255));

            BeginMode3D(camera);
                for (int i = 0; i < frames.atoms[0].natoms; i++) {
                    Vector3 centerPos = {(float)frames.atoms[0].xyz.row(i)(0), (float)frames.atoms[0].xyz.row(i)(1), (float)frames.atoms[0].xyz.row(i)(2)};
                    DrawSphere(centerPos, 1.0f, atomColors[frames.atoms[0].labels[i]]);
                }

                // Can do the highlighting here for selected objects
                if (sphereHitInfo.hit)
                    DrawSphere(Vector3((float)frames.atoms[0].xyz.row(0)(0), (float)frames.atoms[0].xyz.row(0)(1), (float)frames.atoms[0].xyz.row(0)(2)), 1.01f, YELLOW);


                DrawGrid(10, 1.0f);
            EndMode3D();

            DrawText("Welcome to the third dimension!", 10, 40, 20, DARKGRAY);

            DrawFPS(10, 10);

        EndDrawing();
        //----------------------------------------------------------------------------------
    }

    // De-Initialization
    //--------------------------------------------------------------------------------------
    CloseWindow();        // Close window and OpenGL context
    //--------------------------------------------------------------------------------------

    return 0;
}
