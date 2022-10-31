#pragma once

void RotateAroundWorldUp(ActiveContext& context) {
	const Vector3 cameraForward = normalize(context.renderContext.camera.target - context.renderContext.camera.position);
	const Vector3 cameraRight = cross(cameraForward, context.renderContext.camera.up);

	context.renderContext.camera.position = context.renderContext.camera.position + context.rotationSpeed * cameraRight;
	UpdateLighting(context.renderContext);
}

bool rotateAroundTargetView(Camera3D& camera, Vector3 target) {
    Vector2 mouseDelta = GetMouseDelta();

    // The check at the end for magnitude is a hack to remove a bug where sometimes on the very first change of
    // mouse position, GetMouseDelta() will return an incorrect and large result. I'm not sure if it's a bug in
    // raylib or what?
    if ((mouseDelta.x != 0.0f || mouseDelta.y != 0.0f) && abs(mouseDelta.x) < 100 && abs(mouseDelta.y) < 100) {
	    Matrix V = MatrixLookAt(camera.position, camera.target, camera.up);
	    Vector3 pivot = ToVector3(V * (Vector4){target.x, target.y, target.z, 1.0f});
	    Vector3 axis  = (Vector3){mouseDelta.y, mouseDelta.x, 0.0f};
	    float angle = norm(mouseDelta);
	    Matrix R  = MatrixRotate(normalize(axis), angle * DEG2RAD);
	    Matrix RP = MatrixTranslate(-1 * pivot) * R * MatrixTranslate(pivot);
	    Matrix NV = V * RP;
	    Matrix C = MatrixInvert(NV);

	    float targetDist = norm(camera.target - camera.position);
	    camera.position  = (Vector3){C.m12, C.m13, C.m14};
	    camera.target    = camera.position - (targetDist * (Vector3){C.m8, C.m9, C.m10});
	    camera.up        = normalize((Vector3){C.m4, C.m5, C.m6});

	    return true;
	}
	return false;
}

bool zoomOnScroll(Camera3D& camera) {
	float scrollDistance = GetMouseWheelMove();
	if (scrollDistance) {
		const Vector3 cameraForward = camera.target - camera.position;
		if (norm(cameraForward) > 1.0f)
			camera.position += 0.75f * scrollDistance * normalize(cameraForward);
		else if (norm(cameraForward) <= 1.0f && scrollDistance < 0.0f)
			camera.position += 0.75f * scrollDistance * normalize(cameraForward);

		return true;
	}
	return false;
}

void UpdateCamera3D(ActiveContext& context) {
	/*
	Updates a Camera3d object in a "model viewer" style. That is, left click and drag rotates the model,
	scrolling zooms in and out, and right click drag pans the camera.
	*/

	// The update functions return if they were updated so that we can update the lights
	bool updatedCamera = false;
	if (IsMouseButtonDown(MOUSE_BUTTON_LEFT)){
		// Test for hitting mesh here to rotate around a specific target
		// and change to rotateAroundTargetView

		// Check that we arent clicking on the UI
		if ((context.drawUI == false) || GetMouseX() > (context.uiSettings.menuWidth + context.uiSettings.borderWidth))
			updatedCamera = rotateAroundTargetView(context.renderContext.camera, context.renderContext.camera.target);

	}
	updatedCamera |= zoomOnScroll(context.renderContext.camera);
	if (updatedCamera)
		UpdateLighting(context.renderContext);
}
