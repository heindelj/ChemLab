#pragma once

void DrawUI(const ActiveContext& context) {
	// Draw save image button
	bool rotateButtonHover = false;
	bool allFramesButtonHover = false;

	float width = 55.0f;
	float height = 30.0f;

	// Get width-independent x position (since rectangle is in top-right)
	int distanceFromScreenEdge = 40; // pixels
	float xPosScale = ((float)(context.screenWidth - distanceFromScreenEdge) - width) / context.screenWidth;

	Rectangle rotateButtonRec = { (xPosScale * (float)context.screenWidth), 10, width, height };
	if (CheckCollisionPointRec(GetMousePosition(), rotateButtonRec)) rotateButtonHover = true;
    DrawRectangleLinesEx(rotateButtonRec, 2, rotateButtonHover ? GREEN : WHITE);
    DrawText("ROTATE", (int)(xPosScale * (float)(context.screenWidth + 7)), 20, 10, rotateButtonHover ? GREEN : WHITE);
    // ^^^^ The + 7 above is to move the text inside of the rectangle a litte bit.

    width = 85.0f;
    height = 30.0f;
    Rectangle allFramesButtonRec = { (xPosScale * (float)context.screenWidth), 50, width, height };
    if (CheckCollisionPointRec(GetMousePosition(), allFramesButtonRec)) allFramesButtonHover = true;
    DrawRectangleLinesEx(allFramesButtonRec, 2, allFramesButtonHover ? GREEN : WHITE);
    DrawText("ALL FRAMES", (int)(xPosScale * (float)(context.screenWidth + 7)), 60, 10, allFramesButtonHover ? GREEN : WHITE);
    // ^^^^ The + 7 above is to move the text inside of the rectangle a litte bit.
}

void RotateAroundWorldUp(ActiveContext& context) {
	float rotationSpeed = 0.1f;
	Vector3 cameraForward = context.camera.target - context.camera.position;
	cameraForward = normalize(cameraForward);
	Vector3 cameraRight = cross(cameraForward, context.camera.up);

	context.camera.position = context.camera.position + rotationSpeed * cameraLeft;
}

void AnimationModeFrame(MolecularModel& model, ActiveContext& context) {
	DrawUI(context);

	BeginMode3D(context.camera);
	    model.Draw();
	    DrawGrid(10, 1.0f);
	EndMode3D();

	RotateAroundWorldUp(context);

	// check if rotate clicked and then animate the camera appropriately

}