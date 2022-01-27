#pragma once

/*
	Functions for exporting a frame to png or saving out an image sequence associated with an animation
*/

void ExportFrameAsPNG(const ActiveContext& context) {
	// Create a RenderTexture2D to export as PNG
	RenderTexture2D target = LoadRenderTexture(context.screenWidth, context.screenHeight);
	BeginTextureMode(target);
	    ClearBackground(Color(1.0f,1.0f,1.0f,0.0f));  // Clear white texture background
	    BeginMode3D(context.camera);
	    	context.model->Draw();
	    EndMode3D();
	EndTextureMode();
	Image image = LoadImageFromTexture(target.texture);
	ImageFlipVertical(&image);
	ExportImage(image, "my_amazing_texture_painting.png");
	UnloadImage(image);
}