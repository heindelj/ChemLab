#pragma once

void DoFrame(ActiveContext& context) {

	context.screenWidth = GetScreenWidth();
	context.screenHeight = GetScreenHeight();
	ClearBackground(Color(30, 30, 30, 255));

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

	DrawSettingsPanel(context);
	CheckForAndHandleFrameChange(context);
	CheckForAndHandleModeChange(context);

	if (IsKeyPressed(KEY_G))
		context.drawGrid = !context.drawGrid;

}