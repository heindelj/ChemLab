#pragma once

void rotateAroundTargetView(Camera3D& camera, Vector3 target) {
    Vector2 mouseDelta = GetMouseDelta();

    if (mouseDelta.x != 0.0f || mouseDelta.y != 0.0f) {

	    glm::mat4 V = glm::lookAt(fromVector3(camera.position), fromVector3(camera.target), fromVector3(camera.up));

	    glm::vec3 pivot = glm::vec3(V * glm::vec4(target.x, target.y, target.z, 1.0f));
	    glm::vec3 axis  = glm::vec3(mouseDelta.y, mouseDelta.x, 0);
	    float angle = glm::length(fromVector2(mouseDelta));

	    glm::mat4 R  = glm::rotate( glm::mat4(1.0f), glm::radians(angle), axis );
	    glm::mat4 RP = glm::translate(glm::mat4(1.0f), pivot) * R * glm::translate(glm::mat4(1), -pivot);
	    glm::mat4 NV = RP * V;

	    glm::mat4 C = glm::inverse(NV);
	    float targetDist  = glm::length(fromVector3(camera.target) - fromVector3(camera.position));
	    camera.position = (Vector3){C[3].x, C[3].y, C[3].z};
	    camera.target   = fromVector3(fromVector3(camera.position) - glm::vec3(C[2]) * targetDist);
	    camera.up       = (Vector3){C[1].x, C[1].y, C[1].z};
	}
}

void rotateAroundOrigin(Camera3D& camera){
	rotateAroundTargetView(camera, (Vector3){0.0f, 0.0f, 0.0f});
}

void zoomOnScroll(Camera3D& camera) {
	float scrollDistance = GetMouseWheelMove();

	// As a note: forwardVector = target - position
	glm::vec3 deltaPosition = scrollDistance * glm::normalize((fromVector3(camera.target) - fromVector3(camera.position)));
	camera.position = fromVector3(fromVector3(camera.position) + deltaPosition);
	// check if a move changes sign of x, y, and z. If so, don't let the move happen.
}

void updateCamera3D(Camera3D& camera) {
	/*
	Updates a Camera3d object in a "model viewer" style. That is, left click and drag rotates the model,
	scrolling zooms in and out, and right click drag pans the camera.
	*/
	if (IsMouseButtonDown(MOUSE_BUTTON_LEFT)){
		// Test for hitting mesh here to rotate around a specific target
		// and change to rotateAroundTargetView
		rotateAroundOrigin(camera);
	}
	zoomOnScroll(camera);
}
