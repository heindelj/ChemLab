#pragma once

void DrawModeUI(ActiveContext& context) {
	switch (context.mode) {
		case VIEW:
			DrawViewUI(context);
			break;
		case EDIT:
			//DrawEditUI(context); // TODO
			break;
		case ANIMATION:
			DrawAnimationUI(context);
			break;
		default:
			DrawViewUI(context);
	}


}

void DrawSettingsPanel(ActiveContext& context) {
	float center = context.screenWidth / 10.0f;

	// Draw menu background
    DrawRectangle(0, 0, context.uiSettings.menuWidth + 2 * context.uiSettings.borderWidth, context.screenHeight, RED); // Border recntangle
    DrawRectangle(5, 5, context.uiSettings.menuWidth, context.screenHeight-2 * context.uiSettings.borderWidth, LIGHTGRAY); // Foreground rectangle

    // Draw UI specific to a mode
	DrawModeUI(context);

	// Draw frame number
	DrawText((std::to_string(context.activeFrame + 1) + "/" + std::to_string(context.numFrames)).c_str(), 15, context.screenHeight - 40, 30, RAYWHITE);
}