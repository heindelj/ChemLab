#pragma once

void DrawModeUI(ActiveContext& context) {
	switch (context.mode) {
		case VIEW:
			DrawViewUI(context);
			break;
		case BUILD:
			//DrawBuildUI(context); // TODO
			break;
		case ANIMATION:
			DrawAnimationUI(context);
			break;
		default:
			DrawViewUI(context);
	}


}

void CheckForAndHandleModeChange(ActiveContext& context) {
	float sliderHeight = 20;
	Rectangle comboBoxRect = (Rectangle){ context.uiSettings.menuWidth / 2 - context.uiSettings.menuWidth * 0.3f, 2 * context.uiSettings.borderWidth, context.uiSettings.menuWidth * 0.6f, sliderHeight * 1.5f };
	context.mode = static_cast<InteractionMode>(GuiComboBox(comboBoxRect, "VIEW;EDIT;ANIMATION", static_cast<int>(context.mode)));
}

void DrawSettingsPanel(ActiveContext& context) {
	float center = context.screenWidth / 10.0f;
	// Draw menu background
    DrawRectangle(0, 0, context.uiSettings.menuWidth + 2 * context.uiSettings.borderWidth, context.screenHeight, VIOLET); // Border rectangle
    DrawRectangle(5, 5, context.uiSettings.menuWidth, context.screenHeight-2 * context.uiSettings.borderWidth, LIGHTGRAY); // Foreground rectangle

    // Draws mode box
    CheckForAndHandleModeChange(context);

    // Draw UI specific to a mode
	DrawModeUI(context);

	// Draw frame number
	DrawText((std::to_string(context.activeFrame + 1) + "/" + std::to_string(context.numFrames)).c_str(), 15, context.screenHeight - 40, 30, BLACK);
}