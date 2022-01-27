#pragma once

void rotateAroundTargetView(Camera3D& camera, Vector3 target) {
    Vector2 mouseDelta = GetMouseDelta();

    if (mouseDelta.x != 0.0f || mouseDelta.y != 0.0f) {

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
	}
}

void zoomOnScroll(Camera3D& camera) {
	float scrollDistance = GetMouseWheelMove();
	if (scrollDistance) {
		camera.fovy -= 2 * scrollDistance;
		camera.fovy <= 10 ? camera.fovy = 10 : camera.fovy = camera.fovy;
		camera.fovy >= 178 ? camera.fovy = 178 : camera.fovy = camera.fovy;
	}
}

void updateCamera3D(Camera3D& camera) {
	/*
	Updates a Camera3d object in a "model viewer" style. That is, left click and drag rotates the model,
	scrolling zooms in and out, and right click drag pans the camera.
	*/
	if (IsMouseButtonDown(MOUSE_BUTTON_LEFT)){
		// Test for hitting mesh here to rotate around a specific target
		// and change to rotateAroundTargetView
		rotateAroundTargetView(camera, camera.target);
	}
	zoomOnScroll(camera);
}
