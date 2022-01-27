#include "chemlab.h"

int main(void)
{
    Frames frames = readXYZ("assets/test_xyz_file.xyz");

    // initialize context
    ActiveContext context = InitContext(frames, 800, 450);
    context.mode = ANIMATION;

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
        updateCamera3D(context.camera);

        // This draws highlights around things drawn on the texture
        /*BeginTextureMode(target);       // Enable drawing to texture
            ClearBackground(Color(1.0f,1.0f,1.0f,0.0f));  // Clear texture background

            BeginMode3D(camera);        // Begin 3d mode drawing
                DrawSphere(Vector3((float)frames.atoms[0].xyz.row(0)(0), (float)frames.atoms[0].xyz.row(0)(1), (float)frames.atoms[0].xyz.row(0)(2)), 1.0f, RED);
                DrawSphere(Vector3((float)frames.atoms[0].xyz.row(1)(0), (float)frames.atoms[0].xyz.row(1)(1), (float)frames.atoms[0].xyz.row(1)(2)), 1.0f, RAYWHITE);
                DrawSphere(Vector3((float)frames.atoms[0].xyz.row(2)(0), (float)frames.atoms[0].xyz.row(2)(1), (float)frames.atoms[0].xyz.row(2)(2)), 1.0f, RAYWHITE);
            EndMode3D();
        EndTextureMode();*/

        BeginDrawing();

            DoFrame(context);

            // Render generated texture with an outline
            //BeginShaderMode(shdrOutline);
                // NOTE: Render texture must be y-flipped due to default OpenGL coordinates (left-bottom)
            //    DrawTextureRec(target.texture, (Rectangle){ 0, 0, (float)target.texture.width, (float)-target.texture.height }, (Vector2){ 0, 0 }, WHITE);
            //EndShaderMode();

            //OverlayNumbers(frames.atoms[0], camera);

            DrawFPS(10, 10);

        EndDrawing();
    }

    // De-Initialization
    //--------------------------------------------------------------------------------------
    CloseWindow();        // Close window and OpenGL context
    //--------------------------------------------------------------------------------------

    return 0;
}
