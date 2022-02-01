#pragma once

void DoFrame(ActiveContext& context) {
	// NOTE: ALL OF THIS HAPPENS WITHIN THE DRAWING CONTEXT

	context.screenWidth = GetScreenWidth();
	context.screenHeight = GetScreenHeight();
	ClearBackground(Color(30, 30, 30, 255));

	CheckForAndHandleFrameChange(context);
	CheckForAndHandleModeChange(context);

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

	if (IsKeyPressed(KEY_G))
		context.drawGrid = !context.drawGrid;

	// Draw which frame number
	DrawText((std::to_string(context.activeFrame + 1) + "/" + std::to_string(context.numFrames)).c_str(), 10, context.screenHeight - 30, 20, RAYWHITE);
	//DrawActiveMode(context.mode);
}