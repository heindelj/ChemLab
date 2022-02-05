#pragma once

void DoFrame(ActiveContext& context) {
	ClearBackground(context.renderContext.backgroundColor);

	// Draw render texture
	//BeginShaderMode(context.renderContext.outlineShader);
	//    DrawTextureRec(context.renderContext.renderTarget.texture, (Rectangle){ 0, 0, (float)context.renderContext.renderTarget.texture.width, (float)-context.renderContext.renderTarget.texture.height }, (Vector2){ 0, 0 }, WHITE);
	//EndShaderMode();

	switch(context.mode) {
		case VIEW:
			ViewModeFrame(*context.renderContext.model, context);
			break;
		case EDIT:
			EditModeFrame(*context.renderContext.model, context);
			break;
		case ANIMATION:
			AnimationModeFrame(*context.renderContext.model, context);
			break;
	}
	
	CheckForAndHandleFrameChange(context);
	if (IsKeyPressed(KEY_M))
		context.mode == static_cast<InteractionMode>(static_cast<int>(NUMBER_OF_MODES) - 1) ? context.mode = static_cast<InteractionMode>(0) : context.mode = static_cast<InteractionMode>(static_cast<int>(context.mode) + 1);

	if (IsKeyPressed(KEY_H))
		context.drawUI = !context.drawUI;

	if (context.drawUI) {
		DrawSettingsPanel(context);
	} else {
		DrawActiveMode(context.mode);
		DrawText((std::to_string(context.activeFrame + 1) + "/" + std::to_string(context.numFrames)).c_str(), 15, context.screenHeight - 40, 30, RAYWHITE);
	}

	if (IsKeyPressed(KEY_G))
		context.drawGrid = !context.drawGrid;

	OnWindowResize(context);
}

void DoRenderTextureDrawing(ActiveContext& context) {
	BeginTextureMode(context.renderContext.renderTarget);
            ClearBackground(WHITE);
            BeginMode3D(context.renderContext.camera);
            	context.renderContext.model->DrawHighlighted(context.atomsToHighlight);
            EndMode3D();
    EndTextureMode();
}