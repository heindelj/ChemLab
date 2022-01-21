#include "chemlab.h"

int main(void)
{
    Frames frames = readXYZ("assets/test_xyz_file.xyz");

    // Initialization
    //--------------------------------------------------------------------------------------
    const int screenWidth = 800;
    const int screenHeight = 450;

    InitWindow(screenWidth, screenHeight, "ChemLab");

    // Define the camera to look into our 3d world
    Camera3D camera = { 0 };
    camera.position = (Vector3){ -10.0f, 15.0f, -10.0f };   // Camera position
    camera.target = (Vector3){ 0.0f, 0.0f, 0.0f };          // Camera looking at point
    camera.up = (Vector3){ 0.0f, 1.0f, 0.0f };              // Camera up vector (rotation towards target)
    camera.fovy = 45.0f;                                    // Camera field-of-view Y
    camera.projection = CAMERA_PERSPECTIVE;                 // Camera mode type

    // initialize context
    ActiveContext context;
    context.mode = VIEW;
    context.style = BALL_AND_STICK;
    context.camera = camera;
    context.timeOfLastClick = 0.0;
    context.selectionStep = NONE;

    //Ray ray = { 0 };        // Picking ray

    ///////// Outline Shader variables, etc. ///////////
    //Shader shdrOutline = LoadShader(0, "assets/shaders/outline.fs");

    //float outlineSize = 2.0f;
    //float outlineColor[4] = { 1.0f, 1.0f, 0.0f, 1.0f };     // Normalized YELLOW color 
    //float textureSize[2] = { (float)screenWidth, (float)screenHeight };
    
    // Get shader locations
    //int outlineSizeLoc = GetShaderLocation(shdrOutline, "outlineSize");
    //int outlineColorLoc = GetShaderLocation(shdrOutline, "outlineColor");
    //int textureSizeLoc = GetShaderLocation(shdrOutline, "textureSize");
    
    // Set shader values (they can be changed later)
    //SetShaderValue(shdrOutline, outlineSizeLoc, &outlineSize, SHADER_UNIFORM_FLOAT);
    //SetShaderValue(shdrOutline, outlineColorLoc, outlineColor, SHADER_UNIFORM_VEC4);
    //SetShaderValue(shdrOutline, textureSizeLoc, textureSize, SHADER_UNIFORM_VEC2);

    // Create a RenderTexture2D to be used for render to texture
    //RenderTexture2D target = LoadRenderTexture(screenWidth, screenHeight);

    SetTargetFPS(60);
    //--------------------------------------------------------------------------------------

    // Main game loop
    while (!WindowShouldClose())
    {
        // Update
        //----------------------------------------------------------------------------------
        // TODO: Update your variables here
        //----------------------------------------------------------------------------------
        updateCamera3D(context.camera);          // Update camera

        // This draws highlights around things drawn on the texture
        /*BeginTextureMode(target);       // Enable drawing to texture
            ClearBackground(Color(1.0f,1.0f,1.0f,0.0f));  // Clear texture background

            BeginMode3D(camera);        // Begin 3d mode drawing
                DrawSphere(Vector3((float)frames.atoms[0].xyz.row(0)(0), (float)frames.atoms[0].xyz.row(0)(1), (float)frames.atoms[0].xyz.row(0)(2)), 1.0f, RED);
                DrawSphere(Vector3((float)frames.atoms[0].xyz.row(1)(0), (float)frames.atoms[0].xyz.row(1)(1), (float)frames.atoms[0].xyz.row(1)(2)), 1.0f, RAYWHITE);
                DrawSphere(Vector3((float)frames.atoms[0].xyz.row(2)(0), (float)frames.atoms[0].xyz.row(2)(1), (float)frames.atoms[0].xyz.row(2)(2)), 1.0f, RAYWHITE);
            EndMode3D();
        EndTextureMode();*/

        //// Below is all the code for testing for collisions with a sphere
        //RayCollision collision = { 0 };
        //std::string hitObjectName = "None";
        //collision.distance = FLT_MAX;
        //collision.hit = false;

        // Get ray and test against objects
        //ray = GetMouseRay(GetMousePosition(), camera);

        // Check ray collision against test sphere
        //RayCollision sphereHitInfo = GetRayCollisionSphere(ray, Vector3((float)frames.atoms[0].xyz.row(0)(0), (float)frames.atoms[0].xyz.row(0)(1), (oat)//frames.atoms[0].xyz.row(0)(2)), 1.0f);//

        // Draw
        //----------------------------------------------------------------------------------
        BeginDrawing();

            ClearBackground(Color(30, 30, 30, 255));

            ViewModeFrame(frames.atoms[0], context);

            // Render generated texture with an outline
            //BeginShaderMode(shdrOutline);
                // NOTE: Render texture must be y-flipped due to default OpenGL coordinates (left-bottom)
            //    DrawTextureRec(target.texture, (Rectangle){ 0, 0, (float)target.texture.width, (float)-target.texture.height }, (Vector2){ 0, 0 }, WHITE);
            //EndShaderMode();

            //BeginMode3D(camera);
            //    DrawAtoms(frames.atoms[0], BALL_AND_STICK);

                // Can do the highlighting here for selected objects
                //if (sphereHitInfo.hit)
                //    DrawSphere(Vector3((float)frames.atoms[0].xyz.row(0)(0), (float)frames.atoms[0].xyz.row(0)(1), (float)frames.atoms[0].xyz.row(0)(2)), 01f, //YELLOW);//


            //    DrawGrid(10, 1.0f);
            //EndMode3D();

            //OverlayNumbers(frames.atoms[0], camera);
            //DrawLineBetweenAtoms(frames.atoms[0], 1, 3, camera);
            //DrawDashedLineFromPointToCursor(GetWorldToScreen(frames.atoms[0].xyz[0], camera));
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
