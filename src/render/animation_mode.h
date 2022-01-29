#pragma once

std::vector<Vector3> GetRotationAnimationPositions(Camera3D camera, int numFrames) {
	std::vector<Vector3> cameraPositions;

	Vector3 startingForward = normalize(camera.target - camera.position);

	for (int i = 0; i < numFrames; i++) {
		float desiredAngle = ((float)i / (float)numFrames) * (2 * PI);
		float currentAngle = 0.0f;
		Vector3 lastPosition = camera.position;
		Vector3 cameraForward = normalize(camera.target - camera.position);
		Vector3 cameraRight = cross(cameraForward, camera.up);

		// iterate until close to desired point on circle
		int numIter = 0;
		while (abs(currentAngle - desiredAngle) > 0.005f) { // converge to within 0.005 radians = ~0.25 degrees
			cameraForward = normalize(camera.target - camera.position);
			cameraRight = normalize(cross(cameraForward, camera.up));

			if (currentAngle < desiredAngle) {
				camera.position = camera.position + 0.01f * cameraRight;
			} else {
				camera.position = camera.position - 0.01f * cameraRight;
			}
			currentAngle = AngleOnCircle(normalize(camera.target - camera.position), startingForward, camera.up);
			
			numIter += 1;
			if (numIter > 1000) {
				std::cout << "Warning: Failed on iteration " << i << std::endl;
				break;
			}
		}
		cameraPositions.push_back(camera.position);
	}
	return cameraPositions;
}

void ExportFrameAsPNG(const Camera3D& camera, MolecularModel* model, int width, int height, const char* fileName) {
	// Create a RenderTexture2D to export as PNG
	RenderTexture2D renderTarget = LoadRenderTexture(width, height);
	BeginTextureMode(renderTarget);
	    ClearBackground(Color(1.0f,1.0f,1.0f,1.0f));  // Clear white texture background
	    BeginMode3D(camera);
	    	model->Draw();
	    EndMode3D();
	EndTextureMode();
	Image image = LoadImageFromTexture(renderTarget.texture);
	ImageFlipVertical(&image);
	ExportImage(image, fileName);
	UnloadImage(image);
	UnloadRenderTexture(renderTarget);
}

void ExportRotationAnimation(ActiveContext& context, std::string fileName, int width, int height, int numFrames) {
	// Can convert the resulting image sequence to a gif with command: convert -delay 0 -loop 0 -alpha set -dispose 2 my_render_frames_*.png
	// add system() call to convert the gif all at once. Check that convert command is available.

	// Do this in a new thread
	std::vector<Vector3> animationPositions = GetRotationAnimationPositions(context.camera, numFrames);
	Camera3D tempCamera = context.camera;
	for (int i = 0; i < animationPositions.size(); i++) {
		std::string fileNumber = "";
		for (int j = numDigits(i); j < numDigits(animationPositions.size()); j++)
			fileNumber += "0";
		tempCamera.position = animationPositions[i];
		UpdateLighting(context, tempCamera.position, tempCamera.target);
		ExportFrameAsPNG(tempCamera, context.model, width, height, (fileName + "_" + fileNumber + std::to_string(i) + ".png").c_str());
	}
}

void AnimationModeFrame(MolecularModel& model, ActiveContext& context) {
	DrawViewUI(context); // change UI function

	BeginMode3D(context.camera);
	    model.Draw();
	    if (context.drawGrid)
	    	DrawGrid(10, 1.0f);
	EndMode3D();

	HandleSelections(model, context);

	if (context.rotateButtonHover && IsMouseButtonReleased(MOUSE_BUTTON_LEFT))
		ExportRotationAnimation(context, "/home/heindelj/my_render_frames", 1600, 1600, 30);
	if (context.allFramesButtonHover && IsMouseButtonReleased(MOUSE_BUTTON_LEFT))
		context.isCyclingAllFrames = !context.isCyclingAllFrames;

}