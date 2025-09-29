/*
 * PlaybackEngine.cpp
 */

#include "PlaybackEngine.h"

PlaybackEngine::PlaybackEngine()
{
    fileList = nullptr;
    fileCount = 0;
    currentFileIndex = 0;
}

bool PlaybackEngine::begin()
{
    loadFileList();
    DEBUG_PRINTLN("Playback Engine initialized");
    return true;
}

bool PlaybackEngine::loadFileList()
{
    // Clean up the old list
    cleanupFileList();

    // Open the recording directory
    File recordingsDirectory = SD.open(RECORDINGS_DIR);
    if (!recordingsDirectory)
    {
        DEBUG_PRINTLN("Failed to open recording directory");
        return false;
    }

    // Count WAV files first
    String tempList[MAX_FILES_TO_SCAN];
    uint32_t count = 0;

    while (count < MAX_FILES_TO_SCAN)
    {
        File entry = recordingsDirectory.openNextFile();
        if (!entry) {
            break;
        }

        String entryName = entry.name();
        entry.close();

        // Check for WAV extension and our naming pattern
        if (entryName.endsWith(FILE_EXTENSION) && entryName.startsWith(FILE_PREFIX))
        {
            // Store full path
            tempList[count] = String(RECORDINGS_DIR) + "/" + entryName;
            count++;
        }
    }

    recordingsDirectory.close();

    if (count == 0) {
        DEBUG_PRINTLN("No WAV files found");
        return false;
    }

    // Allocate and copy to permanent storage
    fileList = new String[count];
    if (!fileList) {
        DEBUG_PRINTLN("Failed to allocate file list");
        return false;
    }

    for (uint32_t i = 0; i < count; i++) {
        fileList[i] = tempList[i];
    }

    fileCount = count;
    currentFileIndex = fileCount - 1;  // Start with most recent file

    DEBUG_PRINTF("Loaded %d files for playback\n", fileCount);
    return true;
}

bool PlaybackEngine::selectFile(uint32_t index)
{
    if (index >= fileCount) return false;
    currentFileIndex = index;
    return true;
}

bool PlaybackEngine::selectNextFile()
{
    if (fileCount == 0) return false;
    
    currentFileIndex++;
    if (currentFileIndex >= fileCount) {
        currentFileIndex = 0;  // Wrap to beginning
    }
    
    return true;
}

bool PlaybackEngine::selectPreviousFile()
{
    if (fileCount == 0) return false;
    
    if (currentFileIndex == 0) {
        currentFileIndex = fileCount - 1;  // Wrap to end
    } else {
        currentFileIndex--;
    }
    
    return true;
}

String PlaybackEngine::getCurrentFileName() const
{
    if (fileCount == 0 || currentFileIndex >= fileCount) {
        return "";
    }

    // Return just the filename without the path
    String fullPath = fileList[currentFileIndex];
    int lastSlash = fullPath.lastIndexOf('/');
    
    if (lastSlash >= 0) {
        return fullPath.substring(lastSlash + 1);
    }
    
    return fullPath;
}

bool PlaybackEngine::startPlayback(AudioPlaySdWav* playWav)
{
    if (fileCount == 0) {
        DEBUG_PRINTLN("No files to play");
        return false;
    }

    if (!playWav) {
        DEBUG_PRINTLN("AudioPlaySdWav object is null");
        return false;
    }

    // Stop any current playback
    if (playWav->isPlaying()) {
        playWav->stop();
    }

    // Start playing the current file
    if (playWav->play(fileList[currentFileIndex].c_str())) 
    {
        digitalWrite(HPAMP_SHUTDOWN, LOW); // Enable amp when starting playback
        DEBUG_PRINTF("Started playback: %s\n", fileList[currentFileIndex].c_str());
        
        // Wait a moment for the file info to be available
        delay(10);
        
        return true;
    }

    DEBUG_PRINTLN("Playback failed to start");
    return false;
}

bool PlaybackEngine::stopPlayback(AudioPlaySdWav* playWav)
{
    if (!playWav) return false;
    
    if (playWav->isPlaying()) {
        playWav->stop();  
        digitalWrite(HPAMP_SHUTDOWN, HIGH);  // Disable amp when stopping playback
        DEBUG_PRINTLN("Playback stopped");
        return true;
    }
    
    return false;
}

uint32_t PlaybackEngine::getPlaybackPosition(AudioPlaySdWav* playWav) const
{
    if (!playWav || !playWav->isPlaying()) {
        return 0;
    }
    
    return playWav->positionMillis();
}

uint32_t PlaybackEngine::getFileDuration(AudioPlaySdWav* playWav) const
{
    if (!playWav) {
        return 0;
    }
    
    return playWav->lengthMillis();
}

void PlaybackEngine::cleanupFileList()
{
    if (fileList) {
        delete[] fileList;
        fileList = nullptr;
    }
    fileCount = 0;
    currentFileIndex = 0;
}