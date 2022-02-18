// Adapted from: https://github.com/sol-prog/cpp17-filewatcher/blob/master/FileWatcher.h

// Define available file changes
enum class FileStatus {created, modified, erased};

class FileWatcher {
public:
    std::string path_to_watch;
    // Time interval at which we check the base folder for changes
    std::chrono::duration<int, std::milli> delay;

    // Keep a record of files from the base directory and their last modification time
    FileWatcher(std::string path_to_watch, std::chrono::duration<int, std::milli> delay) : path_to_watch{path_to_watch}, delay{delay} {
        for(auto &file : std::filesystem::recursive_directory_iterator(path_to_watch)) {
            paths_[file.path().string()] = std::filesystem::last_write_time(file);
        }
    }

    // Monitor "path_to_watch" for changes and in case of a change execute the user supplied "action" function
    void start(const std::function<void (std::string, FileStatus)> &action) {
        while(running_) {
            // Wait for "delay" milliseconds
            std::this_thread::sleep_for(delay);

            auto it = paths_.begin();
            while (it != paths_.end()) {
                if (!std::filesystem::exists(it->first)) {
                    action(it->first, FileStatus::erased);
                    it = paths_.erase(it);
                }
                else {
                    it++;
                }                    
            }

            // Check if a file was created or modified
            // TODO: Should verify this file was one of the ones we've previously opened
            // and only do something for those. Just ignore any other files that get created.
            for(auto &file : std::filesystem::recursive_directory_iterator(path_to_watch)) {
                auto current_file_last_write_time = std::filesystem::last_write_time(file);

                // File creation
                if(!contains(file.path().string())) {
                    paths_[file.path().string()] = current_file_last_write_time;
                    action(file.path().string(), FileStatus::created);
                // File modification
                } else {
                    if(paths_[file.path().string()] != current_file_last_write_time) {
                        paths_[file.path().string()] = current_file_last_write_time;
                        action(file.path().string(), FileStatus::modified);
                    }
                }
            }
        }
    }
private:
    std::unordered_map<std::string, std::filesystem::file_time_type> paths_;
    bool running_ = true;

    // Check if "paths_" contains a given key
    // If your compiler supports C++20 use paths_.contains(key) instead of this function
    bool contains(const std::string &key) {
        auto el = paths_.find(key);
        return el != paths_.end();
    }
};

bool CheckForFileChangesAndUpdate(Frames* frames) {
	bool didUpdate = false;
	for (const auto& [ currentFile, currentFileStoredWriteTime ] : frames->loadedFiles) {
		try {
			const std::filesystem::path file = currentFile;
			auto currentFileLastWriteTime = std::filesystem::last_write_time(file);
				if (currentFileStoredWriteTime != currentFileLastWriteTime) {
				// File has been modified. We could just reload the file, but in general this will be too slow.
				// Instead, we should probably find a way to do this in a thread and return a future.
				// Also, we should try to seek to the last file position and start reading from there.
				// We also need to disambiguate the cases when the file was appended to versus being overwritten.
				// I don't know how to differentiate these two cases in general without reading the whole file...
				//
				// For now, I'm just going to re-read the file and start from scratch.
				// Feb. 17 2022

				didUpdate = true;

				Frames newFrames = readXYZ(file.string());
				frames->nframes = newFrames.nframes;
				frames->atoms = newFrames.atoms;
				frames->headers = newFrames.headers;
				frames->loadedFiles[file.string()] = currentFileLastWriteTime;
			}
		} catch(std::filesystem::filesystem_error const& ex) {
			// If we get here it's because the file has just been updated and the file is being re-written
			// so it will sometimes appear to be deleted, presumably because the OS creates a temp file
			// I'm not completely sure of the problem, but this fixes it.

			// This also handles the case that one of the file we originally read gets deleted.
			// We should handle that case separately perhaps.
			return false;
		}
	}
	return didUpdate;
}