#pragma once

void DoFrame(ActiveContext& context) {

	// DRAW INTO TEXTURES AS NEEDED //

	// Draw render texture
	//BeginShaderMode(context.renderContext.outlineShader);
	//    DrawTextureRec(context.renderContext.renderTarget.texture, (Rectangle){ 0, 0, (float)context.renderContext.renderTarget.texture.width, (float)-context.renderContext.renderTarget.texture.height }, (Vector2){ 0, 0 }, WHITE);
	//EndShaderMode();

	//////////////////////////////////////////////////////
	// Do 3D drawing and logic handling for active mode //
	//////////////////////////////////////////////////////
	BeginDrawing();
		switch(context.mode) {
			case VIEW:
				ViewModeFrame(context);
				break;
			case BUILD:
				BuildModeFrame(context);
				break;
			case ANIMATION:
				AnimationModeFrame(*context.renderContext.model, context);
				break;
		}
		if (context.drawUI) {
			DrawUI(context);
		} else {
			DrawActiveMode(context.mode);
			DrawText((std::to_string(context.activeFrame + 1) + "/" + std::to_string(context.frames->nframes)).c_str(), 15, context.screenHeight - 40, 30, RAYWHITE);
		}
	EndDrawing();
	
	/////////////////////////////
	// Handle settings changes //
	/////////////////////////////

	CheckForAndHandleFrameChange(context);
	if (IsKeyPressed(KEY_M))
		context.mode == static_cast<InteractionMode>(static_cast<int>(NUMBER_OF_MODES) - 1) ? context.mode = static_cast<InteractionMode>(0) : context.mode = static_cast<InteractionMode>(static_cast<int>(context.mode) + 1);

	if (IsKeyPressed(KEY_G))
		context.drawGrid = !context.drawGrid;
	if (IsKeyPressed(KEY_H))
		OnHideUI(context);
	
	OnWindowResize(context);
}