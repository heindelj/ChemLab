// Here is where we handle higher-level input such as:
// switching modes, getting a console (if we ever do that), etc.

bool CheckForFrameChange(ActiveContext& context) {
	// Handle switching active frame on key press
	if (IsKeyPressed(KEY_RIGHT)) {
		(context.activeFrame + 1) < context.numFrames ? context.activeFrame += 1 : context.activeFrame = context.activeFrame;
		return true;
	}
	else if (IsKeyPressed(KEY_LEFT)) {
		context.activeFrame > 0 ? context.activeFrame -= 1 : context.activeFrame = 0;
		return true;
	}
	return false;
}