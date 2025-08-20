/*
* SDManager.ino
* 
* SD card management and WAV file operations
* 
* Handles:
* - SD card initialization and detection
* - WAV file creation with proper headers
* - File scanning and management
* - Directory operations
*/

#include "SongbirdRecorder.h"

void initializeSDCard()
{
  pinMode(SDCARD_DETECT_PIN,  INPUT_PULLUP);

  SPI.setMOSI(SDCARD_MOSI_PIN);
  SPI.setMISO(SDCARD_MISO_PIN);
  SPI.setSCK(SDCARD_SCK_PIN);

  // Check if SD card is physically present (LOW = card inserted)
  if (digitalRead(SDCARD_DETECT_PIN))  // HIGH = no card
  {
    Serial.println("No SD card detected");
    sdCardReady = false;
    return;
  }

  // Initialize the SD card
  if (!SD.begin(SDCARD_CS_PIN)) 
  {
    Serial.println("SD card initialization failed");
    sdCardReady = false;
    return;
  }
  
  createCallsDirectory();

  // Only mark as ready if directory creation succeeded
  if (sdCardReady)  // createCallsDirectory sets this to false on failure
  {
    // Scan for existing files
    scanForFiles();
    Serial.println("SD card initialized successfully");
  }
}

void createCallsDirectory()
{
  // Create CALLS directory if it doesn't already exist
  if (!SD.exists(CALLS_DIRECTORY)) 
  {
    if (SD.mkdir(CALLS_DIRECTORY)) 
    {
      Serial.println("Created CALLS directory");
      sdCardReady = true;
    }
    else
    {
      Serial.println("Failed to create CALLS directory");
      sdCardReady = false;
    }
  }
  else
  {
    // Directory already exists
    sdCardReady = true;
  }
}

void scanForFiles()
{
  if (!sdCardReady) return;

  totalFiles = 0;
  currentFileIndex = 0;
  currentFilename = "";

  // Count WAV files in CALLS directory
  File dir = SD.open(CALLS_DIRECTORY);

  if (dir) 
  {
    while (true) 
    {
      File entry = dir.openNextFile();
      if (!entry) break;

      String filename = String(entry.name());

      if (!entry.isDirectory() && filename.endsWith(".WAV")) 
      {
        totalFiles ++;
      }

      entry.close();
    }

    dir.close();
  }

  // Load first filename if files exist
  if (totalFiles > 0)
  {
    loadCurrentFilename();
  }

  Serial.print("Found ");
  Serial.print(totalFiles);
  Serial.println(" recording(s)");
}

void loadCurrentFilename()
{
  if (!sdCardReady || totalFiles == 0) 
  {
    currentFilename = "";
    return;
  }
  
  int fileCount = 0;
  File dir = SD.open(CALLS_DIRECTORY);

  if (dir) 
  {
    while (true)
    {
      File entry = dir.openNextFile();
      if (!entry) 
      { 
        break;
      }

      String filename = String(entry.name());

      if (!entry.isDirectory() && filename.endsWith(".WAV")) 
      {
        if (fileCount == currentFileIndex) 
        {
          currentFilename = filename;
          entry.close();
          break;
        }

        fileCount++;
      }

      entry.close();
    }

    dir.close();
  }
}

void createWAVFile(String filename)
{
  // Create the filepath
  String filepath = String(CALLS_DIRECTORY) + "/" + filename;

  // Create file
  recordingFile = SD.open(filepath.c_str(), FILE_WRITE);

  if (!recordingFile) 
  {
    Serial.print("Failed to create file: ");
    Serial.println(filepath);
    return;
  }

  // Create WAV header
  WAVHeader header;

  // RIFF header
  memcpy(header.riff, "RIFF", 4);
  header.fileSize = 0;  // Will be updated when recording finishes
  memcpy(header.wave, "WAVE", 4);

  // Format chunk
  memcpy(header.fmt, "fmt ", 4);
  header.fmtSize = 16;
  header.audioFormat = 1; // PCM
  header.numChannels = AUDIO_CHANNELS;
  header.sampleRate = RECORDING_SAMPLE_RATE;
  header.byteRate = RECORDING_SAMPLE_RATE * AUDIO_CHANNELS * (AUDIO_BITS_PER_SAMPLE / 8);
  header.blockAlign = AUDIO_CHANNELS * (AUDIO_BITS_PER_SAMPLE / 8);
  header.bitsPerSample = AUDIO_BITS_PER_SAMPLE;

  // Data chunk
  memcpy(header.data, "data", 4);
  header.dataSize = 0; // Will be updated when the recording finishes

  // Write header to file
  recordingFile.write((uint8_t*)&header, sizeof(WAVHeader));
  recordingFile.flush();

  // Reset the byte counter
  recordingBytesWritten = 0;

  Serial.print("Created WAV file: ");
  Serial.println(filepath);

  Serial.print("DEBUG: WAV header size: ");
  Serial.println(sizeof(WAVHeader));
  Serial.print("DEBUG: Sample rate: ");
  Serial.println(header.sampleRate);
  Serial.print("DEBUG: Channels: ");
  Serial.println(header.numChannels);
  Serial.print("DEBUG: Bits per sample: ");
  Serial.println(header.bitsPerSample);
}

void writeWAVData(byte *buffer, int length)
{
  if (!recordingFile || !sdCardReady) return;
  
  // Write audio data to file
  size_t bytesWritten = recordingFile.write(buffer, length);
  recordingBytesWritten += bytesWritten;

  // Flush periodically to ensure data is written
  static unsigned long lastFlush = 0;
  if (millis() - lastFlush > 1000) 
  {  
    // Flush every second
    recordingFile.flush();
    lastFlush = millis();
  }
}

void finalizeWAVFile()
{
  if (!recordingFile || !sdCardReady) return;
  
  // Calculate file sizes
  uint32_t fileSize = sizeof(WAVHeader) + recordingBytesWritten - 8;
  uint32_t dataSize = recordingBytesWritten;
  
  Serial.print("DEBUG: fileSize = ");
  Serial.print(fileSize);
  Serial.print(", dataSize = ");
  Serial.println(dataSize);
  
  // Update WAV header with actual sizes
  recordingFile.seek(4); // File size position
  size_t written1 = recordingFile.write((const uint8_t*)&fileSize, 4);
  Serial.print("DEBUG: Wrote fileSize, bytes written: ");
  Serial.println(written1);
  
  recordingFile.seek(40); // Data size position
  size_t written2 = recordingFile.write((const uint8_t*)&dataSize, 4);
  Serial.print("DEBUG: Wrote dataSize, bytes written: ");
  Serial.println(written2);
  
  // Close the file
  recordingFile.flush();
  recordingFile.close();
  
  Serial.print("WAV file finalized: ");
  Serial.print(recordingBytesWritten);
  Serial.println(" bytes of audio data");
}

void listFiles()
{
  if (!sdCardReady)
  {
    Serial.println("SD card not ready");
    return;
  }

  Serial.println("Recordings in CALLS directory:");

  File dir = SD.open(CALLS_DIRECTORY);

  if (dir)
  {
    int fileCount = 0;

    while (true)
    {
      File entry = dir.openNextFile();
      if (!entry) break;

      String filename = String(entry.name());

      if (!entry.isDirectory() && filename.endsWith(".WAV"))
      {
        Serial.print("  [");
        Serial.print(fileCount);
        Serial.print("] ");
        Serial.print(filename);
        Serial.print(" (");
        Serial.print(entry.size());
        Serial.println(" bytes)");
        fileCount++;
      }

      entry.close();
    }

    dir.close();

    if (fileCount == 0)
    {
      Serial.println("  No recordings found");
    }
  }
  else 
  {
    Serial.println("Failed to open CALLS directory");
  }
}

void deleteAll()
{
  // Check if SD card is ready before proceeding
  if (!sdCardReady)
  {
    Serial.println("SD card not ready");
    return;
  }
  
  Serial.println("Deleting recordings in CALLS directory:");
  
  // Open the CALLS directory
  File dir = SD.open(CALLS_DIRECTORY);
  if (dir)
  {
    int filesDeleted = 0;    // Count of successfully deleted files
    int filesFound = 0;      // Count of .WAV files found
    int deletionErrors = 0;  // Count of files that failed to delete
    
    // Iterate through all files in the directory
    while (true)
    {
      File entry = dir.openNextFile();
      if (!entry) break;  // No more files
      
      String filename = String(entry.name());
      
      // Process only .WAV files (skip directories and other file types)
      if (!entry.isDirectory() && filename.endsWith(".WAV"))
      {
        filesFound++;
        
        // Construct the full path for deletion
        String fullPath = String(CALLS_DIRECTORY) + "/" + filename;
        
        // Close the file before attempting deletion
        entry.close();
        
        // Attempt to delete the file
        if (SD.remove(fullPath.c_str()))
        {
          // Successful deletion
          Serial.println("  Deleted: " + filename);
          filesDeleted++;
        }
        else
        {
          // Failed to delete - log error and continue
          Serial.println("  ERROR: Failed to delete " + filename);
          deletionErrors++;
        }
      }
      else
      {
        // Close files that we're not processing
        entry.close();
      }
    }
    
    // Close the directory
    dir.close();
    
    // Print summary results
    Serial.println();
    if (filesFound == 0)
    {
      Serial.println("  No .WAV recordings found");
    }
    else
    {
      Serial.print("  Summary: ");
      Serial.print(filesDeleted);
      Serial.print(" files deleted");
      
      if (deletionErrors > 0)
      {
        Serial.print(", ");
        Serial.print(deletionErrors);
        Serial.print(" errors");
      }
      
      Serial.print(" (");
      Serial.print(filesFound);
      Serial.println(" total .WAV files found)");

      // Rescan files and update current selection
      scanForFiles();
    }
  }
  else
  {
    Serial.println("Failed to open CALLS directory");
  }
}

void deleteFile(String filename)
{
  if (!sdCardReady) 
  {
    Serial.println("SD card not ready");
    return;
  }
  
  // Create the filepath
  String filepath = String(CALLS_DIRECTORY) + "/" + filename;

  // Delete the file
  if (SD.remove(filepath.c_str()))
  {
    Serial.print("Deleted: ");
    Serial.println(filename);
    
    // Rescan files and update current selection
    scanForFiles();
  }
  else 
  {
    Serial.print("Failed to delete: ");
    Serial.println(filename);
  }
}

void checkSDCardStatus()
{
  bool previousState = sdCardReady;

  // Check physical detect pin (LOW = card inserted, HIGH = no card)
  bool cardPresent = !digitalRead(SDCARD_DETECT_PIN);

  if (!sdCardReady && cardPresent)
  {
    // Card is present but not initialized - try to initialize
    if (SD.begin(SDCARD_CS_PIN))
    {
      sdCardReady = true;
      createCallsDirectory();

      // Only continue if directory creation succeeded
      if (sdCardReady)
      {
        scanForFiles();
        Serial.println("SD card detected and initialized");
      }
    }
    else
    {
      Serial.println("SD card detected but initialization failed");
      sdCardReady = false;
    }
  }
  else if (sdCardReady && !cardPresent)
  {
    // Card was ready but is no longer present
    sdCardReady = false;
    totalFiles = 0;
    currentFilename = "";
    Serial.println("SD card removed");
  }
  else if (sdCardReady && cardPresent)
  {
    // Card should still be present and ready - verify it's accessible
    File testDir = SD.open("/");
    if (!testDir)
    {
      // Card present but not accessible - try to reinitialize
      sdCardReady = false;
      totalFiles = 0;
      currentFilename = "";
      Serial.println("SD card error - will retry initialization");
    }
    else
    {
      testDir.close();
    }
  }
}


