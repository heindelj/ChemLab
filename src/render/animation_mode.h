#pragma once

void DrawAnimationUI(ActiveContext& context) {
	
	// Draw text label for settings
	const char* title = "SETTINGS";
	const int fontSize = 30;
	DrawText(title, context.uiSettings.menuWidth / 2 - MeasureText(title, fontSize) / 2,   (context.screenHeight - context.uiSettings.borderWidth) / 2, fontSize, BLACK);
	DrawRectangle(context.uiSettings.menuWidth / 2 - MeasureText(title, fontSize) / 2 - 5, (context.screenHeight - context.uiSettings.borderWidth) / 2 + fontSize, MeasureText(title, fontSize) + 10, 5, BLACK); // underline text
	
	GuiSetStyle(BUTTON, TEXT_ALIGNMENT, GUI_TEXT_ALIGN_CENTER);
	if (GuiButton((Rectangle){context.uiSettings.menuWidth / 2, (context.screenHeight - context.uiSettings.borderWidth) / 2 + 2 * fontSize, 55.0f, 30.0f}, "ROTATE")) {
		context.exportRotation = true;
	}
	if (GuiButton((Rectangle){context.uiSettings.menuWidth / 2, (context.screenHeight - context.uiSettings.borderWidth) / 2 + 4 * fontSize,75.0f, 30.0f}, "ALL FRAMES")) {
		context.exportAllFrames = true;
	}

}

void ExportRenderTexturesToImages(const std::vector<Image>& renderedAnimationFrames, const std::string& fileName){
 	for (int i = 0; i < renderedAnimationFrames.size(); i++) {
    	std::string fileNumber = "";
		for(int j = numDigits(i); j < numDigits(renderedAnimationFrames.size()); j++)
			fileNumber += "0";
		ExportImage(renderedAnimationFrames[i], (fileName + "_" + fileNumber + std::to_string(i) + ".png").c_str());
    	UnloadImage(renderedAnimationFrames[i]);

	    // Use mutex with lock_guard to update status in a thread safe way.
	    //std::lock_guard<std::mutex> guard(mu);
	    //status = i;
  	}
  	// try to convert the pngs to a gif. If successful just delete the pngs.
  	int exitCode = system((std::string("convert -delay 0 -loop 0 -alpha set -dispose 2 -resize 50% ") + fileName + "*.png " + fileName + std::string(".gif")).c_str());
  	if (exitCode != -1) {
  		system((std::string("rm ") + fileName + std::string("*.png")).c_str());
	} else {
		std::cout << "INFO: Failed to directly convert png images to a gif using ImageMagick's convert function. You will need to do it manually or install ImageMgick and try again." << std::endl;
	}
}

void ExportRenderTextureToImage(Image& image, std::string fileName) {
	ExportImage(image, fileName.c_str());
	UnloadImage(image);
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
	Vector3 originalPosition = renderContext.camera.position;

	std::vector<Image> renderedAnimationFrames;
	renderedAnimationFrames.reserve(numFrames);

	std::vector<Vector3> animationPositions = GetRotationAnimationPositions(renderContext.camera, numFrames);
	for (int i = 0; i < animationPositions.size(); i++) {
		renderContext.camera.position = animationPositions[i];
		UpdateLighting(renderContext);
		renderedAnimationFrames.push_back(DrawToRenderTexture(renderContext, width, height));
	}

	renderContext.camera.position = originalPosition;
	UpdateLighting(renderContext);

	return renderedAnimationFrames;
}

std::vector<Image> DrawAllFramesToRenderTextures(ActiveContext& context, int width, int height) {
	int originalFrame = context.activeFrame;

	std::vector<Image> renderedAnimationFrames;
	renderedAnimationFrames.reserve(context.numFrames);

	for (int i = 0; i < context.numFrames; i++) {
		context.activeFrame = i;
		OnFrameChange(context);
		renderedAnimationFrames.push_back(DrawToRenderTexture(context.renderContext, width, height));
	}

	context.activeFrame = originalFrame;
	OnFrameChange(context);

	return renderedAnimationFrames;
}

void TakeScreenshot(ActiveContext& context, std::string& fileName, int width, int height) {
	// Make sure the filename is stripped of an extension before you egt here and that a dialog
	// is used to ask for the width and height
	Image image = DrawToRenderTexture(context.renderContext, width, height);
	ExportRenderTextureToImage(image, (fileName + ".png"));
}

void AnimationModeFrame(MolecularModel& model, ActiveContext& context) {

	BeginMode3D(context.renderContext.camera);
	    model.Draw();
	    if (context.drawGrid)
	    	DrawGrid(10, 1.0f);
	EndMode3D();

	HandleSelections(model, context);

	// Change this to isExporting and check which image we're on to draw the progress bar
	if (context.exportRotation) {
		char* selectedSaveName = tinyfd_saveFileDialog("Select File Name", "rotation_animation.png", 0, NULL, NULL);		
		if (selectedSaveName) {
			std::filesystem::path p = selectedSaveName;
			std::vector<Image> renderedAnimationFrames = DrawRotationAnimationToRenderTextures(context.renderContext, 1600, 1600, 50);
			context.computeThreads.push_back(std::thread(ExportRenderTexturesToImages, renderedAnimationFrames, p.replace_extension()));
		}

		context.exportRotation = false;
	}
	if (context.exportAllFrames) {
		char* selectedSaveName = tinyfd_saveFileDialog("Select File Name", "all_frames_animation.png", 0, NULL, NULL);
		if (selectedSaveName) {
			std::filesystem::path p = selectedSaveName;
			std::vector<Image> renderedAnimationFrames = DrawAllFramesToRenderTextures(context, 1600, 1600);
			context.computeThreads.push_back(std::thread(ExportRenderTexturesToImages, renderedAnimationFrames, p.replace_extension()));
		}
		context.exportAllFrames = false;
	}

}