#pragma once

void DrawAnimationUI(ActiveContext& context) {
	
	GuiSetStyle(BUTTON, TEXT_ALIGNMENT, GUI_TEXT_ALIGN_CENTER);

	// Get width-independent x position (since rectangle is in top-right)
	int distanceFromScreenEdge = 40; // pixels
	float xPosScale = ((float)(context.screenWidth - distanceFromScreenEdge) - 55.0f) / context.screenWidth;

	if (GuiButton((Rectangle){xPosScale * (float)context.screenWidth, 10, 55.0f, 30.0f}, "ROTATE")) {
		context.exportRotation = true;
	}

	if (GuiButton((Rectangle){xPosScale * (float)context.screenWidth, 50, 79.0f, 30.0f}, "ALL FRAMES")) {
		context.exportAllFrames = true;
	}

}

void ExportRenderTexturesToImages(const std::vector<Image>& renderedAnimationFrames){
 	for (int i = 0; i < renderedAnimationFrames.size(); i++) {
    	//Image image = LoadImageFromTexture(renderedAnimationFrames[i].texture);
    	//ImageFlipVertical(&image);
    	std::string fileName = std::string("/home/heindelj/my_render_frames");
    	std::string fileNumber = "";
		for(int j = numDigits(i); j < numDigits(renderedAnimationFrames.size()); j++)
			fileNumber += "0";
		ExportImage(renderedAnimationFrames[i], (fileName + "_" + fileNumber + std::to_string(i) + ".png").c_str());
    	UnloadImage(renderedAnimationFrames[i]);

	    // Use mutex with lock_guard to update status in a thread safe way.
	    //std::lock_guard<std::mutex> guard(mu);
	    //status = i;
  	}
}

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
				std::cout << "Warning: Failed to find animation position on iteration " << i << std::endl;
				break;
			}
		}
		cameraPositions.push_back(camera.position);
	}
	return cameraPositions;
}

Image DrawToRenderTexture(const RenderContext& renderContext, int width, int height) {
	RenderTexture2D renderTarget = LoadRenderTexture(width, height);
	BeginTextureMode(renderTarget);
	    ClearBackground(Color(1.0f,1.0f,1.0f,1.0f));  // Clear white texture background
	    BeginMode3D(renderContext.camera);
	    	renderContext.model->Draw();
	    EndMode3D();
	EndTextureMode();
	Image image = LoadImageFromTexture(renderTarget.texture);
	ImageFlipVertical(&image);
	UnloadRenderTexture(renderTarget);
	return image;
}

std::vector<Image> DrawRotationAnimationToRenderTextures(RenderContext& renderContext, int width, int height, int numFrames) {
	// Can convert the resulting image sequence to a gif with command: convert -delay 0 -loop 0 -alpha set -dispose 2 my_render_frames_*.png
	// add system() call to convert the gif all at once. Check that convert command is available.

	std::vector<Image> renderedAnimationFrames;
	renderedAnimationFrames.reserve(numFrames);

	std::vector<Vector3> animationPositions = GetRotationAnimationPositions(renderContext.camera, numFrames);
	for (int i = 0; i < animationPositions.size(); i++) {
		renderContext.camera.position = animationPositions[i];
		UpdateLighting(renderContext);
		renderedAnimationFrames.push_back(DrawToRenderTexture(renderContext, width, height));
	}
	return renderedAnimationFrames;
}

void AnimationModeFrame(MolecularModel& model, ActiveContext& context) {
	DrawAnimationUI(context); // change UI function

	BeginMode3D(context.renderContext.camera);
	    model.Draw();
	    if (context.drawGrid)
	    	DrawGrid(10, 1.0f);
	EndMode3D();

	HandleSelections(model, context);

	if (context.exportRotation) {
		
		std::vector<Image> renderedAnimationFrames = DrawRotationAnimationToRenderTextures(context.renderContext, 1600, 1600, 30);
		context.otherThread = std::thread(ExportRenderTexturesToImages, renderedAnimationFrames);
		//ExportRenderTexturesToImages(renderedAnimationFrames);
		//std::mutex mu;
		//std::atomic<bool> is_images_loaded = false;

		context.exportRotation = false;
	}
	if (context.exportAllFrames) {
		std::cout << "Implement this bruh!" << std::endl;
		context.exportAllFrames = false;
	}

}